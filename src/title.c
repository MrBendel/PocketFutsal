#pragma bank 255
// Main menu: UP/DOWN chooses futnet or futsal; LEFT/RIGHT or SELECT
// changes that mode's options. START/A launches the selected mode.
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "futsal.h"

#include "scene_text.h"

uint8_t game_mode;

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)

uint8_t title_timer;
uint8_t title_opt_tile;  // the match length line's tiles (drawn last, redrawn on SELECT)

static void draw_length(void) {
    fill_bkg_rect(0, 8, SCREEN_TW, 1, 0);
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
    palettes_title();
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 3, "POCKET FUTSAL", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, TEXT_CENTER, 6, game_mode ? "< 2V2 FUTSAL >" : "< FUTNET SINGLES >", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 10, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 13, game_mode ? "A: SHOOT / SLIDE" : "A: POWER    B: CURVE", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 14, game_mode ? "B: PASS / BURST" : "B, A: LOB    A, B: DROP", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 15, "UP/DN MODE  L/R OPTION", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, 4, 17, "v0.3", t, TITLE_PAL_HINT);
    title_opt_tile = t;  // last: the longest option fits from here on
    draw_length();
}

void title_update(void) BANKED {
    title_timer++;
    if (!(title_timer & (BLINK_MASK - 1))) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & (J_UP | J_DOWN)) {
        game_mode = !game_mode;
        sfx_play(SFX_TOUCH);
        state_restart();
        return;
    }
    if (joy_pressed & (J_SELECT | J_LEFT | J_RIGHT)) {
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
        state_change(game_mode ? STATE_FUTSAL : STATE_PLAY);
    }
}
