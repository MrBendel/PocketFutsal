#pragma bank 255
// CONE SLALOM (MG_SLALOM): TimberMan-style two-lane dribble sprint!
// Supports BOTH:
//   - SOLO   (mg_vs == 0, default): 40m Time Trial! Cone hits add +2.0s
//     (reduced by CONTROL) + brief stumble. Beat GOLD (<=12.0s), SILVER
//     (<=16.0s), or BRONZE (<=21.0s)!
//   - VS CPU (mg_vs == 1): Head-to-head 40m Slalom Race! YOU weave down the
//     left track while the CPU weaves down the right track in real time —
//     first across the 40m finish tape wins!
//
// Key polish:
//   1. Ball is drawn in OAM slots 14..17 (AFTER player slots 2..9) and placed
//      ahead of the player's body so it NEVER draws on top of the player's head!
//   2. Tight slalom lane gap (28 px in Solo, 24 px per track in VS Race).
//   3. Guaranteed-solvable cone generator: always inserts at least 1 open
//      crossover row whenever cones switch sides, and row[1] visually sits
//      right in front of your ball while row[0] sits beside your boots!
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

// --- geometry & rules ---
// Solo tight center weave (28 px gap); VS split-track weave (24 px gap each side).
#define SOLO_LANE_L 66
#define SOLO_LANE_R 94
#define VS_YOU_L 40
#define VS_YOU_R 64
#define VS_CPU_L 96
#define VS_CPU_R 120

#define PLAYER_Y 118       // player feet y (24 px tall -> head top at y = 95)
#define BALL_REST 6        // px ahead of your feet the ball sits between touches: at your toes
#define BALL_LEAD 14       // px ahead a kick sends it as you step, before you gather it back
#define ROW0_CY 104        // row 0 cone top y (104..118: base level with your boots)
                           // row 1 cone top y = 88 (one step ahead = the next hazard)
                           // passed rows -1 / -2 slide down past you and off under the HUD (y 136)
#define STEP_PX 16         // matches the 16 px grass stripes on mg_slalom_bg
#define ROW_COUNT 7        // visible rows 0..6 for YOU
#define ROW_BACK 2         // passed rows still drawn (-1, -2) so cones slide off instead of snapping
#define COURSE_LEN 48      // full pre-generated course (0 .. TARGET_DIST + 7) so VS tracks match fairly
#define TARGET_DIST 40     // 40m sprint to the finish line!

#define CONE_NONE 0
#define CONE_LEFT 1
#define CONE_RIGHT 2

#define MEDAL_NONE 0
#define MEDAL_BRONZE 1
#define MEDAL_SILVER 2
#define MEDAL_GOLD 3

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

// OAM layout. The ball has no shadow (it's on the grass, not in the air). At your boots
// it uses the shell's slot 0/1 (drawn over your feet); rolled ahead, slot 14/15 (behind
// you: players are 2..9, and lower slots draw on top).
#define OAM_BALL_YOU_FRONT OAM_MG_BALL    // 0
#define OAM_BALL_CPU_FRONT OAM_MG_SHADOW  // 1
#define OAM_BALL_YOU 14
#define OAM_BALL_CPU 15
#define OAM_CONES_YOU 16   // 16..24: rows -2..6
#define OAM_CONES_CPU 25   // 25..33: rows -2..6 (VS mode)
#define CONE_SLOTS (ROW_COUNT + ROW_BACK)
#define OAM_FIN 34         // 34..37: checkered finish tape
#define OAM_FLY 38         // 38: flying bonked cone
#define OAM_DUST 39        // 39: turf dust puff
#define OAM_POPUP OAM_CONES_CPU  // "+2S!" popup: Solo only, in the (unused) CPU cone slots

// --- watchable state (global non-static for tests) ---
uint8_t mg_slalom_lane;          // 0 = Left lane, 1 = Right lane
uint8_t mg_slalom_dist;          // YOUR steps completed (0 .. TARGET_DIST)
uint8_t mg_slalom_cdist;         // CPU steps completed in VS mode (0 .. TARGET_DIST)
uint8_t mg_slalom_clane;         // CPU lane (0 or 1)
uint8_t mg_slalom_target;        // TARGET_DIST (40)
uint16_t mg_slalom_tenths;       // total elapsed (+ penalty in Solo) in 0.1s units
uint8_t mg_slalom_pen_tenths;    // cone hit penalty in Solo (20 = +2.0s, scaled by CONTROL)
uint8_t mg_slalom_stumble;       // recovery frames after bonking a cone
uint8_t mg_slalom_stumble_max;   // stumble duration (scaled by REFLEX & CONTROL)
uint8_t mg_slalom_hits;          // YOUR cones hit
uint8_t mg_slalom_chits;         // CPU cones hit
uint8_t mg_slalom_streak;        // consecutive clean dribble steps
uint8_t mg_slalom_best_streak;
uint8_t mg_slalom_medal;         // MEDAL_* (Solo)
uint8_t mg_slalom_row[ROW_COUNT];// visible window course[dist .. dist + 6] (writable by tests!)
int16_t mg_slalom_px;            // YOUR visual x (px)
int16_t mg_slalom_bx, mg_slalom_by;  // YOUR visual ball pos (px)
int16_t mg_slalom_cx;            // CPU visual x (px)
uint8_t mg_slalom_slide;         // remaining turf scroll slide px (glides 16 -> 0)
uint8_t mg_slalom_drib_anim;     // YOUR dribble kick animation timer
uint8_t mg_slalom_foot;          // toggles 0/1 each step for alternating-foot dribble
// The dribbled ball, for YOU [0] and the CPU [1]: how far ahead of the feet it is on
// the grass (1/16 px), its roll speed (1/16 px a frame), and its bounce off each kick.
int16_t mg_slalom_ahead[2];
int16_t mg_slalom_bv[2];
uint8_t mg_slalom_hop[2];

// --- local state ---
static uint8_t course_you[COURSE_LEN];
static uint8_t course_cpu[COURSE_LEN];
static int16_t lane_l, lane_r;
static uint8_t sub6;
static uint8_t cpu_cd, cpu_stumble, cpu_drib, cpu_foot, cpu_slide;
// The ball's bounce off a kick (px up), frame by frame: a hop and a little second one.
#define HOP_FRAMES 12
static const uint8_t hop_z[HOP_FRAMES] = {0, 1, 2, 3, 3, 3, 2, 1, 0, 1, 1, 0};
static uint8_t fly_t;
static int16_t fly_x, fly_y;
static int8_t fly_vx;
static uint8_t pop_t;
static int16_t pop_x, pop_y;
static uint8_t dust_t;
static int16_t dust_x, dust_y;
static uint16_t shown_tenths;
static uint8_t shown_a, shown_b;

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

static uint8_t put_time(char *buf, uint8_t at, uint16_t tenths) {
    uint16_t secs = 0, rem = tenths;
    while (rem >= 10) rem -= 10, secs++;
    if (secs > 99) { secs = 99; rem = 9; }
    at = put_u16(buf, at, secs);
    buf[at++] = '.';
    buf[at++] = '0' + (uint8_t)rem;
    buf[at++] = 'S';
    buf[at] = 0;
    return at;
}

// Build a guaranteed-solvable 40m cone layout:
// - Steps 0 and 1 are open (CONE_NONE) so you get a clean first stride.
// - Cones appear in clusters of 1..3 on one side, ALWAYS followed by at least
//   1 open crossover row (CONE_NONE) before switching to the opposite side!
//   That guarantees you NEVER get pinched between diagonal adjacent cones.
static void build_course(uint8_t *arr) {
    memset(arr, CONE_NONE, COURSE_LEN);
    uint8_t side = (rand() & 1) ? CONE_LEFT : CONE_RIGHT;
    uint8_t i = 2;
    while (i <= TARGET_DIST) {
        uint8_t r = (uint8_t)rand();
        uint8_t len = 1 + (r & 1) + ((r & 4) ? 1 : 0);  // 1..3 cones on `side`
        while (len-- && i <= TARGET_DIST) {
            arr[i++] = side;
        }
        // Mandatory open crossover row before switching sides!
        i++;
        if ((r & 8) && i <= TARGET_DIST) i++;  // occasional 2-row open breather
        side = (side == CONE_LEFT) ? CONE_RIGHT : CONE_LEFT;
    }
}

static void sync_window(void) {
    for (uint8_t r = 0; r < ROW_COUNT; r++) {
        uint8_t idx = mg_slalom_dist + r;
        mg_slalom_row[r] = idx < COURSE_LEN ? course_you[idx] : CONE_NONE;
    }
}

static void finish_solo(void) {
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

    uint16_t sc = mg_slalom_tenths < 260 ? ((280 - mg_slalom_tenths) >> 2) : 5;
    if (mg_slalom_medal == MEDAL_GOLD) sc += 10;
    else if (mg_slalom_medal == MEDAL_SILVER) sc += 5;
    mg_finish(sc);
}

static void finish_vs(uint8_t you_won) {
    uint8_t at = 0;
    mg_won = you_won;
    if (you_won) {
        mg_slalom_medal = MEDAL_GOLD;
        at = put_str(mg_detail, 0, "1ST! ");
        put_time(mg_detail, at, mg_slalom_tenths);
    } else {
        mg_slalom_medal = MEDAL_NONE;
        at = put_str(mg_detail, 0, "2ND  ");
        at = put_u16(mg_detail, at, mg_slalom_dist);
        put_str(mg_detail, at, "M / 40M");
    }
    uint16_t sc = you_won ? (25 + (TARGET_DIST - mg_slalom_cdist) * 2) : (mg_slalom_dist >> 1);
    mg_finish(sc);
}

// The turf's glide after a step: a quarter of what's left (and a pixel) a frame, so
// each step eases to a stop over ~8 frames instead of jumping.
static uint8_t glide(uint8_t *slide) {
    uint8_t s = *slide;
    if (!s) return 0;
    uint8_t step = (s >> 2) + 1;
    if (step > s) step = s;
    *slide = s - step;
    return step;
}

// Kick ball i forward off the foot: far enough to cover the step still to glide and run
// BALL_LEAD out in front. It slows by a quarter a frame (about 4x its speed in all),
// matching the turf's glide; stopped, it eases back to your toes (you gather it).
static void kick_ball(uint8_t i, uint8_t slide) {
    int16_t travel = ((int16_t)(BALL_LEAD + slide) << 4) - mg_slalom_ahead[i];
    if (travel < 64) travel = 64;
    mg_slalom_bv[i] = travel >> 2;
    mg_slalom_hop[i] = HOP_FRAMES;
}

// Ball i a frame: it rolls (and slows), the runner gains `scrolled` px on it, and once
// it's stopped it eases to its resting spot. It can't get behind the feet.
static void ball_tick(uint8_t i, uint8_t scrolled, uint8_t kicked) {
    int16_t a = mg_slalom_ahead[i] - ((int16_t)scrolled << 4) + mg_slalom_bv[i];
    int16_t v = mg_slalom_bv[i];
    v -= (v + 3) >> 2;
    if (v < 0) v = 0;
    mg_slalom_bv[i] = v;
    if (!v && !scrolled && kicked) a += ((BALL_REST << 4) - a) >> 3;
    if (a < 0) a = 0;
    if (a > (40 << 4)) a = 40 << 4;
    mg_slalom_ahead[i] = a;
    if (mg_slalom_hop[i]) mg_slalom_hop[i]--;
}

// Execute one TimberMan dribble step for YOU on `side` (0 = Left, 1 = Right)!
static void step_dribble(uint8_t side) {
    mg_slalom_lane = side;
    mg_slalom_foot ^= 1;
    mg_slalom_drib_anim = 7;

    int16_t target_x = side ? lane_r : lane_l;
    dust_t = 7;
    dust_x = mg_slalom_px - 4;
    dust_y = PLAYER_Y - 6;

    // Check mg_slalom_row[1] (the cone right in front of you at y=80..94; tests may override mg_slalom_row[]!).
    uint8_t hazard = mg_slalom_row[1];
    uint8_t hit = (hazard == (side + 1));

    // Advance 1m forward: shift mg_slalom_row[0..5] from [1..6], pull new row[6] from course_you!
    // (Row 0 goes back into the course first: passed rows are drawn from there.)
    if (mg_slalom_dist < COURSE_LEN) course_you[mg_slalom_dist] = mg_slalom_row[0];
    mg_slalom_dist++;
    for (uint8_t r = 0; r < ROW_COUNT - 1; r++) {
        mg_slalom_row[r] = mg_slalom_row[r + 1];
    }
    uint8_t tail = mg_slalom_dist + ROW_COUNT - 1;
    mg_slalom_row[ROW_COUNT - 1] = tail < COURSE_LEN ? course_you[tail] : CONE_NONE;
    if (mg_slalom_dist < COURSE_LEN) course_you[mg_slalom_dist] = mg_slalom_row[0];

    uint8_t ns = mg_slalom_slide + STEP_PX;
    mg_slalom_slide = ns > 32 ? 32 : ns;
    kick_ball(0, mg_slalom_slide);

    if (hit) {
        mg_slalom_row[0] = CONE_NONE;
        if (mg_slalom_dist < COURSE_LEN) course_you[mg_slalom_dist] = CONE_NONE;
        mg_slalom_hits++;
        mg_slalom_streak = 0;
        if (!mg_vs) {
            mg_slalom_tenths += mg_slalom_pen_tenths;
            if (mg_slalom_tenths > 999) mg_slalom_tenths = 999;
            pop_t = 28;
            pop_x = target_x - 8;
            pop_y = ROW0_CY - 16;
        }
        mg_slalom_stumble = mg_slalom_stumble_max;

        fly_t = 16;
        fly_x = target_x - 4;
        fly_y = ROW0_CY - 8;
        fly_vx = side ? 4 : -4;
        sfx_play(SFX_BOARD);
    } else {
        mg_slalom_streak++;
        if (mg_slalom_streak > mg_slalom_best_streak) mg_slalom_best_streak = mg_slalom_streak;
        sfx_play(SFX_TOUCH);
    }

    if (mg_slalom_dist >= TARGET_DIST) {
        if (mg_vs) finish_vs(1);
        else finish_solo();
    }
}

// Step CPU racer on the right-hand track in VS mode!
static void cpu_step(void) {
    if (!mg_vs) return;
    if (cpu_drib) cpu_drib--;
    if (cpu_stumble) {
        cpu_stumble--;
        return;
    }
    if (cpu_cd) {
        cpu_cd--;
        return;
    }

    // CPU taps at ~3.7 steps/sec (~16 frames/step -> ~11.2s 40m finish), with rare cone clips!
    uint8_t next_c = (mg_slalom_cdist + 1 < COURSE_LEN) ? course_cpu[mg_slalom_cdist + 1] : CONE_NONE;
    uint8_t cside = mg_slalom_clane;
    uint8_t mistake = (next_c != CONE_NONE) && (((uint8_t)rand() & 15) == 0);
    if (mistake) {
        cside = (next_c == CONE_LEFT) ? 0 : 1;
    } else if (next_c == CONE_LEFT) {
        cside = 1;
    } else if (next_c == CONE_RIGHT) {
        cside = 0;
    } else if (((uint8_t)rand() & 3) == 0) {
        cside ^= 1;
    }

    mg_slalom_clane = cside;
    cpu_foot ^= 1;
    cpu_drib = 7;
    mg_slalom_cdist++;
    uint8_t cns = cpu_slide + STEP_PX;
    cpu_slide = cns > 32 ? 32 : cns;
    kick_ball(1, cpu_slide);

    if (next_c == (cside + 1)) {
        course_cpu[mg_slalom_cdist] = CONE_NONE;
        mg_slalom_chits++;
        cpu_stumble = 22;
        cpu_cd = 16;
    } else {
        cpu_cd = 15 + ((uint8_t)rand() & 3);  // 15..18 frames
    }

    if (mg_slalom_cdist >= TARGET_DIST && mg_run == MG_RUN_LIVE) {
        finish_vs(0);
    }
}

static void hud_draw(void) {
    if (mg_vs) {
        if (mg_hud_dirty) {
            mg_hud_dirty = 0;
            fill_win_rect(0, 0, SCREEN_TW, 1, 0);
            win_text(0, 0, "YOU");
            win_text(6, 0, "M");
            win_text(12, 0, "CPU");
            win_text(18, 0, "M");
            shown_a = shown_b = 0xFF;
        }
        uint8_t yrem = mg_slalom_dist >= TARGET_DIST ? 0 : (TARGET_DIST - mg_slalom_dist);
        uint8_t crem = mg_slalom_cdist >= TARGET_DIST ? 0 : (TARGET_DIST - mg_slalom_cdist);
        if (yrem != shown_a) win_number(4, 0, shown_a = yrem, 2, 0);
        if (crem != shown_b) win_number(16, 0, shown_b = crem, 2, 0);
        return;
    }
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "TIME");
        win_text(7, 0, ".");
        win_text(11, 0, "LEFT");
        shown_tenths = 0xFFFF;
        shown_a = 0xFF;
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
    if (left != shown_a) {
        shown_a = left;
        win_number(16, 0, left, 2, 0);
    }
}

// Draw dribbled ball i (no shadow: it's rolling on the grass) for a runner centered on x
// with feet on `feet`. It starts at the kicking boot and rolls out ahead, hopping off each
// kick. At the boots it draws over the feet; rolled out ahead, behind the runner.
static void draw_ball(uint8_t i, int16_t x, int16_t feet, uint8_t foot, uint8_t pal) {
    uint8_t front = i ? OAM_BALL_CPU_FRONT : OAM_BALL_YOU_FRONT;
    uint8_t back = i ? OAM_BALL_CPU : OAM_BALL_YOU;
    int16_t ahead = mg_slalom_ahead[i] >> 4;
    uint8_t h = mg_slalom_hop[i];
    int16_t z = 0;
    if (h) z = hop_z[HOP_FRAMES - h];
    // Beside the kicking foot so it shows past your boots; always drawn under (behind) you.
    int16_t side = 7;
    int16_t bx = foot ? x + side : x - side;
    int16_t ground = feet - ahead;  // where it touches the grass
    int16_t top = ground - 8 - z;
    uint8_t tile = MG_T_BALL;
    if ((mg_slalom_bv[i] > 6 || h) && ((uint8_t)frame_count & 2)) tile += 2;
    uint8_t slot = back, other = front;
    set_sprite_tile(slot, tile);
    set_sprite_prop(slot, pal);
    if (bx <= -4 || bx >= 164 || top <= -16 || top >= 144) move_sprite(slot, 0, 0);
    else move_sprite(slot, (uint8_t)(bx + 4), (uint8_t)(top + 16));
    move_sprite(other, 0, 0);
    if (!i) {
        mg_slalom_bx = bx;
        mg_slalom_by = ground - 4 - z;
    }
}

static void hide_ball(uint8_t i) {
    move_sprite(i ? OAM_BALL_CPU_FRONT : OAM_BALL_YOU_FRONT, 0, 0);
    move_sprite(i ? OAM_BALL_CPU : OAM_BALL_YOU, 0, 0);
}

// One track's cones, rows -ROW_BACK .. ROW_COUNT - 1 around world step `base`: passed
// rows keep sliding down past the runner and off under the HUD rather than vanishing.
static void draw_cones(const uint8_t *course, uint8_t oam, uint8_t base, int16_t lx, int16_t rx, uint8_t you) {
    for (int8_t r = -ROW_BACK; r < ROW_COUNT; r++) {
        uint8_t slot = oam + (uint8_t)(r + ROW_BACK);
        int16_t w = (int16_t)base + r;
        uint8_t c = CONE_NONE;
        if (you && r >= 0) c = mg_slalom_row[r];
        else if (w >= 0 && w < COURSE_LEN) c = course[w];
        if (c == CONE_NONE) {
            move_sprite(slot, 0, 0);
            continue;
        }
        // (Written here, not through the banked mg_sprite: 18 cones a frame in VS.)
        int16_t cy = ROW0_CY - (int16_t)r * STEP_PX - (int16_t)mg_slalom_slide;
        int16_t cx = c == CONE_LEFT ? lx : rx;
        set_sprite_tile(slot, T_CONE);
        set_sprite_prop(slot, PAL_CONE);
        if (cy <= -16 || cy >= 144) move_sprite(slot, 0, 0);
        else move_sprite(slot, (uint8_t)(cx + 4), (uint8_t)(cy + 16));
    }
}

static void draw_scene(void) {
    int16_t target_x = mg_slalom_lane ? lane_r : lane_l;
    int16_t dx = target_x - mg_slalom_px;
    if (dx > 1) mg_slalom_px += (dx >> 1) + 1;
    else if (dx < -1) mg_slalom_px -= ((-dx) >> 1) + 1;
    else mg_slalom_px = target_x;

    draw_ball(0, mg_slalom_px, PLAYER_Y, mg_slalom_foot,
              mg_slalom_streak >= 10 ? SPR_PAL_POWER : SPR_PAL_BALL);

    // YOU sprite (slots 2..5)
    uint8_t f, flip;
    if (mg_run == MG_RUN_OVER) {
        f = mg_won ? MG_F_WIN : MG_F_LOSE;
        flip = 0;
    } else if (mg_slalom_stumble) {
        f = MG_F_LOSE;
        flip = mg_slalom_lane;
    } else if (mg_slalom_drib_anim >= 4) {
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

    // YOUR cones: row 0 level with your boots, row 1 the next step, passed rows sliding off.
    draw_cones(course_you, OAM_CONES_YOU, mg_slalom_dist, lane_l, lane_r, 1);

    // VS CPU Racer & CPU Cones on the right track!
    if (mg_vs) {
        int16_t ctarget_x = mg_slalom_clane ? VS_CPU_R : VS_CPU_L;
        int16_t cdx = ctarget_x - mg_slalom_cx;
        if (cdx > 1) mg_slalom_cx += (cdx >> 1) + 1;
        else if (cdx < -1) mg_slalom_cx -= ((-cdx) >> 1) + 1;
        else mg_slalom_cx = ctarget_x;

        // CPU screen Y relative to YOUR camera! If CPU is ahead (cdist > dist), CPU renders higher up!
        int16_t rel_steps = (int16_t)mg_slalom_cdist - (int16_t)mg_slalom_dist;
        int16_t cfeet = PLAYER_Y - rel_steps * STEP_PX - (int16_t)mg_slalom_slide + (int16_t)cpu_slide;
        if (cfeet >= 20 && cfeet <= 148) {
            draw_ball(1, mg_slalom_cx, cfeet, cpu_foot, SPR_PAL_BALL);

            uint8_t cf;
            if (mg_run == MG_RUN_OVER) cf = mg_won ? MG_F_LOSE : MG_F_WIN;
            else if (cpu_stumble) cf = MG_F_LOSE;
            else if (cpu_drib >= 4) cf = MG_F_KICK;
            else if (cpu_drib > 0) cf = cpu_foot ? MG_F_RUN_A : MG_F_RUN_B;
            else cf = MG_F_READY;
            uint8_t cpal = (cpu_stumble & 2) ? SPR_PAL_FLASH : SPR_PAL_CPU;
            mg_player(1, MG_FRAME(MG_VIEW_BACK, cf), mg_slalom_cx, cfeet, cpal, cpu_foot);
        } else {
            mg_player_hide(1);
            hide_ball(1);
        }

        // CPU's cones at the world steps in YOUR camera window, sliding with your turf.
        draw_cones(course_cpu, OAM_CONES_CPU, mg_slalom_dist, VS_CPU_L, VS_CPU_R, 0);
    } else {
        mg_player_hide(1);
        hide_ball(1);
        // (OAM_CONES_CPU + 0/1 is the Solo popup's: it hides itself.)
        mg_hide(OAM_CONES_CPU + 2, OAM_CONES_CPU + CONE_SLOTS - 1);
    }

    // Checkered finish line across the pitch when TARGET_DIST is within view!
    int16_t rem_steps = (int16_t)TARGET_DIST - (int16_t)mg_slalom_dist;
    if (rem_steps >= 0 && rem_steps < ROW_COUNT) {
        int16_t fy = ROW0_CY - rem_steps * STEP_PX - (int16_t)mg_slalom_slide + 4;
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

    // Penalty popup ("+2S!" or "+1S!", Solo only: its slots are the CPU's cones in VS)
    if (pop_t && !mg_vs) {
        pop_t--;
        if (!(pop_t & 1)) pop_y--;
        uint8_t ptile = mg_slalom_pen_tenths < 16 ? T_PLUS1 : T_PLUS2;
        mg_sprite(OAM_POPUP, ptile, SPR_PAL_CPU, pop_x, pop_y);
        mg_sprite(OAM_POPUP + 1, T_SBANG, SPR_PAL_CPU, pop_x + 8, pop_y);
    } else if (!mg_vs) {
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

    lane_l = mg_vs ? VS_YOU_L : SOLO_LANE_L;
    lane_r = mg_vs ? VS_YOU_R : SOLO_LANE_R;

    uint8_t lv_ref = stat_lv[STAT_REFLEX];
    uint8_t lv_ctl = stat_lv[STAT_CONTROL];
    if (mg_vs) {
        // In head-to-head VS race, cone hits cost real-time stumble frames (26 down to 14)!
        mg_slalom_stumble_max = 26 - (lv_ref >> 2) - (lv_ctl >> 2);
        if (mg_slalom_stumble_max < 14) mg_slalom_stumble_max = 14;
    } else {
        mg_slalom_stumble_max = 12 - (lv_ref >> 2);
        if (mg_slalom_stumble_max < 6) mg_slalom_stumble_max = 6;
    }
    mg_slalom_pen_tenths = 20 - ((lv_ctl * 2) >> 2);
    if (mg_slalom_pen_tenths < 12) mg_slalom_pen_tenths = 12;

    mg_slalom_lane = 0;
    mg_slalom_clane = 0;
    mg_slalom_px = lane_l;
    mg_slalom_cx = VS_CPU_L;
    mg_slalom_bx = lane_l + 5;
    mg_slalom_by = PLAYER_Y - 4;
    // The ball starts at your feet (and the CPU's at theirs), still.
    mg_slalom_ahead[0] = mg_slalom_ahead[1] = 0;
    mg_slalom_bv[0] = mg_slalom_bv[1] = 0;
    mg_slalom_hop[0] = mg_slalom_hop[1] = 0;
    mg_slalom_dist = 0;
    mg_slalom_cdist = 0;
    mg_slalom_target = TARGET_DIST;
    mg_slalom_tenths = 0;
    mg_slalom_stumble = 0;
    mg_slalom_hits = mg_slalom_chits = 0;
    mg_slalom_streak = mg_slalom_best_streak = 0;
    mg_slalom_medal = MEDAL_NONE;
    mg_slalom_slide = 0;
    mg_slalom_drib_anim = 0;
    mg_slalom_foot = 0;
    sub6 = 0;
    cpu_cd = 16;
    cpu_stumble = cpu_drib = cpu_foot = cpu_slide = 0;
    fly_t = pop_t = dust_t = 0;
    scroll_x = 0;
    scroll_y = 0;

    build_course(course_you);
    if (mg_vs) build_course(course_cpu);
    sync_window();

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

    // The turf glides down after each step (easing to a stop), and each ball rolls on
    // as its runner follows it.
    uint8_t moved = glide(&mg_slalom_slide);
    scroll_y -= moved;
    ball_tick(0, moved, mg_slalom_dist > 0);
    if (mg_vs) ball_tick(1, glide(&cpu_slide), mg_slalom_cdist > 0);
    if (mg_slalom_drib_anim) mg_slalom_drib_anim--;

    if (mg_run == MG_RUN_LIVE) {
        if (++sub6 >= 6) {
            sub6 = 0;
            if (mg_slalom_tenths < 999) mg_slalom_tenths++;
        }
        if (mg_slalom_stumble) {
            mg_slalom_stumble--;
        } else {
            if (joy_pressed & (J_LEFT | J_B)) {
                step_dribble(0);
            } else if (joy_pressed & (J_RIGHT | J_A)) {
                step_dribble(1);
            }
        }
        if (mg_run == MG_RUN_LIVE) cpu_step();
        hud_draw();
    }

    draw_scene();
}
