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
#define BALL_REST_Y 89     // ball center y at rest (ahead of head y = 95, occluded behind player!)
#define ROW0_CY 96         // row 0 cone top y (y=96..110: beside your boots, already passed!)
                           // row 1 cone top y = 80 (y=80..94: right in front of you = next step!)
#define STEP_PX 16         // matches the 16 px grass stripes on mg_slalom_bg
#define ROW_COUNT 7        // visible rows 0..6 for YOU
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

// OAM layout (OAM_MG_FREE = 14 .. 39; slots > OAM_MG_PLAYER 2..9 so players ALWAYS occlude balls!).
#define OAM_BALL_YOU 14
#define OAM_SHAD_YOU 15
#define OAM_BALL_CPU 16
#define OAM_SHAD_CPU 17
#define OAM_CONES_YOU 18   // 18..24: 7 rows
#define OAM_CONES_CPU 25   // 25..31: 7 rows (VS mode)
#define OAM_FIN 32         // 32..35: checkered finish tape
#define OAM_FLY 36         // 36: flying bonked cone
#define OAM_POPUP 37       // 37..38: "+2S!" popup
#define OAM_DUST 39        // 39: turf dust puff

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

// --- local state ---
static uint8_t course_you[COURSE_LEN];
static uint8_t course_cpu[COURSE_LEN];
static int16_t lane_l, lane_r;
static uint8_t sub6;
static uint8_t cpu_cd, cpu_stumble, cpu_drib, cpu_foot, cpu_slide;
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
    mg_slalom_dist++;
    for (uint8_t r = 0; r < ROW_COUNT - 1; r++) {
        mg_slalom_row[r] = mg_slalom_row[r + 1];
    }
    uint8_t tail = mg_slalom_dist + ROW_COUNT - 1;
    mg_slalom_row[ROW_COUNT - 1] = tail < COURSE_LEN ? course_you[tail] : CONE_NONE;
    if (mg_slalom_dist < COURSE_LEN) course_you[mg_slalom_dist] = mg_slalom_row[0];

    uint8_t ns = mg_slalom_slide + STEP_PX;
    mg_slalom_slide = ns > 32 ? 32 : ns;

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
    if (cpu_slide) {
        uint8_t s = cpu_slide > 4 ? 4 : cpu_slide;
        cpu_slide -= s;
    }
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
    cpu_slide = STEP_PX;

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

// Draw a ball in a high OAM slot (> OAM_MG_PLAYER) so players ALWAYS occlude the ball!
static void draw_ball_slot(uint8_t oam_b, uint8_t oam_s, int16_t cx, int16_t cy, uint8_t spin, uint8_t pal) {
    uint8_t tile = MG_T_BALL + (spin ? 2 : 0);
    mg_sprite(oam_b, tile, pal, cx - 4, cy - 4);
    mg_sprite(oam_s, MG_T_SHADOW, SPR_PAL_UI, cx - 4, cy);
}

static void draw_scene(void) {
    // Hide default shell ball slots 0..1 (we use 14..17 so player sprites 2..9 draw ON TOP of the ball!).
    move_sprite(OAM_MG_BALL, 0, 0);
    move_sprite(OAM_MG_SHADOW, 0, 0);

    int16_t target_x = mg_slalom_lane ? lane_r : lane_l;
    int16_t dx = target_x - mg_slalom_px;
    if (dx > 1) mg_slalom_px += (dx >> 1) + 1;
    else if (dx < -1) mg_slalom_px -= ((-dx) >> 1) + 1;
    else mg_slalom_px = target_x;

    // Ball sits cleanly IN FRONT OF the player's body (y = 89 at rest, darting out to y = 80 on kick!)
    // AND in OAM slot 14 (> player slots 2..5) so it can NEVER appear on top of your head!
    int16_t push = (int16_t)mg_slalom_drib_anim + (mg_slalom_drib_anim >> 1);  // 0..10 px push ahead
    int16_t foot_ofs = mg_slalom_foot ? 2 : -2;
    mg_slalom_bx = ((mg_slalom_px + target_x) >> 1) + foot_ofs;
    mg_slalom_by = BALL_REST_Y - push;
    uint8_t spin = (mg_slalom_drib_anim > 0 || mg_slalom_slide > 0) && ((uint8_t)frame_count & 2);
    draw_ball_slot(OAM_BALL_YOU, OAM_SHAD_YOU, mg_slalom_bx, mg_slalom_by, spin,
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

    // YOUR cones (rows 0..6: row 0 is beside your feet at cy=96; row 1 is right ahead at cy=80!)
    for (uint8_t r = 0; r < ROW_COUNT; r++) {
        uint8_t c = mg_slalom_row[r];
        if (c == CONE_NONE) {
            move_sprite(OAM_CONES_YOU + r, 0, 0);
            continue;
        }
        int16_t cx = (c == CONE_LEFT ? lane_l : lane_r) - 4;
        int16_t cy = ROW0_CY - (int16_t)(r * STEP_PX) - (int16_t)mg_slalom_slide;
        mg_sprite(OAM_CONES_YOU + r, T_CONE, PAL_CONE, cx, cy);
    }

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
            int16_t cpush = (int16_t)cpu_drib + (cpu_drib >> 1);
            int16_t cby = cfeet - (PLAYER_Y - BALL_REST_Y) - cpush;
            uint8_t cspin = (cpu_drib > 0) && ((uint8_t)frame_count & 2);
            draw_ball_slot(OAM_BALL_CPU, OAM_SHAD_CPU, mg_slalom_cx + (cpu_foot ? 2 : -2), cby, cspin, SPR_PAL_BALL);

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
            mg_hide(OAM_BALL_CPU, OAM_SHAD_CPU);
        }

        // Draw CPU's cones at the exact world steps visible in YOUR camera window (dist .. dist + 6)!
        for (uint8_t r = 0; r < ROW_COUNT; r++) {
            uint8_t wstep = mg_slalom_dist + r;
            uint8_t cc = wstep < COURSE_LEN ? course_cpu[wstep] : CONE_NONE;
            if (cc == CONE_NONE) {
                move_sprite(OAM_CONES_CPU + r, 0, 0);
                continue;
            }
            int16_t ccx = (cc == CONE_LEFT ? VS_CPU_L : VS_CPU_R) - 4;
            int16_t ccy = ROW0_CY - (int16_t)(r * STEP_PX) - (int16_t)mg_slalom_slide;
            mg_sprite(OAM_CONES_CPU + r, T_CONE, PAL_CONE, ccx, ccy);
        }
    } else {
        mg_player_hide(1);
        mg_hide(OAM_BALL_CPU, OAM_CONES_CPU + ROW_COUNT - 1);
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

    // Penalty popup ("+2S!" or "+1S!" in Solo)
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
    mg_slalom_bx = lane_l - 2;
    mg_slalom_by = BALL_REST_Y;
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
