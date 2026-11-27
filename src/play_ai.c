#pragma bank 255
// The AI on your side (from play.c, 2026-11-26, to make room in its bank): your
// buttons pressed for you, read by play_update when you_ai is set.
#include <gb/gb.h>
#include <rand.h>
#include "input.h"
#include "state.h"
#include "game.h"
#include "futnet_core.h"
#include "play_ai.h"

static int16_t abs16(int16_t v) { return v < 0 ? -v : v; }
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint8_t coming(uint8_t i) { return i == SIDE_YOU ? ball_vy > 0 : ball_vy < 0; }
// Loose in player i's half (play.c's loose_here).
static uint8_t loose_here(uint8_t i) {
    if (abs16(ball_vx) + abs16(ball_vy) >= SLOW) return 0;
    return i == SIDE_YOU ? PX(ball_y) >= HALFWAY : PX(ball_y) < HALFWAY;
}

// It plays with your buttons and nothing else, as a fair human might: 12-27 frames
// after the CPU strikes it picks where it'll meet the ball (the game's trace, from
// where it stands; a ball going out it lets go), walks there on the d-pad, and once
// there (or with the ball 40 px off) presses A, or B one time in four, then once or
// twice more, holding the d-pad away from the CPU (its aim) three times in four.
uint8_t you_ai;      // non-zero: the AI plays your side
uint8_t ai_seen;     // the CPU's strikes when it last looked
uint8_t ai_wait;     // frames until it reacts (0: not waiting)
uint8_t ai_plan;     // non-zero: it's walking to ai_tx, ai_ty
int16_t ai_tx, ai_ty;
uint8_t ai_btn;      // J_A or J_B
uint8_t ai_aim;      // J_LEFT, J_RIGHT or 0, held once queued
uint8_t ai_presses;  // presses still to make
uint8_t ai_next;     // frames to the next
uint8_t ai_sp_t;     // the power slam: it presses this many frames out (0-7; within 5 is timed)

void ai_input(void) BANKED {
    uint8_t pad = 0, press = 0;
    if (pl_shots[SIDE_CPU] != ai_seen) {
        ai_seen = pl_shots[SIDE_CPU];
        ai_wait = 12 + (rand() & 15);
        ai_plan = ai_presses = 0;
        ai_btn = (rand() & 3) ? J_A : J_B;
        ai_aim = !(rand() & 3) ? 0 : PX(pl_x[SIDE_CPU]) < CENTER_X ? J_RIGHT : J_LEFT;
        ai_sp_t = rand() & 7;
    }
    if (phase == PHASE_SERVE) {
        if (server == SIDE_YOU && !serve_timer && !((uint8_t)frame_count & 15)) press = J_A;
    } else if (phase == PHASE_RALLY) {
        // The power slam coming: one press, timed by its distance over its speed.
        if (ball_special && last_touch == SIDE_CPU && !sp_try[SIDE_YOU]) {
            int16_t d = PX(pl_y[SIDE_YOU]) - PX(ball_y) - REACH_FWD;
            if ((d << 4) <= (int16_t)ai_sp_t * abs16(ball_vy)) press = ai_btn;
        }
        if (ai_wait && !--ai_wait && coming(SIDE_YOU) && !meet_out[SIDE_YOU]) {
            ai_plan = 1;
            ai_tx = meet_x[SIDE_YOU];
            ai_ty = clamp16(meet_y[SIDE_YOU], 140, 205);
        }
        if (pl_shot[SIDE_YOU]) {
            pad = ai_aim;
            if (ai_presses && !--ai_next && !ball_special) press = ai_btn, ai_presses--, ai_next = 8;
        } else if (loose_here(SIDE_YOU) && !pl_move_timer[SIDE_YOU] && !ball_special) {
            press = ai_btn;  // a loose ball: queue, and the game runs you to it
        } else if (ai_plan) {
            if (meet_out[SIDE_YOU] || !coming(SIDE_YOU)) {
                ai_plan = 0;
            } else {
                int16_t dx = ai_tx - PX(pl_x[SIDE_YOU]), dy = ai_ty - PX(pl_y[SIDE_YOU]);
                if (dx > 3) pad = J_RIGHT; else if (dx < -3) pad = J_LEFT;
                if (dy > 3) pad |= J_DOWN; else if (dy < -3) pad |= J_UP;
                // (The power slam coming, it saves its press for the timing, above.)
                if (!ball_special && ((abs16(dx) <= 4 && abs16(dy) <= 6) || abs16(PX(ball_y) - PX(pl_y[SIDE_YOU])) < 40)) {
                    pad = 0;
                    press = pl_power[SIDE_YOU] == POWER_MAX ? J_A | J_B : ai_btn;  // full: the power slam
                    ai_presses = 1 + (rand() & 1);
                    ai_next = 8;
                    ai_plan = 0;
                }
            }
        }
    }
    joy = pad | press;
    joy_pressed = press;
}

