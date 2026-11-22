// The match (play.c): futnet singles (football-tennis) against the CPU, Mario
// Tennis style: you and the CPU each in your own half, a net between. Watchable globals, for the tests and the playtest page.
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// Positions are in sixteenths of a unit (SUB) on the flat court: x across, y in
// depth from the far baseline (the CPU's) to the near one (yours). One unit is
// a pixel at the near end; the court is drawn in perspective (proj_y, proj_s in
// play.c), squashed toward the far end. Speeds are in sixteenths a frame. A
// player's position is the point under its feet; the ball's is the point under it
// on the court, with its height z (in screen pixels) above that.
#define SUB 16
#define PX(v) ((v) >> 4)
#define TO_SUB(px) ((int16_t)(px) << 4)

// The court (tools/tile-art.mjs draws it to match): futnet singles, 9 m x 12.8 m,
// so each half is a little wider than it's deep. A ball on a line is in.
#define COURT_X0 8     // the sidelines
#define COURT_X1 152
#define CENTER_X 80
#define COURT_FAR 0    // the CPU's baseline
#define COURT_NEAR 224 // yours
#define HALFWAY 112    // the net
#define NET_H 10       // px: the net's height (1.10 m is about 14 at this scale; lower
                       // to play well on a small screen). Lower than this, it's in the net.
#define OUT_X0 -24     // the floor round the court: a ball stops at these edges
#define OUT_X1 184
#define OUT_Y0 -16
#define OUT_Y1 236

// The perspective (tools/tile-art.mjs projY, projS).
#define COURT_SY 32    // the far baseline's map row, px
#define PROJ_K0 92     // rows per unit of depth, /256, at the far and near lines
#define PROJ_K1 164
#define PROJ_S0 111    // the width scale at the far line, /128 (128 at the near)
#define MAP_H 168      // the background: 160 x 168, scrolled up and down
#define CAM_MAX 32     // its scroll: MAP_H - the 136 rows above the HUD

// Sides. You (SIDE_YOU) play the near half.
#define SIDE_YOU 0
#define SIDE_CPU 1
#define NOBODY 0xFF

// Scoring (futnet): a set is first to 11 by two, or to 15 (so 15-14 at most); the
// match is one set, or the best of three (match_sets, chosen on the title).
#define SET_POINTS 11
#define SET_CAP 15

// The rules of the rally (futnet singles), and how a point was won (point_reason):
// - two touches a side (both yours, one after the other if you like), one bounce;
// - the ball must go over the net and land in, on a line or inside it;
// - a ball off the net is dead: it can't be played again.
#define POINT_NET 1      // in the net
#define POINT_OUT 2      // struck out of the court
#define POINT_BOUNCE 3   // bounced twice (or died) in a half: the other side's point
#define POINT_TOUCHES 4  // a third touch
#define POINT_FAULT 5    // a serve that didn't go over

// Strikes, readied before the ball arrives: A a power shot, straight where the
// d-pad points; B a curve, bending in to the side the d-pad holds. B then A is a
// lob over the other player; A then B a drop shot, dying just past halfway.
#define SHOT_NONE 0
#define SHOT_POWER 1
#define SHOT_CURVE 2
#define SHOT_LOB 3
#define SHOT_DROP 4
// Readying a strike plants you until it goes, and you can only ready one with the
// ball coming your way (or loose in your half). The meter fills as you wait, and
// each tap of the button again adds more (up to TAP_MAX taps). A full meter on A or
// B sets the ball on fire. Meeting a ball on fire needs half a meter.
#define CHARGE_MAX 32
#define TAP_CHARGE 3
#define TAP_MAX 5

// What a strike turned out to be, by the ball's height (pl_move, the pose).
#define MOVE_NONE 0
#define MOVE_KICK 1     // on the ground (and the serve)
#define MOVE_VOLLEY 2   // 5-15 px up
#define MOVE_HEADER 3   // head high, in front
#define MOVE_BICYCLE 4  // overhead and just behind
#define MOVE_SLIDE 5    // planted out of reach: a slide for it, then on the floor
#define MOVE_DOWN 6     // knocked down by a power shot
#define MOVE_CONTROL 7  // reached unready: a first touch, up in front of you, to play again
// A ball only just reached (a slide, or a ball on fire met with less than three
// quarters of a meter) pops up over the net. A pop-up (or your own control) met in
// the air with half a meter is a slam, on fire.

// The rally's phase.
#define PHASE_SERVE 0  // the server has the ball at their feet
#define PHASE_TOSS 1   // it's tossed up; struck on the way down
#define PHASE_RALLY 2

// The seed for rand(): taken from frame_count when START is pressed on the title,
// or from test_seed when it's non-zero (tests and replays set it).
extern uint16_t game_seed;
extern uint16_t test_seed;

// --- the match ---
extern uint8_t score[2];      // points, by side
extern uint8_t winner;        // the side that won (game over shows it)
extern uint8_t last_scorer;   // the side that scored last (NOBODY before the first)
extern uint8_t server;        // the side serving this point
extern uint8_t phase;         // PHASE_*
extern uint8_t serve_timer;   // counts down before the server may toss (READY)
extern uint8_t goal_timer;    // a point's celebration counts down, then the serve
extern uint8_t point_reason;  // POINT_*: how the last point was won
extern uint8_t sets[2];       // sets won, by side
extern uint8_t match_sets;    // sets it takes to win the match: 1, or 2 (best of three)
extern uint8_t holder;        // the side whose half the ball is in: its touches and bounces
extern uint8_t touches;       // the holder's touches so far
extern uint8_t ball_landings; // bounces in the holder's half
extern uint8_t ball_netted;   // it hit the net: dead
extern uint8_t ball_serve;    // a serve, not yet over the net
extern uint8_t paused;
extern uint8_t cam_y;         // the background's scroll, 0 .. CAM_MAX

// --- the players, by side ---
extern int16_t pl_x[2], pl_y[2];
extern int8_t pl_vx[2], pl_vy[2];  // this frame's step
extern uint8_t pl_face[2];         // 0 right, 1 left (which way strikes and slides go)
extern uint8_t pl_shot[2];         // SHOT_* readied (planted), or SHOT_NONE
extern uint8_t pl_charge[2];       // 0 .. CHARGE_MAX
extern uint8_t pl_hold[2];         // frames charged at the full meter (it gives up after a while)
extern uint8_t pl_move[2];         // MOVE_*: the strike or dive in progress
extern uint8_t pl_move_timer[2];   // its pose shows while this counts down; no touches meanwhile
extern uint8_t pl_shots[2];        // strikes this match
extern uint8_t pl_last_move[2];    // the last strike's MOVE_*, for the tests
extern uint8_t pl_taps[2];         // taps added to this charge
extern uint8_t pl_aim[2];          // 0 left, 1 middle, 2 right: the last way held while queued
extern uint8_t pl_depth[2];        // 0 short, 1 as charged, 2 deep: UP / DOWN while queued
extern int16_t aim_tx, aim_land;   // where a strike's aimed to land (court px)
extern int8_t pl_slide_vx[2], pl_slide_vy[2];  // a slide's step a frame
extern int16_t cpu_tx, cpu_ty;     // where the CPU is heading (court px)
extern int16_t meet_x[2], meet_y[2];  // where each side means to meet the ball (court px)
extern uint8_t meet_out[2];        // the ball's landing out: that side lets it go
extern uint8_t cpu_aim;            // its next aim: 0 left corner, 1 middle, 2 right
extern uint8_t cpu_idle;           // non-zero: the CPU's player stands still

// --- the ball ---
extern int16_t ball_x, ball_y, ball_z;
extern int16_t ball_vx, ball_vy, ball_vz;
extern uint8_t ball_power;  // a power shot, on fire until it slows or is met
extern int8_t ball_curve;   // a curve's sideways push a frame, in 1/16 SUB
extern uint8_t ball_curve_t; // frames of it left
extern uint8_t ball_pop;    // a pop-up: met in the air, a slam
extern uint8_t ball_drop;   // a drop shot: it barely bounces
extern uint8_t ball_trail;  // the trail behind it: 0 none, TRAIL_POWER (red), TRAIL_CURVE (blue)
#define TRAIL_POWER 1
#define TRAIL_CURVE 2
extern uint8_t trail_len;   // its dots
extern uint8_t ball_lob;    // a lob: it sits up where it lands (slowed, a high bounce)
extern uint8_t last_touch;  // the side that touched it last

// Menu mode: 0 futnet, 1 futsal.
extern uint8_t game_mode;
#endif
