// Your player's progression: the RPG side of PocketFutsal. Four stats, each
// with its own level, raised by the training mini games (src/minigame.c) and
// saved to the cartridge's battery RAM (MBC5 + 8 KB SRAM, tools/build.mjs).
//
// For the match modes: read stat_lv[] (1 .. STAT_LV_MAX) to scale your player
// (e.g. POWER the strike speed, CONTROL the first touch, AIM the spread, REFLEX
// the reach or reaction). progress_load() must have run once (the title does).
#ifndef PROGRESS_H
#define PROGRESS_H

#include <gb/gb.h>
#include <stdint.h>

#define STAT_POWER 0    // how hard you strike
#define STAT_CONTROL 1  // touch: juggling, first touches
#define STAT_AIM 2      // accuracy: where the ball goes
#define STAT_REFLEX 3   // reactions: returns, saves
#define STAT_COUNT 4
#define STAT_LV_MAX 20

// The mini games, in the training menu's order.
#define MG_MACHINE 0   // the ball machine: return its shots in bounds
#define MG_WALL 1      // the target wall: hit the panel that lights up
#define MG_JUGGLE 2    // keepy-uppy: juggle as long as you can
#define MG_SHOOTOUT 3  // penalties: shoot, then keep, against the CPU
#define MG_COUNT 4

// --- watchable state (saved) ---
extern uint8_t stat_lv[STAT_COUNT];    // 1 .. STAT_LV_MAX
extern uint16_t stat_xp[STAT_COUNT];   // XP into the current level
extern uint16_t mg_best[MG_COUNT];     // best score in each mini game
extern uint16_t mg_plays[MG_COUNT];    // times played
extern uint8_t progress_loaded;        // progress_load has run
extern uint8_t progress_from_save;     // ... and found a save

// XP to go from level lv to lv + 1.
uint16_t stat_xp_next(uint8_t lv) BANKED;
// Your overall level: 1 + every stat level gained.
uint8_t player_level(void) BANKED;
// Add XP to a stat; returns the levels gained.
uint8_t progress_award(uint8_t stat, uint16_t xp) BANKED;
// Read the save (once; later calls do nothing), or start fresh.
void progress_load(void) BANKED;
void progress_save(void) BANKED;
// Back to a fresh player (and save it).
void progress_reset(void) BANKED;

#endif
