// Burst effects (vfx.c): one at a time, on the court where it happened.
#ifndef VFX_H
#define VFX_H
#include <gb/gb.h>
#include "vfx_tiles.h"
#define OAM_VFX 20  // hardware sprites 20 .. 27
void vfx_load(void) BANKED;  // its tiles into sprite VRAM (bank 0), at a scene's start
// Start effect `kind` (VFX_*) centred on (hx, wy): hardware x, and hardware y plus the
// camera (so it stays put on the court as the camera moves).
void vfx_start(uint8_t kind, int16_t hx, int16_t wy) BANKED;
void vfx_draw(int16_t cam_y) BANKED;  // a frame of it (or nothing), after the scene's sprites
#endif
