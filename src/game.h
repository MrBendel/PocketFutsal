// The match (play.c): two players, two keepers and the ball on a one-screen
// pitch. Watchable globals, for the tests and the playtest page.
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// Positions are in sixteenths of a pixel (SUB), screen pixels from the
// top-left of the pitch; speeds in sixteenths of a pixel a frame. A player's or
// keeper's position is the point under its feet; the ball's is the point under
// it on the ground, with its height z above that.
#define SUB 16
#define PX(v) ((v) >> 4)
#define TO_SUB(px) ((int16_t)(px) << 4)

// The pitch (tools/tile-art.mjs draws it to match): boards all round, the
// field inside them, a goal mouth in each end board.
#define FIELD_X0 8    // the left goal line
#define FIELD_X1 152  // the right goal line
#define FIELD_Y0 8
#define FIELD_Y1 128
#define MOUTH_Y0 48   // the goal mouths, between the posts
#define MOUTH_Y1 88
#define CENTER_X 80
#define CENTER_Y 68
#define CROSSBAR 16   // px: a ball higher than this goes over

// Sides. You (SIDE_YOU) defend the left goal and attack the right one.
#define SIDE_YOU 0
#define SIDE_CPU 1
#define NOBODY 0xFF

#define WIN_GOALS 5  // first to five wins the match

// Shots: A drives the ball along the ground, B chips it up over the keeper.
#define SHOT_NONE 0
#define SHOT_DRIVE 1
#define SHOT_CHIP 2
// Readying a shot (pressing A or B) charges it for as long as the ball takes
// to arrive, Mario Tennis style: the player slows to a shuffle while charging.
// A full meter is a power shot: the ball catches fire, and keepers can only
// beat it away.
#define CHARGE_MAX 32

// The seed for rand(): taken from frame_count when START is pressed on the title,
// or from test_seed when it's non-zero (tests and replays set it).
extern uint16_t game_seed;
extern uint16_t test_seed;

// --- the match ---
extern uint8_t score[2];      // goals, by side
extern uint8_t winner;        // the side that won (game over shows it)
extern uint8_t last_scorer;   // the side that scored last (NOBODY before the first)
extern uint8_t kickoff_timer; // the players wait for the whistle while this counts down
extern uint8_t goal_timer;    // a goal's celebration counts down, then the kickoff
extern uint8_t paused;

// --- the players, by side ---
extern int16_t pl_x[2], pl_y[2];
extern int8_t pl_vx[2], pl_vy[2];  // this frame's step
extern uint8_t pl_face[2];         // 0 right, 1 left
extern uint8_t pl_shot[2];         // SHOT_* readied, or SHOT_NONE
extern uint8_t pl_charge[2];       // 0 .. CHARGE_MAX
extern uint8_t pl_hold[2];         // frames charged at the full meter (it gives up after a while)
extern uint8_t pl_kick_timer[2];   // the kick pose shows while this counts down; no touches meanwhile
extern uint8_t pl_shots[2];        // shots struck this match
extern uint8_t cpu_aim;            // the CPU's next target: 0 top corner, 1 middle, 2 bottom
extern uint8_t cpu_idle;           // non-zero: the CPU's player stands still

// --- the keepers, by side the goal they guard (0: the left) ---
extern int16_t kp_x[2], kp_y[2];
extern uint8_t kp_hold;            // frames the keeper holding the ball keeps it
extern uint8_t kp_save_timer[2];   // the save pose shows while this counts down

// --- the ball ---
extern int16_t ball_x, ball_y, ball_z;
extern int16_t ball_vx, ball_vy, ball_vz;
extern uint8_t ball_power;  // a power shot, on fire until it slows or is saved
extern uint8_t ball_owner;  // NOBODY, or the side whose keeper is holding it
extern uint8_t last_touch;  // the side that touched it last
extern uint8_t bounces;     // board bounces (the tests watch them)

#endif
