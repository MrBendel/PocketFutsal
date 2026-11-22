#pragma bank 255
// Placeholder for the juggle mini game (see minigame.h for the contract): the
// player stands with the ball, each A is a point, and the clock runs out after
// ten seconds. Replace with the real game.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"
#include "input.h"
#include "minigame.h"
#include "mg_juggle_bg.h"
#include "mg_juggle_spr.h"

uint16_t mg_juggle_points;

static const uint16_t juggle_floor[4] = {RGB8(64, 160, 176), RGB8(48, 136, 152), RGB8(40, 56, 88), RGB8(240, 248, 240)};

void mg_juggle_enter(void) BANKED {
    memcpy(pal_base_bg, juggle_floor, sizeof(juggle_floor));
    mg_load_bg(mg_juggle_bg_tiles, MG_JUGGLE_BG_TILE_COUNT, mg_juggle_bg_map, mg_juggle_bg_attr,
               MG_JUGGLE_BG_MAP_W, MG_JUGGLE_BG_MAP_H, BANK(mg_juggle_bg));
    mg_load_spr(mg_juggle_spr_tiles, MG_JUGGLE_SPR_TILE_COUNT, BANK(mg_juggle_spr));
    mg_juggle_points = 0;
    mg_player(0, MG_FRAME(MG_VIEW_BACK, MG_F_STAND), 80, 110, SPR_PAL_YOU, 0);
    mg_ball(88, 112, 0, 0, SPR_PAL_BALL);
}

void mg_juggle_update(void) BANKED {
    if (mg_run != MG_RUN_LIVE) return;
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "POINTS");
    }
    if (joy_pressed & J_A) mg_juggle_points++;
    win_number(7, 0, mg_juggle_points, 3, 0);
    if (mg_timer >= 600) mg_finish(mg_juggle_points);
}