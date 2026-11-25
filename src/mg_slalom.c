#pragma bank 255
// CONE SLALOM (MG_SLALOM): TimberMan-style two-lane dribble sprint!
//
// Each press of LEFT (or B) dribbles into the Left lane and steps 1m forward;
// each press of RIGHT (or A) dribbles into the Right lane and steps 1m forward!
// Avoid the cones on the field across a 40m sprint:
//   - Clean step: crisp alternating-foot dribble touch pushes the ball ahead
//     and scrolls the turf & cones down smoothly.
//   - Hitting a cone: BONK! Knocks the cone flying, briefly stumbles you, and
//     adds a time penalty (+2.0s base, reduced by CONTROL) to your total time!
//
// Medals awarded at the 40m finish line:
//   - GOLD:   <= 12.0s  (mg_won = 1)
//   - SILVER: <= 16.0s  (mg_won = 1)
//   - BRONZE: <= 21.0s  (mg_won = 1)
//
// Trains REFLEX (all XP: faster step recovery / shorter stumble) +
// CONTROL (half XP: smaller cone time penalty + tighter ball touch).
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

// --- course & TimberMan rules ---
#define LANE_L 52
#define LANE_R 108
#define PLAYER_Y 116       // player feet y (body spans 92..116)
#define ROW0_Y 92          // row 0 (current ball row); row r sits at ROW0_Y - r * STEP_PX
#define STEP_PX 16         // matches the 16 px grass stripes on mg_slalom_bg!
#define ROW_COUNT 7        // rows 0..6 visible/incoming
#define TARGET_DIST 40     // 40m sprint to the finish line!

#define CONE_NONE 0
#define CONE_LEFT 1
#define CONE_RIGHT 2

#define MEDAL_NONE 0
#define MEDAL_BRONZE 1
#define MEDAL_SILVER 2
#define MEDAL_GOLD 3

// Medal thresholds in tenths of a second (0.1s):
#define GOLD_TENTHS 120    // 12.0s
#define SILVER_TENTHS 160  // 16.0s
#define BRONZE_TENTHS 210  // 21.0s

// Sprite tiles in VRAM bank 0 (loaded at MG_SPR_TILES = 16, 2 tiles per 8x16).
#define T_CONE (MG_SPR_TILES + 0)
#define T_CONE_HIT (MG_SPR_TILES + 2)
#define T_FINISH (MG_SPR_TILES + 4)
#define T_PLUS2 (MG_SPR_TILES + 6)
#define T_PLUS1 (MG_SPR_TILES + 8)
#define T_SBANG (MG_SPR_TILES + 10)
#define T_DUST (MG_SPR_TILES + 12)

#define PAL_CONE 3
#define PAL_GOLD_FX 4

// OAM layout (OAM_MG_FREE = 14 .. 39).
#define OAM_CONES OAM_MG_FREE         // 14..20: 7 cone rows
#define OAM_FLY (OAM_MG_FREE + 7)     // 21: knocked cone flying off
#define OAM_FIN (OAM_MG_FREE + 8)     // 22..25: checkered finish banner
#define OAM_POPUP (OAM_MG_FREE + 12)  // 26..27: "+2S!" penalty popup
#define OAM_DUST (OAM_MG_FREE + 14)   // 28: turf dribble dust puff

// --- watchable state (global non-static for tests) ---
uint8_t mg_slalom_lane;          // 0 = Left lane (LANE_L), 1 = Right lane (LANE_R)
uint8_t mg_slalom_dist;          // steps completed (0 .. TARGET_DIST)
uint8_t mg_slalom_target;        // TARGET_DIST (40)
uint16_t mg_slalom_tenths;       // total elapsed + penalty time in 0.1s units
uint8_t mg_slalom_pen_tenths;    // cone hit penalty in 0.1s units (20 = +2.0s, scaled by CONTROL)
uint8_t mg_slalom_stumble;       // recovery frames after bonking a cone
uint8_t mg_slalom_stumble_max;   // stumble duration (scaled by REFLEX)
uint8_t mg_slalom_hits;          // cones hit
uint8_t mg_slalom_streak;        // consecutive clean dribble steps
uint8_t mg_slalom_best_streak;
uint8_t mg_slalom_medal;         // MEDAL_* set on finish
uint8_t mg_slalom_row[ROW_COUNT];// CONE_NONE / CONE_LEFT / CONE_RIGHT for rows 0..6
int16_t mg_slalom_px;            // visual player x (px, glides smoothly to lane_x)
int16_t mg_slalom_bx, mg_slalom_by;  // visual ball screen pos (px)
uint8_t mg_slalom_slide;         // remaining scroll slide px (glides 16 -> 0 after each step)
uint8_t mg_slalom_drib_anim;     // dribble kick/stride animation timer
uint8_t mg_slalom_foot;          // toggles 0/1 each step for alternating-foot dribble!

// --- local state ---
static uint8_t sub6;             // 0..5 frame counter -> +1 tenth every 6 frames (exact 0.1s at 60fps!)
static uint8_t run_left;         // remaining cones in current same-side cluster
static uint8_t run_side;         // CONE_LEFT or CONE_RIGHT
static uint8_t fly_t;
static int16_t fly_x, fly_y;
static int8_t fly_vx;
static uint8_t pop_t;
static int16_t pop_x, pop_y;
static uint8_t dust_t;
static int16_t dust_x, dust_y;
static uint16_t shown_tenths;
static uint8_t shown_rem;

// --- Mario Tennis GBC style palettes ---
#define C(r, g, b) RGB8(r, g, b)
static const uint16_t pal_turf[4] = {C(32, 176, 80), C(24, 144, 64), C(16, 80, 40), C(248, 252, 248)};
static const uint16_t pal_boards[4] = {C(24, 24, 56), C(200, 48, 64), C(248, 192, 48), C(255, 244, 216)};
static const uint16_t pal_cone[4] = {0, C(255, 255, 255), C(248, 104, 24), C(24, 20, 32)};
static const uint16_t pal_fx[4] = {0, C(255, 255, 224), C(254, 208, 40), C(24, 20, 32)};

static uint8_t put_str(char *buf, uint8_t at, const char *s) {
    while (*s) buf[at++] = *s++;
    buf[at] = 0;
    return at;
}

static uint8_t put_u16(char *buf, uint8_t at, uint16_t n) {
    static const uint16_t tens[3] = {100, 10, 1};
    uint8_t started = 0;
    for (uint8_t k = 0; k < 3; k++) {
        uint8_t d = 0;
        while (n >= tens[k]) n -= tens[k], d++;
        if (d || started || k == 2) buf[at++] = '0' + d, started = 1;
    }
    buf[at] = 0;
    return at;
}

// Format tenths (e.g. 114 -> "11.4S") without any division!
static uint8_t put_time(char *buf, uint8_t at, uint16_t tenths) {
    uint16_t secs = 0;
    uint16_t rem = tenths;
    while (rem >= 10) rem -= 10, secs++;
    if (secs > 99) { secs = 99; rem = 9; }
    at = put_u16(buf, at, secs);
    buf[at++] = '.';
    buf[at++] = '0' + (uint8_t)rem;
    buf[at++] = 'S';
    buf[at] = 0;
    return at;
}

// Generate cone for an upcoming row `step_idx` (1 .. TARGET_DIST + 6).
static uint8_t gen_cone(uint8_t step_idx) {
    if (step_idx == 0 || step_idx > TARGET_DIST) return CONE_NONE;
    if (run_left == 0) {
        // Flip side in clusters of 1..3 cones (classic TimberMan rhythm!),
        // with an occasional 1-row open gap on transitions so weaving feels natural.
        uint8_t r = (uint8_t)rand();
        run_side = (run_side == CONE_LEFT) ? CONE_RIGHT : CONE_LEFT;
        run_left = 1 + (r & 1) + ((r & 4) ? 1 : 0);  // 1..3 cones
        if ((r & 15) == 0 && step_idx > 2) return CONE_NONE;
    }
    run_left--;
    return run_side;
}

static void finish_course(void) {
    uint8_t at = 0;
    if (mg_slalom_tenths <= GOLD_TENTHS) {
        mg_slalom_medal = MEDAL_GOLD;
        mg_won = 1;
        at = put_str(mg_detail, 0, "GOLD! ");
    } else if (mg_slalom_tenths <= SILVER_TENTHS) {
        mg_slalom_medal = MEDAL_SILVER;
        mg_won = 1;
        at = put_str(mg_detail, 0, "SILVER! ");
    } else if (mg_slalom_tenths <= BRONZE_TENTHS) {
        mg_slalom_medal = MEDAL_BRONZE;
        mg_won = 1;
        at = put_str(mg_detail, 0, "BRONZE! ");
    } else {
        mg_slalom_medal = MEDAL_NONE;
        mg_won = 0;
        at = put_str(mg_detail, 0, "TIME ");
    }
    put_time(mg_detail, at, mg_slalom_tenths);

    // Higher score for faster time so mg_best tracks your fastest run!
    uint16_t sc = mg_slalom_tenths < 260 ? ((280 - mg_slalom_tenths) >> 2) : 5;
    if (mg_slalom_medal == MEDAL_GOLD) sc += 10;
    else if (mg_slalom_medal == MEDAL_SILVER) sc += 5;
    mg_finish(sc);
}

// Execute one TimberMan dribble step on `side` (0 = Left, 1 = Right)!
static void step_dribble(uint8_t side) {
    mg_slalom_lane = side;
    mg_slalom_foot ^= 1;
    mg_slalom_drib_anim = 8;

    int16_t target_x = side ? LANE_R : LANE_L;
    dust_t = 7;
    dust_x = mg_slalom_px - 4;
    dust_y = PLAYER_Y - 6;

    // Check row 1 (the cone row right in front of us that we are stepping into!):
    uint8_t hazard = mg_slalom_row[1];
    uint8_t hit = (hazard == (side + 1));

    // Advance the field by 1 row!
    mg_slalom_dist++;
    for (uint8_t r = 0; r < ROW_COUNT - 1; r++) {
        mg_slalom_row[r] = mg_slalom_row[r + 1];
    }
    mg_slalom_row[ROW_COUNT - 1] = gen_cone((uint8_t)(mg_slalom_dist + ROW_COUNT - 1));

    uint8_t ns = mg_slalom_slide + STEP_PX;
    mg_slalom_slide = ns > 32 ? 32 : ns;

    if (hit) {
        // BONK! Knock the cone flying, stumble briefly, and add time penalty!
        mg_slalom_row[0] = CONE_NONE;
        mg_slalom_hits++;
        mg_slalom_streak = 0;
        mg_slalom_tenths += mg_slalom_pen_tenths;
        if (mg_slalom_tenths > 999) mg_slalom_tenths = 999;
        mg_slalom_stumble = mg_slalom_stumble_max;

        fly_t = 16;
        fly_x = target_x - 4;
        fly_y = ROW0_Y - 8;
        fly_vx = side ? 4 : -4;

        pop_t = 28;
        pop_x = target_x - 8;
        pop_y = ROW0_Y - 20;
        sfx_play(SFX_BOARD);
    } else {
        mg_slalom_streak++;
        if (mg_slalom_streak > mg_slalom_best_streak) mg_slalom_best_streak = mg_slalom_streak;
        sfx_play(SFX_TOUCH);
    }

    if (mg_slalom_dist >= TARGET_DIST) {
        finish_course();
    }
}

static void hud_draw(void) {
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "TIME");
        win_text(7, 0, ".");
        win_text(11, 0, "LEFT");
        shown_tenths = 0xFFFF;
        shown_rem = 0xFF;
    }
    if (mg_slalom_tenths != shown_tenths) {
        shown_tenths = mg_slalom_tenths;
        uint16_t secs = 0, rem = mg_slalom_tenths;
        while (rem >= 10) rem -= 10, secs++;
        if (secs > 99) { secs = 99; rem = 9; }
        win_number(5, 0, secs, 2, 1);
        win_number(8, 0, rem, 1, 0);
    }
    uint8_t left = mg_slalom_dist >= TARGET_DIST ? 0 : (TARGET_DIST - mg_slalom_dist);
    if (left != shown_rem) {
        shown_rem = left;
        win_number(16, 0, left, 2, 0);
    }
}

static void draw_scene(void) {
    int16_t target_x = mg_slalom_lane ? LANE_R : LANE_L;
    // Smooth horizontal glide into target lane (~3 frames).
    int16_t dx = target_x - mg_slalom_px;
    if (dx > 1) mg_slalom_px += (dx >> 1) + 1;
    else if (dx < -1) mg_slalom_px -= ((-dx) >> 1) + 1;
    else mg_slalom_px = target_x;

    // Authentic dribbling ball motion: pushes out ahead of your boot on each tap,
    // leads slightly into your lane cut, and gathers back toward your toes!
    int16_t push = (int16_t)mg_slalom_drib_anim + (mg_slalom_drib_anim >> 1);  // 0..12 px push ahead
    int16_t foot_ofs = mg_slalom_foot ? 3 : -3;
    mg_slalom_bx = ((mg_slalom_px + target_x) >> 1) + foot_ofs;
    mg_slalom_by = PLAYER_Y - 19 - push;

    uint8_t spin = (mg_slalom_drib_anim > 0 || mg_slalom_slide > 0) && ((uint8_t)frame_count & 2);
    mg_ball(mg_slalom_bx, mg_slalom_by + 4, 0, spin,
            mg_slalom_streak >= 10 ? SPR_PAL_POWER : SPR_PAL_BALL);

    // Player animation: lively alternating-foot kick & run stride on every dribble tap!
    uint8_t f, flip;
    if (mg_run == MG_RUN_OVER) {
        f = mg_won ? MG_F_WIN : MG_F_STAND;
        flip = 0;
    } else if (mg_slalom_stumble) {
        f = MG_F_LOSE;
        flip = mg_slalom_lane;
    } else if (mg_slalom_drib_anim >= 5) {
        f = MG_F_KICK;
        flip = dx < -2 ? 1 : dx > 2 ? 0 : mg_slalom_foot;
    } else if (mg_slalom_drib_anim > 0) {
        f = mg_slalom_foot ? MG_F_RUN_A : MG_F_RUN_B;
        flip = mg_slalom_foot;
    } else {
        f = MG_F_READY;
        flip = 0;
    }
    uint8_t ppal = (mg_slalom_stumble & 2) ? SPR_PAL_FLASH : SPR_PAL_YOU;
    mg_player(0, MG_FRAME(MG_VIEW_BACK, f), mg_slalom_px, PLAYER_Y, ppal, flip);

    // Cones on rows 0..6 (planted on the grass: shifted up by mg_slalom_slide as turf glides down!).
    for (uint8_t r = 0; r < ROW_COUNT; r++) {
        uint8_t c = mg_slalom_row[r];
        if (c == CONE_NONE) {
            move_sprite(OAM_CONES + r, 0, 0);
            continue;
        }
        int16_t cx = (c == CONE_LEFT ? LANE_L : LANE_R) - 4;
        int16_t cy = ROW0_Y - (int16_t)(r * STEP_PX) - (int16_t)mg_slalom_slide - 12;
        mg_sprite(OAM_CONES + r, T_CONE, PAL_CONE, cx, cy);
    }

    // Checkered finish line across the pitch when TARGET_DIST is within view!
    int16_t rem_steps = (int16_t)TARGET_DIST - (int16_t)mg_slalom_dist;
    if (rem_steps >= 0 && rem_steps < ROW_COUNT) {
        int16_t fy = ROW0_Y - rem_steps * STEP_PX - (int16_t)mg_slalom_slide - 4;
        mg_sprite(OAM_FIN + 0, T_FINISH, PAL_GOLD_FX, 26, fy);
        mg_sprite(OAM_FIN + 1, T_FINISH, PAL_GOLD_FX, 34, fy);
        mg_sprite(OAM_FIN + 2, T_FINISH, PAL_GOLD_FX, 118, fy);
        mg_sprite(OAM_FIN + 3, T_FINISH, PAL_GOLD_FX, 126, fy);
    } else {
        mg_hide(OAM_FIN, OAM_FIN + 3);
    }

    // Flying knocked-over cone
    if (fly_t) {
        fly_t--;
        fly_x += fly_vx;
        fly_y -= 2;
        mg_sprite(OAM_FLY, T_CONE_HIT, PAL_CONE | (fly_vx < 0 ? S_FLIPX : 0), fly_x, fly_y);
    } else {
        move_sprite(OAM_FLY, 0, 0);
    }

    // Penalty popup ("+2S!" or "+1S!")
    if (pop_t) {
        pop_t--;
        if (!(pop_t & 1)) pop_y--;
        uint8_t ptile = mg_slalom_pen_tenths < 16 ? T_PLUS1 : T_PLUS2;
        mg_sprite(OAM_POPUP, ptile, SPR_PAL_CPU, pop_x, pop_y);
        mg_sprite(OAM_POPUP + 1, T_SBANG, SPR_PAL_CPU, pop_x + 8, pop_y);
    } else {
        mg_hide(OAM_POPUP, OAM_POPUP + 1);
    }

    // Turf dust puff behind boots
    if (dust_t) {
        dust_t--;
        mg_sprite(OAM_DUST, T_DUST, PAL_GOLD_FX, dust_x, dust_y);
    } else {
        move_sprite(OAM_DUST, 0, 0);
    }
}

void mg_slalom_enter(void) BANKED {
    mg_load_bg(mg_slalom_bg_tiles, MG_SLALOM_BG_TILE_COUNT, mg_slalom_bg_map, mg_slalom_bg_attr,
               MG_SLALOM_BG_MAP_W, MG_SLALOM_BG_MAP_H, BANK(mg_slalom_bg));
    // Copy the 20x4 strip down rows 4..31 so hardware scroll_y wraps seamlessly!
    uint8_t strip[20 * 4];
    get_bkg_tiles(0, 0, 20, 4, strip);
    for (uint8_t r = 4; r < 32; r += 4) set_bkg_tiles(0, r, 20, 4, strip);
    VBK_REG = 1;
    get_bkg_tiles(0, 0, 20, 4, strip);
    for (uint8_t r = 4; r < 32; r += 4) set_bkg_tiles(0, r, 20, 4, strip);
    VBK_REG = 0;
    memcpy(&pal_base_bg[0], pal_turf, sizeof(pal_turf));
    memcpy(&pal_base_bg[4], pal_boards, sizeof(pal_boards));
    mg_load_spr(mg_slalom_spr_tiles, MG_SLALOM_SPR_TILE_COUNT, BANK(mg_slalom_spr));
    memcpy(&pal_base_obj[PAL_CONE << 2], pal_cone, sizeof(pal_cone));
    memcpy(&pal_base_obj[PAL_GOLD_FX << 2], pal_fx, sizeof(pal_fx));

    uint8_t lv_ref = stat_lv[STAT_REFLEX];
    uint8_t lv_ctl = stat_lv[STAT_CONTROL];
    // REFLEX shortens cone-bonk stumble (12 frames down to 6); CONTROL cuts time penalty (2.0s down to 1.2s)!
    mg_slalom_stumble_max = 12 - (lv_ref >> 2);
    if (mg_slalom_stumble_max < 6) mg_slalom_stumble_max = 6;
    mg_slalom_pen_tenths = 20 - ((lv_ctl * 2) >> 2);
    if (mg_slalom_pen_tenths < 12) mg_slalom_pen_tenths = 12;

    mg_slalom_lane = 0;
    mg_slalom_px = LANE_L;
    mg_slalom_bx = LANE_L - 3;
    mg_slalom_by = PLAYER_Y - 19;
    mg_slalom_dist = 0;
    mg_slalom_target = TARGET_DIST;
    mg_slalom_tenths = 0;
    mg_slalom_stumble = 0;
    mg_slalom_hits = 0;
    mg_slalom_streak = mg_slalom_best_streak = 0;
    mg_slalom_medal = MEDAL_NONE;
    mg_slalom_slide = 0;
    mg_slalom_drib_anim = 0;
    mg_slalom_foot = 0;
    sub6 = 0;
    fly_t = pop_t = dust_t = 0;
    scroll_x = 0;
    scroll_y = 0;

    run_side = (rand() & 1) ? CONE_LEFT : CONE_RIGHT;
    run_left = 2;
    mg_slalom_row[0] = CONE_NONE;
    for (uint8_t r = 1; r < ROW_COUNT; r++) {
        mg_slalom_row[r] = gen_cone(r);
    }

    mg_player_hide(1);
    mg_player_hide(2);
    mg_hide(OAM_MG_FREE, OAM_MG_COUNT - 1);
    draw_scene();
}

void mg_slalom_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw_scene();
        return;
    }

    // Smooth turf scroll glide on every dribble step.
    if (mg_slalom_slide) {
        uint8_t step = mg_slalom_slide > 4 ? 4 : mg_slalom_slide;
        mg_slalom_slide -= step;
        scroll_y -= step;
    }
    if (mg_slalom_drib_anim) mg_slalom_drib_anim--;

    if (mg_run == MG_RUN_LIVE) {
        if (++sub6 >= 6) {
            sub6 = 0;
            if (mg_slalom_tenths < 999) mg_slalom_tenths++;
        }
        if (mg_slalom_stumble) {
            mg_slalom_stumble--;
        } else {
            // LEFT (or B) dribbles Left; RIGHT (or A) dribbles Right!
            if (joy_pressed & (J_LEFT | J_B)) {
                step_dribble(0);
            } else if (joy_pressed & (J_RIGHT | J_A)) {
                step_dribble(1);
            }
        }
        hud_draw();
    }

    draw_scene();
}
