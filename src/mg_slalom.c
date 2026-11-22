#pragma bank 255
// CONE SLALOM (MG_SLALOM): dribble the ball through timed cone gates on a
// vertical-scrolling training pitch. Trains REFLEX (all XP) + CONTROL (half).
//
// Controls:
//   LEFT / RIGHT   steer across the lane
//   UP / DOWN      sprint faster / ease off for tight turns
//   A              quick cut / tap in your D-pad direction (short cooldown)
//   B (hold)       close-control trap (cushions the ball tight to your feet)
//
// Heavy touches: sharp cuts at high speed knock the ball loose ahead/sideways;
// chase it down or tap A/B before it clips a cone or rolls out of bounds.
// Clearing gates builds a streak (x2 bonus at 3+ consecutive clean gates).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <string.h>
#include "gfx.h"
#include "state.h"
#include "input.h"
#include "sfx.h"
#include "game.h"
#include "minigame.h"
#include "progress.h"
#include "mg_slalom_bg.h"
#include "mg_slalom_spr.h"

// --- geometry (screen px, 160x136 playfield above HUD) ---
#define LANE_X0 26
#define LANE_X1 134
#define OUT_X0_PX 20
#define OUT_X1_PX 140
#define PLAYER_Y 108
#define BALL_HOLD_DY 24  // ball sits ahead of the player, clearly visible in front of them

#define GATE_MAX 3
#define GATE_ACTIVE 0
#define GATE_CLEARED 1
#define GATE_MISSED 2

#define RUN_SECS 45
#define RUN_FRAMES (RUN_SECS * 60)

// Sprite tiles in VRAM bank 0 (loaded at MG_SPR_TILES = 16, 2 tiles per 8x16).
#define T_CONE (MG_SPR_TILES + 0)
#define T_CONE_HIT (MG_SPR_TILES + 2)
#define T_CHEVRON (MG_SPR_TILES + 4)
#define T_PLUS1 (MG_SPR_TILES + 6)
#define T_PLUS2 (MG_SPR_TILES + 8)
#define T_HIT_L (MG_SPR_TILES + 10)
#define T_HIT_R (MG_SPR_TILES + 12)

// OAM layout (OAM_MG_FREE = 14 .. 39).
#define OAM_GATES OAM_MG_FREE        // 3 gates * 3 sprites (L cone, R cone, chevron) = 14..22
#define OAM_POPUP (OAM_MG_FREE + 9)  // 2 sprites: 23..24
#define OAM_TRAIL (OAM_MG_FREE + 11) // 3 sprites: 25..27

// --- watchable state (non-static for tests) ---
uint16_t mg_slalom_points;
uint16_t mg_slalom_clock;      // frames remaining (2700 -> 0)
uint8_t mg_slalom_gates;       // gates cleared
uint8_t mg_slalom_streak;      // consecutive clean gates
uint8_t mg_slalom_best_streak;
uint8_t mg_slalom_hits;        // cones bumped
uint8_t mg_slalom_outs;        // balls lost out of bounds
uint8_t mg_slalom_misses;      // gates missed
uint8_t mg_slalom_loose;       // 1 when a heavy touch knocked the ball loose
uint8_t mg_slalom_cut_cd;      // cooldown frames for A quick-cut
uint8_t mg_slalom_stumble;     // recovery frames after cone bump / out
uint8_t mg_slalom_steer_spd;   // lateral speed (SUB/frame, scaled by REFLEX)
uint8_t mg_slalom_ctrl;        // control bonus (scaled by CONTROL)
int16_t mg_slalom_px;          // player x (SUB)
int16_t mg_slalom_pvx;         // player lateral velocity (SUB/frame)
int16_t mg_slalom_bx, mg_slalom_by;    // ball screen pos (SUB)
int16_t mg_slalom_bvx, mg_slalom_bvy;  // ball velocity (SUB/frame)
uint8_t mg_slalom_spd;         // forward course speed (SUB/frame)
int16_t mg_slalom_gy[GATE_MAX];        // gate y (SUB)
uint8_t mg_slalom_gx[GATE_MAX];        // gate center x (px)
uint8_t mg_slalom_gw[GATE_MAX];        // gate half-width (px)
uint8_t mg_slalom_gst[GATE_MAX];       // GATE_ACTIVE / CLEARED / MISSED
uint8_t mg_slalom_ghit[GATE_MAX];      // hit animation timer per gate
uint8_t mg_slalom_next_side;           // alternates left/right slalom bias

// --- local visual state ---
static uint16_t scroll_sub;
static uint8_t hud_sec, hud_mult;
static uint16_t hud_pts;
static uint8_t pop_kind, pop_timer;  // 1=+1, 2=+2, 3=HIT!
static int16_t pop_x, pop_y;
static uint8_t kick_anim;
static uint8_t heavy_ticks;  // consecutive high-speed turn frames
static uint8_t loose_lock;   // brief frames before a knocked-loose ball can re-collect
static int8_t last_dir;

// --- Mario Tennis GBC style palettes ---
#define C(r, g, b) RGB8(r, g, b)
// BG pal 0: bright emerald turf, darker mowed stripe, forest apron, crisp white lines
static const uint16_t pal_turf[4] = {C(32, 176, 80), C(24, 144, 64), C(16, 80, 40), C(248, 252, 248)};
// BG pal 1: dark navy frame, crimson board, gold rail/rivets, bright cream highlight
static const uint16_t pal_boards[4] = {C(24, 24, 56), C(200, 48, 64), C(248, 192, 48), C(255, 244, 216)};
// OBJ pal 3 (SPR_PAL_TRAIL_POWER): bright white reflective stripe, vivid orange cone, dark ink
static const uint16_t pal_cone[4] = {0, C(255, 255, 255), C(248, 104, 24), C(24, 20, 32)};
// OBJ pal 4 (SPR_PAL_TRAIL_CURVE): white core, glowing gold/lime chevron & popups, dark ink
static const uint16_t pal_gate[4] = {0, C(255, 255, 224), C(254, 208, 40), C(24, 20, 32)};

static int16_t ab16(int16_t v) { return v < 0 ? -v : v; }

static uint8_t num_cat(char *buf, uint8_t at, uint16_t n) {
    char tmp[5];
    uint8_t k = 0;
    do tmp[k++] = '0' + (uint8_t)(n % 10), n /= 10; while (n);
    while (k) buf[at++] = tmp[--k];
    buf[at] = 0;
    return at;
}

static uint8_t str_cat(char *buf, uint8_t at, const char *s) {
    while (*s) buf[at++] = *s++;
    buf[at] = 0;
    return at;
}

static void show_popup(uint8_t kind, int16_t x, int16_t y) {
    pop_kind = kind;
    pop_timer = 28;
    pop_x = x;
    pop_y = y;
}

// Half-width of a new gate narrows gradually as more gates are cleared.
static uint8_t gate_half_width(void) {
    uint8_t shrink = mg_slalom_gates >> 1;
    if (shrink > 8) shrink = 8;
    return 22 - shrink;  // 22 px (44 px gap) down to 14 px (28 px gap)
}

// Vertical spacing between gates (px) tightens slightly with progress.
static int16_t gate_spacing_px(void) {
    uint8_t tighten = mg_slalom_gates;
    if (tighten > 14) tighten = 14;
    return 56 - tighten;  // 56 px down to 42 px
}

static void spawn_gate(uint8_t i, int16_t y_px) {
    uint8_t gw = gate_half_width();
    uint8_t lo = LANE_X0 + gw + 4;
    uint8_t hi = LANE_X1 - gw - 4;
    uint8_t mid = 80;
    uint8_t gx;
    // Alternate left / right halves so it's a real slalom weave.
    if (mg_slalom_next_side == 0) {
        uint8_t span = mid > lo ? mid - lo : 1;
        gx = lo + (uint8_t)(rand() & 31);
        if (gx > mid - 4) gx = lo + (gx % span);
    } else {
        uint8_t span = hi > mid ? hi - mid : 1;
        gx = mid + 4 + (uint8_t)(rand() & 31);
        if (gx > hi) gx = mid + (gx % span);
    }
    if (gx < lo) gx = lo;
    if (gx > hi) gx = hi;
    mg_slalom_next_side ^= 1;
    mg_slalom_gx[i] = gx;
    mg_slalom_gw[i] = gw;
    mg_slalom_gy[i] = TO_SUB(y_px);
    mg_slalom_gst[i] = GATE_ACTIVE;
    mg_slalom_ghit[i] = 0;
}

static void hud_draw(uint8_t force) {
    // 60 frames/sec; approximate remaining seconds cleanly without division per frame:
    // keep hud_sec tracked, or update every 60 frames via a counter.
    uint8_t mult = mg_slalom_streak >= 3 ? 2 : 1;
    if (force) {
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "TIME");
        win_text(8, 0, "PTS");
        win_text(17, 0, "X");
        hud_pts = 0xFFFF;
        hud_mult = 0xFF;
    }
    if (force || hud_pts != mg_slalom_points) {
        hud_pts = mg_slalom_points;
        win_number(12, 0, mg_slalom_points, 3, 0);
    }
    if (force || hud_mult != mult) {
        hud_mult = mult;
        win_number(18, 0, mult, 1, 0);
    }
    win_number(5, 0, hud_sec, 2, 0);
}

static void reset_ball_to_feet(void) {
    mg_slalom_bx = mg_slalom_px;
    mg_slalom_by = TO_SUB(PLAYER_Y - BALL_HOLD_DY);
    mg_slalom_bvx = 0;
    mg_slalom_bvy = 0;
    mg_slalom_loose = 0;
    heavy_ticks = 0;
    loose_lock = 0;
}

static void draw_scene(void) {
    uint8_t frame = MG_F_STAND;
    if (mg_run == MG_RUN_LIVE || mg_run == MG_RUN_OVER) {
        if (mg_slalom_stumble) frame = MG_F_LOSE;
        else if (kick_anim) frame = MG_F_KICK;
        else frame = ((uint8_t)mg_timer & 8) ? MG_F_RUN_A : MG_F_RUN_B;
    }
    uint8_t flip = mg_slalom_pvx < 0 ? 1 : 0;
    uint8_t ppal = (mg_slalom_stumble && ((uint8_t)mg_timer & 2)) ? SPR_PAL_FLASH : SPR_PAL_YOU;
    mg_player(0, MG_FRAME(MG_VIEW_BACK, frame), PX(mg_slalom_px), PLAYER_Y, ppal, flip);

    // Ball: glows fire when on a 3+ gate streak!
    uint8_t bpal = (mg_slalom_streak >= 3 && !mg_slalom_loose) ? SPR_PAL_POWER : SPR_PAL_BALL;
    mg_ball(PX(mg_slalom_bx), PX(mg_slalom_by) + 4, mg_slalom_loose ? 2 : 0, 1, bpal);

    // Find the lowest (closest) active gate to show the target chevron.
    uint8_t nearest = 0xFF;
    int16_t best_y = -32000;
    for (uint8_t i = 0; i < GATE_MAX; i++) {
        if (mg_slalom_gst[i] == GATE_ACTIVE && mg_slalom_gy[i] > best_y) {
            best_y = mg_slalom_gy[i];
            nearest = i;
        }
    }

    // Draw the 3 cone gates.
    for (uint8_t i = 0; i < GATE_MAX; i++) {
        uint8_t o = OAM_GATES + i * 3;
        int16_t gy = PX(mg_slalom_gy[i]);
        if (gy < -12 || gy > 142) {
            mg_hide(o, o + 2);
            continue;
        }
        uint8_t tile = mg_slalom_ghit[i] ? T_CONE_HIT : T_CONE;
        uint8_t cpal = (mg_slalom_gst[i] == GATE_CLEARED) ? SPR_PAL_TRAIL_CURVE : SPR_PAL_TRAIL_POWER;
        int16_t lx = (int16_t)mg_slalom_gx[i] - (int16_t)mg_slalom_gw[i] - 4;
        int16_t rx = (int16_t)mg_slalom_gx[i] + (int16_t)mg_slalom_gw[i] - 4;
        mg_sprite(o, tile, cpal, lx, gy - 12);
        mg_sprite(o + 1, tile, cpal, rx, gy - 12);
        if (i == nearest && ((uint8_t)frame_count & 8)) {
            mg_sprite(o + 2, T_CHEVRON, SPR_PAL_TRAIL_CURVE, (int16_t)mg_slalom_gx[i] - 4, gy - 10);
        } else {
            move_sprite(o + 2, 0, 0);
        }
    }

    // Floating popup (+1 / +2 / HIT!).
    if (pop_timer) {
        int16_t py = pop_y - ((28 - pop_timer) >> 2);
        if (pop_kind == 1) {
            mg_sprite(OAM_POPUP, T_PLUS1, SPR_PAL_TRAIL_CURVE, pop_x - 4, py);
            move_sprite(OAM_POPUP + 1, 0, 0);
        } else if (pop_kind == 2) {
            mg_sprite(OAM_POPUP, T_PLUS2, SPR_PAL_POWER, pop_x - 4, py);
            move_sprite(OAM_POPUP + 1, 0, 0);
        } else {
            mg_sprite(OAM_POPUP, T_HIT_L, SPR_PAL_TRAIL_POWER, pop_x - 8, py);
            mg_sprite(OAM_POPUP + 1, T_HIT_R, SPR_PAL_TRAIL_POWER, pop_x, py);
        }
    } else {
        mg_hide(OAM_POPUP, OAM_POPUP + 1);
    }

    // Subtle trail dot behind a loose / quick-cut ball.
    if (mg_slalom_loose || kick_anim) {
        mg_sprite(OAM_TRAIL, MG_T_TRAIL + 4, SPR_PAL_TRAIL_CURVE,
                  PX(mg_slalom_bx - mg_slalom_bvx) - 4, PX(mg_slalom_by - mg_slalom_bvy) - 2);
    } else {
        move_sprite(OAM_TRAIL, 0, 0);
    }
    mg_hide(OAM_TRAIL + 1, OAM_TRAIL + 2);
}

void mg_slalom_enter(void) BANKED {
    uint8_t map_buf[MG_SLALOM_BG_MAP_W * MG_SLALOM_BG_MAP_H];
    uint8_t attr_buf[MG_SLALOM_BG_MAP_W * MG_SLALOM_BG_MAP_H];

    memcpy(&pal_base_bg[0], pal_turf, sizeof(pal_turf));
    memcpy(&pal_base_bg[4], pal_boards, sizeof(pal_boards));
    memcpy(&pal_base_obj[SPR_PAL_TRAIL_POWER << 2], pal_cone, sizeof(pal_cone));
    memcpy(&pal_base_obj[SPR_PAL_TRAIL_CURVE << 2], pal_gate, sizeof(pal_gate));

    // Tile the seamless 20x4 track 8 times vertically across the 32-row BG map.
    banked_bkg_data(SCENE_TILES, MG_SLALOM_BG_TILE_COUNT, mg_slalom_bg_tiles, BANK(mg_slalom_bg));
    banked_memcpy(map_buf, mg_slalom_bg_map, sizeof(map_buf), BANK(mg_slalom_bg));
    banked_memcpy(attr_buf, mg_slalom_bg_attr, sizeof(attr_buf), BANK(mg_slalom_bg));
    for (uint8_t ty = 0; ty < 32; ty += MG_SLALOM_BG_MAP_H) {
        set_bkg_tiles(0, ty, MG_SLALOM_BG_MAP_W, MG_SLALOM_BG_MAP_H, map_buf);
    }
    VBK_REG = 1;
    for (uint8_t ty = 0; ty < 32; ty += MG_SLALOM_BG_MAP_H) {
        set_bkg_tiles(0, ty, MG_SLALOM_BG_MAP_W, MG_SLALOM_BG_MAP_H, attr_buf);
    }
    VBK_REG = 0;

    mg_load_spr(mg_slalom_spr_tiles, MG_SLALOM_SPR_TILE_COUNT, BANK(mg_slalom_spr));

    // Stat scaling: REFLEX boosts steering speed; CONTROL tightens dribble touch.
    uint8_t rf = stat_lv[STAT_REFLEX];
    uint8_t ct = stat_lv[STAT_CONTROL];
    mg_slalom_steer_spd = 20 + (rf >> 1);  // 20 (lv 1) .. 30 (lv 20) SUB/frame
    mg_slalom_ctrl = ct;                   // 1 .. 20

    mg_slalom_points = 0;
    mg_slalom_clock = RUN_FRAMES;
    hud_sec = RUN_SECS;
    mg_slalom_gates = 0;
    mg_slalom_streak = 0;
    mg_slalom_best_streak = 0;
    mg_slalom_hits = 0;
    mg_slalom_outs = 0;
    mg_slalom_misses = 0;
    mg_slalom_cut_cd = 0;
    mg_slalom_stumble = 0;
    mg_slalom_spd = 14;
    mg_slalom_px = TO_SUB(80);
    mg_slalom_pvx = 0;
    mg_slalom_next_side = rand() & 1;
    reset_ball_to_feet();

    scroll_sub = 0;
    scroll_y = 0;
    pop_kind = pop_timer = 0;
    kick_anim = 0;
    heavy_ticks = 0;
    last_dir = 0;

    // Place 3 initial gates staggered up the pitch.
    spawn_gate(0, 48);
    spawn_gate(1, -8);
    spawn_gate(2, -64);

    draw_scene();
}

static void on_cone_hit(uint8_t g, int16_t cone_x_sub) {
    if (mg_slalom_ghit[g]) return;
    mg_slalom_ghit[g] = 24;
    mg_slalom_hits++;
    mg_slalom_streak = 0;
    mg_slalom_stumble = 16;
    // Knock the ball away from the cone!
    mg_slalom_loose = 1;
    loose_lock = 12;
    mg_slalom_bvx = mg_slalom_bx < cone_x_sub ? -24 : 24;
    mg_slalom_bvy = 14;
    sfx_play(SFX_PARRY);
    show_popup(3, PX(cone_x_sub), PX(mg_slalom_gy[g]) - 14);
}

static void finish_run(void) {
    uint8_t n = str_cat(mg_detail, 0, "GATES ");
    n = num_cat(mg_detail, n, mg_slalom_gates);
    n = str_cat(mg_detail, n, "  HITS ");
    num_cat(mg_detail, n, mg_slalom_hits);
    mg_finish(mg_slalom_points);
}

static uint8_t sec_tick;

void mg_slalom_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        sec_tick = 0;
        draw_scene();
        return;
    }
    if (mg_run == MG_RUN_OVER) {
        if (pop_timer) pop_timer--;
        draw_scene();
        return;
    }

    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        hud_draw(1);
    }

    // Countdown clock (45 seconds, no division).
    if (mg_slalom_clock) {
        mg_slalom_clock--;
        sec_tick++;
        if (sec_tick >= 60) {
            sec_tick = 0;
            if (hud_sec) hud_sec--;
            hud_draw(0);
        }
    }
    if (!mg_slalom_clock) {
        finish_run();
        draw_scene();
        return;
    }

    if (mg_slalom_cut_cd) mg_slalom_cut_cd--;
    if (mg_slalom_stumble) mg_slalom_stumble--;
    if (kick_anim) kick_anim--;
    if (pop_timer) pop_timer--;
    if (loose_lock) loose_lock--;

    // Forward pace ramps with cleared gates, adjusted by UP/DOWN/B/stumble.
    uint8_t base_spd = 14 + (mg_slalom_gates >> 1);
    if (base_spd > 28) base_spd = 28;
    int16_t spd = base_spd;
    uint8_t trapping = (joy & J_B) != 0;
    if (mg_slalom_stumble) spd -= 7;
    else if (trapping) spd -= 5;
    else if (joy & J_UP) spd += 8;
    else if (joy & J_DOWN) spd -= 6;
    if (spd < 6) spd = 6;
    if (spd > 34) spd = 34;
    mg_slalom_spd = (uint8_t)spd;

    // Hardware scroll upward (pitch glides down toward player).
    scroll_sub -= mg_slalom_spd;
    scroll_y = (uint8_t)(scroll_sub >> 4);

    // Player lateral steering.
    int8_t dir = ((joy & J_RIGHT) != 0) - ((joy & J_LEFT) != 0);
    int16_t max_vx = trapping ? (int16_t)(mg_slalom_steer_spd - 4) : (int16_t)mg_slalom_steer_spd;
    if (mg_slalom_stumble) max_vx >>= 1;
    int16_t target_vx = dir * max_vx;
    if (mg_slalom_pvx < target_vx) {
        mg_slalom_pvx += 6;
        if (mg_slalom_pvx > target_vx) mg_slalom_pvx = target_vx;
    } else if (mg_slalom_pvx > target_vx) {
        mg_slalom_pvx -= 6;
        if (mg_slalom_pvx < target_vx) mg_slalom_pvx = target_vx;
    }
    mg_slalom_px += mg_slalom_pvx;
    if (mg_slalom_px < TO_SUB(LANE_X0)) mg_slalom_px = TO_SUB(LANE_X0), mg_slalom_pvx = 0;
    if (mg_slalom_px > TO_SUB(LANE_X1)) mg_slalom_px = TO_SUB(LANE_X1), mg_slalom_pvx = 0;

    // Quick cut / tap (A): snaps the ball sharply in your D-pad direction.
    if ((joy_pressed & J_A) && !mg_slalom_cut_cd && !mg_slalom_stumble) {
        int16_t dx_b = ab16(mg_slalom_bx - mg_slalom_px);
        int16_t dy_b = ab16(mg_slalom_by - TO_SUB(PLAYER_Y - BALL_HOLD_DY));
        if (!mg_slalom_loose || (dx_b < TO_SUB(18) && dy_b < TO_SUB(18))) {
            uint8_t cd = 14 - (stat_lv[STAT_REFLEX] >> 2);
            if (cd < 7) cd = 7;
            mg_slalom_cut_cd = cd;
            kick_anim = 8;
            sfx_play(SFX_KICK);
            if (dir != 0) {
                // Crisp sideways cut; higher CONTROL keeps it tighter instead of running away.
                int16_t kick_vx = dir * (28 - (int16_t)(mg_slalom_ctrl >> 2));
                mg_slalom_bvx = kick_vx;
                mg_slalom_bvy = (joy & J_UP) ? -10 : -4;
                mg_slalom_loose = (joy & J_UP) ? 1 : 0;
                mg_slalom_bx += dir * TO_SUB(4);
            } else {
                // Neutral A: tidy knock-on straight ahead.
                mg_slalom_bvx = 0;
                mg_slalom_bvy = -12;
                mg_slalom_loose = 1;
            }
        }
    }

    // Dribble & heavy-touch physics.
    if (!mg_slalom_loose) {
        // Detect sharp high-speed cut without close-control (B): heavy touch knocks ball loose!
        uint8_t sprinting = (joy & J_UP) != 0 && !trapping;
        uint8_t reversed = (dir != 0 && last_dir != 0 && dir != last_dir);
        if (sprinting && dir != 0) {
            if (heavy_ticks < 255) heavy_ticks++;
        } else {
            heavy_ticks = 0;
        }
        // Threshold rises with CONTROL stat (lv 1: 12 frames; lv 20: 21 frames).
        uint8_t thresh = 12 + (mg_slalom_ctrl >> 1);
        if ((sprinting && reversed && mg_slalom_ctrl < 14) || heavy_ticks >= thresh) {
            mg_slalom_loose = 1;
            loose_lock = 12;
            heavy_ticks = 0;
            // Ball knocks ahead and sideways away from your stride!
            int16_t extra = 22 - (int16_t)(mg_slalom_ctrl >> 1);
            if (extra < 10) extra = 10;
            mg_slalom_bvx = mg_slalom_pvx + (dir ? dir * extra : 0);
            mg_slalom_bvy = -12;
            sfx_play(SFX_TOUCH);
        } else {
            // Soft dribble: ball tracks just ahead of your boots with slight inertia.
            int16_t want_x = mg_slalom_px + (mg_slalom_pvx >> 1);
            int16_t want_y = TO_SUB(PLAYER_Y - BALL_HOLD_DY);
            if (trapping) {
                mg_slalom_bx += (want_x - mg_slalom_bx) >> 1;
                mg_slalom_by += (want_y - mg_slalom_by) >> 1;
            } else {
                mg_slalom_bx += (want_x - mg_slalom_bx) >> 2;
                mg_slalom_by += (want_y - mg_slalom_by) >> 2;
            }
            mg_slalom_bvx = mg_slalom_pvx;
            mg_slalom_bvy = 0;
        }
    } else {
        // Loose ball rolls on the grass (relative to scrolling pitch).
        mg_slalom_bx += mg_slalom_bvx;
        mg_slalom_by += mg_slalom_bvy + (mg_slalom_spd >> 2);
        // Grass friction damps velocity.
        if (mg_slalom_bvx > 0) mg_slalom_bvx--;
        else if (mg_slalom_bvx < 0) mg_slalom_bvx++;
        if (mg_slalom_bvy > 0) mg_slalom_bvy--;
        else if (mg_slalom_bvy < 0) mg_slalom_bvy++;

        // Re-collect when player runs back up to the ball (or immediately on B trap).
        int16_t reach = trapping ? TO_SUB(18) : TO_SUB(12);
        if (!mg_slalom_stumble && (trapping || !loose_lock) &&
            ab16(mg_slalom_bx - mg_slalom_px) < reach &&
            ab16(mg_slalom_by - TO_SUB(PLAYER_Y - BALL_HOLD_DY)) < reach) {
            mg_slalom_loose = 0;
            loose_lock = 0;
            sfx_play(SFX_TOUCH);
        }

        // Out of bounds (sidelines or off top/bottom): reset to feet with a penalty.
        int16_t bx_px = PX(mg_slalom_bx), by_px = PX(mg_slalom_by);
        if (bx_px < OUT_X0_PX || bx_px > OUT_X1_PX || by_px < 4 || by_px > 132) {
            mg_slalom_outs++;
            mg_slalom_streak = 0;
            mg_slalom_stumble = 14;
            sfx_play(SFX_BOARD);
            show_popup(3, bx_px < 30 ? 30 : bx_px > 130 ? 130 : bx_px, PLAYER_Y - 18);
            reset_ball_to_feet();
        }
    }
    if (dir != 0) last_dir = dir;

    // Advance gates, check cone collisions & gate clears, recycle off-bottom gates.
    int16_t top_y = 32000;
    for (uint8_t i = 0; i < GATE_MAX; i++) {
        if (mg_slalom_gy[i] < top_y) top_y = mg_slalom_gy[i];
    }

    int16_t bx_px = PX(mg_slalom_bx), by_px = PX(mg_slalom_by);
    int16_t px_px = PX(mg_slalom_px);

    for (uint8_t i = 0; i < GATE_MAX; i++) {
        if (mg_slalom_ghit[i]) mg_slalom_ghit[i]--;
        mg_slalom_gy[i] += mg_slalom_spd;
        int16_t gy_px = PX(mg_slalom_gy[i]);

        int16_t lx = (int16_t)mg_slalom_gx[i] - (int16_t)mg_slalom_gw[i];
        int16_t rx = (int16_t)mg_slalom_gx[i] + (int16_t)mg_slalom_gw[i];

        // Cone collision (ball or player clips either cone).
        if (!mg_slalom_ghit[i]) {
            if (ab16(by_px - gy_px) < 7) {
                if (ab16(bx_px - lx) < 6) on_cone_hit(i, TO_SUB(lx));
                else if (ab16(bx_px - rx) < 6) on_cone_hit(i, TO_SUB(rx));
            }
            if (!mg_slalom_ghit[i] && ab16(PLAYER_Y - 4 - gy_px) < 7) {
                if (ab16(px_px - lx) < 7) on_cone_hit(i, TO_SUB(lx));
                else if (ab16(px_px - rx) < 7) on_cone_hit(i, TO_SUB(rx));
            }
        }

        // Gate crossing check: when the gate line passes the ball.
        if (mg_slalom_gst[i] == GATE_ACTIVE && gy_px >= by_px) {
            if (bx_px > lx + 2 && bx_px < rx - 2 && !mg_slalom_ghit[i]) {
                mg_slalom_gst[i] = GATE_CLEARED;
                mg_slalom_gates++;
                mg_slalom_streak++;
                if (mg_slalom_streak > mg_slalom_best_streak) mg_slalom_best_streak = mg_slalom_streak;
                uint8_t gain = mg_slalom_streak >= 3 ? 2 : 1;
                mg_slalom_points = (uint16_t)(mg_slalom_points + gain);
                sfx_play(gain > 1 ? SFX_CHARGE : SFX_TOUCH);
                show_popup(gain, mg_slalom_gx[i], gy_px - 22);
                hud_draw(0);
            } else {
                mg_slalom_gst[i] = GATE_MISSED;
                mg_slalom_misses++;
                mg_slalom_streak = 0;
                sfx_play(SFX_BOARD);
                hud_draw(0);
            }
        }

        // Recycle gate once it scrolls off the bottom.
        if (gy_px > 148) {
            int16_t ny = PX(top_y) - gate_spacing_px();
            if (ny > -16) ny = -16;
            spawn_gate(i, ny);
            top_y = mg_slalom_gy[i];
        }
    }

    draw_scene();
}
