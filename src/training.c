#pragma bank 255
// Training: the hub for the mini games. Your player's level and four stats (each
// with its level and a bar to the next), the mini games with your best in each,
// and what the chosen one trains. UP/DOWN chooses, A plays (its brief first),
// B goes back to the title.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "sfx.h"
#include "minigame.h"
#include "progress.h"
#include "sprites.h"
#include "hud.h"
#include "scene_text.h"


uint8_t train_sel;       // the chosen game (MG_*)
uint8_t train_tile;      // the "TRAINS ..." line's tiles (redrawn as the choice moves)

#define ROW_LEVEL 4
#define ROW_STATS 6      // four rows
#define ROW_GAMES 11     // four rows
#define ROW_TRAINS 16
#define ROW_HINT 17
#define BAR_X 13         // the XP bars: tile columns 13 .. 18
#define BAR_W 6
#define T_BAR 253        // the meter's tiles (hud asset): empty, half, full
#define PAL_BAR 4        // the bars' palette
#define CURSOR_X 10      // the cursor (a ball sprite), screen px

#define C(r, g, b) RGB8(r, g, b)
// The title's arena blue behind, a gold bar in a pale rim (colors 2 and 3).
static const uint16_t bar_pal[4] = {C(16, 24, 48), C(16, 24, 48), C(254, 196, 52), C(120, 140, 192)};
static const uint16_t ball_pal[4] = {0, C(255, 255, 255), C(96, 96, 112), C(24, 20, 32)};

static uint8_t num(char *buf, uint16_t n) {
    char t[5];
    uint8_t k = 0, len = 0;
    do t[k++] = '0' + n % 10, n /= 10; while (n);
    while (k) buf[len++] = t[--k];
    buf[len] = 0;
    return len;
}

static void draw_bar(uint8_t row, uint8_t s) {
    uint8_t cells[BAR_W], attrs[BAR_W];
    uint8_t steps = BAR_W * 2;
    uint8_t fill = stat_lv[s] >= STAT_LV_MAX ? steps : (uint8_t)(((uint16_t)stat_xp[s] * steps) / stat_xp_next(stat_lv[s]));
    for (uint8_t k = 0; k < BAR_W; k++) {
        uint8_t full = (k << 1) + 2;
        cells[k] = T_BAR + (fill >= full ? 2 : fill + 1 == full ? 1 : 0);
        attrs[k] = PAL_BAR;
    }
    set_bkg_tiles(BAR_X, row, BAR_W, 1, cells);
    VBK_REG = 1;
    set_bkg_tiles(BAR_X, row, BAR_W, 1, attrs);
    VBK_REG = 0;
}

// Append s (from this bank, or RAM) at buf + at; returns the new length.
static uint8_t cat(char *buf, uint8_t at, const char *s) {
    while (*s) buf[at++] = *s++;
    buf[at] = 0;
    return at;
}

static void draw_trains(void) {
    char line[TEXT_MAX + 1], s[12];
    uint8_t n;
    fill_bkg_rect(0, ROW_TRAINS, SCREEN_TW, 1, 0);
    n = cat(line, 0, "TRAINS ");
    stat_name(mg_trains(train_sel, 0), s);
    n = cat(line, n, s);
    n = cat(line, n, " + ");
    stat_name(mg_trains(train_sel, 1), s);
    cat(line, n, s);
    text_draw(FONT_COMPACT, TEXT_CENTER, ROW_TRAINS, line, train_tile, TITLE_PAL_HINT);
}

static void draw_cursor(void) {
    set_sprite_tile(0, 0);  // the ball
    set_sprite_prop(0, SPR_PAL_BALL);
    move_sprite(0, CURSOR_X + 8, ((ROW_GAMES + train_sel) << 3) + 16);
}

void training_enter(void) BANKED {
    char line[TEXT_MAX + 1];
    uint8_t t = SCENE_TILES;
    progress_load();
    palettes_menu();
    memcpy(&pal_base_bg[PAL_BAR << 2], bar_pal, sizeof(bar_pal));
    memcpy(&pal_base_obj[SPR_PAL_BALL << 2], ball_pal, sizeof(ball_pal));
    banked_bkg_data(T_BAR, HUD_TILE_COUNT, hud_tiles, BANK(hud));
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    if (train_sel >= MG_COUNT) train_sel = 0;

    t += draw(FONT_DISPLAY, TEXT_CENTER, 1, "TRAINING", t, TITLE_PAL_GOLD);
    cat(line, 0, "PLAYER LV ");
    num(line + 10, player_level());
    t += text_draw(FONT_COMPACT, TEXT_CENTER, ROW_LEVEL, line, t, TITLE_PAL_TEXT);
    for (uint8_t s = 0; s < STAT_COUNT; s++) {
        stat_name(s, line);
        t += text_draw(FONT_COMPACT, 16, ROW_STATS + s, line, t, TITLE_PAL_TEXT);
        cat(line, 0, "LV ");
        num(line + 3, stat_lv[s]);
        t += text_draw(FONT_COMPACT, 68, ROW_STATS + s, line, t, TITLE_PAL_HINT);
        draw_bar(ROW_STATS + s, s);
    }
    for (uint8_t g = 0; g < MG_COUNT; g++) {
        mg_name(g, line);
        t += text_draw(FONT_COMPACT, 24, ROW_GAMES + g, line, t, TITLE_PAL_TEXT);
        if (mg_best[g]) {
            cat(line, 0, "BEST ");
            num(line + 5, mg_best[g]);
            t += text_draw(FONT_COMPACT, 148 - text_width(FONT_COMPACT, line), ROW_GAMES + g, line, t, TITLE_PAL_HINT);
        }
    }
    t += draw(FONT_COMPACT, TEXT_CENTER, ROW_HINT, "A: PLAY   B: BACK", t, TITLE_PAL_HINT);
    train_tile = t;  // last: the longest "TRAINS ..." fits from here
    draw_trains();
    draw_cursor();
}

void training_update(void) BANKED {
    if (joy_pressed & (J_UP | J_DOWN)) {
        if (joy_pressed & J_UP) train_sel = train_sel ? train_sel - 1 : MG_COUNT - 1;
        else train_sel = train_sel + 1 < MG_COUNT ? train_sel + 1 : 0;
        sfx_play(SFX_TOUCH);
        draw_trains();
        draw_cursor();
    }
    if (joy_pressed & (J_A | J_START)) {
        sfx_play(SFX_START);
        mg_id = train_sel;
        mg_phase = MG_PH_BRIEF;
        state_change(STATE_MINIGAME);
    } else if (joy_pressed & J_B) {
        sfx_play(SFX_TOUCH);
        state_change(STATE_TITLE);
    }
}
