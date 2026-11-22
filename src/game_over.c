#pragma bank 255
// Full time: who won, and the score. START (or A), after a moment so a held
// button doesn't skip it, returns to the title.
#include <gb/gb.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "scene_text.h"

#define INPUT_DELAY 30  // frames before START counts

uint8_t game_over_timer;

void game_over_enter(void) BANKED {
    game_over_timer = 0;
    palettes_menu();
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 4, game_mode ? (winner == SIDE_YOU ? "BLUE WINS!" : "RED WINS!") : (winner == SIDE_YOU ? "YOU WIN!" : "CPU WINS"), t, TITLE_PAL_GOLD);
    // "YOU 11 - 7 CPU": the last set's points, one or two digits each; for the
    // best of three, "SETS 2 - 1" under it.
    char line[16];
    uint8_t n = 0;
    line[n++] = game_mode ? 'B' : 'Y', line[n++] = game_mode ? 'L' : 'O', line[n++] = 'U', line[n++] = ' ';
    for (uint8_t s = 0; s < 2; s++) {
        uint8_t v = score[s];
        if (v >= 10) line[n++] = '0' + v / 10;
        line[n++] = '0' + v % 10;
        if (!s) line[n++] = ' ', line[n++] = '-', line[n++] = ' ';
    }
    line[n++] = ' ', line[n++] = game_mode ? 'R' : 'C', line[n++] = game_mode ? 'E' : 'P', line[n++] = game_mode ? 'D' : 'U';
    line[n] = 0;
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 8, line, t, TITLE_PAL_TEXT);
    if (match_sets > 1) {
        char sl[] = "SETS 0 - 0";
        sl[5] = '0' + sets[SIDE_YOU];
        sl[9] = '0' + sets[SIDE_CPU];
        t += text_draw(FONT_COMPACT, TEXT_CENTER, 10, sl, t, TITLE_PAL_TEXT);
    }
    draw(FONT_COMPACT, TEXT_CENTER, 14, "PRESS START", t, TITLE_PAL_TEXT);
}

void game_over_update(void) BANKED {
    if (game_over_timer < INPUT_DELAY) {
        game_over_timer++;
        return;
    }
    if (joy_pressed & (J_START | J_A)) state_change(STATE_TITLE);
}
