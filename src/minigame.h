// The training mini games: a shell (src/minigame.c) that every game plugs into,
// and the helpers games share. One scene, STATE_MINIGAME, in three phases, each
// its own screen entered with a fade:
//
//   MG_PH_BRIEF   a card: the game's name, the stats it trains, how to play,
//                 your best. A plays, B goes back to the training menu.
//   MG_PH_PLAY    the game itself: "READY" .. "GO!" on the HUD, then the game
//                 plays until it calls mg_finish(score). START pauses (and
//                 SELECT, paused, quits without XP).
//   MG_PH_RESULT  the score, your best, the XP won and any level ups. A plays
//                 again, B goes back to the training menu.
//
// Adding a game: an MG_* id in progress.h, a src/mg_<name>.c with
//   void mg_<name>_enter(void) BANKED;   // load its art, set up (display off)
//   void mg_<name>_update(void) BANKED;  // once a frame in MG_PH_PLAY
// (both cases in minigame.c's dispatch), its row in minigame.c's tables (name,
// stats, XP, how to play), and its art in tools/art/mg_<name>.mjs.
//
// A game's update runs every frame of MG_PH_PLAY except while paused:
//   mg_run == MG_RUN_READY  the countdown: draw the opening scene, ignore input
//   mg_run == MG_RUN_LIVE   play. Redraw the HUD when mg_hud_dirty is set (then
//                           clear it): the shell drew over it.
//   mg_run == MG_RUN_OVER   after mg_finish: keep things moving if you like, no input
#ifndef MINIGAME_H
#define MINIGAME_H

#include <gb/gb.h>
#include <stdint.h>
#include "progress.h"

#define MG_PH_BRIEF 0
#define MG_PH_PLAY 1
#define MG_PH_RESULT 2

#define MG_RUN_READY 0
#define MG_RUN_LIVE 1
#define MG_RUN_OVER 2
#define MG_READY_FRAMES 90  // "READY" for 60, "GO!" for 30
#define MG_OVER_FRAMES 90   // the end's banner, then the results

// --- watchable state ---
extern uint8_t mg_id;         // the game (MG_* in progress.h)
extern uint8_t mg_vs;         // 0 solo, 1 vs CPU (for games with a mode toggle)
extern uint8_t mg_phase;      // MG_PH_*
extern uint8_t mg_run;        // MG_RUN_*, in MG_PH_PLAY
extern uint8_t mg_paused;
extern uint8_t mg_hud_dirty;  // the game should redraw its HUD
extern uint16_t mg_timer;     // frames since mg_run last changed
extern uint16_t mg_score;     // the result (mg_finish)
extern uint8_t mg_won;        // set by a game with a win (the shootout): bonus XP
extern uint8_t mg_new_best;   // results: a new best
extern uint16_t mg_xp[2];     // results: XP to the game's first and second stat
extern uint8_t mg_level_ups[2];
// A line for the results card (e.g. "YOU 4 - 3 CPU"), filled by the game before
// mg_finish; empty for none. Upper case, digits and simple punctuation.
#define MG_DETAIL_MAX 20
extern char mg_detail[MG_DETAIL_MAX + 1];

// The game ends: score it, show the end's banner, then the results.
void mg_finish(uint16_t score) BANKED;

// For menus in other banks: a game's name / a stat's name copied into buf (RAM,
// 16 bytes), and the stats a game trains (which: 0 its first, 1 its second).
void mg_name(uint8_t id, char *buf) BANKED;
void stat_name(uint8_t s, char *buf) BANKED;
uint8_t mg_trains(uint8_t id, uint8_t which) BANKED;
uint8_t mg_has_vs(uint8_t id) BANKED;

// --- sprites ---
// OAM: the ball and its shadow, three players (4 sprites each), then the game's own.
#define OAM_MG_BALL 0
#define OAM_MG_SHADOW 1
#define OAM_MG_PLAYER 2   // + 4 * slot, slots 0..2
#define OAM_MG_FREE 14    // the game's: 14..39
#define OAM_MG_COUNT 40
// Sprite tiles in VRAM bank 0, loaded by the shell (tools/tile-art.mjs SPRITE_TILES),
// then the game's own (tools/art/mg_*.mjs SPR) from MG_SPR_TILES.
#define MG_T_BALL 0       // and MG_T_BALL + 2, its other roll
#define MG_T_SHADOW 4
#define MG_T_AIM 6
#define MG_T_TRAIL 8      // four dots, largest first, 2 tiles each
#define MG_SPR_TILES 16
// Player frames in VRAM bank 1 (S_BANK), 8 tiles each: the back view (as you,
// in a match), then the front view, then lying down (a dive). tile-art.mjs.
#define MG_F_STAND 0
#define MG_F_RUN_A 1
#define MG_F_RUN_B 2
#define MG_F_READY 3
#define MG_F_KICK 4
#define MG_F_VOLLEY 5
#define MG_F_HEADER 6
#define MG_F_BICYCLE 7
#define MG_F_WIN 8
#define MG_F_LOSE 9
#define MG_VIEW_BACK 0
#define MG_VIEW_FRONT 80
#define MG_FRAME(view, f) ((view) + ((f) << 3))
// Sprite palettes: as in a match (gfx.h SPR_PAL_*), set by the shell before the
// game's enter (a game may change them).

// Screen coordinates (0,0 the top left of the screen; the playfield is 160 x 136
// above the HUD). Off the playfield, a sprite is hidden.
void mg_sprite(uint8_t n, uint8_t tile, uint8_t prop, int16_t x, int16_t y) BANKED;
void mg_hide(uint8_t first, uint8_t last) BANKED;  // OAM first .. last
// A player standing (16 x 24): centered on cx, its feet on line feet. tile:
// MG_FRAME(view, frame). flip: facing the other way.
void mg_player(uint8_t slot, uint8_t tile, int16_t cx, int16_t feet, uint8_t pal, uint8_t flip) BANKED;
// A player lying (24 x 16, a dive or a fall), centered on (cx, cy), head to the right
// (flipped: to the left).
void mg_player_lying(uint8_t slot, int16_t cx, int16_t cy, uint8_t pal, uint8_t flip) BANKED;
void mg_player_hide(uint8_t slot) BANKED;
// The ball (8 x 8), centered on x, its bottom z px above the ground at line gy,
// rolling if roll; its shadow on the ground (no shadow if gy < 0). pal:
// SPR_PAL_BALL, or SPR_PAL_POWER (on fire).
void mg_ball(int16_t x, int16_t gy, int16_t z, uint8_t roll, uint8_t pal) BANKED;

// --- loading (in a game's enter, with the display off) ---
// A background from a tools/art/mg_*.mjs asset: tiles from SCENE_TILES, its map and
// palettes at the top left. Pass BANK(asset).
void mg_load_bg(const uint8_t *tiles, uint8_t n, const uint8_t *map, const uint8_t *attr,
                uint8_t w, uint8_t h, uint8_t bank) BANKED;
// The game's own sprite tiles, from MG_SPR_TILES.
void mg_load_spr(const uint8_t *tiles, uint8_t n, uint8_t bank) BANKED;

// --- the games ---
void mg_machine_enter(void) BANKED;
void mg_machine_update(void) BANKED;
void mg_wall_enter(void) BANKED;
void mg_wall_update(void) BANKED;
void mg_juggle_enter(void) BANKED;
void mg_juggle_update(void) BANKED;
void mg_shootout_enter(void) BANKED;
void mg_shootout_update(void) BANKED;
void mg_slalom_enter(void) BANKED;
void mg_slalom_update(void) BANKED;
void mg_rally_enter(void) BANKED;
void mg_rally_update(void) BANKED;

#endif
