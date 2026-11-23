// Shared Futnet movement, charge-up, power/curve/lob/drop strikes, and shot
// trail engine (src/futnet_core.c). Used by Futnet Singles (src/play.c) and
// the court-based training mini games (src/mg_machine.c, src/mg_wall.c) so
// movement momentum, charging animations, and shots feel identical everywhere.
#ifndef FUTNET_CORE_H
#define FUTNET_CORE_H

#include <gb/gb.h>
#include <stdint.h>
#include "game.h"

// Shared tuning constants (exact Futnet values).
#define FC_SPEED_YOU 22
#define FC_SPEED_CPU 20
#define FC_ACCEL 3
#define FC_GRIP 1
#define FC_BRAKE 4
#define FC_SKID_FRAMES 9
#define FC_YOU_TAP 4
#define FC_SLAM_CHARGE 8
#define FC_COMBO_FRAMES 24
#define FC_POWER_SPEED 48
#define FC_GOLD_SPEED 56
#define FC_DRIVE_SPEED 28
#define FC_CURVE_SPEED 25
#define FC_CURVE_OFS 44
#define FC_HEADER_SPEED 24
#define FC_CHIP_LIFT 28
#define FC_ARC_FLAT 60
#define FC_ARC_MAX 100
#define FC_TRAIL_MAX 4
#define FC_S_TRAIL 8

// Flags for fc_strike_ball (packed into one byte to keep strike frames fast).
#define FC_F_SCUFF 0x04
#define FC_F_SLAM  0x08
#define FC_F_SERVE 0x10
#define FC_FLAGS(aim, scuff, slam, serve) \
    (((aim) & 3) | ((scuff) ? FC_F_SCUFF : 0) | ((slam) ? FC_F_SLAM : 0) | ((serve) ? FC_F_SERVE : 0))

// Shared state (defined in play.c, read/written by futnet_core.c & callers).
extern int8_t pl_top[2];
extern uint8_t pl_accel[2], pl_grip[2];
extern uint8_t pl_skid[2];
extern uint8_t pl_side[2];
extern uint8_t pl_combo[2];
extern uint8_t pl_flash[2];
extern uint8_t meet_slam[2];
extern uint8_t ball_gold;
extern uint8_t ball_dropped;
extern int16_t ball_cacc;
extern int16_t arc_t;
extern uint8_t arc_clear;
extern uint8_t trail_n, trail_i, trail_pal;
extern int16_t trail_x[16], trail_y[16];
// Movement bounds (SUB) for fc_step_move, set once per scene/side.
extern int16_t fc_min_x, fc_max_x, fc_min_y, fc_max_y;

// Clear queued shot / charge / combo for player i.
void fc_unready(uint8_t i);
// Clear ball flight flags (power, gold, curve, pop, drop, lob, trail).
void fc_ball_plain(void);
// Queue or tap-charge a strike: b=0 (A: power), b=1 (B: curve).
// Handles B->A lob, A->B drop, gold slam (when allow_gold), tap charge & SFX.
void fc_ready(uint8_t i, uint8_t b, uint8_t allow_gold);
// Step combo-arm countdown for player i (reverts expired lob->curve, drop->power).
#define fc_combo_tick(i) do { \
    if (pl_combo[(i)] && (i) == SIDE_YOU && !--pl_combo[(i)]) { \
        if (pl_shot[(i)] == SHOT_LOB) pl_shot[(i)] = SHOT_CURVE; \
        else if (pl_shot[(i)] == SHOT_DROP) pl_shot[(i)] = SHOT_POWER; \
    } \
} while (0)
// Step player i's velocity (approach with accel/grip/skid/brake) toward (tx, ty),
// integrate position, clamp to [fc_min_x..fc_max_x] x [fc_min_y..fc_max_y], and update facing.
void fc_step_move(uint8_t i, int8_t tx, int8_t ty, uint8_t firm);
// Compute player i's palette with the 3-stage charge-up flash (own -> white -> fire).
uint8_t fc_charge_pal(uint8_t i, uint8_t base_pal);
// Set ball_vx/ball_vy toward (dx, dy) px away at speed (|vx|+|vy|).
void fc_launch_vel(int16_t dx, int16_t dy, uint8_t speed);
// Compute ball_vz to land at court depth `land` (px); sets arc_clear = clear.
void fc_arc(int16_t land, uint8_t clear);
// Lift ball_vz if needed to clear the net at HALFWAY (call frame after strike).
void fc_net_clear(void);
// Execute a Futnet strike (power, curve, lob, drop, header, bicycle, or slam):
// sets ball_vx/vy/vz, curve, power/gold flags, trail, and plays SFX.
// Returns the chosen move (MOVE_BICYCLE if slam, else `move`).
uint8_t fc_strike_ball(uint8_t i, uint8_t move, int16_t tx, int16_t land, uint8_t flags);
// Apply one frame of lateral curve bending (ball_curve / ball_curve_t / ball_cacc).
#define fc_curve_step() do { \
    if (ball_curve_t) { \
        ball_curve_t--; \
        ball_cacc += ball_curve; \
        ball_vx += ball_cacc >> 4; \
        ball_cacc &= 15; \
    } \
} while (0)
// Record (sx, wy) and draw the 4-dot shrinking red/blue/gold shot trail on
// OAM slots oam_first .. oam_first + 3.
void fc_trail_draw(uint8_t oam_first, int16_t sx, int16_t wy, int16_t cam_y);

#endif
