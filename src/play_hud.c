#pragma bank 255
// The match's score and overlay, out of play.c's bank. No HUD row in a rally: the
// court has the whole screen. The score's on a board in the court art, above the far
// end, as Mario Tennis has it (its digits swapped through the vblank queue when they
// change); before each serve a box in the corner shows each player's face and score;
// a point's call, or PAUSED, is a banner along the bottom for a moment. Your power
// meter's a gauge under your feet (the CPU's under its), sprites that follow you.
#include <gb/gb.h>
#include "gfx.h"
#include "game.h"
#include "play_hud.h"
#include "state.h"
#include "portrait.h"

extern uint8_t note_timer, set_won, hud_pending;
uint8_t hud_mode = 0xFF;      // the overlay showing (HUD_*; 0xFF: none yet)
uint8_t board_shown[4];       // the board's points and sets as drawn (0xFF: redraw)
uint8_t gauge_shown[3];       // each gauge sprite's fill, px (0xFF: redraw)
static uint8_t gauge_buf[3][6];  // their bars: three rows of the sprite's top tile
static uint8_t strip_buf[5];

#define DIGIT(n) (FONT_TILES + ('0' - ' ') + (n))
#define CHAR(c) (FONT_TILES + ((c) - ' '))

static void two_digits(uint8_t *t, uint8_t n) {
    uint8_t tens = 0;
    while (n >= 10) {
        n -= 10;
        tens++;
    }
    t[0] = DIGIT(tens);
    t[1] = DIGIT(n);
}

// The board: [YOU 07-05 CPU] in a frame, SETS 1-0 on its top edge in a best of three.
static void board_update(void) {
    if (score[0] != board_shown[0] || score[1] != board_shown[1]) {
        board_shown[0] = score[0];
        board_shown[1] = score[1];
        two_digits(strip_buf, score[0]);
        strip_buf[2] = CHAR('-');
        two_digits(strip_buf + 3, score[1]);
        uint8_t *t = gfx_strip(BOARD_X + 5, BOARD_Y + 1, 0, 5);
        for (uint8_t k = 0; k < 5; k++) t[k] = strip_buf[k];
    }
    if (match_sets > 1 && (sets[0] != board_shown[2] || sets[1] != board_shown[3])) {
        board_shown[2] = sets[0];
        board_shown[3] = sets[1];
        uint8_t *t = gfx_strip(BOARD_X + 8, BOARD_Y, 0, 3);
        t[0] = DIGIT(sets[0]);
        t[1] = CHAR('-');
        t[2] = DIGIT(sets[1]);
    }
}

static const uint8_t zeros[16 * 6];

void hud_init(void) BANKED {
    banked_bkg_data(T_FACE, PORTRAIT_TILE_COUNT, portrait_tiles, BANK(portrait));
    // The gauges: blank tiles (only their top three rows are ever drawn), sprites hidden.
    set_sprite_data(S_GAUGE, 6, zeros);
    for (uint8_t k = 0; k < 3; k++) {
        set_sprite_tile(OAM_GAUGE + k, S_GAUGE + (k << 1));
        move_sprite(OAM_GAUGE + k, 0, 0);
        gauge_shown[k] = 0xFF;
    }
    // The board, in the UI's palette.
    bkg_frame(BOARD_X, BOARD_Y, BOARD_W, 3);
    bkg_text(BOARD_X + 1, BOARD_Y + 1, "YOU");
    bkg_text(BOARD_X + 11, BOARD_Y + 1, "CPU");
    if (match_sets > 1) bkg_text(BOARD_X + 3, BOARD_Y, "SETS");
    VBK_REG = 1;
    fill_bkg_rect(BOARD_X, BOARD_Y, BOARD_W, 3, BG_PAL_UI);
    VBK_REG = 0;
    for (uint8_t k = 0; k < 4; k++) board_shown[k] = 0xFF;
    hud_mode = 0xFF;
}

// The faces' cells in the box: their own palettes (2: you, 3: the CPU) while it
// shows, the UI's again after (other scenes' windows expect it everywhere).
static const uint8_t face_tiles[4] = {T_FACE, T_FACE + 2, T_FACE + 1, T_FACE + 3};
static void face_attrs(uint8_t you, uint8_t cpu) {
    uint8_t a[4];
    VBK_REG = 1;
    a[0] = a[1] = a[2] = a[3] = you;
    set_win_tiles(0, 0, 2, 2, a);
    a[0] = a[1] = a[2] = a[3] = cpu;
    set_win_tiles(0, 2, 2, 2, a);
    VBK_REG = 0;
}

// One player's rows in the box: the face, a > by the server, the name; under it the
// sets won (a best of three) and the points.
static void box_player(uint8_t i, uint8_t y) {
    set_win_tiles(0, y, 2, 2, face_tiles);
    if (server == i) win_text(2, y, ">");
    win_text(3, y, i ? "CPU" : "YOU");
    if (match_sets > 1) win_number(3, y + 1, sets[i], 1, 0);
    win_number(5, y + 1, score[i], 2, 0);
}

void hud_draw(void) BANKED {
    uint8_t mode = paused || goal_timer ? HUD_BANNER : phase == PHASE_SERVE ? HUD_BOX : HUD_NONE;
    board_update();
    if (mode == HUD_BOX && hud_mode == HUD_BOX) return;  // (the serve's note running out: as it was)
    if (hud_mode == HUD_BOX && mode != HUD_BOX) face_attrs(BG_PAL_UI, BG_PAL_UI);
    hud_mode = mode;
    if (mode == HUD_NONE) {
        HIDE_WIN;
        return;
    }
    if (mode == HUD_BOX) {
        move_win(7 + (BOX_X << 3), (SCREEN_TH - BOX_ROWS) << 3);
        fill_win_rect(0, 0, SCREEN_TW - BOX_X, BOX_ROWS, 0);
        face_attrs(BG_PAL_FACE_YOU, BG_PAL_FACE_CPU);
        box_player(SIDE_YOU, 0);
        box_player(SIDE_CPU, 2);
        SHOW_WIN;
        return;
    }
    move_win(7, (SCREEN_TH - 1) << 3);
    fill_win_rect(0, 0, SCREEN_TW, 1, 0);
    SHOW_WIN;
    if (paused) {
        win_text(7, 0, "PAUSED");
        return;
    }
    uint8_t you = last_scorer == SIDE_YOU;
    uint8_t r = point_reason;
    if (set_won && goal_timer < GOAL_FRAMES / 2) win_text(4, 0, you ? "SET TO YOU" : "SET TO CPU");
    else if (r == POINT_NET) win_text(3, 0, you ? "NET! POINT YOU" : "NET! POINT CPU");
    else if (r == POINT_OUT) win_text(3, 0, you ? "OUT! POINT YOU" : "OUT! POINT CPU");
    else if (r == POINT_TOUCHES) win_text(0, 0, you ? "3 TOUCHES! POINT YOU" : "3 TOUCHES! POINT CPU");
    else if (r == POINT_FAULT) win_text(2, 0, you ? "FAULT! POINT YOU" : "FAULT! POINT CPU");
    else win_text(0, 0, you ? "2 BOUNCES! POINT YOU" : "2 BOUNCES! POINT CPU");
}

// A gauge sprite's bar, n px of 8 filled: an outline row (2), the fill (1) then the
// empty (3), an outline row. Copied in at the next vblank when it changes.
static void gauge_fill(uint8_t k, uint8_t n) {
    if (n == gauge_shown[k]) return;
    gauge_shown[k] = n;
    uint8_t *b = gauge_buf[k];
    b[0] = 0x00;
    b[1] = 0xFF;
    b[2] = 0xFF;
    b[3] = n >= 8 ? 0x00 : (uint8_t)(0xFF >> n);
    b[4] = 0x00;
    b[5] = 0xFF;
    gfx_vram_copy((uint8_t *)(0x8000 + ((uint16_t)(S_GAUGE + (k << 1)) << 4)), b, 6, 0);
}

void hud_update(uint8_t busy) BANKED {
    // The box goes once the serve's tossed (and comes back for the next: on the
    // frame after, hud_pending, not on the serve's set-up, a full frame already).
    if (!paused && !goal_timer && !hud_pending && (phase == PHASE_SERVE) != (hud_mode == HUD_BOX)) hud_draw();
    // The gauges, under each player's shadow (draw() placed it this frame): yours
    // 16 px, a pixel for each 4 of the meter, the CPU's 8; full or armed, flashing.
    // Empty, hidden.
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t p = pl_power[i], full = pl_special[i] || p == POWER_MAX;
        OAM_item_t *s = &shadow_OAM[OAM_PLAYER_SHADOW + (i << 1)];
        OAM_item_t *o = &shadow_OAM[OAM_GAUGE + (i << 1)];
        uint8_t y = (p || full) ? s->y + 8 : 0;
        uint8_t pal = full && (frame_count & 8) ? SPR_PAL_POWER : SPR_PAL_UI;
        if (i == SIDE_YOU) {
            uint8_t n = full ? 16 : p >> 2;
            if (!busy) {
                gauge_fill(0, n > 8 ? 8 : n);
                gauge_fill(1, n > 8 ? n - 8 : 0);
            }
            o->y = y;
            o->x = s->x;
            o->prop = pal;
            o[1].y = y;
            o[1].x = s->x + 8;
            o[1].prop = pal;
        } else {
            if (!busy) gauge_fill(2, full ? 8 : p >> 3);
            o->y = y;
            o->x = s->x + 4;
            o->prop = pal;
        }
    }
}
