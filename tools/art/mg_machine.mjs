// BALL MACHINE (src/mg_machine.c): its background and its own sprite tiles.
// Placeholder until the game's art is drawn: a plain screen, and no sprites.
//
// BG:  20 x 18 tiles (the window's HUD covers the bottom row), drawn on a canvas
//      in color indices 0-3; palOf picks each tile's palette (0-5, the scene's,
//      set in src/mg_machine.c). Loaded from background tile SCENE_TILES (80) on.
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode: top, then bottom), loaded into
//      sprite VRAM bank 0 after the shared ball/shadow tiles (MG_SPR_TILES in
//      src/minigame.h). Colors 1-3 (0 is see-through).
import { BLANK, canvas, toBackground } from './common.mjs';

export const BG = toBackground(canvas(20, 18, 0));
export const SPR = [BLANK, BLANK];
