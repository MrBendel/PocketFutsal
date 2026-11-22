#pragma bank 255
// The match's HUD text, out of play.c's bank (cold: when a note comes or goes, and
// its strings): YOU 0 [meter] 0 CPU (this set's points), or a note for a moment.
#include <gb/gb.h>
#include "gfx.h"
#include "game.h"
#include "play_hud.h"

extern uint8_t note_timer, set_won, meter_shown, shot_shown;

void hud_draw(void) BANKED {
    fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
    if (paused) {
        win_text(7, 0, "PAUSED");
        return;
    }
    if (note_timer) {
        uint8_t you = last_scorer == SIDE_YOU;
        uint8_t r = point_reason;
        if (!goal_timer) win_text(5, 0, server == SIDE_YOU ? "YOU SERVE" : "CPU SERVES");
        else if (set_won && goal_timer < GOAL_FRAMES / 2) win_text(4, 0, you ? "SET TO YOU" : "SET TO CPU");
        else if (r == POINT_NET) win_text(3, 0, you ? "NET! POINT YOU" : "NET! POINT CPU");
        else if (r == POINT_OUT) win_text(3, 0, you ? "OUT! POINT YOU" : "OUT! POINT CPU");
        else if (r == POINT_TOUCHES) win_text(0, 0, you ? "3 TOUCHES! POINT YOU" : "3 TOUCHES! POINT CPU");
        else if (r == POINT_FAULT) win_text(2, 0, you ? "FAULT! POINT YOU" : "FAULT! POINT CPU");
        else win_text(0, 0, you ? "2 BOUNCES! POINT YOU" : "2 BOUNCES! POINT CPU");
        return;
    }
    // YOU 10 [meter] 9 CPU: two digits each (a set can go to 15).
    win_text(0, 0, "YOU");
    win_number(4, 0, score[SIDE_YOU], 2, 0);
    win_number(14, 0, score[SIDE_CPU], 2, 0);
    win_text(17, 0, "CPU");
    meter_shown = 0xFF;  // the meter's tiles were cleared: draw them again
    shot_shown = 0xFF;
}
