#pragma bank 255
// Color palettes for each scene (banked: set when a scene starts, before it
// fades in; gfx_set_fade puts them on screen).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"

#define C(r, g, b) RGB8(r, g, b)

static void set_bg(uint8_t p, const uint16_t *c) {
    memcpy(&pal_base_bg[p << 2], c, 8);
}

// The title: a night-time arena blue behind, the display font in gold, the
// compact font pale blue, dim blue for the hints. Palette 3 is the blinking prompt.
#define ARENA C(16, 24, 48)
static const uint16_t title_text[4] = {ARENA, C(120, 140, 192), C(72, 88, 136), C(224, 232, 255)};
static const uint16_t title_gold[4] = {ARENA, C(254, 231, 97), C(254, 174, 52), C(24, 20, 37)};
static const uint16_t title_hint[4] = {ARENA, C(72, 88, 136), C(72, 88, 136), C(112, 128, 176)};

void palettes_title(void) BANKED {
    set_bg(TITLE_PAL_TEXT, title_text);
    set_bg(TITLE_PAL_GOLD, title_gold);
    set_bg(TITLE_PAL_HINT, title_hint);
    set_bg(TITLE_PAL_PROMPT, title_text);
}

static void set_obj(uint8_t p, const uint16_t *c) {
    memcpy(&pal_base_obj[p << 2], c, 8);
}

// Play (tools/tile-art.mjs draws the pitch in these color indices).
// The field: light and dark grass stripes, white lines.
#define GRASS C(96, 176, 72)
#define GRASS_DARK C(72, 148, 56)
#define LINE C(240, 248, 232)
static const uint16_t field[4] = {GRASS, GRASS_DARK, C(40, 88, 40), LINE};
// The boards: blue face, navy top and shadow, a white stripe.
static const uint16_t boards[4] = {GRASS, C(64, 112, 200), C(24, 40, 96), LINE};
// The nets: the dark inside of the goal, the net, the white posts.
static const uint16_t nets[4] = {C(24, 32, 40), C(176, 184, 192), C(96, 104, 112), LINE};
// Sprites (color 0 is see-through): 1 skin, 2 jersey, 3 outline.
#define SKIN C(248, 200, 152)
#define INK C(24, 20, 32)
static const uint16_t you[4] = {0, SKIN, C(56, 112, 248), INK};
static const uint16_t cpu[4] = {0, SKIN, C(232, 48, 48), INK};
static const uint16_t keep_you[4] = {0, SKIN, C(48, 200, 96), INK};
static const uint16_t keep_cpu[4] = {0, SKIN, C(248, 208, 32), INK};
// The ball: white, grey patches; on fire after a power shot.
static const uint16_t ball[4] = {0, C(255, 255, 255), C(96, 96, 112), INK};
static const uint16_t power[4] = {0, C(255, 240, 120), C(255, 112, 24), C(176, 24, 16)};
// The shadow (3) and the marker over you (1, outlined 3).
static const uint16_t marks[4] = {0, C(255, 232, 64), C(40, 88, 40), C(40, 96, 40)};
// The HUD row (the GB Studio font): a navy band, light letters; the meter in gold.
static const uint16_t hud[4] = {C(24, 32, 64), C(24, 32, 64), C(254, 196, 52), C(232, 236, 255)};

void palettes_play(void) BANKED {
    set_bg(PLAY_PAL_FIELD, field);
    set_bg(PLAY_PAL_BOARD, boards);
    set_bg(PLAY_PAL_NET, nets);
    set_bg(BG_PAL_UI, hud);  // the window: the HUD
    set_obj(SPR_PAL_YOU, you);
    set_obj(SPR_PAL_CPU, cpu);
    set_obj(SPR_PAL_KEEP_YOU, keep_you);
    set_obj(SPR_PAL_KEEP_CPU, keep_cpu);
    set_obj(SPR_PAL_BALL, ball);
    set_obj(SPR_PAL_POWER, power);
    set_obj(SPR_PAL_UI, marks);
}

// Menus: the title's colors for the compact and display fonts.
void palettes_menu(void) BANKED {
    palettes_title();
}
