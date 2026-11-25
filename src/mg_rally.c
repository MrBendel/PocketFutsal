#pragma bank 255
// WALL RALLY (MG_RALLY): street-futsal / squash rebound wall mini game!
// Tuned to match Futnet Singles' exact pace, 3/4-SUB gravity, first-bounce
// hangtime, Mario Tennis momentum/skid, planting & tap-charging, and slide saves.
//
// Modes (toggled on brief card via mg_vs):
//   - VS CPU (mg_vs == 1, default): 1v1 Wall Squash! Alternate hitting rebounds
//     above the red tin line (z >= 8). First to 5 points wins!
//   - SOLO   (mg_vs == 0): Endless rebound streak! 3 misses and you are out.
//
// Stats trained: REFLEX (speed + reach) + POWER (charge rate + power drive).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include <rand.h>
#include "gfx.h"
#include "state.h"
#include "input.h"
#include "sfx.h"
#include "game.h"
#include "futnet_core.h"
#include "minigame.h"
#include "progress.h"
#include "mg_rally_bg.h"
#include "mg_rally_spr.h"

// --- court & Futnet-matched physics constants (world px, SUB = 16ths) ---
// Gravity is 3/4 SUB/frame (3 frames in 4, exact Futnet gravity!) so arcs and
// bounces hang in the air with the exact ~1.3s Mario Tennis / Futnet rhythm.
#define WALL_FOOT 56
#define TIN_Z 8            // hitting the wall below z = 8 px clangs the red tin (fault!)
#define PL_D_MIN 46
#define PL_D_MAX 104
#define PL_X_MIN 10
#define PL_X_MAX 150
#define BALL_X_MIN 6
#define BALL_X_MAX 154
#define D_OUT 118          // past baseline
#define REACH_FWD 10
#define REACH_BACK 8
#define REACH_Z 32
#define SLIDE_REACH 36     // planted out of reach: diving slide save!
#define SLIDE_FRAMES 22
#define KICK_FRAMES 14     // matches Futnet KICK_FRAMES
#define BOUNCE_MIN 20      // Futnet first-bounce sit-up (~52 frames hangtime)
#define DROP_HOP 14        // Futnet drop-shot low hop (~38 frames)
#define LOB_BOUNCE 24      // high wall lob bounce
#define BOUNCE_DEAD 4
#define WIN_PTS 5
#define SOLO_LIVES 3
#define WAIT_SERVE 50

// Ball states.
#define BALL_WAIT 0        // between points / waiting to serve
#define BALL_TO_WALL 1     // struck toward the rebound wall
#define BALL_FROM_WALL 2   // rebounded off the wall: next player's turn!
#define BALL_DEAD 3        // point ended (tin fault or 2nd bounce), rolling out

// Popup callouts.
#define POP_NONE 0
#define POP_PT 1
#define POP_TIN 2
#define POP_OUT 3

// Sprite tile offsets (from MG_SPR_TILES, tools/art/mg_rally.mjs).
#define RT_STAR 0
#define RT_P 2
#define RT_T 4
#define RT_BANG 6
#define RT_I 8
#define RT_N 10
#define RT_O 12
#define RT_U 14

// OAM slots (14..39).
#define OAM_MARK OAM_MG_FREE         // 14: floor bounce marker on YOUR turn
#define OAM_AIM (OAM_MG_FREE + 1)    // 15: wall aim marker while planted
#define OAM_STAR (OAM_MG_FREE + 2)   // 16: wall impact starburst
#define OAM_POP (OAM_MG_FREE + 3)    // 17..19: 3-letter callout ("PT!", "TIN", "OUT")
#define OAM_TRAIL (OAM_MG_FREE + 6)  // 20..23: 4-dot Futnet shot trail

// Perspective lookup tables (every 2 px of depth 0..126, matching tools/art/mg_rally.mjs).
static const uint8_t gy_tab[64] = {
    56, 57, 58, 59, 60, 61, 62, 63, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 77, 78, 79, 81,
    82, 83, 85, 86, 87, 89, 90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 106, 107, 109, 111,
    112, 114, 116, 118, 119, 121, 123, 125, 127, 129, 131, 132, 134, 136, 138, 140, 142, 144,
    146, 149, 151, 153,
};
static const uint8_t s_tab[64] = {
    96, 97, 97, 98, 98, 99, 100, 100, 101, 102, 102, 103, 103, 104, 105, 105, 106, 106, 107,
    108, 108, 109, 110, 110, 111, 111, 112, 113, 113, 114, 114, 115, 116, 116, 117, 118, 118,
    119, 119, 120, 121, 121, 122, 122, 123, 124, 124, 125, 126, 126, 127, 127, 128, 129, 129,
    130, 130, 131, 132, 132, 133, 134, 134, 135,
};
// round(128 * 16 / t) for t = 18 .. 57 (40 entries): vel_sub = (delta_px * RCP(t)) >> 7.
static const uint8_t rcp_tab[40] = {
    114, 108, 102, 98, 93, 89, 85, 82, 79, 76, 73, 71, 68, 66, 64, 62, 60, 58, 57, 55,
    54, 53, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 39, 38, 37, 37, 36,
};
#define RCP(t) rcp_tab[(t) - 18]

// Background palettes 0..4 (matching tools/art/mg_rally.mjs).
#define C(r, g, b) RGB8(r, g, b)
static const uint16_t bg_pals[20] = {
    C(56, 132, 148), C(42, 108, 126), C(22, 42, 56), C(244, 248, 248),   // 0: slate-teal court + white lines
    C(212, 148, 112), C(168, 76, 56), C(76, 32, 28), C(240, 208, 180),   // 1: warm brick piers
    C(196, 208, 220), C(156, 172, 190), C(64, 76, 96), C(250, 252, 255), // 2: painted rebound wall + white target
    C(40, 52, 76), C(248, 212, 96), C(16, 22, 38), C(255, 252, 220),     // 3: night floodlight rafters
    C(224, 52, 52), C(152, 28, 32), C(32, 16, 24), C(252, 248, 248),     // 4: crimson squash TIN + white foul tape
};
static const uint16_t ui_pal[4] = {0, C(255, 232, 64), C(232, 136, 24), C(40, 24, 8)};

// --- watchable state ---
int16_t mg_rally_bx, mg_rally_bd, mg_rally_bz;
int16_t mg_rally_vx, mg_rally_vd, mg_rally_vz;
uint8_t mg_rally_fall;       // 3-frames-in-4 gravity counter (exact Futnet ball_fall!)
uint8_t mg_rally_ball;       // BALL_*
uint8_t mg_rally_turn;       // 0 = YOU next strike, 1 = CPU next strike
uint8_t mg_rally_last;       // 0 = YOU struck to wall, 1 = CPU struck to wall
uint8_t mg_rally_bounces;    // floor bounces since wall rebound
uint8_t mg_rally_drop;       // 1 if shot to wall is a soft drop
uint8_t mg_rally_lob;        // 1 if shot to wall is a high lob
int16_t mg_rally_px, mg_rally_pd;
int16_t mg_rally_cx, mg_rally_cd;
int16_t mg_rally_land_x, mg_rally_land_d;  // predicted first bounce (world px)
uint8_t mg_rally_you_pts, mg_rally_cpu_pts;
uint16_t mg_rally_points;
uint8_t mg_rally_streak, mg_rally_best_streak;
uint8_t mg_rally_misses;
uint8_t mg_rally_server;
uint8_t mg_rally_wait;
uint8_t mg_rally_charge, mg_rally_charging;
uint8_t mg_rally_swing;      // buffered swing frames when tapped unplanted
uint8_t mg_rally_kick, mg_rally_ckick;
uint8_t mg_rally_slide, mg_rally_cslide;
int8_t mg_rally_slide_vx;
uint8_t mg_rally_walk, mg_rally_cwalk;
uint8_t mg_rally_cface;
uint8_t mg_rally_star, mg_rally_pop, mg_rally_pop_kind;
int16_t mg_rally_star_x, mg_rally_star_y;
uint8_t mg_rally_speed, mg_rally_reach, mg_rally_charge_rate;
uint8_t mg_rally_over;

static uint8_t shown_a, shown_b, shown_c;
static uint16_t shown_pts;

// --- helpers ---
static uint8_t didx(int16_t d_sub) {
    int16_t i = d_sub >> 5;
    if (i < 0) return 0;
    if (i > 63) return 63;
    return (uint8_t)i;
}
static int16_t proj_x(int16_t x_sub, uint8_t i) {
    return 80 + (int16_t)(((x_sub >> 4) - 80) * s_tab[i] >> 7);
}
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

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

static void predict_bounce(void) {
    // Fast 2-frames-per-step trace under 3/4 SUB/frame gravity (up to 80 frames).
    int16_t x = mg_rally_bx, d = mg_rally_bd, z = mg_rally_bz;
    int16_t vx = mg_rally_vx, vd = mg_rally_vd, vz = mg_rally_vz;
    uint8_t fall = mg_rally_fall;
    for (uint8_t k = 0; k < 40; k++) {
        x += vx << 1;
        d += vd << 1;
        if (++fall & 3) vz--;
        z += vz;
        if (++fall & 3) vz--;
        z += vz;
        if (x < (BALL_X_MIN << 4)) { x = BALL_X_MIN << 4; vx = -vx; }
        else if (x > (BALL_X_MAX << 4)) { x = BALL_X_MAX << 4; vx = -vx; }
        if (z <= 0) break;
    }
    mg_rally_land_x = clamp16(x >> 4, 16, 144);
    mg_rally_land_d = clamp16(d >> 4, PL_D_MIN, PL_D_MAX - 4);
}

// Launch toward the wall over `t` frames (18..57) under 3/4 SUB/frame gravity.
static void launch_to_wall(uint8_t who, int16_t tx, int16_t tz, uint8_t t,
                           uint8_t curve, uint8_t power, uint8_t drop, uint8_t lob) {
    if (t < 18) t = 18;
    if (t > 57) t = 57;
    fc_ball_plain();
    if (mg_rally_bz < (3 << 4)) mg_rally_bz = 3 << 4;
    if (curve) {
        int8_t side = tx < 64 ? -1 : tx > 96 ? 1 : ((mg_rally_bx >> 4) < 80 ? 1 : -1);
        int16_t wide_tx = tx - side * 28;
        mg_rally_vx = (int16_t)((wide_tx - (mg_rally_bx >> 4)) * RCP(t)) >> 7;
        ball_curve = side > 0 ? 10 : -10;
        ball_curve_t = t;
    } else {
        mg_rally_vx = (int16_t)((tx - (mg_rally_bx >> 4)) * RCP(t)) >> 7;
    }
    int16_t d_px = mg_rally_bd >> 4;
    if (d_px < 6) d_px = 6;
    mg_rally_vd = -((int16_t)(d_px * RCP(t)) >> 7);
    // Exact 3/4-gravity vertical velocity to arrive at height `tz` after `t` frames:
    mg_rally_vz = (((int16_t)(tz - (mg_rally_bz >> 4)) * RCP(t)) >> 7) + ((3 * (int16_t)(t + 1)) >> 3);
    mg_rally_ball = BALL_TO_WALL;
    mg_rally_last = who;
    mg_rally_drop = drop;
    mg_rally_lob = lob;
    mg_rally_bounces = 0;
    ball_power = power;
    ball_trail = (drop || lob) ? 0 : (curve ? TRAIL_CURVE : TRAIL_POWER);
    trail_len = power ? FC_TRAIL_MAX : 2;
    trail_n = 0;
    sfx_play(power ? SFX_POWER : ((drop || lob) ? SFX_TOUCH : SFX_KICK));
}

// Serve the ball cleanly off the wall at Futnet pace to start a rally!
static void serve_ball(void) {
    fc_ball_plain();
    mg_rally_charge = mg_rally_charging = mg_rally_swing = 0;
    mg_rally_slide = mg_rally_cslide = 0;
    fc_unready(SIDE_YOU);
    fc_unready(SIDE_CPU);
    mg_rally_bounces = 0;
    mg_rally_drop = mg_rally_lob = 0;
    mg_rally_streak = 0;

    if (mg_vs && mg_rally_server == 1) {
        mg_rally_bx = mg_rally_cx;
        mg_rally_bd = mg_rally_cd - (4 << 4);
        mg_rally_bz = 6 << 4;
        int16_t tx = (mg_rally_px >> 4) < 80 ? 108 : 52;
        launch_to_wall(1, tx, 24, 40, 0, 0, 0, 0);
        mg_rally_ckick = KICK_FRAMES;
    } else {
        int16_t sx = 56 + ((uint8_t)rand() & 47);
        mg_rally_bx = sx << 4;
        mg_rally_bd = 2 << 4;
        mg_rally_bz = 24 << 4;
        mg_rally_vx = ((mg_rally_px - mg_rally_bx) >> 6);
        mg_rally_vd = 20;
        mg_rally_vz = 6;
        mg_rally_fall = 0;
        mg_rally_ball = BALL_FROM_WALL;
        mg_rally_last = 1;
        mg_rally_turn = 0;
        predict_bounce();
        sfx_play(SFX_TOUCH);
    }
}

static int16_t player_aim_tx(void) {
    uint8_t aim = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : pl_shot[SIDE_YOU] ? pl_aim[SIDE_YOU] : 1;
    if (aim == 0) return 28;
    if (aim == 2) return 132;
    return mg_vs ? ((mg_rally_cx >> 4) < 80 ? 114 : 46) : (64 + ((uint8_t)rand() & 31));
}

static void player_strike(uint8_t is_slide) {
    uint8_t kind = pl_shot[SIDE_YOU] ? pl_shot[SIDE_YOU] : SHOT_POWER;
    uint8_t dep = (joy & J_UP) ? 2 : (joy & J_DOWN) ? 0 : pl_shot[SIDE_YOU] ? pl_depth[SIDE_YOU] : 1;
    uint8_t drop = !is_slide && (kind == SHOT_DROP || (dep == 0 && kind == SHOT_CURVE));
    uint8_t lob = is_slide || kind == SHOT_LOB;
    uint8_t curve = !drop && !lob && (kind == SHOT_CURVE);
    uint8_t power = !is_slide && !drop && !lob &&
                    (mg_rally_charge >= 48 || pl_charge[SIDE_YOU] >= CHARGE_MAX);
    uint8_t lv_p = stat_lv[STAT_POWER];

    int16_t tx = player_aim_tx();
    int16_t tz = drop ? 13 : lob ? 38 : (dep == 2 ? 32 : 22);
    uint8_t c_bonus = (pl_charge[SIDE_YOU] >> 2);  // 0..8 faster with charge
    uint8_t t = is_slide ? 48 : drop ? 48 : lob ? 50 : power ? (28 - (lv_p >> 2)) : (40 - c_bonus);

    launch_to_wall(0, tx, tz, t, curve, power, drop, lob);
    mg_rally_charge = mg_rally_charging = mg_rally_swing = 0;
    fc_unready(SIDE_YOU);
    if (!is_slide) mg_rally_kick = KICK_FRAMES;
}

static void cpu_strike(uint8_t stretched) {
    int16_t tx = (mg_rally_px >> 4) < 80 ? (112 + ((uint8_t)rand() & 15)) : (32 + ((uint8_t)rand() & 15));
    uint8_t r = (uint8_t)rand() & 7;
    uint8_t drop = !stretched && r == 0 && (mg_rally_pd >> 4) > 84;
    uint8_t lob = stretched || (r == 7 && (mg_rally_pd >> 4) < 62);
    uint8_t curve = !stretched && !drop && !lob && (r == 1 || r == 2);
    uint8_t power = !stretched && !drop && !lob && r == 3 && mg_rally_streak >= 4;
    int16_t tz = drop ? 13 : lob ? 36 : 22 + (r & 7);
    // At full stretch against a heavy/drop ball, CPU occasionally clips the red tin!
    if (stretched && (mg_rally_drop || ball_power) && ((uint8_t)rand() & 3) == 0) tz = 4;
    uint8_t t = lob ? 50 : drop ? 48 : power ? 28 : 40;

    launch_to_wall(1, tx, tz, t, curve, power, drop, lob);
    fc_unready(SIDE_CPU);
    mg_rally_ckick = KICK_FRAMES;
}

static void end_point(uint8_t winner_side, uint8_t pop_kind) {
    uint8_t at;
    mg_rally_ball = BALL_DEAD;
    mg_rally_pop = 40;
    mg_rally_pop_kind = pop_kind;
    mg_rally_charging = mg_rally_charge = mg_rally_swing = 0;
    fc_unready(SIDE_YOU);
    fc_unready(SIDE_CPU);
    ball_trail = 0;
    ball_curve_t = 0;

    if (mg_vs) {
        if (winner_side == 0) {
            mg_rally_you_pts++;
            mg_rally_server = 1;
            sfx_play(SFX_GOAL);
        } else {
            mg_rally_cpu_pts++;
            mg_rally_server = 0;
            sfx_play(SFX_PARRY);
        }
        if (mg_rally_you_pts >= WIN_PTS || mg_rally_cpu_pts >= WIN_PTS) {
            mg_rally_over = 1;
            mg_won = mg_rally_you_pts > mg_rally_cpu_pts;
            at = put_str(0, "YOU ");
            at = put_num(at, mg_rally_you_pts);
            at = put_str(at, " - ");
            at = put_num(at, mg_rally_cpu_pts);
            at = put_str(at, " CPU");
            mg_detail[at] = 0;
            mg_finish((uint16_t)mg_rally_you_pts * 4 + mg_rally_best_streak);
        } else {
            mg_rally_wait = WAIT_SERVE;
        }
    } else {
        mg_rally_misses++;
        sfx_play(SFX_PARRY);
        if (mg_rally_misses >= SOLO_LIVES) {
            mg_rally_over = 1;
            mg_won = 0;
            at = put_str(0, "BEST RALLY ");
            at = put_num(at, mg_rally_best_streak);
            mg_detail[at] = 0;
            mg_finish(mg_rally_points);
        } else {
            mg_rally_wait = WAIT_SERVE;
        }
    }
}

static void wall_rebound(void) {
    int16_t z = mg_rally_bz >> 4;
    mg_rally_star = 12;
    mg_rally_star_x = proj_x(mg_rally_bx, 0) - 4;
    mg_rally_star_y = WALL_FOOT - (z < 2 ? 2 : z > 44 ? 44 : z) - 4;

    if (z < TIN_Z) {
        // CLANG! Hit the red squash tin -> fault by mg_rally_last!
        sfx_play(SFX_BOARD);
        mg_rally_bd = 0;
        mg_rally_vd = 8;
        mg_rally_vz = 6;
        end_point(1 - mg_rally_last, POP_TIN);
        return;
    }

    // Clean wall hit above the tin — rebounds at Futnet pace!
    sfx_play(SFX_BOARD);
    mg_rally_streak++;
    if (mg_rally_streak > mg_rally_best_streak) mg_rally_best_streak = mg_rally_streak;
    if (!mg_vs) {
        mg_rally_points += 1 + ball_power;
    }

    uint8_t pace = mg_rally_streak >> 3;  // gentle ramp over long rallies (0..4)
    if (pace > 4) pace = 4;
    mg_rally_bd = 0;
    if (mg_rally_drop) {
        mg_rally_vd = 16;
        mg_rally_vz = 2;
        mg_rally_vx >>= 1;
    } else if (mg_rally_lob) {
        mg_rally_vd = 19;
        mg_rally_vz = 14;
        mg_rally_vx = (mg_rally_vx * 3) >> 2;
    } else if (ball_power) {
        mg_rally_vd = 27 + pace;
        mg_rally_vz = 4;
    } else {
        mg_rally_vd = 21 + pace;
        mg_rally_vz = 6;
    }
    if (ball_curve) {
        mg_rally_vx += ball_curve > 0 ? 5 : -5;
    }
    mg_rally_vx = clamp16(mg_rally_vx, -18, 18);
    mg_rally_ball = BALL_FROM_WALL;
    mg_rally_bounces = 0;
    mg_rally_turn = mg_vs ? (1 - mg_rally_last) : 0;
    ball_trail = 0;
    ball_curve_t = 0;
    predict_bounce();
}

static void ball_step(void) {
    if (mg_rally_ball == BALL_WAIT) return;
    if (mg_rally_ball == BALL_TO_WALL && ball_curve_t) {
        ball_vx = mg_rally_vx;
        fc_curve_step();
        mg_rally_vx = ball_vx;
    }
    mg_rally_bx += mg_rally_vx;
    mg_rally_bd += mg_rally_vd;
    if (++mg_rally_fall & 3) mg_rally_vz--;  // Futnet 3/4 SUB/frame gravity!
    mg_rally_bz += mg_rally_vz;

    if (mg_rally_bx < (BALL_X_MIN << 4)) {
        mg_rally_bx = BALL_X_MIN << 4;
        mg_rally_vx = -mg_rally_vx;
    } else if (mg_rally_bx > (BALL_X_MAX << 4)) {
        mg_rally_bx = BALL_X_MAX << 4;
        mg_rally_vx = -mg_rally_vx;
    }

    if (mg_rally_bz <= 0) {
        uint8_t first = (mg_rally_bounces == 0);
        int16_t up = (-mg_rally_vz * 3) >> 2;
        mg_rally_bz = 0;
        if (mg_rally_ball == BALL_FROM_WALL) {
            // Exact Futnet first-bounce sit-up so you have time to read, run, & strike!
            if (mg_rally_drop) {
                if (first && up < DROP_HOP) up = DROP_HOP;
                mg_rally_vx >>= 1;
                mg_rally_vd >>= 1;
            } else if (mg_rally_lob) {
                if (first && up < LOB_BOUNCE) up = LOB_BOUNCE;
                mg_rally_vx >>= 1;
                mg_rally_vd >>= 1;
            } else {
                if (first && up < BOUNCE_MIN) up = BOUNCE_MIN;
                mg_rally_vx -= mg_rally_vx >> 2;
                mg_rally_vd -= mg_rally_vd >> 2;
            }
            mg_rally_vz = up > BOUNCE_DEAD ? up : 0;
            if (mg_rally_vz) sfx_play(SFX_TOUCH);
            if (++mg_rally_bounces >= 2 || !mg_rally_vz) {
                end_point(1 - mg_rally_turn, mg_vs ? POP_PT : POP_OUT);
                return;
            }
        } else {
            mg_rally_vz = up > BOUNCE_DEAD ? up : 0;
            mg_rally_vx -= mg_rally_vx >> 2;
            mg_rally_vd -= mg_rally_vd >> 2;
        }
    }

    if (mg_rally_ball == BALL_TO_WALL && mg_rally_bd <= 0) {
        wall_rebound();
        return;
    }
    if (mg_rally_ball == BALL_FROM_WALL && mg_rally_bd > (D_OUT << 4)) {
        end_point(1 - mg_rally_turn, mg_vs ? POP_PT : POP_OUT);
    }
}

static uint8_t in_player_reach(void) {
    if (mg_rally_ball != BALL_FROM_WALL || (mg_vs && mg_rally_turn != 0)) return 0;
    int16_t dx = (mg_rally_bx - mg_rally_px) >> 4;
    int16_t dd = (mg_rally_bd - mg_rally_pd) >> 4;
    int16_t r = (int16_t)mg_rally_reach;
    return dx >= -r && dx <= r && dd >= -REACH_FWD && dd <= REACH_BACK && (mg_rally_bz >> 4) <= REACH_Z;
}

static void player_step(void) {
    if (mg_rally_kick) mg_rally_kick--;
    if (mg_rally_swing) mg_rally_swing--;
    fc_combo_tick(SIDE_YOU);

    // Diving slide recovery (exact Futnet slide feel).
    if (mg_rally_slide) {
        mg_rally_slide--;
        if (mg_rally_slide > 8) {
            pl_x[SIDE_YOU] = clamp16(pl_x[SIDE_YOU] + mg_rally_slide_vx, PL_X_MIN << 4, PL_X_MAX << 4);
            mg_rally_px = pl_x[SIDE_YOU];
            if (in_player_reach()) player_strike(1);
        }
        return;
    }

    uint8_t your_ball = (mg_rally_ball == BALL_FROM_WALL && (!mg_vs || mg_rally_turn == 0));
    if (!your_ball && pl_shot[SIDE_YOU]) {
        mg_rally_charging = mg_rally_charge = 0;
        fc_unready(SIDE_YOU);
    }

    // Queue / tap-charge A (power), B (curve), B->A (lob), A->B (drop) once it's your ball!
    if (joy_pressed & (J_A | J_B)) {
        uint8_t b = (joy_pressed & J_B) ? 1 : 0;
        if (your_ball) {
            fc_ready(SIDE_YOU, b, 0);
            mg_rally_charging = 1;
            uint8_t fc_c = (uint8_t)(pl_charge[SIDE_YOU] + (pl_charge[SIDE_YOU] >> 1));
            if (fc_c > mg_rally_charge) mg_rally_charge = fc_c;
            if (in_player_reach()) {
                player_strike(0);
                return;
            }
        } else {
            mg_rally_swing = 10;
        }
    }
    if (joy_pressed & J_SELECT) {
        mg_rally_charging = mg_rally_charge = mg_rally_swing = 0;
        fc_unready(SIDE_YOU);
    }

    // While planted (pl_shot[SIDE_YOU]), D-pad aims your shot & you brake to a plant,
    // exact Futnet style! Unplanted, you run with full Futnet momentum & turn skids.
    if (pl_shot[SIDE_YOU]) {
        if (joy & J_LEFT) pl_aim[SIDE_YOU] = 0;
        else if (joy & J_RIGHT) pl_aim[SIDE_YOU] = 2;
        if (joy & J_UP) pl_depth[SIDE_YOU] = 2;
        else if (joy & J_DOWN) pl_depth[SIDE_YOU] = 0;
        if ((joy & (J_A | J_B)) && mg_rally_charge < 48 && ((uint8_t)frame_count & 1)) {
            uint8_t next = mg_rally_charge + mg_rally_charge_rate;
            mg_rally_charge = next > 48 ? 48 : next;
            pl_charge[SIDE_YOU] = (uint8_t)((mg_rally_charge * 5) >> 3);
        }
    }

    int8_t sp = (int8_t)mg_rally_speed;
    int8_t tx = 0, ty = 0;
    uint8_t firm = 2;  // firm=2: Futnet skid when reversing direction!
    if (pl_shot[SIDE_YOU]) {
        firm = 1;      // planted: brake cleanly in place
    } else {
        tx = (joy & J_RIGHT) ? sp : (joy & J_LEFT) ? -sp : 0;
        ty = (joy & J_DOWN) ? sp : (joy & J_UP) ? -sp : 0;
    }

    fc_min_x = PL_X_MIN << 4;
    fc_max_x = PL_X_MAX << 4;
    fc_min_y = PL_D_MIN << 4;
    fc_max_y = PL_D_MAX << 4;
    fc_step_move(SIDE_YOU, tx, ty, firm);
    mg_rally_px = pl_x[SIDE_YOU];
    mg_rally_pd = pl_y[SIDE_YOU];
    if (pl_vx[SIDE_YOU] || pl_vy[SIDE_YOU]) mg_rally_walk++;

    if (your_ball && pl_shot[SIDE_YOU]) {
        if (in_player_reach()) {
            player_strike(0);
            return;
        }
        // Planted & ball is crossing our depth outside reach -> Futnet diving slide!
        int16_t dx = (mg_rally_bx - mg_rally_px) >> 4;
        int16_t adx = dx < 0 ? -dx : dx;
        int16_t dd = (mg_rally_bd - mg_rally_pd) >> 4;
        if (adx > mg_rally_reach && adx <= SLIDE_REACH && dd >= -6 && dd <= 8 && (mg_rally_bz >> 4) <= 24) {
            mg_rally_slide = SLIDE_FRAMES;
            mg_rally_slide_vx = dx < 0 ? -36 : 36;
            pl_face[SIDE_YOU] = dx < 0;
            sfx_play(SFX_TOUCH);
        }
    }
}

static void cpu_step(void) {
    if (!mg_vs) return;
    if (mg_rally_ckick) {
        mg_rally_ckick--;
        return;
    }
    int16_t tx_px, td_px;
    int8_t cspd = FC_SPEED_CPU + (mg_rally_streak >> 3);
    if (cspd > 23) cspd = 23;
    pl_top[SIDE_CPU] = cspd;

    if (mg_rally_ball == BALL_FROM_WALL && mg_rally_turn == 1) {
        tx_px = mg_rally_land_x;
        td_px = clamp16(mg_rally_land_d + 4, PL_D_MIN + 4, PL_D_MAX - 6);
        int16_t dx = (mg_rally_bx - mg_rally_cx) >> 4;
        int16_t adx = dx < 0 ? -dx : dx;
        int16_t dd = (mg_rally_bd - mg_rally_cd) >> 4;
        // Plant & ready like Futnet CPU once near the bounce spot!
        if (!pl_shot[SIDE_CPU] && adx <= 12 && dd >= -28) {
            fc_ready(SIDE_CPU, (rand() & 3) == 0 ? 1 : 0, 0);
            pl_charge[SIDE_CPU] = 8;
        }
        if (adx <= 16 && dd >= -REACH_FWD && dd <= REACH_BACK && (mg_rally_bz >> 4) <= REACH_Z) {
            cpu_strike(adx >= 13);
            return;
        }
    } else if (mg_rally_ball == BALL_TO_WALL && mg_rally_last == 0) {
        fc_unready(SIDE_CPU);
        tx_px = clamp16((mg_rally_bx >> 4) + (mg_rally_vx >> 1), 32, 128);
        td_px = 78;
    } else {
        fc_unready(SIDE_CPU);
        tx_px = (mg_rally_px >> 4) < 80 ? 92 : 68;
        td_px = 80;
    }

    int8_t vx_to = 0, vy_to = 0;
    if (!pl_shot[SIDE_CPU]) {
        int16_t ex = tx_px - (mg_rally_cx >> 4);
        int16_t ed = td_px - (mg_rally_cd >> 4);
        if (ex > 2) vx_to = ex > 8 ? cspd : (cspd >> 1);
        else if (ex < -2) vx_to = ex < -8 ? -cspd : -(cspd >> 1);
        if (ed > 2) vy_to = ed > 8 ? cspd : (cspd >> 1);
        else if (ed < -2) vy_to = ed < -8 ? -cspd : -(cspd >> 1);
    }
    fc_min_x = PL_X_MIN << 4;
    fc_max_x = PL_X_MAX << 4;
    fc_min_y = PL_D_MIN << 4;
    fc_max_y = PL_D_MAX << 4;
    pl_x[SIDE_CPU] = mg_rally_cx;
    pl_y[SIDE_CPU] = mg_rally_cd;
    fc_step_move(SIDE_CPU, vx_to, vy_to, 1);
    mg_rally_cx = pl_x[SIDE_CPU];
    mg_rally_cd = pl_y[SIDE_CPU];
    mg_rally_cface = pl_face[SIDE_CPU];
    if (pl_vx[SIDE_CPU] || pl_vy[SIDE_CPU]) mg_rally_cwalk++;
}

// --- HUD & drawing ---
static void hud_draw(void) {
    if (mg_vs) {
        if (mg_hud_dirty) {
            mg_hud_dirty = 0;
            fill_win_rect(0, 0, SCREEN_TW, 1, 0);
            win_text(0, 0, "YOU");
            win_text(7, 0, "RLY");
            win_text(15, 0, "CPU");
            shown_a = shown_b = shown_c = 0xFF;
        }
        if (mg_rally_you_pts != shown_a) win_number(4, 0, shown_a = mg_rally_you_pts, 1, 0);
        if (mg_rally_streak != shown_b) win_number(11, 0, shown_b = mg_rally_streak, 2, 0);
        if (mg_rally_cpu_pts != shown_c) win_number(19, 0, shown_c = mg_rally_cpu_pts, 1, 0);
    } else {
        if (mg_hud_dirty) {
            mg_hud_dirty = 0;
            fill_win_rect(0, 0, SCREEN_TW, 1, 0);
            win_text(0, 0, "PTS");
            win_text(9, 0, "RLY");
            win_text(16, 0, "L");
            shown_pts = 0xFFFF;
            shown_b = shown_c = 0xFF;
        }
        uint8_t lives = mg_rally_misses >= SOLO_LIVES ? 0 : SOLO_LIVES - mg_rally_misses;
        if (mg_rally_points != shown_pts) win_number(4, 0, shown_pts = mg_rally_points, 3, 0);
        if (mg_rally_streak != shown_b) win_number(13, 0, shown_b = mg_rally_streak, 2, 0);
        if (lives != shown_c) win_number(18, 0, shown_c = lives, 1, 0);
    }
}

static void draw_scene(void) {
    uint8_t you_slot = (mg_vs && mg_rally_cd > mg_rally_pd) ? 1 : 0;
    uint8_t cpu_slot = 1 - you_slot;

    // 1) YOU (standing/running/charging/kicking, or lying during a diving slide!)
    uint8_t pi = didx(mg_rally_pd);
    uint8_t ppal = pl_shot[SIDE_YOU] ? fc_charge_pal(SIDE_YOU, SPR_PAL_YOU) : SPR_PAL_YOU;
    if (mg_rally_charging && mg_rally_charge >= 48 && ((uint8_t)mg_timer & 2)) ppal = SPR_PAL_POWER;
    if (mg_rally_slide) {
        mg_player_lying(you_slot, proj_x(mg_rally_px, pi), gy_tab[pi] - 6, SPR_PAL_YOU, pl_face[SIDE_YOU]);
    } else {
        uint8_t pf;
        if (mg_run == MG_RUN_OVER) pf = (mg_vs && !mg_won) ? MG_F_LOSE : MG_F_WIN;
        else if (mg_rally_kick) pf = (mg_rally_bz >> 4) >= 14 ? MG_F_VOLLEY : MG_F_KICK;
        else if (pl_shot[SIDE_YOU] && !(pl_vx[SIDE_YOU] | pl_vy[SIDE_YOU])) pf = MG_F_READY;
        else if (pl_vx[SIDE_YOU] || pl_vy[SIDE_YOU]) pf = ((uint8_t)frame_count & 8) ? MG_F_RUN_A : MG_F_RUN_B;
        else pf = MG_F_STAND;
        mg_player(you_slot, MG_FRAME(MG_VIEW_BACK, pf), proj_x(mg_rally_px, pi), gy_tab[pi], ppal, pl_face[SIDE_YOU]);
    }

    // 2) CPU (VS mode)
    if (mg_vs) {
        uint8_t ci = didx(mg_rally_cd);
        uint8_t cpal = pl_shot[SIDE_CPU] ? fc_charge_pal(SIDE_CPU, SPR_PAL_CPU) : SPR_PAL_CPU;
        uint8_t cf;
        if (mg_run == MG_RUN_OVER) cf = mg_won ? MG_F_LOSE : MG_F_WIN;
        else if (mg_rally_ckick) cf = (mg_rally_bz >> 4) >= 14 ? MG_F_VOLLEY : MG_F_KICK;
        else if (pl_shot[SIDE_CPU] && !(pl_vx[SIDE_CPU] | pl_vy[SIDE_CPU])) cf = MG_F_READY;
        else if (pl_vx[SIDE_CPU] || pl_vy[SIDE_CPU]) cf = ((uint8_t)frame_count & 8) ? MG_F_RUN_B : MG_F_RUN_A;
        else cf = MG_F_STAND;
        mg_player(cpu_slot, MG_FRAME(MG_VIEW_BACK, cf), proj_x(mg_rally_cx, ci), gy_tab[ci], cpal, mg_rally_cface);
    } else {
        mg_player_hide(1);
    }

    // 3) Ball + shared Futnet shot trail
    if (mg_rally_ball == BALL_WAIT) {
        mg_ball(0, -16, 0, 0, SPR_PAL_BALL);
        mg_hide(OAM_TRAIL, OAM_TRAIL + FC_TRAIL_MAX - 1);
    } else {
        uint8_t bi = didx(mg_rally_bd);
        int16_t bsx = proj_x(mg_rally_bx, bi);
        int16_t bgy = gy_tab[bi];
        int16_t bsz = mg_rally_bz >> 4;
        uint8_t roll = (mg_rally_vx | mg_rally_vd) && ((uint8_t)mg_timer & 4);
        mg_ball(bsx, bgy, bsz, roll, ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
        if (mg_rally_ball == BALL_TO_WALL && ball_trail) {
            fc_trail_draw(OAM_TRAIL, bsx, bgy - bsz - 4, 0);
        } else {
            mg_hide(OAM_TRAIL, OAM_TRAIL + FC_TRAIL_MAX - 1);
        }
    }

    // 4) Floor bounce reticle on YOUR turn + Wall aim marker while planted!
    if (mg_run == MG_RUN_LIVE && mg_rally_ball == BALL_FROM_WALL && (!mg_vs || mg_rally_turn == 0) &&
        mg_rally_bounces == 0) {
        uint8_t li = didx(mg_rally_land_d << 4);
        uint8_t mpal = ((uint8_t)frame_count & 4) ? SPR_PAL_POWER : SPR_PAL_UI;
        mg_sprite(OAM_MARK, MG_T_AIM, mpal, proj_x(mg_rally_land_x << 4, li) - 4, gy_tab[li] - 4);
    } else {
        move_sprite(OAM_MARK, 0, 0);
    }
    if (mg_run == MG_RUN_LIVE && pl_shot[SIDE_YOU]) {
        int16_t ax = player_aim_tx();
        uint8_t dep = (joy & J_UP) ? 2 : (joy & J_DOWN) ? 0 : pl_depth[SIDE_YOU];
        int16_t az = (pl_shot[SIDE_YOU] == SHOT_DROP) ? 13 : (dep == 2 ? 32 : 22);
        mg_sprite(OAM_AIM, MG_T_AIM, SPR_PAL_UI, proj_x(ax << 4, 0) - 4, WALL_FOOT - az - 4);
    } else {
        move_sprite(OAM_AIM, 0, 0);
    }

    // 5) Wall impact starburst
    if (mg_rally_star) {
        mg_rally_star--;
        mg_sprite(OAM_STAR, MG_SPR_TILES + RT_STAR, SPR_PAL_UI, mg_rally_star_x, mg_rally_star_y);
    } else {
        move_sprite(OAM_STAR, 0, 0);
    }

    // 6) Point / Tin / Out callout popup
    if (mg_rally_pop) {
        mg_rally_pop--;
        int16_t py = 44 - ((40 - mg_rally_pop) >> 2);
        uint8_t t0, t1, t2, pal;
        if (mg_rally_pop_kind == POP_TIN) {
            t0 = RT_T; t1 = RT_I; t2 = RT_N; pal = SPR_PAL_CPU;
        } else if (mg_rally_pop_kind == POP_OUT) {
            t0 = RT_O; t1 = RT_U; t2 = RT_T; pal = SPR_PAL_CPU;
        } else {
            t0 = RT_P; t1 = RT_T; t2 = RT_BANG; pal = SPR_PAL_UI;
        }
        mg_sprite(OAM_POP, MG_SPR_TILES + t0, pal, 68, py);
        mg_sprite(OAM_POP + 1, MG_SPR_TILES + t1, pal, 76, py);
        mg_sprite(OAM_POP + 2, MG_SPR_TILES + t2, pal, 84, py);
    } else {
        mg_hide(OAM_POP, OAM_POP + 2);
    }
}

void mg_rally_enter(void) BANKED {
    mg_load_bg(mg_rally_bg_tiles, MG_RALLY_BG_TILE_COUNT, mg_rally_bg_map, mg_rally_bg_attr,
               MG_RALLY_BG_MAP_W, MG_RALLY_BG_MAP_H, BANK(mg_rally_bg));
    memcpy(pal_base_bg, bg_pals, sizeof(bg_pals));
    mg_load_spr(mg_rally_spr_tiles, MG_RALLY_SPR_TILE_COUNT, BANK(mg_rally_spr));
    memcpy(&pal_base_obj[SPR_PAL_UI << 2], ui_pal, sizeof(ui_pal));

    uint8_t lv_r = stat_lv[STAT_REFLEX];
    uint8_t lv_p = stat_lv[STAT_POWER];
    mg_rally_speed = FC_SPEED_YOU + (lv_r >> 2);
    mg_rally_reach = 16 + (lv_r >> 2);
    mg_rally_charge_rate = 1 + (lv_p >= 7) + (lv_p >= 14);

    for (uint8_t s = 0; s < 2; s++) {
        fc_unready(s);
        pl_vx[s] = pl_vy[s] = 0;
        pl_accel[s] = FC_ACCEL;
        pl_grip[s] = FC_GRIP;
        pl_skid[s] = pl_side[s] = pl_flash[s] = 0;
    }
    fc_ball_plain();
    pl_x[SIDE_YOU] = mg_rally_px = (mg_vs ? 54 : 80) << 4;
    pl_y[SIDE_YOU] = mg_rally_pd = 84 << 4;
    pl_face[SIDE_YOU] = 0;
    pl_top[SIDE_YOU] = (int8_t)mg_rally_speed;

    pl_x[SIDE_CPU] = mg_rally_cx = 106 << 4;
    pl_y[SIDE_CPU] = mg_rally_cd = 80 << 4;
    pl_face[SIDE_CPU] = mg_rally_cface = 1;
    pl_top[SIDE_CPU] = FC_SPEED_CPU;

    mg_rally_ball = BALL_WAIT;
    mg_rally_fall = 0;
    mg_rally_turn = mg_rally_last = mg_rally_bounces = mg_rally_drop = mg_rally_lob = 0;
    mg_rally_you_pts = mg_rally_cpu_pts = 0;
    mg_rally_points = 0;
    mg_rally_streak = mg_rally_best_streak = mg_rally_misses = 0;
    mg_rally_server = 0;
    mg_rally_wait = 0;
    mg_rally_charge = mg_rally_charging = mg_rally_swing = 0;
    mg_rally_kick = mg_rally_ckick = mg_rally_walk = mg_rally_cwalk = 0;
    mg_rally_slide = mg_rally_cslide = 0;
    mg_rally_star = mg_rally_pop = mg_rally_pop_kind = mg_rally_over = 0;
    mg_rally_land_x = 80;
    mg_rally_land_d = 80;
    mg_player_hide(2);
    mg_hide(OAM_MG_FREE, OAM_MG_COUNT - 1);
    draw_scene();
}

void mg_rally_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw_scene();
        return;
    }
    if (mg_run == MG_RUN_OVER) {
        ball_step();
        draw_scene();
        return;
    }
    if (mg_rally_ball == BALL_WAIT) {
        serve_ball();
    } else if (mg_rally_ball == BALL_DEAD) {
        if (mg_rally_wait && --mg_rally_wait == 0) serve_ball();
    }
    player_step();
    cpu_step();
    ball_step();
    hud_draw();
    draw_scene();
}
