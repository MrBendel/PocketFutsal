#pragma bank 255
// Main menu: all three modes on one screen (Futnet, 2v2 Futsal, Mini Games).
// UP/DOWN moves the cursor, LEFT/RIGHT or SELECT changes the selected mode's
// option, and START/A launches it (Mini Games opens its submenu).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "futsal.h"
#include "progress.h"
#include "sprites.h"

#include "scene_text.h"

uint8_t game_mode;  // MODE_*

#define MODE_FUTNET 0
#define MODE_FUTSAL 1
#define MODE_TRAINING 2
#define MODE_COUNT 3

#define ROW_MODES 6
#define ROW_OPT 10
#define ROW_PROMPT 12
#define ROW_HINT1 14
#define ROW_HINT2 15
#define CURSOR_X 28

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)

uint8_t title_timer;
uint8_t title_opt_tile;   // option line tiles (redrawn on mode/option change)
uint8_t title_aux_tile;
uint8_t title_hint_tile;  // mode hint line tiles (redrawn on mode change)

#define C(r, g, b) RGB8(r, g, b)
static const uint16_t ball_pal[4] = {0, C(255, 255, 255), C(96, 96, 112), C(24, 20, 32)};

static void draw_cursor(void) {
    set_sprite_tile(0, 0);
    set_sprite_prop(0, SPR_PAL_BALL);
    move_sprite(0, CURSOR_X + 8, ((ROW_MODES + game_mode) << 3) + 16);
}

// Futnet's option: the difficulty (the CPU's Power), then one set or the best of three.
static const char *const fn_line[6] = {
    "< CASUAL  1 SET >", "< CASUAL  3 SETS >", "< NORMAL  1 SET >",
    "< NORMAL  3 SETS >", "< EXPERT  1 SET >", "< EXPERT  3 SETS >",
};

static void draw_length(void) {
    fill_bkg_rect(0, ROW_OPT, SCREEN_TW, 1, 0);
    if (game_mode == MODE_TRAINING) {
        char line[] = "PLAYER LV   ";
        uint8_t lv = player_level();
        if (lv >= 10) line[10] = '0' + lv / 10, line[11] = '0' + lv % 10;
        else line[10] = '0' + lv, line[11] = 0;
        text_draw(FONT_COMPACT, TEXT_CENTER, ROW_OPT, line, title_opt_tile, TITLE_PAL_TEXT);
        return;
    }
    draw(FONT_COMPACT, TEXT_CENTER, ROW_OPT,
         game_mode ? (fs_pace==0 ? (fs_spectator?"< CASUAL WATCH >":"< CASUAL PLAY >")
                    :fs_pace==1 ? (fs_spectator?"< NORMAL WATCH >":"< NORMAL PLAY >")
                    :(fs_spectator?"< EXPERT WATCH >":"< EXPERT PLAY >"))
                   : fn_line[fn_pace * 2 + (match_sets == 2)],
         title_opt_tile, TITLE_PAL_TEXT);
}

// (Each line's text takes a few frames to draw: redraw only the one that changed.)
static void draw_aux(void) {
    fill_bkg_rect(0, ROW_OPT+1, SCREEN_TW, 1, 0);
    if(game_mode==MODE_FUTSAL)
        draw(FONT_COMPACT,TEXT_CENTER,ROW_OPT+1,"SELECT: WATCH / PLAY",title_aux_tile,TITLE_PAL_HINT);
    else if(game_mode==MODE_FUTNET)
        draw(FONT_COMPACT,TEXT_CENTER,ROW_OPT+1,you_ai?"SELECT: AI PLAYS":"SELECT: YOU PLAY",title_aux_tile,TITLE_PAL_HINT);
}

static void draw_hint(void) {
    draw_aux();
    fill_bkg_rect(0, ROW_HINT1, SCREEN_TW, 1, 0);
    draw(FONT_COMPACT, TEXT_CENTER, ROW_HINT1,
         game_mode == MODE_TRAINING ? "5 DRILLS TO LEVEL UP"
                                    : game_mode ? "A: SHOOT/SLIDE  B: PASS"
                                                : "A: POWER  B: CURVE",
         title_hint_tile, TITLE_PAL_HINT);
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
    memcpy(&pal_base_obj[SPR_PAL_BALL << 2], ball_pal, sizeof(ball_pal));
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    if (game_mode >= MODE_COUNT) game_mode = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 2, "POCKET FUTSAL", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, 44, ROW_MODES + 0, "FUTNET SINGLES", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, 44, ROW_MODES + 1, "2V2 FUTSAL", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, 44, ROW_MODES + 2, "MINI GAMES", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, TEXT_CENTER, ROW_PROMPT, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, ROW_HINT2, "UP/DN MODE  L/R OPTION", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, 4, 17, "v0.3", t, TITLE_PAL_HINT);
    title_opt_tile = t;
    t += 20;  // room for the longest option line ("< EXPERT  3 SETS >")
    title_aux_tile=t;t+=12;
    title_hint_tile = t;
    draw_length();
    draw_hint();
    draw_cursor();
}

void title_update(void) BANKED {
    title_timer++;
    if (!(title_timer & (BLINK_MASK - 1))) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & (J_UP | J_DOWN)) {
        if (joy_pressed & J_DOWN) game_mode = game_mode + 1 < MODE_COUNT ? game_mode + 1 : 0;
        else game_mode = game_mode ? game_mode - 1 : MODE_COUNT - 1;
        sfx_play(SFX_TOUCH);
        draw_cursor();
        draw_length();
        draw_hint();
        return;
    }
    if ((joy_pressed & (J_SELECT | J_LEFT | J_RIGHT)) && game_mode != MODE_TRAINING) {
        if (game_mode) {
            if(joy_pressed&J_SELECT)fs_spectator=!fs_spectator;
            else if(joy_pressed&J_LEFT)fs_pace=fs_pace?fs_pace-1:2;
            else fs_pace=fs_pace<2?fs_pace+1:0;
        }
        else if (joy_pressed & J_SELECT) you_ai = !you_ai;  // Futnet: you or the AI play
        else {  // Futnet: step through fn_line, wrapping
            uint8_t k = fn_pace * 2 + (match_sets == 2);
            k = joy_pressed & J_LEFT ? (k ? k - 1 : 5) : (k < 5 ? k + 1 : 0);
            fn_pace = k >> 1;
            match_sets = (k & 1) + 1;
        }
        sfx_play(SFX_TOUCH);
        if (game_mode == MODE_FUTNET && (joy_pressed & J_SELECT)) draw_aux();
        else draw_length();
    }
    if (joy_pressed & (J_START | J_A)) {
        // The frame START lands on seeds the match: different every time for a
        // player, reproducible for a replay. Tests can set test_seed instead.
        game_seed = test_seed ? test_seed : frame_count;
        sfx_play(SFX_START);
        state_change(game_mode == MODE_TRAINING ? STATE_TRAINING : game_mode ? STATE_FUTSAL : STATE_PLAY);
    }
}
