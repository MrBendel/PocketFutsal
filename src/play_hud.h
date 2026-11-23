// The match's HUD text (play_hud.c): YOU 0 [meter] 0 CPU, or a note for a moment.
#ifndef PLAY_HUD_H
#define PLAY_HUD_H
#include <gb/gb.h>
#include "gfx.h"
#include "pitch.h"
#define T_METER (SCENE_TILES + PITCH_TILE_COUNT)  // the meter's tiles: empty, half, full
void hud_draw(void) BANKED;
void hud_update(void) BANKED;
#endif
