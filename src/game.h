// The match (play.c): you and the CPU, each in your own half of a futsal court,
// Mario Tennis style. Watchable globals, for the tests and the playtest page.
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// Positions are in sixteenths of a unit (SUB) on the flat court: x across, y in
// depth from the far goal line (the CPU's) to the near one (yours). One unit is
// a pixel at the near end; the court is drawn in perspective (proj_y, proj_s in
// play.c), squashed toward the far end. Speeds are in sixteenths a frame. A
// player's position is the point under its feet; the ball's is the point under it
// on the court, with its height z (in screen pixels) above that.
#define SUB 16
#define PX(v) ((v) >> 4)
#define TO_SUB(px) ((int16_t)(px) << 4)

// The court (tools/tile-art.mjs draws it to match).
#define COURT_X0 8     // the touchlines (boards beyond)
#define COURT_X1 152
#define CENTER_X 80
#define COURT_FAR 0    // the CPU's goal line
#define COURT_NEAR 224 // yours
#define HALFWAY 112
#define GOAL_X0 32     // the goal mouths, between the posts: two thirds of the line
#define GOAL_X1 128
#define CROSSBAR 16    // px: a ball higher than this hits the boards above the goal

// The perspective (tools/tile-art.mjs projY, projS).
#define COURT_SY 32    // the far goal line's map row, px
#define PROJ_K0 92     // rows per unit of depth, /256, at the far and near lines
#define PROJ_K1 164
#define PROJ_S0 111    // the width scale at the far line, /128 (128 at the near)
#define MAP_H 168      // the background: 160 x 168, scrolled up and down
#define CAM_MAX 32     // its scroll: MAP_H - the 136 rows above the HUD

// Sides. You (SIDE_YOU) play the near half, toward the far goal.
#define SIDE_YOU 0
#define SIDE_CPU 1
#define NOBODY 0xFF

#define WIN_POINTS 7  // first to seven, by two

// Strikes: A drives the ball, B chips it (a lob).
#define SHOT_NONE 0
#define SHOT_DRIVE 1
#define SHOT_CHIP 2
// Readying a strike (pressing A or B) charges it for as long as the ball takes to
// arrive, Mario Tennis style: the player slows to a shuffle while charging. A full
// meter on a drive is a power shot: on fire, and it knocks down a player who meets
// it with less than half a meter.
#define CHARGE_MAX 32

// What a strike turned out to be, by the ball's height (pl_move, the pose).
#define MOVE_NONE 0
#define MOVE_KICK 1     // on the ground (and the serve)
#define MOVE_VOLLEY 2   // 5-15 px up
#define MOVE_HEADER 3   // head high, in front
#define MOVE_BICYCLE 4  // overhead and just behind
#define MOVE_DIVE 5     // stretching for one out of reach, then on the floor
#define MOVE_DOWN 6     // knocked down by a power shot

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
extern uint8_t goal_timer;    // a goal's celebration counts down, then the serve
extern uint8_t paused;
extern uint8_t cam_y;         // the background's scroll, 0 .. CAM_MAX

// --- the players, by side ---
extern int16_t pl_x[2], pl_y[2];
extern int8_t pl_vx[2], pl_vy[2];  // this frame's step
extern uint8_t pl_face[2];         // 0 right, 1 left (which way strikes and dives go)
extern uint8_t pl_shot[2];         // SHOT_* readied, or SHOT_NONE
extern uint8_t pl_charge[2];       // 0 .. CHARGE_MAX
extern uint8_t pl_hold[2];         // frames charged at the full meter (it gives up after a while)
extern uint8_t pl_move[2];         // MOVE_*: the strike or dive in progress
extern uint8_t pl_move_timer[2];   // its pose shows while this counts down; no touches meanwhile
extern uint8_t pl_shots[2];        // strikes this match
extern uint8_t pl_last_move[2];    // the last strike's MOVE_*, for the tests
extern int16_t cpu_tx, cpu_ty;     // where the CPU is heading (court px)
extern uint8_t cpu_aim;            // its next aim: 0 left corner, 1 middle, 2 right
extern uint8_t cpu_idle;           // non-zero: the CPU's player stands still

// --- the ball ---
extern int16_t ball_x, ball_y, ball_z;
extern int16_t ball_vx, ball_vy, ball_vz;
extern uint8_t ball_power;  // a power shot, on fire until it slows or is met
extern uint8_t last_touch;  // the side that touched it last
extern uint8_t bounces;     // board bounces (the tests watch them)

#endif
