#pragma bank 255
// SHOOTOUT: penalties against the CPU (see minigame.h for the shell's contract).
//
// The camera stands behind the penalty spot, facing the goal. Five kicks each,
// you first, then sudden death if level (up to MAX_ROUNDS); it stops early once
// one side can't catch up. The score is your goals plus your saves.
//
// Shooting: the d-pad picks one of six spots in the goal (3 across, 2 high), A
// starts the power meter sweeping, A again locks it and you shoot. Too weak and the
// keeper reads it; too strong and it flies over (a low aim rises first); the gold
// sweet spot makes a fast ball the keeper can't reach in a corner. The CPU keeper
// guesses a side, weighted by where you've shot before, and dives after a reaction
// delay: it saves when it went the right way and gets there before the ball.
//
// Keeping: the CPU runs up, its target picked from a favorite side for the match.
// In the last frames of its run it gives itself away (it drifts away from the side
// it means, and turns toward it): higher REFLEX shows that earlier and clearer.
// Hold LEFT or RIGHT (and UP for a high dive) and press A to dive; A alone stays
// in the middle. Dive more than EARLY_WIN frames before the kick and the CPU may see
// you and switch; too late and you don't get there before the ball.
//
// POWER (all the XP) widens the sweet spot, slows the meter and speeds your shot.
// REFLEX (half) makes the CPU's tell earlier and bigger, and your dives quicker.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <string.h>
#include "gfx.h"
#include "input.h"
#include "sfx.h"
#include "minigame.h"
#include "progress.h"
#include "mg_shootout_bg.h"
#include "mg_shootout_spr.h"

// --- the pitch (tools/art/mg_shootout.mjs draws it to match) ---
#define GOAL_LINE 88
#define SPOT_X 80
#define SPOT_Y 118
#define KEEPER_FEET 86
#define MAX_ROUNDS 10
#define REGULATION 5

// --- phases ---
#define P_INTRO 0   // the next kick lines up
#define P_AIM 1     // you: pick a spot with the d-pad, A
#define P_POWER 2   // you: the meter sweeps, A locks it (B back to aiming)
#define P_RUNUP 3   // you run up to the ball
#define P_CPURUN 4  // the CPU runs up; you may dive
#define P_FLIGHT 5  // the ball is on its way
#define P_RESULT 6  // GOAL! / SAVED! / OVER! / WIDE!
#define P_DONE 7

#define KICK_YOU 0
#define KICK_CPU 1

#define R_NONE 0
#define R_GOAL 1
#define R_SAVED 2
#define R_OVER 3
#define R_WIDE 4

#define MARK_NONE 0
#define MARK_GOAL 1
#define MARK_MISS 2

// --- the meter (0 .. 63) ---
#define METER_MAX 63
#define WEAK_BELOW 18    // under this: the keeper reads it
#define SWEET_TOP 55     // the sweet spot's top; over it: too strong
#define SWEET_BOTTOM 48  // at POWER level 1 (lower with POWER)
#define OVER_FROM_LOW 60 // a low aim this strong goes over too (56..59 rise to high)
#define T_WEAK 40
#define T_SWEET 16
#define T_RISE 18

// --- timing ---
#define INTRO_FRAMES 40
#define RUNUP_FRAMES 16
#define RESULT_FRAMES 80
#define EARLY_WIN 10     // dive earlier than this before the CPU's kick and it may switch
#define CPU_KEEPER_REACT 8

// --- OAM (minigame.h: 14..39 are the game's) ---
#define OAM_AIM 14
#define OAM_METER 15     // 4 sprites
#define OAM_CURSOR 19
#define OAM_TICK 20      // 2
#define OAM_MARKS 22     // 10: yours 22..26, the CPU's 27..31
#define OAM_BANNER 32    // 6
#define BANNER_MAX 6

// --- sprite tiles (tools/art/mg_shootout.mjs SPR order) ---
#define T_MARK_GOAL (MG_SPR_TILES + 0)
#define T_MARK_MISS (MG_SPR_TILES + 2)
#define T_METER (MG_SPR_TILES + 4)
#define T_CURSOR (MG_SPR_TILES + 12)
#define T_TICK (MG_SPR_TILES + 14)
#define T_LETTERS (MG_SPR_TILES + 16)  // "GOAL!SVEDRWI", 2 tiles each

#define PAL_MARKS 3  // our own: the trail palettes, overwritten
#define PAL_METER 4

#define METER_X 144
#define METER_BOTTOM 130  // the meter's bottom row (power 0)
#define BANNER_Y 94

// --- watchable state ---
uint8_t mg_shootout_phase;
uint8_t mg_shootout_t;          // frames in the phase
uint8_t mg_shootout_kicker;     // KICK_YOU / KICK_CPU
uint8_t mg_shootout_you_goals, mg_shootout_cpu_goals;
uint8_t mg_shootout_you_kicks, mg_shootout_cpu_kicks;
uint8_t mg_shootout_saves;
uint8_t mg_shootout_marks_you[MAX_ROUNDS], mg_shootout_marks_cpu[MAX_ROUNDS];
uint8_t mg_shootout_col, mg_shootout_row;  // the kick's target (0..2, 0 low / 1 high)
uint8_t mg_shootout_power;      // the meter, 0 .. METER_MAX
uint16_t mg_shootout_power_sub; // ... in sixteenths
uint8_t mg_shootout_power_up;   // sweeping up
uint8_t mg_shootout_sweet_lo;   // the sweet spot's bottom (POWER)
uint8_t mg_shootout_flight;     // frames from the kick to the goal
uint8_t mg_shootout_over;       // the shot goes over / wide
uint8_t mg_shootout_result;     // R_*
uint8_t mg_shootout_hist[3];    // your shots to each column (the keeper learns)
uint8_t mg_shootout_kguess;     // the keeper's column (CPU keeper, or your dive)
uint8_t mg_shootout_kup;        // the keeper dives high
uint8_t mg_shootout_kmove;      // frames from starting the dive to getting there
uint8_t mg_shootout_dived;      // the keeper has committed
int16_t mg_shootout_dive_at;    // clock when the keeper started to dive
uint8_t mg_shootout_fav;        // the CPU's favorite side this match (0 or 2)
uint8_t mg_shootout_run;        // the CPU's run-up, frames
uint8_t mg_shootout_tell;       // frames of tell before the kick (REFLEX)
uint8_t mg_shootout_switched;   // the CPU saw your early dive and switched
uint8_t mg_shootout_miss;       // the CPU's kick is off target
int16_t mg_shootout_clock;      // frames since the kick's run-up started
int16_t mg_shootout_contact;    // clock at the kick (or -1 before it)
// The ball, in sixteenths: x, its ground line y, its height z, and their speeds.
int16_t mg_shootout_bx, mg_shootout_by, mg_shootout_bz;
int16_t mg_shootout_bvx, mg_shootout_bvy, mg_shootout_bvz;

static uint8_t hud_shown;      // what the HUD shows (redraw on change)
static uint8_t shooter_x, shooter_feet;
static uint16_t recip[49];      // 4096 / n: speeds without dividing a frame
static uint8_t banner_n;

static const uint8_t col_x[3] = {52, 80, 108};
static const uint8_t row_z[2] = {6, 36};
static const uint8_t slot_x[5] = {24, 34, 44, 54, 64};

static const uint16_t bg_pals[6][4] = {
    // 0 the court: teal, its dark band, navy, white lines
    {RGB8(64, 160, 176), RGB8(48, 136, 152), RGB8(40, 56, 88), RGB8(240, 248, 240)},
    // 1 the boards: navy, blue face, dark, white
    {RGB8(40, 56, 88), RGB8(64, 112, 200), RGB8(24, 40, 96), RGB8(240, 248, 240)},
    // 2 the crowd: dark stands, red shirts, faces, gold shirts
    {RGB8(36, 30, 60), RGB8(208, 72, 80), RGB8(240, 192, 144), RGB8(240, 200, 72)},
    // 3 the goal: the net's dark inside, mesh, white frame, grey
    {RGB8(30, 44, 72), RGB8(120, 144, 176), RGB8(248, 248, 248), RGB8(168, 176, 200)},
    // 4 the scoreboard: panel, text, slot rings, gold
    {RGB8(20, 22, 40), RGB8(232, 236, 255), RGB8(80, 92, 136), RGB8(254, 196, 52)},
    // 5 unused
    {RGB8(0, 0, 0), RGB8(0, 0, 0), RGB8(0, 0, 0), RGB8(0, 0, 0)},
};
static const uint16_t marks_pal[4] = {0, RGB8(72, 216, 96), RGB8(240, 56, 56), RGB8(20, 20, 32)};
static const uint16_t meter_pal[4] = {0, RGB8(120, 224, 120), RGB8(240, 64, 48), RGB8(20, 24, 48)};

// --- small helpers ---
static uint8_t lv(uint8_t s) {
    return stat_lv[s];
}
// A random number below n (n > 0), without dividing.
static uint8_t below(uint8_t n) {
    uint8_t r = rand();
    while (r >= n) r -= n;
    return r;
}
// v * recip[n] >> 8: v / n in sixteenths (v px, |v| < 64, n 8..48).
static int16_t per_frame(int16_t v, uint8_t n) {
    return (int16_t)((v * (int16_t)recip[n]) >> 8);
}

// --- the HUD ---
#define HUD_SCORE 0
static const char *hint(void) {
    switch (mg_shootout_phase) {
        case P_INTRO: return mg_shootout_kicker == KICK_YOU ? "YOUR KICK" : " SAVE IT!";
        case P_AIM: return "  AIM + A";
        case P_POWER: return " A: SHOOT";
        case P_CPURUN: return " DIVE: A!";
        default: return "         ";
    }
}
static uint8_t put_num(char *b, uint8_t at, uint8_t n) {
    if (n >= 10) b[at++] = '0' + (n >= 20 ? 2 : 1), n -= (n >= 20 ? 20 : 10);
    b[at++] = '0' + n;
    return at;
}
static void hud_draw(void) {
    char b[SCREEN_TW + 1];
    uint8_t n = 0;
    const char *h = hint();
    memset(b, ' ', SCREEN_TW);
    b[SCREEN_TW] = 0;
    b[n++] = 'Y', b[n++] = 'O', b[n++] = 'U', b[n++] = ' ';
    n = put_num(b, n, mg_shootout_you_goals);
    b[n++] = '-';
    n = put_num(b, n, mg_shootout_cpu_goals);
    b[n++] = ' ', b[n++] = 'C', b[n++] = 'P', b[n++] = 'U';
    n = SCREEN_TW - 9;
    while (*h) b[n++] = *h++;
    win_text(0, 0, b);
}
static void hud_update(void) {
    uint8_t key = (uint8_t)(mg_shootout_phase | (mg_shootout_kicker << 3));
    key ^= (uint8_t)((mg_shootout_you_goals << 4) + mg_shootout_cpu_goals * 37);
    if (mg_hud_dirty || key != hud_shown) {
        mg_hud_dirty = 0;
        hud_shown = key;
        hud_draw();
    }
}

// --- the scoreboard's marks ---
static void marks_draw(void) {
    // Rounds 1-5 on the first page; sudden death pages through 6-10.
    uint8_t page = (mg_shootout_you_kicks > REGULATION || mg_shootout_cpu_kicks > REGULATION ||
                    (mg_shootout_you_kicks == REGULATION && mg_shootout_cpu_kicks == REGULATION &&
                     mg_shootout_phase == P_INTRO))
                       ? REGULATION
                       : 0;
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t m = mg_shootout_marks_you[page + i], c = mg_shootout_marks_cpu[page + i];
        if (m) mg_sprite(OAM_MARKS + i, m == MARK_GOAL ? T_MARK_GOAL : T_MARK_MISS, PAL_MARKS, slot_x[i], 4);
        else move_sprite(OAM_MARKS + i, 0, 0);
        if (c) mg_sprite(OAM_MARKS + 5 + i, c == MARK_GOAL ? T_MARK_GOAL : T_MARK_MISS, PAL_MARKS, slot_x[i] + 64, 4);
        else move_sprite(OAM_MARKS + 5 + i, 0, 0);
    }
}

// --- the banner ---
static uint8_t letter_tile(char ch) {
    static const char order[] = "GOAL!SVEDRWI";
    for (uint8_t i = 0; order[i]; i++)
        if (order[i] == ch) return T_LETTERS + (i << 1);
    return 0xFF;
}
static void banner_show(const char *s, uint8_t pal) {
    uint8_t n = 0;
    while (s[n]) n++;
    int16_t x = 80 - (n << 2);
    banner_n = n;
    for (uint8_t i = 0; i < BANNER_MAX; i++) {
        if (i < n) mg_sprite(OAM_BANNER + i, letter_tile(s[i]), pal, x + (i << 3), BANNER_Y);
        else move_sprite(OAM_BANNER + i, 0, 0);
    }
}
static void banner_hide(void) {
    mg_hide(OAM_BANNER, OAM_BANNER + BANNER_MAX - 1);
    banner_n = 0;
}

// --- the meter ---
static void meter_show(uint8_t on) {
    if (!on) {
        mg_hide(OAM_METER, OAM_TICK + 1);
        return;
    }
    for (uint8_t s = 0; s < 4; s++)
        mg_sprite(OAM_METER + s, T_METER + (s << 1), PAL_METER, METER_X, METER_BOTTOM - 63 + (s << 4));
    mg_sprite(OAM_CURSOR, T_CURSOR, SPR_PAL_UI, METER_X - 8, METER_BOTTOM - mg_shootout_power - 3);
    mg_sprite(OAM_TICK, T_TICK, SPR_PAL_UI, METER_X + 8, METER_BOTTOM - SWEET_TOP - 2);
    mg_sprite(OAM_TICK + 1, T_TICK, SPR_PAL_UI, METER_X + 8, METER_BOTTOM - mg_shootout_sweet_lo - 2);
}

// --- the rules (each pinned by tests/mg_shootout.test.mjs) ---

// Frames for a keeper to get from the middle to (col, row) once it moves.
// The CPU's keeper.
static uint8_t cpu_keeper_move(uint8_t col, uint8_t row) {
    if (col == 1) return row ? 8 : 0;
    return row ? 16 : 12;
}
// Yours: quicker with REFLEX; UP makes high dives quicker and low ones slower.
static uint8_t your_keeper_move(uint8_t col, uint8_t row, uint8_t up) {
    uint8_t m, bonus = (uint8_t)(((uint16_t)lv(STAT_REFLEX) * 86) >> 8);
    if (col == 1) return 3;
    if (row) m = up ? 12 : 18;
    else m = up ? 18 : 14;
    return m > bonus + 6 ? m - bonus : 6;
}

// Your shot leaves the foot: its flight time from the meter, whether it goes over.
static void shot_from_power(void) {
    uint8_t p = mg_shootout_power, t;
    mg_shootout_over = 0;
    if (p < WEAK_BELOW) {
        t = T_WEAK;
    } else if (p > SWEET_TOP) {
        if (mg_shootout_row || p >= OVER_FROM_LOW) mg_shootout_over = 1;
        mg_shootout_row = 1;
        t = T_RISE;
    } else if (p >= mg_shootout_sweet_lo) {
        t = T_SWEET - (lv(STAT_POWER) >> 2);
    } else {
        t = 34 - (uint8_t)(((uint16_t)(p - WEAK_BELOW) * 13) >> 5);
        t -= lv(STAT_POWER) >> 2;
    }
    if (t < 12) t = 12;
    mg_shootout_flight = t;
}

// The CPU keeper picks a column at your kick: it learns where you like to shoot.
static void cpu_keeper_guess(void) {
    uint8_t w[3], total = 0;
    if (mg_shootout_power < WEAK_BELOW) {
        mg_shootout_kguess = mg_shootout_col;  // it has time to read a weak one
    } else {
        for (uint8_t c = 0; c < 3; c++) {
            uint8_t h = mg_shootout_hist[c] > 8 ? 8 : mg_shootout_hist[c];
            w[c] = (c == 1 ? 2 : 3) + h * 3;
            total += w[c];
        }
        uint8_t r = below(total);
        mg_shootout_kguess = r < w[0] ? 0 : r < w[0] + w[1] ? 1 : 2;
    }
    mg_shootout_kup = mg_shootout_row;  // it reads the height
    mg_shootout_kmove = cpu_keeper_move(mg_shootout_kguess, mg_shootout_kup);
    // It reacts a little quicker as the shootout goes on.
    uint8_t react = CPU_KEEPER_REACT - (mg_shootout_you_kicks > 6 ? 3 : mg_shootout_you_kicks >> 1);
    mg_shootout_dive_at = mg_shootout_contact + react;
    mg_shootout_dived = 1;
}

// The outcome when the ball reaches the goal line (from the globals, so a test can
// set up any kick): over / wide, saved, or a goal.
static uint8_t outcome(void) {
    if (mg_shootout_over)
        return (mg_shootout_kicker == KICK_CPU && mg_shootout_row == 0 && mg_shootout_col != 1) ? R_WIDE : R_OVER;
    if (!mg_shootout_dived) {
        // Your keeper never moved: it stands in the middle and stops what hits it.
        return mg_shootout_col == 1 ? R_SAVED : R_GOAL;
    }
    if (mg_shootout_kguess != mg_shootout_col) return R_GOAL;
    uint8_t move = mg_shootout_kicker == KICK_YOU
                       ? cpu_keeper_move(mg_shootout_col, mg_shootout_row)
                       : your_keeper_move(mg_shootout_col, mg_shootout_row, mg_shootout_kup);
    return mg_shootout_dive_at + move <= mg_shootout_contact + mg_shootout_flight ? R_SAVED : R_GOAL;
}

// Your dive, keeping: the d-pad picks the side, A goes.
static void your_dive(void) {
    mg_shootout_kguess = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : 1;
    mg_shootout_kup = (joy & J_UP) ? 1 : 0;
    mg_shootout_dive_at = mg_shootout_clock;
    mg_shootout_dived = 1;
    mg_shootout_kmove = your_keeper_move(mg_shootout_kguess, mg_shootout_kup, mg_shootout_kup);
    sfx_play(SFX_CATCH);
    // Too early, and the right way: the CPU sees you go and (mostly) switches.
    if (mg_shootout_contact < 0 && mg_shootout_clock + EARLY_WIN < (int16_t)mg_shootout_run &&
        mg_shootout_kguess == mg_shootout_col && (rand() & 3) != 0) {
        mg_shootout_switched = 1;
        if (mg_shootout_col == 1) mg_shootout_col = (rand() & 1) ? 2 : 0;
        else mg_shootout_col = (rand() & 1) ? 2 - mg_shootout_col : 1;
        if ((rand() & 3) == 0) mg_shootout_col = 2 - mg_shootout_kguess == 1 ? 0 : 2 - mg_shootout_kguess;
    }
}

// The CPU's plan for its kick: its favorite side mostly, the other, or the middle.
static void cpu_plan(void) {
    uint8_t r = rand() & 15;
    if (r < 7) mg_shootout_col = mg_shootout_fav;
    else if (r < 12) mg_shootout_col = 2 - mg_shootout_fav;
    else mg_shootout_col = 1;
    mg_shootout_row = below(3) == 0 ? 1 : 0;
    // Off target now and then, less often as the pressure rises.
    mg_shootout_miss = below(mg_shootout_cpu_kicks >= REGULATION ? 16 : 10) == 0;
    mg_shootout_run = 44 + (rand() & 15) + (rand() & 7);
    // The tell: REFLEX makes it start earlier.
    mg_shootout_tell = 6 + lv(STAT_REFLEX);
    if (mg_shootout_tell > 26) mg_shootout_tell = 26;
    mg_shootout_switched = 0;
}

// The CPU's flight time: quicker as the shootout goes on.
static void cpu_flight(void) {
    uint8_t ramp = mg_shootout_cpu_kicks > 4 ? 4 : mg_shootout_cpu_kicks;
    mg_shootout_flight = 20 + (rand() & 3) - ramp;
    if (mg_shootout_miss) mg_shootout_over = 1;
}

// After a kick: has the shootout been decided?
static uint8_t decided(void) {
    uint8_t yk = mg_shootout_you_kicks, ck = mg_shootout_cpu_kicks;
    uint8_t yg = mg_shootout_you_goals, cg = mg_shootout_cpu_goals;
    if (yk <= REGULATION && ck <= REGULATION) {
        uint8_t yleft = REGULATION - yk, cleft = REGULATION - ck;
        if (yg + yleft < cg || cg + cleft < yg) return 1;
        if (yk == REGULATION && ck == REGULATION) return yg != cg;
        return 0;
    }
    // Sudden death: after each pair of kicks.
    if (yk == ck) return yg != cg || yk >= MAX_ROUNDS;
    return 0;
}

static void finish(void) {
    char *d = mg_detail;
    uint8_t n = 0;
    mg_won = mg_shootout_you_goals > mg_shootout_cpu_goals;
    d[n++] = 'Y', d[n++] = 'O', d[n++] = 'U', d[n++] = ' ';
    n = put_num(d, n, mg_shootout_you_goals);
    d[n++] = ' ', d[n++] = '-', d[n++] = ' ';
    n = put_num(d, n, mg_shootout_cpu_goals);
    d[n++] = ' ', d[n++] = 'C', d[n++] = 'P', d[n++] = 'U';
    d[n] = 0;
    mg_shootout_phase = P_DONE;
    mg_finish(mg_shootout_you_goals + mg_shootout_saves);
}

// --- drawing ---
static void draw_shooter(uint8_t frame, uint8_t flip) {
    mg_player(0, MG_FRAME(MG_VIEW_BACK, frame), shooter_x, shooter_feet,
              mg_shootout_kicker == KICK_YOU ? SPR_PAL_YOU : SPR_PAL_CPU, flip);
}

// The keeper: standing ready, or on its way along the dive.
static void draw_keeper(void) {
    uint8_t pal = mg_shootout_kicker == KICK_YOU ? SPR_PAL_CPU : SPR_PAL_YOU;
    int16_t since = mg_shootout_clock - mg_shootout_dive_at;
    if (!mg_shootout_dived || since < 0) {
        // A little sway from side to side while it waits.
        int8_t sway = ((uint8_t)mg_shootout_clock & 32) ? 1 : -1;
        if (mg_shootout_phase == P_INTRO || mg_shootout_phase == P_AIM || mg_shootout_phase == P_POWER) sway = 0;
        mg_player(1, MG_FRAME(MG_VIEW_FRONT, MG_F_READY), 80 + sway, KEEPER_FEET, pal, 0);
        return;
    }
    uint8_t m = mg_shootout_kmove ? mg_shootout_kmove : 1;
    uint8_t k = since >= m ? m : (uint8_t)since;  // 0 .. m
    uint16_t frac = ((uint16_t)k * recip[m]) >> 4;  // 0 .. 256
    uint8_t col = mg_shootout_kguess, up = mg_shootout_kup;
    if (col == 1) {
        uint8_t lift = up ? (uint8_t)((14 * frac) >> 8) : 0;
        if (mg_shootout_phase == P_RESULT && mg_shootout_t > 30) lift = 0;
        mg_player(1, MG_FRAME(MG_VIEW_FRONT, up ? MG_F_HEADER : MG_F_READY), 80, KEEPER_FEET - lift, pal, 0);
        return;
    }
    int16_t dx = (int16_t)col_x[col] - 80;
    int16_t cy_end = up ? 52 : 79;
    int16_t x = 80 + (int16_t)((dx * (int16_t)frac) >> 8);
    int16_t cy = 72 + (int16_t)(((cy_end - 72) * (int16_t)frac) >> 8);
    // After the kick, a high dive drops back to the floor.
    if (mg_shootout_phase == P_RESULT && up && mg_shootout_t > 20) {
        uint8_t f = mg_shootout_t - 20;
        cy += f > 27 ? 27 : f;
    }
    mg_player_lying(1, x, cy, pal, col == 0);
}

static void draw_ball(void) {
    mg_ball(mg_shootout_bx >> 4, mg_shootout_by >> 4, mg_shootout_bz >> 4, mg_shootout_phase >= P_FLIGHT,
            mg_shootout_kicker == KICK_YOU && mg_shootout_flight <= T_SWEET && mg_shootout_phase == P_FLIGHT
                ? SPR_PAL_POWER
                : SPR_PAL_BALL);
}

static void draw_aim(uint8_t on) {
    if (!on || (mg_shootout_phase == P_AIM && (mg_shootout_t & 8))) {
        move_sprite(OAM_AIM, 0, 0);
        return;
    }
    mg_sprite(OAM_AIM, MG_T_AIM, SPR_PAL_UI, col_x[mg_shootout_col] - 4,
              GOAL_LINE - row_z[mg_shootout_row] - 8);
}

// --- the phases ---
static void ball_on_spot(void) {
    mg_shootout_bx = SPOT_X << 4;
    mg_shootout_by = SPOT_Y << 4;
    mg_shootout_bz = 0;
    mg_shootout_bvx = mg_shootout_bvy = mg_shootout_bvz = 0;
}

static void begin_kick(void) {
    mg_shootout_kicker = mg_shootout_you_kicks == mg_shootout_cpu_kicks ? KICK_YOU : KICK_CPU;
    mg_shootout_phase = P_INTRO;
    mg_shootout_t = 0;
    mg_shootout_result = R_NONE;
    mg_shootout_dived = 0;
    mg_shootout_dive_at = 0;
    mg_shootout_over = 0;
    mg_shootout_clock = 0;
    mg_shootout_contact = -1;
    mg_shootout_col = 1;
    mg_shootout_row = 0;
    mg_shootout_power = 0;
    mg_shootout_power_sub = 0;
    mg_shootout_power_up = 1;
    mg_shootout_kup = 0;
    ball_on_spot();
    banner_hide();
    meter_show(0);
    move_sprite(OAM_AIM, 0, 0);
    if (mg_shootout_kicker == KICK_CPU) cpu_plan();
    shooter_x = 62;
    shooter_feet = 150;
    marks_draw();
}

// Launch the ball at its target: velocity over the flight's frames.
static void launch(void) {
    int16_t tx = col_x[mg_shootout_col], tz = row_z[mg_shootout_row];
    if (mg_shootout_over) {
        if (mg_shootout_kicker == KICK_CPU && mg_shootout_row == 0 && mg_shootout_col != 1)
            tx = mg_shootout_col == 0 ? 20 : 140;
        else tz = 64;
    }
    mg_shootout_contact = mg_shootout_clock;
    mg_shootout_bvx = per_frame(tx - SPOT_X, mg_shootout_flight);
    mg_shootout_bvy = per_frame(GOAL_LINE - SPOT_Y, mg_shootout_flight);
    mg_shootout_bvz = per_frame(tz, mg_shootout_flight);
    mg_shootout_phase = P_FLIGHT;
    mg_shootout_t = 0;
    sfx_play(mg_shootout_kicker == KICK_YOU && mg_shootout_flight <= T_SWEET ? SFX_POWER : SFX_KICK);
}

static void update_intro(void) {
    // The kicker walks up behind the ball.
    if (shooter_feet > 132) shooter_feet--;
    draw_shooter(shooter_feet > 132 ? ((mg_shootout_t & 8) ? MG_F_RUN_A : MG_F_RUN_B) : MG_F_STAND, 0);
    if (mg_shootout_t == 1) sfx_play(SFX_WHISTLE);
    if (mg_shootout_t >= INTRO_FRAMES) {
        shooter_feet = 132;
        mg_shootout_t = 0;
        if (mg_shootout_kicker == KICK_YOU) {
            mg_shootout_phase = P_AIM;
            meter_show(1);
        } else {
            mg_shootout_phase = P_CPURUN;
            shooter_feet = 150;
        }
    }
}

static void update_aim(void) {
    uint8_t p = joy_pressed;
    if ((p & J_LEFT) && mg_shootout_col > 0) mg_shootout_col--, sfx_play(SFX_TOUCH);
    if ((p & J_RIGHT) && mg_shootout_col < 2) mg_shootout_col++, sfx_play(SFX_TOUCH);
    if ((p & J_UP) && !mg_shootout_row) mg_shootout_row = 1, sfx_play(SFX_TOUCH);
    if ((p & J_DOWN) && mg_shootout_row) mg_shootout_row = 0, sfx_play(SFX_TOUCH);
    draw_shooter(MG_F_STAND, 0);
    draw_aim(1);
    if (p & J_A) {
        mg_shootout_phase = P_POWER;
        mg_shootout_t = 0;
        mg_shootout_power = 0;
        mg_shootout_power_sub = 0;
        mg_shootout_power_up = 1;
        sfx_play(SFX_TOUCH);
    }
}

static void update_power(void) {
    // The meter sweeps up and down: slower with POWER.
    uint8_t lv_p = lv(STAT_POWER);
    uint8_t step = lv_p < 24 ? 40 - lv_p : 16;
    if (mg_shootout_power_up) {
        mg_shootout_power_sub += step;
        if (mg_shootout_power_sub >= (METER_MAX << 4)) mg_shootout_power_sub = METER_MAX << 4, mg_shootout_power_up = 0;
    } else {
        if (mg_shootout_power_sub <= step) mg_shootout_power_sub = 0, mg_shootout_power_up = 1;
        else mg_shootout_power_sub -= step;
    }
    mg_shootout_power = mg_shootout_power_sub >> 4;
    draw_shooter(MG_F_READY, 0);
    draw_aim(1);
    meter_show(1);
    if (joy_pressed & J_A) {
        // Lock it: the run-up, then the kick.
        shot_from_power();
        mg_shootout_phase = P_RUNUP;
        mg_shootout_t = 0;
        shooter_feet = 146;
        if (mg_shootout_power >= mg_shootout_sweet_lo && mg_shootout_power <= SWEET_TOP) sfx_play(SFX_CHARGE);
        else sfx_play(SFX_TOUCH);
    } else if (joy_pressed & J_B) {
        mg_shootout_phase = P_AIM;
        mg_shootout_t = 0;
        mg_shootout_power = 0;
        mg_shootout_power_sub = 0;
        meter_show(1);
    }
}

static void update_runup(void) {
    uint8_t t = mg_shootout_t;
    draw_aim(0);
    if (t < RUNUP_FRAMES) {
        // From behind and to the left, up to the ball.
        shooter_feet = 146 - t;
        shooter_x = 58 + (t >> 1) + (t >> 3);
        draw_shooter((t & 4) ? MG_F_RUN_A : MG_F_RUN_B, 0);
        return;
    }
    draw_shooter(MG_F_KICK, 0);
    meter_show(0);
    mg_shootout_hist[mg_shootout_col]++;
    launch();
    cpu_keeper_guess();
}

static void update_cpurun(void) {
    int16_t left = (int16_t)mg_shootout_run - mg_shootout_clock;  // frames to the kick
    uint8_t t = (uint8_t)mg_shootout_clock;
    int16_t x, feet;
    uint8_t flip = 0;
    if (!mg_shootout_dived && (joy_pressed & J_A)) your_dive();
    // Straight up from behind: x 58 -> 70, feet 150 -> 128.
    {
        int16_t done = mg_shootout_clock;
        int16_t run = mg_shootout_run;
        // feet: 22 px over the run; x: 12 px. (run <= 66: 22 * 66 fits easily.)
        int16_t f = done * 22, g = done * 12;
        // f / run without dividing: subtract.
        int16_t fy = 0, gx = 0;
        while (f >= run) f -= run, fy++;
        while (g >= run) g -= run, gx++;
        feet = 150 - fy;
        x = 58 + gx;
    }
    // The tell: in its last frames it drifts away from its side and turns to it.
    if (left <= (int16_t)mg_shootout_tell && mg_shootout_col != 1) {
        uint8_t e = mg_shootout_tell - (uint8_t)left;
        uint8_t amp = 3 + (lv(STAT_REFLEX) >> 2);
        uint8_t off = (e + 1) >> 1;
        if (off > amp) off = amp;
        if (mg_shootout_col == 2) x -= off;
        else x += off;
        flip = mg_shootout_col == 0;
    }
    shooter_x = (uint8_t)x;
    shooter_feet = (uint8_t)feet;
    draw_shooter(left <= 2 ? MG_F_KICK : (t & 4) ? MG_F_RUN_A : MG_F_RUN_B, flip);
    if (left <= 0) {
        cpu_flight();
        launch();
    }
}

static void update_flight(void) {
    // You may still dive after the kick, keeping.
    if (mg_shootout_kicker == KICK_CPU && !mg_shootout_dived && (joy_pressed & J_A)) your_dive();
    // The CPU keeper's dive starts after its reaction.
    if (mg_shootout_kicker == KICK_YOU && mg_shootout_clock == mg_shootout_dive_at && mg_shootout_kguess != 1)
        sfx_play(SFX_CATCH);
    mg_shootout_bx += mg_shootout_bvx;
    mg_shootout_by += mg_shootout_bvy;
    mg_shootout_bz += mg_shootout_bvz;
    if (mg_shootout_t >= mg_shootout_flight) {
        uint8_t r = outcome();
        mg_shootout_result = r;
        mg_shootout_phase = P_RESULT;
        mg_shootout_t = 0;
        if (r == R_GOAL) {
            // Snap into the net; it drops to the net's floor.
            mg_shootout_bx = col_x[mg_shootout_col] << 4;
            mg_shootout_by = (GOAL_LINE - 4) << 4;
            mg_shootout_bz = (row_z[mg_shootout_row] - 0) << 4;
            mg_shootout_bvx = 0, mg_shootout_bvy = 0, mg_shootout_bvz = 0;
            sfx_play(SFX_GOAL);
        } else if (r == R_SAVED) {
            // Palmed away: back out and to the side.
            mg_shootout_bvx = mg_shootout_col == 0 ? -20 : mg_shootout_col == 2 ? 20 : ((rand() & 1) ? 14 : -14);
            mg_shootout_bvy = 18;
            mg_shootout_bvz = 24;
            sfx_play(SFX_PARRY);
        } else {
            sfx_play(SFX_BOARD);
        }
        if (mg_shootout_kicker == KICK_YOU) {
            mg_shootout_marks_you[mg_shootout_you_kicks] = r == R_GOAL ? MARK_GOAL : MARK_MISS;
            mg_shootout_you_kicks++;
            if (r == R_GOAL) mg_shootout_you_goals++;
            banner_show(r == R_GOAL ? "GOAL!" : r == R_SAVED ? "SAVED!" : "OVER!",
                        r == R_GOAL ? SPR_PAL_POWER : SPR_PAL_CPU);
        } else {
            mg_shootout_marks_cpu[mg_shootout_cpu_kicks] = r == R_GOAL ? MARK_GOAL : MARK_MISS;
            mg_shootout_cpu_kicks++;
            if (r == R_GOAL) mg_shootout_cpu_goals++;
            if (r == R_SAVED) mg_shootout_saves++;
            banner_show(r == R_GOAL ? "GOAL!" : r == R_SAVED ? "SAVED!" : r == R_WIDE ? "WIDE!" : "OVER!",
                        r == R_GOAL ? SPR_PAL_CPU : SPR_PAL_YOU);
        }
        marks_draw();
    }
}

static void update_result(void) {
    uint8_t r = mg_shootout_result;
    if (r == R_GOAL) {
        // Drop to the floor of the net.
        if (mg_shootout_bz > 0) {
            mg_shootout_bvz -= 3;
            mg_shootout_bz += mg_shootout_bvz;
            if (mg_shootout_bz < 0) mg_shootout_bz = 0;
        }
    } else {
        mg_shootout_bx += mg_shootout_bvx;
        mg_shootout_by += mg_shootout_bvy;
        if (r == R_SAVED) {
            mg_shootout_bz += mg_shootout_bvz;
            mg_shootout_bvz -= 2;
            if (mg_shootout_bz < 0) {
                mg_shootout_bz = 0;
                mg_shootout_bvz = (-mg_shootout_bvz) >> 1;
                mg_shootout_bvx >>= 1;
                mg_shootout_bvy >>= 1;
            }
        } else {
            mg_shootout_bz += mg_shootout_bvz;
        }
    }
    // Keep the ball on the screen's sprites only while it's there.
    if (mg_shootout_bx < 0 || mg_shootout_bx > (168 << 4) || mg_shootout_bz > (100 << 4)) {
        mg_shootout_bvx = mg_shootout_bvy = mg_shootout_bvz = 0;
        mg_shootout_by = -1 << 4;  // hides the shadow
        mg_shootout_bz = 0;
        mg_shootout_bx = -16 << 4;
    }
    // The kicker celebrates or hangs its head.
    uint8_t happy = (r == R_GOAL) == (mg_shootout_kicker == KICK_YOU);
    (void)happy;
    draw_shooter(r == R_GOAL ? MG_F_WIN : MG_F_LOSE, 0);
    if (mg_shootout_t >= RESULT_FRAMES) {
        banner_hide();
        if (decided()) finish();
        else begin_kick();
    }
}

// --- the entry points ---
void mg_shootout_enter(void) BANKED {
    memcpy(pal_base_bg, bg_pals, sizeof(bg_pals));
    memcpy(&pal_base_obj[PAL_MARKS << 2], marks_pal, sizeof(marks_pal));
    memcpy(&pal_base_obj[PAL_METER << 2], meter_pal, sizeof(meter_pal));
    mg_load_bg(mg_shootout_bg_tiles, MG_SHOOTOUT_BG_TILE_COUNT, mg_shootout_bg_map, mg_shootout_bg_attr,
               MG_SHOOTOUT_BG_MAP_W, MG_SHOOTOUT_BG_MAP_H, BANK(mg_shootout_bg));
    mg_load_spr(mg_shootout_spr_tiles, MG_SHOOTOUT_SPR_TILE_COUNT, BANK(mg_shootout_spr));
    recip[0] = 4096;
    for (uint8_t n = 1; n < 49; n++) recip[n] = 4096 / n;
    mg_shootout_you_goals = mg_shootout_cpu_goals = 0;
    mg_shootout_you_kicks = mg_shootout_cpu_kicks = 0;
    mg_shootout_saves = 0;
    memset(mg_shootout_marks_you, 0, sizeof(mg_shootout_marks_you));
    memset(mg_shootout_marks_cpu, 0, sizeof(mg_shootout_marks_cpu));
    memset(mg_shootout_hist, 0, sizeof(mg_shootout_hist));
    mg_shootout_fav = (rand() & 1) ? 2 : 0;
    {
        uint8_t bonus = (lv(STAT_POWER) - 1) >> 1;
        mg_shootout_sweet_lo = bonus > 14 ? SWEET_BOTTOM - 14 : SWEET_BOTTOM - bonus;
    }
    hud_shown = 0xFF;
    begin_kick();
    shooter_feet = 132;
    draw_shooter(MG_F_STAND, 0);
    draw_keeper();
    draw_ball();
}

void mg_shootout_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw_shooter(MG_F_STAND, 0);
        draw_keeper();
        draw_ball();
        return;
    }
    if (mg_run == MG_RUN_OVER) {
        draw_keeper();
        draw_ball();
        return;
    }
    hud_update();
    if (mg_shootout_t < 255) mg_shootout_t++;
    switch (mg_shootout_phase) {
        case P_INTRO: update_intro(); break;
        case P_AIM: update_aim(); break;
        case P_POWER: update_power(); break;
        case P_RUNUP: update_runup(); break;
        case P_CPURUN: update_cpurun(); break;
        case P_FLIGHT: update_flight(); break;
        case P_RESULT: update_result(); break;
    }
    if (mg_shootout_phase == P_CPURUN || mg_shootout_phase == P_FLIGHT || mg_shootout_phase == P_RESULT ||
        mg_shootout_phase == P_RUNUP)
        mg_shootout_clock++;
    if (mg_shootout_phase != P_DONE) {
        draw_keeper();
        draw_ball();
    }
}
