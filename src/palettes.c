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

// Play (tools/tile-art.mjs draws the court in these color indices).
// The court: teal in light and dark bands (and the nets), the navy floor round
// it, white lines and posts.
#define COURT C(64, 160, 176)
#define COURT_DARK C(48, 136, 152)
#define FLOOR C(40, 56, 88)
#define LINE C(240, 248, 240)
static const uint16_t field[4] = {COURT, COURT_DARK, FLOOR, LINE};
// The boards: blue face, navy top and shadow, a white stripe.
static const uint16_t boards[4] = {FLOOR, C(64, 112, 200), C(24, 40, 96), LINE};
// Sprites (color 0 is see-through): 1 skin, 2 jersey, 3 outline.
#define SKIN C(248, 200, 136)
#define INK C(24, 32, 48)
static const uint16_t you[4] = {0, SKIN, C(40, 120, 232), INK};
static const uint16_t cpu[4] = {0, SKIN, C(232, 64, 72), INK};
// Charging: the player flashes through this (and the fire colors).
static const uint16_t flash[4] = {0, C(255, 255, 255), C(255, 255, 208), C(112, 112, 160)};
// The ball: white, grey patches; on fire after a power shot.
static const uint16_t ball[4] = {0, C(248, 248, 232), C(136, 152, 168), INK};
static const uint16_t power[4] = {0, C(255, 240, 120), C(255, 112, 24), C(176, 24, 16)};
// The trails behind a struck ball: hot to dim (1 the dot's core, 2 its middle, 3 its edge).
static const uint16_t trail_power[4] = {0, C(255, 232, 160), C(248, 72, 40), C(176, 24, 24)};
static const uint16_t trail_curve[4] = {0, C(208, 240, 255), C(72, 152, 248), C(32, 64, 184)};
// The ball's shadow (3).
static const uint16_t marks[4] = {0, C(255, 232, 64), C(32, 88, 104), C(32, 96, 112)};
// The HUD row (the GB Studio font): a navy band, light letters; the meter in gold.
static const uint16_t hud[4] = {C(24, 32, 64), C(24, 32, 64), C(254, 196, 52), C(232, 236, 255)};
// The faces in the score box (play_hud.c), on a light card: your blue, the CPU's red.
#define CARD C(120, 136, 184)
static const uint16_t face_you[4] = {CARD, SKIN, C(40, 120, 232), INK};
static const uint16_t face_cpu[4] = {CARD, SKIN, C(232, 64, 72), INK};

void palettes_play(void) BANKED {
    set_bg(PLAY_PAL_FIELD, field);
    set_bg(PLAY_PAL_BOARD, boards);
    set_bg(BG_PAL_UI, hud);  // the window: the score box, a banner; the board
    set_bg(2, face_you);     // play_hud.h BG_PAL_FACE_YOU
    set_bg(3, face_cpu);     // BG_PAL_FACE_CPU
    set_obj(SPR_PAL_YOU, you);
    set_obj(SPR_PAL_CPU, cpu);
    set_obj(SPR_PAL_FLASH, flash);
    set_obj(SPR_PAL_BALL, ball);
    set_obj(SPR_PAL_POWER, power);
    set_obj(SPR_PAL_UI, marks);
    set_obj(SPR_PAL_TRAIL_POWER, trail_power);
    set_obj(SPR_PAL_TRAIL_CURVE, trail_curve);
}

// Four characters share palette indices with their fixed player slots in futsal.
// Ball and UI keep palettes 5 and 7; futnet uses its original setup.
void palettes_futsal(void) BANKED {
    palettes_play();
    static const uint16_t club_field[4] = {C(48, 144, 184), C(40, 132, 172), C(88, 160, 64), C(248, 240, 208)};
    static const uint16_t club_fence[4] = {C(88, 160, 64), C(40, 88, 56), C(24, 48, 56), C(208, 224, 184)};
    static const uint16_t club_bench[4] = {C(88, 160, 64), C(208, 152, 72), C(96, 64, 40), C(248, 240, 208)};
    static const uint16_t club_grass[4] = {C(136, 192, 72), C(40, 104, 56), C(88, 160, 64), C(248, 240, 208)};
    set_bg(0, club_field); set_bg(1, club_fence);
    set_bg(2, club_bench); set_bg(3, club_grass);
    static const uint16_t slam_trail[4] = {0, C(255, 248, 184), C(255, 160, 32), C(224, 64, 24)};
    set_obj(4, slam_trail);
    for (uint8_t i = 0; i < 4; i++) {
        pal_base_obj[(i << 2)] = 0;
        pal_base_obj[(i << 2) + 1] = i == 2 ? C(184, 120, 72) : C(248, 200, 136);
        pal_base_obj[(i << 2) + 2] = i < 2 ? C(40, 120, 232) : C(232, 64, 72);
        pal_base_obj[(i << 2) + 3] = C(24, 32, 48);
    }
}

// Menus: the title's colors for the compact and display fonts.
void palettes_menu(void) BANKED {
    palettes_title();
}
