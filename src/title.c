#pragma bank 255
// Main menu: UP/DOWN chooses futnet, futsal or training; LEFT/RIGHT or SELECT
// changes that mode's options. START/A launches the selected mode.
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "futsal.h"
#include "progress.h"

#include "scene_text.h"

uint8_t game_mode;  // MODE_*

#define MODE_FUTNET 0
#define MODE_FUTSAL 1
#define MODE_TRAINING 2
#define MODE_COUNT 3

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)

uint8_t title_timer;
uint8_t title_opt_tile;  // the match length line's tiles (drawn last, redrawn on SELECT)

static void draw_length(void) {
    fill_bkg_rect(0, 8, SCREEN_TW, 1, 0);
    if (game_mode == MODE_TRAINING) {
        char line[] = "PLAYER LV   ";
        uint8_t lv = player_level();
        if (lv >= 10) line[10] = '0' + lv / 10, line[11] = '0' + lv % 10;
        else line[10] = '0' + lv, line[11] = 0;
        text_draw(FONT_COMPACT, TEXT_CENTER, 8, line, title_opt_tile, TITLE_PAL_TEXT);
        return;
    }
    draw(FONT_COMPACT, TEXT_CENTER, 8, game_mode ? (fs_spectator ? "< WATCH FOUR AI >" : "< PLAY AS BLUE #1 >") : (match_sets == 1 ? "< ONE SET TO 11 >" : "< BEST OF 3 SETS >"), title_opt_tile,
         TITLE_PAL_TEXT);
}

// The prompt blinks by its palette: ink, or the background's color.
static void prompt_ink(uint8_t on) {
    pal_base_bg[(TITLE_PAL_PROMPT << 2) + 3] = pal_base_bg[(TITLE_PAL_TEXT << 2) + (on ? 3 : 0)];
    gfx_set_fade(0);
}

void title_enter(void) BANKED {
    title_timer = 0;
    progress_load();
    palettes_title();
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    uint8_t train = game_mode == MODE_TRAINING;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 3, "POCKET FUTSAL", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, TEXT_CENTER, 6, train ? "< TRAINING >" : game_mode ? "< 2V2 FUTSAL >" : "< FUTNET SINGLES >", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 10, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 13, train ? "MINI GAMES TO LEVEL UP" : game_mode ? "A: SHOOT / SLIDE" : "A: POWER    B: CURVE", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 14, train ? "POWER CONTROL AIM REFLEX" : game_mode ? "B: PASS / BURST" : "B, A: LOB    A, B: DROP", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 15, "UP/DN MODE  L/R OPTION", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, 4, 17, "v0.3", t, TITLE_PAL_HINT);
    title_opt_tile = t;  // last: the longest option fits from here on
    draw_length();
}

void title_update(void) BANKED {
    title_timer++;
    if (!(title_timer & (BLINK_MASK - 1))) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & (J_UP | J_DOWN)) {
        if (joy_pressed & J_DOWN) game_mode = game_mode + 1 < MODE_COUNT ? game_mode + 1 : 0;
        else game_mode = game_mode ? game_mode - 1 : MODE_COUNT - 1;
        sfx_play(SFX_TOUCH);
        state_restart();
        return;
    }
    if ((joy_pressed & (J_SELECT | J_LEFT | J_RIGHT)) && game_mode != MODE_TRAINING) {
        if (game_mode) fs_spectator = !fs_spectator;
        else match_sets = match_sets == 1 ? 2 : 1;
        sfx_play(SFX_TOUCH);
        draw_length();
    }
    if (joy_pressed & (J_START | J_A)) {
        // The frame START lands on seeds the match: different every time for a
        // player, reproducible for a replay. Tests can set test_seed instead.
        game_seed = test_seed ? test_seed : frame_count;
        sfx_play(SFX_START);
        state_change(game_mode == MODE_TRAINING ? STATE_TRAINING : game_mode ? STATE_FUTSAL : STATE_PLAY);
    }
}
