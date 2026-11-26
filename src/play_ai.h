// The AI on your side: play_ai.c.
#ifndef PLAY_AI_H
#define PLAY_AI_H
#include <gb/gb.h>
// Tuning shared with play.c (the minigames have their own REACH_FWD).
#define REACH_FWD 8       // px in front a strike reaches
#define SLOW 8            // a ball slower than this (|vx| + |vy|) is loose
void ai_input(void) BANKED;  // sets joy and joy_pressed for you
extern uint8_t ai_seen, ai_wait, ai_plan, ai_presses, ai_sp_t;
extern uint8_t sp_try[2];  // play.c's: pressed since the power slam was struck at you
#endif
