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
#include <gb/cgb.h>
#include "sfx.h"
#include "vfx.h"

extern uint8_t note_timer, set_won, hud_pending;
uint8_t hud_mode = 0xFF;      // the overlay showing (HUD_*; 0xFF: none yet)
uint8_t board_shown[4];       // the board's points and sets as drawn (0xFF: redraw)
uint8_t gauge_shown[4];       // each shadow tile's bar as drawn (yours: pixels filled, 0x80/0x81 armed; the CPU's: 0 none, 0x7F dark, 0xFF gold; 0xFE: redraw)
static uint8_t gauge_buf[4][6];  // their bars: three rows of the shadow's lower tile
static uint8_t strip_buf[5];
uint8_t meter_seen, meter_show, glow_level, glow_flash, ready_spark, hint_timer, hint_done;
static uint8_t was_special;
extern uint8_t vfx_kind;

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

void hud_init(void) BANKED {
    banked_bkg_data(T_FACE, PORTRAIT_TILE_COUNT, portrait_tiles, BANK(portrait));
    // The shadows (sprite tiles 16-19, set up by gfx_player_shadow_init) copied for
    // each player, their lower halves blank till a gauge is drawn in; the shadow
    // sprites pointed at the copies.
    uint8_t t[64];
    get_sprite_data(16, 4, t);
    for (uint8_t k = 16; k < 32; k++) t[k] = t[k + 32] = 0;
    set_sprite_data(S_GAUGE, 4, t);
    set_sprite_data(S_GAUGE + 4, 4, t);
    for (uint8_t k = 0; k < 4; k++) {
        set_sprite_tile(OAM_PLAYER_SHADOW + k, S_GAUGE + (k << 1));
        gauge_shown[k] = 0xFE;
    }
    // The hint's letters (5 x 6, gold), outlined (dark), in the tops of their tiles.
    static const uint8_t glyphs[3][6] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11},   // A
        {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04},   // +
        {0x1E, 0x11, 0x1E, 0x11, 0x11, 0x1E}};  // B
    for (uint8_t g = 0; g < 3; g++) {
        uint8_t l[8];
        l[0] = l[7] = 0;
        for (uint8_t r = 0; r < 6; r++) l[r + 1] = glyphs[g][r] << 1;
        for (uint8_t k = 0; k < 32; k++) t[k] = 0;
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t o = l[r] | (l[r] << 1) | (l[r] >> 1);
            if (r) o |= l[r - 1];
            if (r < 7) o |= l[r + 1];
            o &= ~l[r];
            t[r << 1] = l[r] | o;  // 1: the letter, 3: its outline
            t[(r << 1) + 1] = o;
        }
        set_sprite_data(S_HINT + (g << 1), 2, t);
        set_sprite_tile(OAM_HINT + g, S_HINT + (g << 1));
        set_sprite_prop(OAM_HINT + g, SPR_PAL_UI);
        move_sprite(OAM_HINT + g, 0, 0);
    }
    meter_seen = meter_show = glow_flash = ready_spark = hint_timer = hint_done = was_special = 0;
    glow_level = 0xFF;
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

// The bar in shadow tile k's lower half: over the columns in m, an outline row (2),
// the fill (1; 3 in the columns in e: empty), an outline row. Copied in at the next
// vblank when it changes (key: what's drawn).
static void gauge_fill(uint8_t k, uint8_t key, uint8_t m, uint8_t e) {
    if (key == gauge_shown[k]) return;
    gauge_shown[k] = key;
    uint8_t *b = gauge_buf[k];
    b[0] = 0x00;
    b[1] = m;
    b[2] = m;
    b[3] = e;
    b[4] = 0x00;
    b[5] = m;
    gfx_vram_copy((uint8_t *)(0x8000 + ((uint16_t)(S_GAUGE + (k << 1) + 1) << 4)), b, 6, 0);
}

// Your outline (colour 3 of your palette) as the meter stands: ink; full, a slow gold
// pulse; armed, a fast bright one; the slam struck, a white flash. Uploaded only when
// it changes (gfx_obj_color: 8 bytes at the vblank).
static const uint16_t glow_colors[6] = {
    RGB8(24, 32, 48), RGB8(104, 80, 32), RGB8(184, 136, 24), RGB8(255, 200, 40), RGB8(255, 240, 150), RGB8(255, 255, 255)};
static void glow_update(void) {
    uint8_t level = 0;
    if (glow_flash) {
        glow_flash--;
        level = 5;
    } else if (pl_special[SIDE_YOU] || pl_power[SIDE_YOU] == POWER_MAX) {
        uint8_t armed = pl_special[SIDE_YOU];
        uint8_t t = ((uint8_t)frame_count >> (armed ? 1 : 3)) & 7;
        level = (t < 4 ? t : 7 - t) + armed;  // a triangle: up and down
    }
    if (level != glow_level) {
        glow_level = level;
        gfx_obj_color(SPR_PAL_YOU, 3, glow_colors[level]);
    }
}

// The ready moment (your meter just full): the sound, a sparkle on you once the
// strike's own burst is done, and the first time in a match, A+B over you.
static void meter_events(void) {
    uint8_t p = pl_power[SIDE_YOU], sp = pl_special[SIDE_YOU];
    if (was_special && !sp) glow_flash = 6;  // the power slam struck: the glow snaps off in a flash
    was_special = sp;
    if (p != meter_seen) {
        uint8_t was = meter_seen >> 4, now = p >> 4;
        meter_seen = p;
        if (p == POWER_MAX) {
            sfx_play(SFX_READY);
            ready_spark = 40;
            meter_show = 0;
            if (!hint_done) {
                hint_done = 1;
                hint_timer = HINT_FRAMES;
            }
        } else if (p) {
            meter_show = 60;  // a strike filled it: the pips show a moment
            if (now > was) sfx_play(SFX_PIP + now - 1);
        }
    }
    if (ready_spark) {
        ready_spark--;
        if (vfx_kind == 0xFF) {
            vfx_start(VFX_SPARK, you_sx + 8, you_sy + cam_y - 12);
            ready_spark = 0;
        }
    }
    if (sp || goal_timer) hint_timer = 0;
    uint8_t hx = 0, hy = 0;
    if (hint_timer) {
        hint_timer--;
        int16_t y = you_sy - 20 - (((uint8_t)frame_count >> 3) & 1);  // bobbing over your head
        if (y > 0 && y < 160 && you_sx > 4 && you_sx < 156) hx = (uint8_t)(you_sx - 4), hy = (uint8_t)y;
    }
    for (uint8_t g = 0; g < 3; g++) {
        move_sprite(OAM_HINT + g, hx, hy);
        if (hx) hx += 8;
    }
}

void hud_update(uint8_t busy) BANKED {
    // The box goes once the serve's tossed (and comes back for the next: on the
    // frame after, hud_pending, not on the serve's set-up, a full frame already).
    if (!paused && !goal_timer && !hud_pending && (phase == PHASE_SERVE) != (hud_mode == HUD_BOX)) hud_draw();
    if (paused) return;
    if (!busy) meter_events();  // (a strike's busy frame: its sound and pips a frame on)
    glow_update();
    // Your meter: four pips in your shadow (16 px, a 3 px pip and a gap each), shown a
    // moment when a strike adds to it (playtest: "we don't need the power meter to be
    // persistent"), gold up to where it's filled, dark past; full, all gold while it's
    // ready; armed, blinking. The CPU's: an 8 px bar, blinking, ready or armed. The
    // shadow stays grey ("I liked the grey shadows better"). Tiles redrawn only on a
    // change, off a strike's busy frame.
    if (meter_show) meter_show--;
    if (busy) return;
    uint8_t lit = (frame_count & 8) ? 0xFF : 0;
    uint8_t on = (pl_special[SIDE_CPU] || pl_power[SIDE_CPU] == POWER_MAX) ? 0xFF : 0, e = on & ~lit;
    uint8_t key = on & (lit | 0x7F);  // 0 none, 0x7F dark, 0xFF gold
    gauge_fill(2, key, on & 0x0F, e & 0x0F);
    gauge_fill(3, key, on & 0xF0, e & 0xF0);
    uint8_t p = pl_power[SIDE_YOU];
    if (pl_special[SIDE_YOU]) {
        gauge_fill(0, 0x80 | (lit & 1), 0xEE, 0xEE & ~lit);
        gauge_fill(1, 0x80 | (lit & 1), 0xEE, 0xEE & ~lit);
    } else if (p == POWER_MAX || meter_show) {
        // Filled pixels: 12 of the pips' at a full meter (p * 3 / 16).
        uint8_t f = (uint8_t)((p << 1) + p) >> 4, fill[2] = {0, 0};
        if (f + 1 == gauge_shown[0] && f + 1 == gauge_shown[1]) return;  // (as drawn)
        for (uint8_t k = 0, n = 0; k < 16; k++) {
            if ((k & 3) == 3) continue;
            if (n++ < f) fill[k >> 3] |= 0x80 >> (k & 7);
        }
        gauge_fill(0, f + 1, 0xEE, 0xEE & ~fill[0]);
        gauge_fill(1, f + 1, 0xEE, 0xEE & ~fill[1]);
    } else {
        gauge_fill(0, 0, 0, 0);
        gauge_fill(1, 0, 0, 0);
    }
}
