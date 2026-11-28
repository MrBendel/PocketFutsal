#pragma bank 255
// CONE SLALOM (MG_SLALOM): TimberMan-style two-lane dribble sprint!
// Supports BOTH:
//   - SOLO   (mg_vs == 0, default): 40m Time Trial! Start slow and build
//     sprint speed with every consecutive clean move. Hitting a cone resets
//     your momentum back to slow (no time penalty). Beat GOLD (<=12.0s),
//     SILVER (<=16.0s), or BRONZE (<=21.0s)!
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
#include "shootout_reactions.h"
#include "reaction_art.h"

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
// Animated finish-line win/loss reactions streamed into free VRAM bank 0 sprite tiles.
#define T_REACT_YOU 32
#define T_REACT_CPU 40

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

// --- watchable state (global non-static for tests) ---
uint8_t mg_slalom_lane;          // 0 = Left lane, 1 = Right lane
uint8_t mg_slalom_dist;          // YOUR steps completed (0 .. TARGET_DIST)
uint8_t mg_slalom_cdist;         // CPU steps completed in VS mode (0 .. TARGET_DIST)
uint8_t mg_slalom_clane;         // CPU lane (0 or 1)
uint8_t mg_slalom_target;        // TARGET_DIST (40)
uint16_t mg_slalom_tenths;       // elapsed time in 0.1s units (no cone time penalty)
uint8_t mg_slalom_pace;          // current step cadence in frames (starts slow, drops with streak)
uint8_t mg_slalom_pace_start;    // starting slow cadence (scaled by CONTROL)
uint8_t mg_slalom_pace_min;      // top-speed floor cadence (scaled by REFLEX)
uint8_t mg_slalom_step_cd;       // frames remaining before next dribble step can fire
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
// The dribbled ball, for YOU [0] and the CPU [1]: independent X (1/16 px), lateral
// kick velocity, how far ahead of the feet it is (1/16 px), forward roll speed, and hop.
int16_t mg_slalom_bx_sub[2];
int16_t mg_slalom_bvx[2];
int16_t mg_slalom_ahead[2];
int16_t mg_slalom_bv[2];
uint8_t mg_slalom_hop[2];

// --- local state ---
static uint8_t course_you[COURSE_LEN];
static uint8_t course_cpu[COURSE_LEN];
static int16_t lane_l, lane_r;
static uint8_t sub6;
static uint8_t buf_side, buf_t;  // short input buffer so early taps fire cleanly on ready frame
static uint8_t cpu_cd, cpu_streak, cpu_stumble, cpu_drib, cpu_foot, cpu_slide;
// The ball's bounce off a kick (px up), frame by frame: a hop and a little second one.
#define HOP_FRAMES 12
static const uint8_t hop_z[HOP_FRAMES] = {0, 1, 2, 3, 3, 3, 2, 1, 0, 1, 1, 0};
static uint8_t fly_t;
static int16_t fly_x, fly_y;
static int8_t fly_vx;
static uint8_t dust_t;
static int16_t dust_x, dust_y;
static uint16_t shown_tenths;
static uint8_t shown_a, shown_b;
static uint8_t react_loaded[2];
static const uint8_t react_win_lift[6] = {0, 0, 1, 3, 0, 0};

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

// Compute step cadence (frames between steps) from current clean streak:
// starts slow (24 frames/step, ~2.5 steps/s) and gradually accelerates by 1 frame
// per consecutive clean move down to blazing top speed (3 frames/step; 2 with high REFLEX).
static uint8_t pace_for_streak(uint8_t streak) {
    uint8_t span = mg_slalom_pace_start - mg_slalom_pace_min;
    if (streak >= span) return mg_slalom_pace_min;
    return mg_slalom_pace_start - streak;
}

// The turf's glide after a step: visibly slow on the first few strides,
// accelerating into a blazing sprint as your streak builds!
static uint8_t glide(uint8_t *slide, uint8_t pace) {
    uint8_t s = *slide;
    if (!s) return 0;
    uint8_t step;
    if (pace >= 18) step = (s >> 3) + 1;
    else if (pace >= 10) step = (s >> 2) + 1;
    else if (pace >= 5) step = (s >> 1) + 1;
    else step = (s >> 1) + 4;
    if (step > s) step = s;
    *slide = s - step;
    return step;
}

// Kick ball i diagonally off the foot toward `target_x` and BALL_LEAD ahead, independent
// of the player's own glide so you visibly kick the ball left/right and then run onto it.
static void kick_ball(uint8_t i, uint8_t slide, int16_t target_x, uint8_t pace) {
    uint8_t lead = pace >= 18 ? (BALL_LEAD - 1) : BALL_LEAD;
    uint8_t s_add = (pace >= 18 && slide >= 2) ? (slide - 2) : slide;
    int16_t travel = ((int16_t)(lead + s_add) << 4) - mg_slalom_ahead[i];
    if (travel < 64) travel = 64;
    mg_slalom_bv[i] = travel >> 2;
    int16_t dx = ((int16_t)target_x << 4) - mg_slalom_bx_sub[i];
    mg_slalom_bvx[i] = dx >> 2;
    mg_slalom_hop[i] = HOP_FRAMES;
}

// Ball i a frame: rolls forward and sideways independently of the player, slowing by a
// quarter each frame, then eases to center at the player's toes once gathered.
static void ball_tick(uint8_t i, uint8_t scrolled, uint8_t kicked, int16_t target_x) {
    int16_t a = mg_slalom_ahead[i] - ((int16_t)scrolled << 4) + mg_slalom_bv[i];
    int16_t v = mg_slalom_bv[i];
    v -= (v + 3) >> 2;
    if (v < 0) v = 0;
    mg_slalom_bv[i] = v;
    if (!v && !scrolled && kicked) a += ((BALL_REST << 4) - a) >> 3;
    if (a < 0) a = 0;
    if (a > (40 << 4)) a = 40 << 4;
    mg_slalom_ahead[i] = a;

    int16_t bx = mg_slalom_bx_sub[i] + mg_slalom_bvx[i];
    int16_t vx = mg_slalom_bvx[i];
    if (vx > 0) {
        vx -= (vx + 3) >> 2;
        if (vx < 0) vx = 0;
    } else if (vx < 0) {
        vx += ((-vx) + 3) >> 2;
        if (vx > 0) vx = 0;
    }
    mg_slalom_bvx[i] = vx;
    if (!vx) {
        int16_t tx = target_x << 4;
        int16_t ex = tx - bx;
        if (ex > -8 && ex < 8) bx = tx;
        else bx += ex >> 2;
    }
    mg_slalom_bx_sub[i] = bx;

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

    if (hit) {
        mg_slalom_row[0] = CONE_NONE;
        if (mg_slalom_dist < COURSE_LEN) course_you[mg_slalom_dist] = CONE_NONE;
        mg_slalom_hits++;
        // Reset movement & momentum back to starting slow speed (no time penalty!).
        mg_slalom_streak = 0;
        mg_slalom_pace = mg_slalom_pace_start;
        mg_slalom_step_cd = 0;
        buf_t = 0;
        mg_slalom_bv[0] = 0;
        mg_slalom_bvx[0] = 0;
        mg_slalom_ahead[0] = 0;
        mg_slalom_hop[0] = 0;
        mg_slalom_stumble = mg_slalom_stumble_max;

        fly_t = 16;
        fly_x = target_x - 4;
        fly_y = ROW0_CY - 8;
        fly_vx = side ? 4 : -4;
        sfx_play(SFX_BOARD);
    } else {
        if (mg_slalom_streak < 255) mg_slalom_streak++;
        if (mg_slalom_streak > mg_slalom_best_streak) mg_slalom_best_streak = mg_slalom_streak;
        mg_slalom_pace = pace_for_streak(mg_slalom_streak);
        mg_slalom_step_cd = mg_slalom_pace;
        if (mg_slalom_pace <= 5) mg_slalom_drib_anim = 4;
        kick_ball(0, mg_slalom_slide, target_x, mg_slalom_pace);
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

    // CPU builds speed with consecutive clean moves and resets to slow on a cone clip!
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

    if (next_c == (cside + 1)) {
        course_cpu[mg_slalom_cdist] = CONE_NONE;
        mg_slalom_chits++;
        cpu_streak = 0;
        cpu_stumble = 20;
        cpu_cd = 22;
        mg_slalom_bv[1] = 0;
        mg_slalom_bvx[1] = 0;
        mg_slalom_ahead[1] = 0;
        mg_slalom_hop[1] = 0;
    } else {
        if (cpu_streak < 255) cpu_streak++;
        uint8_t cpace = cpu_streak < 14 ? (22 - cpu_streak) : 8;
        cpu_cd = cpace + ((uint8_t)rand() & 1);
        kick_ball(1, cpu_slide, cside ? VS_CPU_R : VS_CPU_L, cpace);
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
static void draw_ball(uint8_t i, int16_t feet, uint8_t pal) {
    uint8_t front = i ? OAM_BALL_CPU_FRONT : OAM_BALL_YOU_FRONT;
    uint8_t back = i ? OAM_BALL_CPU : OAM_BALL_YOU;
    int16_t ahead = mg_slalom_ahead[i] >> 4;
    uint8_t h = mg_slalom_hop[i];
    int16_t z = 0;
    if (h) z = hop_z[HOP_FRAMES - h];
    // Centered on its own kicked X (independent of player X), always drawn under the runner.
    int16_t bx = mg_slalom_bx_sub[i] >> 4;
    int16_t ground = feet - ahead;  // where it touches the grass
    int16_t top = ground - 8 - z;
    uint8_t tile = MG_T_BALL;
    if ((mg_slalom_bv[i] > 6 || mg_slalom_bvx[i] > 6 || mg_slalom_bvx[i] < -6 || h) && ((uint8_t)frame_count & 2))
        tile += 2;
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
// Tile/prop for all cone OAM slots are initialized once in mg_slalom_enter().
static void draw_cones(const uint8_t *course, uint8_t oam, uint8_t base, uint8_t ox_l, uint8_t ox_r, uint8_t you) {
    int16_t cy = (ROW0_CY + ROW_BACK * STEP_PX + 16) - (int16_t)mg_slalom_slide;
    uint8_t slot = oam;
    for (int8_t r = -ROW_BACK; r < ROW_COUNT; r++, slot++, cy -= STEP_PX) {
        uint8_t c = CONE_NONE;
        if (you && r >= 0) {
            c = mg_slalom_row[r];
        } else {
            int8_t w = (int8_t)base + r;
            if (w >= 0 && (uint8_t)w < COURSE_LEN) c = course[(uint8_t)w];
        }
        if (c == CONE_NONE || cy <= 0 || cy >= 160) {
            move_sprite(slot, 0, 0);
        } else {
            move_sprite(slot, c == CONE_LEFT ? ox_l : ox_r, (uint8_t)cy);
        }
    }
}

// Draw finish-line animated win/loss reaction from the canonical sprite sheet
// (shootout_reactions_tiles, Rows 6 & 7 of complete-blue.png) into VRAM bank 0.
static void draw_reaction(uint8_t slot, uint8_t won, int16_t cx, int16_t feet, uint8_t pal) {
    uint8_t phase = ((uint8_t)mg_timer) >> 3;
    while (phase >= 6) phase -= 6;
    uint8_t pose = phase + (won ? 0 : 6);
    uint8_t tile = slot ? T_REACT_CPU : T_REACT_YOU;
    if (react_loaded[slot] != pose) {
        uint8_t *dst = (uint8_t *)(0x8000u + ((uint16_t)tile << 4));
        gfx_vram_copy(dst, shootout_reactions_tiles + ((uint16_t)pose << 7), 128, BANK(shootout_reactions));
        react_loaded[slot] = pose;
    }
    uint8_t n = OAM_MG_PLAYER + (slot << 2);
    int16_t x = cx - 8;
    int16_t y = feet - 23 - (won ? react_win_lift[phase] : 0);
    mg_sprite(n + 0, tile + 0, pal, x, y);
    mg_sprite(n + 1, tile + 2, pal, x, y + 8);
    mg_sprite(n + 2, tile + 4, pal, x + 8, y);
    mg_sprite(n + 3, tile + 6, pal, x + 8, y + 8);
}

static void draw_scene(void) {
    int16_t target_x = mg_slalom_lane ? lane_r : lane_l;
    int16_t dx = target_x - mg_slalom_px;
    // At slower speeds, plant 1 frame so the ball visibly launches left/right first;
    // at turbo sprint speed, weave instantaneously so you keep up with 20+ steps/sec!
    if (mg_slalom_drib_anim < 6 || mg_slalom_pace <= 5) {
        if (mg_slalom_pace <= 5) {
            if (dx > 2) mg_slalom_px += (dx >> 1) + 2;
            else if (dx < -2) mg_slalom_px -= ((-dx) >> 1) + 2;
            else mg_slalom_px = target_x;
        } else {
            uint8_t sh = mg_slalom_pace >= 18 ? 3 : 2;
            if (dx > 1) mg_slalom_px += (dx >> sh) + 1;
            else if (dx < -1) mg_slalom_px -= ((-dx) >> sh) + 1;
            else mg_slalom_px = target_x;
        }
    }

    draw_ball(0, PLAYER_Y, mg_slalom_streak >= 14 ? SPR_PAL_POWER : SPR_PAL_BALL);

    // YOU sprite (slots 2..5): canonical back-view poses during play, animated reaction at finish.
    if (mg_run == MG_RUN_OVER) {
        draw_reaction(0, mg_won, mg_slalom_px, PLAYER_Y, SPR_PAL_YOU);
    } else {
        uint8_t f, flip;
        uint8_t kick_cut = mg_slalom_pace <= 5 ? 3 : 4;
        if (mg_slalom_stumble) {
            f = MG_F_READY;
            flip = mg_slalom_lane;
        } else if (mg_slalom_drib_anim >= kick_cut) {
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
    }

    // YOUR cones: row 0 level with your boots, row 1 the next step, passed rows sliding off.
    draw_cones(course_you, OAM_CONES_YOU, mg_slalom_dist, (uint8_t)(lane_l + 4), (uint8_t)(lane_r + 4), 1);

    // VS CPU Racer & CPU Cones on the right track!
    if (mg_vs) {
        int16_t ctarget_x = mg_slalom_clane ? VS_CPU_R : VS_CPU_L;
        int16_t cdx = ctarget_x - mg_slalom_cx;
        if (cpu_drib < 6) {
            if (cdx > 1) mg_slalom_cx += (cdx >> 2) + 1;
            else if (cdx < -1) mg_slalom_cx -= ((-cdx) >> 2) + 1;
            else mg_slalom_cx = ctarget_x;
        }

        // CPU screen Y relative to YOUR camera! If CPU is ahead (cdist > dist), CPU renders higher up!
        int16_t rel_steps = (int16_t)mg_slalom_cdist - (int16_t)mg_slalom_dist;
        int16_t cfeet = PLAYER_Y - rel_steps * STEP_PX - (int16_t)mg_slalom_slide + (int16_t)cpu_slide;
        if (cfeet >= 20 && cfeet <= 148) {
            draw_ball(1, cfeet, cpu_streak >= 12 ? SPR_PAL_POWER : SPR_PAL_BALL);

            if (mg_run == MG_RUN_OVER) {
                draw_reaction(1, !mg_won, mg_slalom_cx, cfeet, SPR_PAL_CPU);
            } else {
                uint8_t cf;
                if (cpu_stumble) cf = MG_F_READY;
                else if (cpu_drib >= 4) cf = MG_F_KICK;
                else if (cpu_drib > 0) cf = cpu_foot ? MG_F_RUN_A : MG_F_RUN_B;
                else cf = MG_F_READY;
                uint8_t cpal = (cpu_stumble & 2) ? SPR_PAL_FLASH : SPR_PAL_CPU;
                mg_player(1, MG_FRAME(MG_VIEW_BACK, cf), mg_slalom_cx, cfeet, cpal, cpu_foot);
            }
        } else {
            mg_player_hide(1);
            hide_ball(1);
        }

        // CPU's cones at the world steps in YOUR camera window, sliding with your turf.
        draw_cones(course_cpu, OAM_CONES_CPU, mg_slalom_dist, VS_CPU_L + 4, VS_CPU_R + 4, 0);
    } else {
        mg_player_hide(1);
        hide_ball(1);
        mg_hide(OAM_CONES_CPU, OAM_CONES_CPU + CONE_SLOTS - 1);
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
        mg_slalom_stumble_max = 22 - (lv_ref >> 2) - (lv_ctl >> 2);
        if (mg_slalom_stumble_max < 12) mg_slalom_stumble_max = 12;
    } else {
        mg_slalom_stumble_max = 14 - (lv_ref >> 2);
        if (mg_slalom_stumble_max < 8) mg_slalom_stumble_max = 8;
    }
    // CONTROL improves starting pace (24 down to 20); REFLEX improves top speed (3 down to 2).
    mg_slalom_pace_start = 24 - (lv_ctl >> 2);
    if (mg_slalom_pace_start < 20) mg_slalom_pace_start = 20;
    mg_slalom_pace_min = lv_ref >= 12 ? 2 : 3;
    mg_slalom_pace = mg_slalom_pace_start;
    mg_slalom_step_cd = 0;

    mg_slalom_lane = 0;
    mg_slalom_clane = 0;
    mg_slalom_px = lane_l;
    mg_slalom_cx = VS_CPU_L;
    mg_slalom_bx = lane_l;
    mg_slalom_by = PLAYER_Y - 4;
    // The ball starts centered at your feet (and the CPU's at theirs), still.
    mg_slalom_bx_sub[0] = lane_l << 4;
    mg_slalom_bx_sub[1] = VS_CPU_L << 4;
    mg_slalom_bvx[0] = mg_slalom_bvx[1] = 0;
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
    buf_side = buf_t = 0;
    cpu_cd = 22;
    cpu_streak = cpu_stumble = cpu_drib = cpu_foot = cpu_slide = 0;
    fly_t = dust_t = 0;
    react_loaded[0] = react_loaded[1] = 255;
    scroll_x = 0;
    scroll_y = 0;

    build_course(course_you);
    if (mg_vs) build_course(course_cpu);
    sync_window();

    mg_player_hide(1);
    mg_player_hide(2);
    mg_hide(OAM_MG_FREE, OAM_MG_COUNT - 1);
    for (uint8_t s = OAM_CONES_YOU; s < OAM_CONES_CPU + CONE_SLOTS; s++) {
        set_sprite_tile(s, T_CONE);
        set_sprite_prop(s, PAL_CONE);
    }
    draw_scene();
}

void mg_slalom_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw_scene();
        return;
    }

    // The turf glides down after each step (slower at low streak, snappier at top speed),
    // and each ball rolls on as its runner follows it.
    uint8_t moved = glide(&mg_slalom_slide, mg_slalom_pace);
    scroll_y -= moved;
    ball_tick(0, moved, mg_slalom_dist > 0, mg_slalom_lane ? lane_r : lane_l);
    if (mg_vs) {
        uint8_t cpace = cpu_streak < 14 ? (22 - cpu_streak) : 8;
        ball_tick(1, glide(&cpu_slide, cpace), mg_slalom_cdist > 0, mg_slalom_clane ? VS_CPU_R : VS_CPU_L);
    }
    if (mg_slalom_drib_anim) mg_slalom_drib_anim--;

    if (mg_run == MG_RUN_LIVE) {
        if (++sub6 >= 6) {
            sub6 = 0;
            if (mg_slalom_tenths < 999) mg_slalom_tenths++;
        }
        if (mg_slalom_step_cd) mg_slalom_step_cd--;
        if (buf_t) buf_t--;
        if (mg_slalom_stumble) {
            mg_slalom_stumble--;
        } else {
            if (joy_pressed & (J_LEFT | J_B)) {
                buf_side = 0;
                buf_t = 6;
            } else if (joy_pressed & (J_RIGHT | J_A)) {
                buf_side = 1;
                buf_t = 6;
            }
            if (buf_t && !mg_slalom_step_cd) {
                buf_t = 0;
                step_dribble(buf_side);
            }
        }
        if (mg_run == MG_RUN_LIVE) cpu_step();
        hud_draw();
    }

    draw_scene();
}
