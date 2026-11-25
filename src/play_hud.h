// The match's score and overlay (play_hud.c): no HUD row in a rally. The score's on
// a board in the court art and, before each serve, in a box with each player's face;
// a point's call or PAUSED is a banner for a moment; a power gauge under each player.
#ifndef PLAY_HUD_H
#define PLAY_HUD_H
#include <gb/gb.h>
#include "gfx.h"
#include "pitch.h"
// Background tiles after the court's: the faces (4: top left, bottom left, top right,
// bottom right). In the 0x8800 block, shared with sprite tiles of the same numbers.
#define T_FACE (SCENE_TILES + PITCH_TILE_COUNT)
// Each player's shadow, with their power gauge drawn into its blank lower half: own
// copies of the shadow's 8x16 tiles (yours 140-143, the CPU's 144-147), the gauge's
// rows copied in when it changes. One pair of sprites under each player, not two:
// in 8x16 mode a 3 px bar still counts on 16 lines of the ten a line can show.
#define S_GAUGE 140
#define OAM_PLAYER_SHADOW 15  // two columns per player, below the body (play.c places them)
// "A+B" over you the first time your meter's full in a match: three 8x16 sprites (the
// letters in their top halves), tiles past the slam zone's.
#define S_HINT 154    // 6 tiles
#define OAM_HINT 35   // 3
#define HINT_FRAMES 120
// Background palettes for the faces (palettes.c).
#define BG_PAL_FACE_YOU 2
#define BG_PAL_FACE_CPU 3
// The overlay (hud_mode).
#define HUD_NONE 0
#define HUD_BANNER 1  // a row along the bottom: a point's call, PAUSED
#define HUD_BOX 2     // the score box, bottom right, before each serve
#define BOX_X 13      // its tile column on the screen (7 wide) ...
#define BOX_ROWS 4    // ... and height
// The board: the score in the court art, in the floor above the far end (map tiles).
#define BOARD_X 2
#define BOARD_Y 1
#define BOARD_W 15
extern uint8_t hud_mode;
// The power meter (hud_update): watchable.
extern uint8_t meter_seen;    // your meter as last seen (a change shows the pips)
extern uint8_t meter_show;    // frames the pips still show
extern uint8_t glow_level;    // your outline's glow as set (0: none ... 5: the slam's flash)
extern uint8_t glow_flash;    // frames of the flash still to show (the power slam struck)
extern uint8_t ready_spark;   // frames left to start the ready sparkle (waiting on a burst)
extern uint8_t hint_timer;    // frames the A+B hint still shows
extern uint8_t hint_done;     // shown this match
extern int16_t you_sx, you_sy;  // where you're drawn this frame (play.c)
void hud_init(void) BANKED;    // the board, the faces, the gauges: once, entering
void hud_draw(void) BANKED;    // the overlay, for the state of play (cold)
void hud_update(uint8_t busy) BANKED;  // each frame, after draw(); busy: a strike's frame
#endif
