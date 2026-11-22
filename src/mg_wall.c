#pragma bank 255
// Placeholder for the wall mini game (see minigame.h for the contract): the
// player stands with the ball, each A is a point, and the clock runs out after
// ten seconds. Replace with the real game.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"
#include "input.h"
#include "minigame.h"
#include "mg_wall_bg.h"
#include "mg_wall_spr.h"

uint16_t mg_wall_points;

static const uint16_t wall_floor[4] = {RGB8(64, 160, 176), RGB8(48, 136, 152), RGB8(40, 56, 88), RGB8(240, 248, 240)};

void mg_wall_enter(void) BANKED {
    memcpy(pal_base_bg, wall_floor, sizeof(wall_floor));
    mg_load_bg(mg_wall_bg_tiles, MG_WALL_BG_TILE_COUNT, mg_wall_bg_map, mg_wall_bg_attr,
               MG_WALL_BG_MAP_W, MG_WALL_BG_MAP_H, BANK(mg_wall_bg));
    mg_load_spr(mg_wall_spr_tiles, MG_WALL_SPR_TILE_COUNT, BANK(mg_wall_spr));
    mg_wall_points = 0;
    mg_player(0, MG_FRAME(MG_VIEW_BACK, MG_F_STAND), 80, 110, SPR_PAL_YOU, 0);
    mg_ball(88, 112, 0, 0, SPR_PAL_BALL);
}

void mg_wall_update(void) BANKED {
    if (mg_run != MG_RUN_LIVE) return;
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        fill_win_rect(0, 0, SCREEN_TW, 1, 0);
        win_text(0, 0, "POINTS");
    }
    if (joy_pressed & J_A) mg_wall_points++;
    win_number(7, 0, mg_wall_points, 3, 0);
    if (mg_timer >= 600) mg_finish(mg_wall_points);
}