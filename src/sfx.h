// Placeholder sound effects: short register-level sounds on the Game Boy's
// square channels (1 and 2) and its noise channel (4). Channel 3 (wave) is left
// for music. The engine is RogueGB's: each sound is a few steps of register
// values; the hardware's own envelopes and sweeps do the shaping. A sound with
// a higher priority cuts off the one playing on its channel; a lower one is
// dropped.
#ifndef SFX_H
#define SFX_H

#include <gb/gb.h>
#include <stdint.h>

#define SFX_START 0    // a game starts (title)
#define SFX_KICK 1     // a strike
#define SFX_TOUCH 2    // a soft tick: off a player, the serve's toss, a diving touch
#define SFX_POWER 3    // a full-charge power shot
#define SFX_GOAL 4     // a goal: a chime up a chord
#define SFX_WHISTLE 5  // the match starts
#define SFX_BOARD 6    // the ball off the boards
#define SFX_CATCH 7    // a dive
#define SFX_PARRY 8    // knocked down by a power shot
#define SFX_CHARGE 9   // the meter is full
#define SFX_TICK 10    // a press on the meter
#define SFX_HALF 11    // the meter's at half: a slam's ready
#define SFX_COUNT 12

// --- watchable state ---
extern uint8_t sfx_last;      // the last sound started (0xFF: none yet)
extern uint16_t sfx_played;   // sounds started

void sfx_init(void) BANKED;    // sound on (once, at boot)
void sfx_play(uint8_t id) BANKED;
void sfx_update(void) BANKED;  // per frame: advance multi-step sounds

#endif
