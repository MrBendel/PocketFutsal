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


uint8_t train_sel;        // the chosen game (MG_*)
uint8_t train_scroll;     // top visible game index in the scrollable list
uint8_t train_tile;       // the "TRAINS ..." line's tiles (redrawn as the choice moves)

static uint8_t list_map[MG_COUNT][SCREEN_TW];
static uint8_t list_attr[MG_COUNT][SCREEN_TW];

#define ROW_LEVEL 4
#define ROW_STATS 6      // four rows
#define ROW_GAMES 11     // VISIBLE_GAMES rows
#define VISIBLE_GAMES 4
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
    move_sprite(0, CURSOR_X + 8, ((ROW_GAMES + (train_sel - train_scroll)) << 3) + 16);
}

static void draw_games(void) {
    for (uint8_t r = 0; r < VISIBLE_GAMES; r++) {
        uint8_t g = train_scroll + r;
        if (g >= MG_COUNT) {
            fill_bkg_rect(0, ROW_GAMES + r, SCREEN_TW, 1, 0);
            continue;
        }
        set_bkg_tiles(0, ROW_GAMES + r, SCREEN_TW, 1, list_map[g]);
        VBK_REG = 1;
        set_bkg_tiles(0, ROW_GAMES + r, SCREEN_TW, 1, list_attr[g]);
        VBK_REG = 0;
    }
    // Scroll indicators on the right edge when more games lie above or below.
    uint8_t up = train_scroll > 0 ? ('^' - 32) : 0;
    uint8_t dn = train_scroll + VISIBLE_GAMES < MG_COUNT ? ('V' - 32) : 0;
    set_bkg_tile_xy(19, ROW_GAMES, up);
    set_bkg_tile_xy(19, ROW_GAMES + VISIBLE_GAMES - 1, dn);
    VBK_REG = 1;
    set_bkg_tile_xy(19, ROW_GAMES, TITLE_PAL_HINT);
    set_bkg_tile_xy(19, ROW_GAMES + VISIBLE_GAMES - 1, TITLE_PAL_HINT);
    VBK_REG = 0;
}

static uint8_t sync_scroll(void) {
    uint8_t prev = train_scroll;
    uint8_t max_scroll = MG_COUNT > VISIBLE_GAMES ? MG_COUNT - VISIBLE_GAMES : 0;
    if (train_scroll > max_scroll) train_scroll = max_scroll;
    if (train_sel < train_scroll) train_scroll = train_sel;
    else if (train_sel >= train_scroll + VISIBLE_GAMES) train_scroll = train_sel - VISIBLE_GAMES + 1;
    return train_scroll != prev;
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
    sync_scroll();

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
    t += draw(FONT_COMPACT, TEXT_CENTER, ROW_HINT, "A: PLAY   B: BACK", t, TITLE_PAL_HINT);
    train_tile = t;
    t += 16;  // room for the longest "TRAINS CONTROL + REFLEX" line
    // Pre-render all game rows while DISPLAY_OFF so scrolling is instant.
    for (uint8_t g = 0; g < MG_COUNT; g++) {
        fill_bkg_rect(0, ROW_GAMES, SCREEN_TW, 1, 0);
        VBK_REG = 1;
        fill_bkg_rect(0, ROW_GAMES, SCREEN_TW, 1, TITLE_PAL_TEXT);
        VBK_REG = 0;
        mg_name(g, line);
        t += text_draw(FONT_COMPACT, 24, ROW_GAMES, line, t, TITLE_PAL_TEXT);
        if (mg_best[g]) {
            cat(line, 0, "BEST ");
            num(line + 5, mg_best[g]);
            t += text_draw(FONT_COMPACT, 148 - text_width(FONT_COMPACT, line), ROW_GAMES, line, t, TITLE_PAL_HINT);
        }
        get_bkg_tiles(0, ROW_GAMES, SCREEN_TW, 1, list_map[g]);
        VBK_REG = 1;
        get_bkg_tiles(0, ROW_GAMES, SCREEN_TW, 1, list_attr[g]);
        VBK_REG = 0;
    }
    draw_trains();
    draw_games();
    draw_cursor();
}

void training_update(void) BANKED {
    if (joy_pressed & (J_UP | J_DOWN)) {
        if (joy_pressed & J_UP) train_sel = train_sel ? train_sel - 1 : MG_COUNT - 1;
        else train_sel = train_sel + 1 < MG_COUNT ? train_sel + 1 : 0;
        sfx_play(SFX_TOUCH);
        if (sync_scroll()) draw_games();
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
