#pragma bank 255
// KEEPY-UPPY (minigame.h): juggle the ball as long as you can. A training ground
// seen side-on; the ball falls under gravity with some drift, and you keep it up
// with touches timed to the moment it reaches the touch's height:
//
//   A        a kick, the ball at your foot
//   UP + A   a knee, the ball at your knee (a PERFECT knee kills the drift)
//   B        a header, the ball at your head (worth 2)
//   D-pad    stay under it
//
// A touch within pw frames of the moment is PERFECT (straight back up, over you);
// within gw frames GOOD (it drifts off: follow it); outside that, or out of reach,
// a MISS. Press early and you swing early, meeting the ball when it arrives. The
// ring on the floor under the ball tightens as it nears your foot and lights up:
// white while a kick would be GOOD, gold while it would be PERFECT.
//
// The ball touching the grass ends it. Points: 1 a kick or knee, 2 a header, and
// +2 for the touch called above your head ("HEAD!", "KNEE!"). Milestones (10, 25,
// 50 ...) chime and sparkle. Every milestone tier pops higher, falls faster and
// drifts more, the windows narrow, and gusts of wind push the ball about. CONTROL
// widens the windows; REFLEX quickens your feet.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include <rand.h>
#include "gfx.h"
#include "input.h"
#include "sfx.h"
#include "minigame.h"
#include "progress.h"
#include "mg_juggle_bg.h"
#include "mg_juggle_spr.h"

#define P(v) ((v) >> 4)  // sixteenths -> pixels
#define FLOOR_Y 124       // the player's feet, the ball's ground line (screen y)
#define X_MIN (12 << 4)   // the player's range
#define X_MAX (148 << 4)
#define BALL_MIN (8 << 4)  // the ball's, off the screen's edges
#define BALL_MAX (152 << 4)

// Sprite tiles (tools/art/mg_juggle.mjs checks these offsets).
#define T(n) (MG_SPR_TILES + (n))
#define T_RING_DARK 0  // six sizes, 4 tiles each (two columns)
#define T_RING_LIT 24
#define T_PERFECT 48  // 4 columns
#define T_GOOD 56     // 3
#define T_MISS 62     // 3
#define T_HEAD 68     // 3
#define T_KNEE 74     // 3
#define T_PLUS2 80    // 2 each: +2, +3, +4
#define T_STAR 92     // 2 frames
#define T_STREAK 96
#define T_NICE 98  // 3

// OAM: the ball 0, the ring 1 and 14, the player 2-5, then:
#define OAM_RING_R 14
#define OAM_WORD 15   // the grade, 4
#define OAM_BONUS 19  // +N, 2
#define OAM_CALL 21   // HEAD! / KNEE!, 3
#define OAM_STAR 24   // 8
#define OAM_WIND 32   // 3
#define OAM_LAST 34

// Sprite palettes 3 and 4 (the trails' in a match) are ours.
#define PAL_GOOD 3
#define PAL_COOL 4

// Touches.
#define TOUCH_NONE 0
#define TOUCH_FOOT 1
#define TOUCH_KNEE 2
#define TOUCH_HEAD 3
// Grades (and the words shown above the player).
#define GRADE_NONE 0
#define GRADE_GOOD 1
#define GRADE_PERFECT 2
#define GRADE_MISS 3
#define GRADE_NICE 4  // a milestone

#define WORD_FRAMES 40
#define STAR_FRAMES 36
#define FIRE_FRAMES 60
#define WIND_FRAMES 150
#define TIER_MAX 5

// --- watchable state ---
int16_t mg_juggle_px;            // the player (sixteenths)
int16_t mg_juggle_bx, mg_juggle_bz;  // the ball: x, height of its bottom above the grass
int16_t mg_juggle_vx, mg_juggle_vz;  // its velocity (vz up)
uint16_t mg_juggle_points;       // the score: the HUD's JUGGLES
uint16_t mg_juggle_touches;      // touches, each 1
uint16_t mg_juggle_next;         // the next milestone
uint8_t mg_juggle_perfects, mg_juggle_headers;
uint8_t mg_juggle_tier;          // 0 .. TIER_MAX: the difficulty
uint8_t mg_juggle_g;             // gravity, sixteenths a frame a frame
uint8_t mg_juggle_pw, mg_juggle_gw;  // the PERFECT and GOOD windows, frames either side
uint8_t mg_juggle_speed;         // the player's speed, sixteenths a frame
uint8_t mg_juggle_swing;         // the touch being made (TOUCH_*), for the pose
uint8_t mg_juggle_swing_t;       // frames left in its pose
uint8_t mg_juggle_cool;          // frames before another touch can start
uint8_t mg_juggle_pending;       // an early swing: frames until it meets the ball (0 none)
uint8_t mg_juggle_pend_grade;
uint8_t mg_juggle_grade, mg_juggle_grade_t;  // the word above the player, frames left
uint8_t mg_juggle_bonus;         // the +N beside it (0 none)
uint8_t mg_juggle_call;          // the touch called for (TOUCH_KNEE / TOUCH_HEAD, 0 none)
int8_t mg_juggle_wind;           // a gust: -1 left, 1 right
uint8_t mg_juggle_wind_t;        // frames left in it
uint8_t mg_juggle_below[4];      // frames the falling ball has been below each touch's height
uint8_t mg_juggle_cue;           // the ring: GRADE_NONE, GRADE_GOOD or GRADE_PERFECT
uint8_t mg_juggle_face;          // 1: facing left
uint8_t mg_juggle_moving;
uint8_t mg_juggle_dropped;       // the ball hit the grass
uint8_t mg_juggle_started;       // the opening kick-up happened
uint8_t mg_juggle_fire_t, mg_juggle_star_t;

static int16_t shown_points, shown_best;
static int16_t star_x[8], star_y[8];
static uint8_t wind_x[3];

// Each touch's height (the ball's bottom, sixteenths) and reach (px either side).
static const int16_t touch_z[4] = {0, 5 << 4, 10 << 4, 26 << 4};
static const uint8_t touch_reach[4] = {0, 14, 12, 9};
// The tiers: from these points on; gravity; the kick's pop (sqrt(2 g H) for a
// rise of H = 64, 80, 80, 96, 96, 100 px); a GOOD touch's drift (least, + 0..7).
static const uint16_t tier_at[TIER_MAX] = {10, 25, 50, 75, 100};
static const uint8_t tier_g[TIER_MAX + 1] = {2, 2, 3, 3, 4, 4};
static const uint8_t tier_pop[TIER_MAX + 1] = {64, 72, 88, 96, 111, 113};
static const uint8_t tier_drift[TIER_MAX + 1] = {5, 6, 8, 10, 12, 13};
static const uint8_t tier_narrow[TIER_MAX + 1] = {0, 0, 1, 1, 2, 2};
// Milestone sparkles: their steps (every other frame).
static const int8_t star_dx[8] = {3, 2, 0, -2, -3, -2, 0, 2};
static const int8_t star_dy[8] = {0, -2, -3, -2, 0, 2, 3, 2};
static const uint8_t wind_y[3] = {18, 40, 66};

// Background palettes 0-3 (tools/art/mg_juggle.mjs).
#define SKY RGB8(160, 212, 248)
#define DARK RGB8(32, 56, 64)
static const uint16_t juggle_pals[16] = {
    SKY, RGB8(255, 255, 255), RGB8(96, 160, 232), DARK,                          // sky
    SKY, RGB8(136, 200, 80), RGB8(64, 140, 64), DARK,                            // trees
    RGB8(24, 40, 96), RGB8(56, 104, 200), RGB8(248, 248, 248), RGB8(216, 56, 48),  // boards
    RGB8(120, 196, 80), RGB8(92, 168, 60), RGB8(240, 248, 240), RGB8(48, 104, 52),  // grass
};
static const uint16_t good_pal[4] = {0, RGB8(152, 240, 104), RGB8(72, 176, 64), RGB8(16, 56, 32)};
static const uint16_t cool_pal[4] = {0, RGB8(255, 255, 255), RGB8(176, 216, 248), RGB8(48, 88, 168)};

// --- rules ---
static void set_windows(void) {
    uint8_t c = stat_lv[STAT_CONTROL];
    uint8_t pw = 1 + (c >= 8) + (c >= 16);
    uint8_t gw = 4 + (c >= 4) + (c >= 10) + (c >= 16) - tier_narrow[mg_juggle_tier];
    if (mg_juggle_tier >= 4 && pw > 1) pw--;
    if (gw < pw + 1) gw = pw + 1;
    mg_juggle_pw = pw;
    mg_juggle_gw = gw;
    mg_juggle_g = tier_g[mg_juggle_tier];
}

// Frames until the ball (at or above z) falls below z: 1 .. gw, or 0 if not that soon.
static uint8_t frames_to(int16_t z) {
    int16_t bz = mg_juggle_bz, vz = mg_juggle_vz;
    uint8_t g = mg_juggle_g;
    for (uint8_t n = 1; n <= mg_juggle_gw; n++) {
        bz += vz;
        vz -= g;
        if (bz < z) return n;
    }
    return 0;
}

static void show(uint8_t grade, uint8_t bonus) {
    mg_juggle_grade = grade;
    mg_juggle_grade_t = WORD_FRAMES;
    mg_juggle_bonus = bonus;
}

static void miss(void) {
    show(GRADE_MISS, 0);
    sfx_play(SFX_CATCH);
}

static void milestone(void) {
    uint16_t m = mg_juggle_next;
    mg_juggle_next = m < 25 ? 25 : m < 100 ? m + 25 : m + 50;
    show(GRADE_NICE, 0);
    mg_juggle_star_t = STAR_FRAMES;
    mg_juggle_fire_t = FIRE_FRAMES;
    for (uint8_t i = 0; i < 8; i++) star_x[i] = P(mg_juggle_px), star_y[i] = FLOOR_Y - 14;
    sfx_play(SFX_GOAL);
}

// The ball meets touch t, graded: up it goes (or a miss, out of reach).
static void contact(uint8_t t, uint8_t grade) {
    int16_t dx = mg_juggle_bx - mg_juggle_px;
    uint8_t pop, gain, call = mg_juggle_call;
    if (dx < 0) dx = -dx;
    if (P(dx) > touch_reach[t]) {
        miss();
        return;
    }
    mg_juggle_bz = touch_z[t];
    pop = tier_pop[mg_juggle_tier];
    if (t == TOUCH_KNEE) pop -= pop >> 3;
    else if (t == TOUCH_HEAD) pop -= pop >> 2;
    mg_juggle_vz = pop;
    if (grade == GRADE_PERFECT) {
        // Straight up, curling back over you.
        mg_juggle_vx = (mg_juggle_px - mg_juggle_bx) >> 6;
        if (mg_juggle_tier >= 3 && t != TOUCH_KNEE) mg_juggle_vx += (int8_t)(rand() & 3) - 1;
        if (mg_juggle_perfects < 255) mg_juggle_perfects++;
        sfx_play(SFX_KICK);
    } else {
        int16_t d = tier_drift[mg_juggle_tier] + (rand() & 7);
        if (t == TOUCH_KNEE) d >>= 1;
        mg_juggle_vx = (rand() & 1) ? d : -d;
        sfx_play(SFX_TOUCH);
    }
    gain = 1;
    if (t == TOUCH_HEAD) {
        gain = 2;
        if (mg_juggle_headers < 255) mg_juggle_headers++;
    }
    mg_juggle_call = 0;
    if (call) {
        if (call == t) gain += 2, sfx_play(SFX_CHARGE);
    } else if (mg_juggle_tier >= 1 && (rand() & 3) == 0) {
        mg_juggle_call = (rand() & 1) ? TOUCH_HEAD : TOUCH_KNEE;
    }
    for (uint8_t k = 0; k < gain; k++) mg_juggle_points++;
    mg_juggle_touches++;
    show(grade, gain > 1 ? gain : 0);
    while (mg_juggle_tier < TIER_MAX && mg_juggle_points >= tier_at[mg_juggle_tier]) {
        mg_juggle_tier++;
        set_windows();
    }
    if (mg_juggle_points >= mg_juggle_next) milestone();
    if (mg_juggle_tier >= 2 && !mg_juggle_wind_t && (rand() & 7) == 0) {
        mg_juggle_wind = (rand() & 1) ? 1 : -1;
        mg_juggle_wind_t = WIND_FRAMES;
        wind_x[0] = 10, wind_x[1] = 70, wind_x[2] = 130;
    }
    mg_juggle_below[1] = mg_juggle_below[2] = mg_juggle_below[3] = 0;
}

// A press of touch t: early (the swing waits for the ball), late, or a whiff.
static void try_touch(uint8_t t) {
    int16_t z = touch_z[t];
    uint8_t n;
    mg_juggle_swing = t;
    mg_juggle_face = mg_juggle_bx < mg_juggle_px;
    if (mg_juggle_bz >= z) {
        n = frames_to(z);
        if (n) {
            mg_juggle_pending = n;
            mg_juggle_pend_grade = n <= mg_juggle_pw ? GRADE_PERFECT : GRADE_GOOD;
            mg_juggle_swing_t = n + 8;
            mg_juggle_cool = n + 10;
            return;
        }
    } else {
        n = mg_juggle_below[t];
        if (n && n <= mg_juggle_gw) {
            mg_juggle_swing_t = 10;
            mg_juggle_cool = 10;
            contact(t, n <= mg_juggle_pw ? GRADE_PERFECT : GRADE_GOOD);
            return;
        }
    }
    mg_juggle_swing_t = 12;
    mg_juggle_cool = 18;
    miss();
}

// "PERFECT 12 HEAD 3" for the results card.
static uint8_t put_num(uint8_t at, uint16_t n) {
    static const uint16_t tens[3] = {100, 10, 1};
    uint8_t started = 0;
    for (uint8_t k = 0; k < 3; k++) {
        uint8_t d = 0;
        while (n >= tens[k]) n -= tens[k], d++;
        if (d || started || k == 2) mg_detail[at++] = '0' + d, started = 1;
    }
    return at;
}
static uint8_t put_str(uint8_t at, const char *s) {
    while (*s) mg_detail[at++] = *s++;
    return at;
}

static void drop(void) {
    uint8_t at;
    mg_juggle_dropped = 1;
    mg_juggle_pending = 0;
    mg_juggle_call = 0;
    sfx_play(SFX_PARRY);
    at = put_str(0, "PERFECT ");
    at = put_num(at, mg_juggle_perfects);
    at = put_str(at, " HEAD ");
    at = put_num(at, mg_juggle_headers);
    mg_detail[at] = 0;
    mg_finish(mg_juggle_points);
}

// The ball's flight, a frame.
static void physics(void) {
    if (mg_juggle_wind_t) {
        mg_juggle_wind_t--;
        if (!((uint8_t)mg_timer & 3) && !mg_juggle_dropped) {
            mg_juggle_vx += mg_juggle_wind;
            if (mg_juggle_vx > 28) mg_juggle_vx = 28;
            if (mg_juggle_vx < -28) mg_juggle_vx = -28;
        }
    }
    mg_juggle_bx += mg_juggle_vx;
    if (mg_juggle_bx < BALL_MIN) {
        mg_juggle_bx = BALL_MIN;
        mg_juggle_vx = -mg_juggle_vx;
        sfx_play(SFX_BOARD);
    } else if (mg_juggle_bx > BALL_MAX) {
        mg_juggle_bx = BALL_MAX;
        mg_juggle_vx = -mg_juggle_vx;
        sfx_play(SFX_BOARD);
    }
    mg_juggle_bz += mg_juggle_vz;
    mg_juggle_vz -= mg_juggle_g;
    if (mg_juggle_bz <= 0) {
        // On the grass: bounce, dying away.
        mg_juggle_bz = 0;
        if (mg_juggle_vz < 0) {
            mg_juggle_vz = (-mg_juggle_vz) >> 1;
            mg_juggle_vx >>= 1;
            if (mg_juggle_vz < 12) mg_juggle_vz = 0, mg_juggle_vx = 0;
        }
    }
    for (uint8_t t = 1; t <= 3; t++) {
        if (mg_juggle_bz < touch_z[t] && mg_juggle_vz < 0) {
            if (mg_juggle_below[t] != 255) mg_juggle_below[t]++;
        } else {
            mg_juggle_below[t] = 0;
        }
    }
}

// The ring's light: what a kick now would be.
static void cue(void) {
    uint8_t n;
    mg_juggle_cue = GRADE_NONE;
    if (mg_juggle_dropped || !mg_juggle_started) return;
    if (mg_juggle_bz >= touch_z[TOUCH_FOOT]) n = frames_to(touch_z[TOUCH_FOOT]);
    else n = mg_juggle_below[TOUCH_FOOT] <= mg_juggle_gw ? mg_juggle_below[TOUCH_FOOT] : 0;
    if (n) mg_juggle_cue = n <= mg_juggle_pw ? GRADE_PERFECT : GRADE_GOOD;
}

// --- drawing ---
static void word(uint8_t oam, uint8_t tile, uint8_t cols, uint8_t pal, int16_t cx, int16_t y, uint8_t room) {
    int16_t x = cx - (cols << 2);
    for (uint8_t k = 0; k < room; k++) {
        if (k < cols) mg_sprite(oam + k, T(tile + (k << 1)), pal, x + (k << 3), y);
        else move_sprite(oam + k, 0, 0);
    }
}

static void draw(void) {
    int16_t px = P(mg_juggle_px), bx = P(mg_juggle_bx), bz = P(mg_juggle_bz);
    uint8_t f, flip = mg_juggle_face, tile, pal;
    int16_t feet = FLOOR_Y;

    // The player.
    if (mg_juggle_dropped) f = MG_F_LOSE;
    else if (mg_juggle_swing_t) {
        if (mg_juggle_swing == TOUCH_HEAD) f = MG_F_HEADER, feet = FLOOR_Y - 3;
        else f = mg_juggle_swing == TOUCH_KNEE ? MG_F_VOLLEY : MG_F_KICK;
    } else if (mg_juggle_moving) f = ((uint8_t)mg_timer & 8) ? MG_F_RUN_A : MG_F_RUN_B;
    else if (mg_juggle_star_t) f = MG_F_WIN;
    else f = MG_F_STAND;
    mg_player(0, MG_FRAME(MG_VIEW_FRONT, f), px, feet, SPR_PAL_YOU, flip);

    // The ball, spinning in the air; on fire after a milestone.
    tile = MG_T_BALL + (bz && ((uint8_t)mg_timer & 4) ? 2 : 0);
    mg_sprite(OAM_MG_BALL, tile, mg_juggle_fire_t ? SPR_PAL_POWER : SPR_PAL_BALL, bx - 4, FLOOR_Y - bz - 8);

    // The ring under it: tighter as it nears your foot, lit in the window.
    {
        int16_t d = mg_juggle_bz - touch_z[TOUCH_FOOT];
        uint8_t size = d <= 0 ? 0 : d >= (5 << 8) ? 5 : (uint8_t)(d >> 8);
        uint8_t base = mg_juggle_cue ? T_RING_LIT : T_RING_DARK;
        pal = mg_juggle_cue == GRADE_GOOD ? PAL_COOL : SPR_PAL_UI;
        base += size << 2;
        mg_sprite(OAM_MG_SHADOW, T(base), pal, bx - 8, FLOOR_Y - 8);
        mg_sprite(OAM_RING_R, T(base + 2), pal, bx, FLOOR_Y - 8);
    }

    // The grade, floating up over the player, and any bonus beside it.
    if (mg_juggle_grade_t) {
        int16_t y = FLOOR_Y - 56 - ((WORD_FRAMES - mg_juggle_grade_t) >> 2);
        uint8_t g = mg_juggle_grade;
        if (g == GRADE_PERFECT) word(OAM_WORD, T_PERFECT, 4, SPR_PAL_UI, px, y, 4);
        else if (g == GRADE_GOOD) word(OAM_WORD, T_GOOD, 3, PAL_GOOD, px, y, 4);
        else if (g == GRADE_MISS) word(OAM_WORD, T_MISS, 3, SPR_PAL_CPU, px, y, 4);
        else word(OAM_WORD, T_NICE, 3, SPR_PAL_UI, px, y, 4);
        if (mg_juggle_bonus) word(OAM_BONUS, T_PLUS2 + ((mg_juggle_bonus - 2) << 2), 2, SPR_PAL_UI, px + 26, y - 8, 2);
        else mg_hide(OAM_BONUS, OAM_BONUS + 1);
    } else {
        mg_hide(OAM_WORD, OAM_BONUS + 1);
    }

    // The call: HEAD! or KNEE!, blinking above.
    if (mg_juggle_call && !((uint8_t)mg_timer & 16)) {
        word(OAM_CALL, mg_juggle_call == TOUCH_HEAD ? T_HEAD : T_KNEE, 3, PAL_COOL, px, FLOOR_Y - 66, 3);
    } else {
        mg_hide(OAM_CALL, OAM_CALL + 2);
    }

    // Milestone sparkles.
    if (mg_juggle_star_t) {
        tile = T(T_STAR + (((uint8_t)mg_timer & 4) ? 2 : 0));
        for (uint8_t i = 0; i < 8; i++) mg_sprite(OAM_STAR + i, tile, SPR_PAL_UI, star_x[i] - 4, star_y[i] - 6);
    } else {
        mg_hide(OAM_STAR, OAM_STAR + 7);
    }

    // A gust: streaks across the sky.
    if (mg_juggle_wind_t) {
        pal = PAL_COOL | (mg_juggle_wind < 0 ? S_FLIPX : 0);
        for (uint8_t i = 0; i < 3; i++) mg_sprite(OAM_WIND + i, T(T_STREAK), pal, wind_x[i], wind_y[i]);
    } else {
        mg_hide(OAM_WIND, OAM_WIND + 2);
    }
}

// Timers and effects, a frame.
static void tick(void) {
    if (mg_juggle_swing_t) mg_juggle_swing_t--;
    if (mg_juggle_cool) mg_juggle_cool--;
    if (mg_juggle_grade_t) mg_juggle_grade_t--;
    if (mg_juggle_fire_t) mg_juggle_fire_t--;
    if (mg_juggle_star_t) {
        mg_juggle_star_t--;
        if (!((uint8_t)mg_timer & 1)) {
            for (uint8_t i = 0; i < 8; i++) star_x[i] += star_dx[i], star_y[i] += star_dy[i];
        }
    }
    if (mg_juggle_wind_t) {
        for (uint8_t i = 0; i < 3; i++) wind_x[i] += mg_juggle_wind < 0 ? -3 : 3;
    }
}

static void hud(void) {
    uint16_t best = mg_best[MG_JUGGLE];
    if (mg_juggle_points > best) best = mg_juggle_points;
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "JUGGLES");
        win_text(13, 0, "BEST");
        shown_points = shown_best = -1;
    }
    if ((int16_t)mg_juggle_points != shown_points) {
        shown_points = mg_juggle_points;
        win_number(8, 0, mg_juggle_points, 3, 0);
    }
    if ((int16_t)best != shown_best) {
        shown_best = best;
        win_number(17, 0, best, 3, 0);
    }
}

// --- the entry points ---
void mg_juggle_enter(void) BANKED {
    memcpy(pal_base_bg, juggle_pals, sizeof(juggle_pals));
    memcpy(&pal_base_obj[PAL_GOOD << 2], good_pal, sizeof(good_pal));
    memcpy(&pal_base_obj[PAL_COOL << 2], cool_pal, sizeof(cool_pal));
    mg_load_bg(mg_juggle_bg_tiles, MG_JUGGLE_BG_TILE_COUNT, mg_juggle_bg_map, mg_juggle_bg_attr,
               MG_JUGGLE_BG_MAP_W, MG_JUGGLE_BG_MAP_H, BANK(mg_juggle_bg));
    mg_load_spr(mg_juggle_spr_tiles, MG_JUGGLE_SPR_TILE_COUNT, BANK(mg_juggle_spr));
    mg_juggle_px = 80 << 4;
    mg_juggle_bx = 90 << 4;
    mg_juggle_bz = mg_juggle_vx = mg_juggle_vz = 0;
    mg_juggle_points = mg_juggle_touches = 0;
    mg_juggle_next = 10;
    mg_juggle_perfects = mg_juggle_headers = 0;
    mg_juggle_tier = 0;
    set_windows();
    mg_juggle_speed = 20 + stat_lv[STAT_REFLEX];
    mg_juggle_swing = mg_juggle_swing_t = mg_juggle_cool = mg_juggle_pending = 0;
    mg_juggle_grade = mg_juggle_grade_t = mg_juggle_bonus = mg_juggle_call = 0;
    mg_juggle_wind = 0;
    mg_juggle_wind_t = 0;
    wind_x[0] = 16;
    wind_x[1] = 72;
    wind_x[2] = 128;
    mg_juggle_below[0] = mg_juggle_below[1] = mg_juggle_below[2] = mg_juggle_below[3] = 0;
    mg_juggle_cue = mg_juggle_face = mg_juggle_moving = 0;
    mg_juggle_dropped = mg_juggle_started = 0;
    mg_juggle_fire_t = mg_juggle_star_t = 0;
    mg_hide(OAM_MG_FREE, OAM_LAST);
    draw();
}

void mg_juggle_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw();
        return;
    }
    if (mg_run == MG_RUN_LIVE) {
        hud();
        if (!mg_juggle_started) {
            // The kick-up: off the grass and up over your head.
            mg_juggle_started = 1;
            mg_juggle_swing = TOUCH_FOOT;
            mg_juggle_swing_t = 10;
            mg_juggle_cool = 16;
            mg_juggle_face = 0;
            mg_juggle_bz = touch_z[TOUCH_FOOT];
            mg_juggle_vz = tier_pop[0];
            mg_juggle_vx = (mg_juggle_px - mg_juggle_bx) >> 6;
            sfx_play(SFX_KICK);
        } else if (!mg_juggle_dropped) {
            // Feet: the d-pad.
            mg_juggle_moving = 0;
            if (joy & J_LEFT) {
                mg_juggle_px -= mg_juggle_speed;
                if (mg_juggle_px < X_MIN) mg_juggle_px = X_MIN;
                mg_juggle_moving = 1;
            } else if (joy & J_RIGHT) {
                mg_juggle_px += mg_juggle_speed;
                if (mg_juggle_px > X_MAX) mg_juggle_px = X_MAX;
                mg_juggle_moving = 1;
            }
            if (!mg_juggle_swing_t) {
                if (mg_juggle_bx < mg_juggle_px - 32) mg_juggle_face = 1;
                else if (mg_juggle_bx > mg_juggle_px + 32) mg_juggle_face = 0;
            }
            // Touches.
            if (!mg_juggle_cool) {
                if (joy_pressed & J_B) try_touch(TOUCH_HEAD);
                else if (joy_pressed & J_A) try_touch((joy & J_UP) ? TOUCH_KNEE : TOUCH_FOOT);
            }
        }
    } else {
        mg_juggle_moving = 0;
    }
    physics();
    if (mg_juggle_pending && !mg_juggle_dropped && --mg_juggle_pending == 0) {
        contact(mg_juggle_swing, mg_juggle_pend_grade);
    }
    if (mg_juggle_bz <= 0 && mg_run == MG_RUN_LIVE && mg_juggle_started && !mg_juggle_dropped) {
        drop();
    }
    cue();
    tick();
    draw();
}
