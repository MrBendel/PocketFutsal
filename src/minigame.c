#pragma bank 255
// The mini games' shell (minigame.h): the brief and results cards, the countdown,
// pause, scoring and XP, and the drawing helpers the games share.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "sfx.h"
#include "game.h"
#include "minigame.h"
#include "progress.h"
#include "sprites.h"
#include "players.h"
#include "scene_text.h"

uint8_t mg_id, mg_phase, mg_run, mg_paused, mg_hud_dirty, mg_won, mg_new_best;
uint16_t mg_timer, mg_score;
uint16_t mg_xp[2];
uint8_t mg_level_ups[2];
char mg_detail[MG_DETAIL_MAX + 1];
uint8_t mg_card_delay;  // frames before a card takes a button (one held through the fade)

#define CARD_DELAY 20
#define XP_CAP 400

// --- the games' details, by MG_* id ---
static const char *const names[MG_COUNT] = {"BALL MACHINE", "TARGET WALL", "KEEPY-UPPY", "SHOOTOUT"};
// The stats each trains: the first takes all the XP, the second half.
static const uint8_t trains[MG_COUNT][2] = {
    {STAT_REFLEX, STAT_CONTROL},  // the machine: get to it, put it back in
    {STAT_AIM, STAT_POWER},       // the wall: hit the lit panel
    {STAT_CONTROL, STAT_REFLEX},  // keepy-uppy: touch and timing
    {STAT_POWER, STAT_REFLEX},    // the shootout: shoot, then save
};
// XP a point of score. Bonuses: a new best, a win.
static const uint8_t xp_per[MG_COUNT] = {3, 4, 2, 5};
#define XP_BEST 10
#define XP_WIN 20
// How to play: four lines each, short enough for the compact font (about 36
// letters across the screen).
static const char *const brief[MG_COUNT][4] = {
    {"THE MACHINE FIRES BALLS AT YOU.", "RUN TO EACH ONE AND PRESS A", "TO RETURN IT INSIDE THE LINES.",
     "D-PAD AIMS. THREE MISSES: OUT!"},
    {"HIT THE BALL AT THE WALL.", "ONLY THE PANEL THAT LIGHTS UP", "SCORES. HOLD THE D-PAD TO AIM,",
     "A TO STRIKE. BEAT THE CLOCK!"},
    {"KEEP THE BALL UP! PRESS A AS", "IT DROPS TO YOUR FOOT. B FOR", "A HEADER WHEN IT IS HIGH. STAY",
     "UNDER IT WITH THE D-PAD."},
    {"FIVE PENALTIES EACH. SHOOTING:", "AIM WITH THE D-PAD, A TO SHOOT.", "IN GOAL: GUESS THE CPU'S SIDE",
     "AND DIVE WITH A. TIMING!"},
};
static const char *const stat_names[STAT_COUNT] = {"POWER", "CONTROL", "AIM", "REFLEX"};

// For other banks (training.c): copies, since a string here isn't visible there.
static void copy(char *dst, const char *s) {
    while ((*dst++ = *s++)) {}
}
void mg_name(uint8_t id, char *buf) BANKED {
    copy(buf, names[id]);
}
void stat_name(uint8_t s, char *buf) BANKED {
    copy(buf, stat_names[s]);
}
uint8_t mg_trains(uint8_t id, uint8_t which) BANKED {
    return trains[id][which];
}

// "123" into buf; returns its length.
static uint8_t num(char *buf, uint16_t n) {
    char t[5];
    uint8_t k = 0, len = 0;
    do t[k++] = '0' + n % 10, n /= 10; while (n);
    while (k) buf[len++] = t[--k];
    buf[len] = 0;
    return len;
}
// Append s (from this bank, or RAM) at buf + at; returns the new length.
static uint8_t cat(char *buf, uint8_t at, const char *s) {
    while (*s) buf[at++] = *s++;
    buf[at] = 0;
    return at;
}

// --- the HUD's banner ---
static void banner(const char *s) {
    uint8_t n = 0;
    while (s[n]) n++;
    fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
    win_text((SCREEN_TW - n) >> 1, 0, s);
}

// --- the dispatch ---
static void game_enter(void) {
    switch (mg_id) {
        case MG_MACHINE: mg_machine_enter(); break;
        case MG_WALL: mg_wall_enter(); break;
        case MG_JUGGLE: mg_juggle_enter(); break;
        case MG_SHOOTOUT: mg_shootout_enter(); break;
    }
}
static void game_update(void) {
    switch (mg_id) {
        case MG_MACHINE: mg_machine_update(); break;
        case MG_WALL: mg_wall_update(); break;
        case MG_JUGGLE: mg_juggle_update(); break;
        case MG_SHOOTOUT: mg_shootout_update(); break;
    }
}

// --- the cards ---
static void card_begin(void) {
    palettes_menu();
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    mg_card_delay = CARD_DELAY;
}

static void brief_enter(void) {
    char line[TEXT_MAX + 1];
    uint8_t t = SCENE_TILES, n;
    card_begin();
    t += draw(FONT_DISPLAY, TEXT_CENTER, 1, names[mg_id], t, TITLE_PAL_GOLD);
    // TRAINS REFLEX + CONTROL
    n = cat(line, 0, "TRAINS ");
    n = cat(line, n, stat_names[trains[mg_id][0]]);
    n = cat(line, n, " + ");
    cat(line, n, stat_names[trains[mg_id][1]]);
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 4, line, t, TITLE_PAL_TEXT);
    for (uint8_t i = 0; i < 4; i++) t += draw(FONT_COMPACT, TEXT_CENTER, 7 + i, brief[mg_id][i], t, TITLE_PAL_HINT);
    // BEST 12   PLAYED 3
    n = cat(line, 0, "BEST ");
    n += num(line + n, mg_best[mg_id]);
    n = cat(line, n, "    PLAYED ");
    num(line + n, mg_plays[mg_id]);
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 13, line, t, TITLE_PAL_TEXT);
    draw(FONT_COMPACT, TEXT_CENTER, 16, "A: START     B: BACK", t, TITLE_PAL_PROMPT);
}

static void result_enter(void) {
    char line[TEXT_MAX + 1];
    uint8_t t = SCENE_TILES, n, row = 5;
    card_begin();
    t += draw(FONT_DISPLAY, TEXT_CENTER, 1, names[mg_id], t, TITLE_PAL_GOLD);
    n = cat(line, 0, "SCORE ");
    num(line + n, mg_score);
    t += text_draw(FONT_COMPACT, TEXT_CENTER, row++, line, t, TITLE_PAL_TEXT);
    if (mg_detail[0]) t += text_draw(FONT_COMPACT, TEXT_CENTER, row++, mg_detail, t, TITLE_PAL_TEXT);
    if (mg_new_best && mg_score) {
        t += draw(FONT_COMPACT, TEXT_CENTER, row, "NEW BEST!", t, TITLE_PAL_PROMPT);
    } else {
        n = cat(line, 0, "BEST ");
        num(line + n, mg_best[mg_id]);
        t += text_draw(FONT_COMPACT, TEXT_CENTER, row, line, t, TITLE_PAL_HINT);
    }
    row += 2;
    // +36 XP REFLEX, and REFLEX UP! LV 3 under it if it went up.
    for (uint8_t k = 0; k < 2; k++) {
        uint8_t s = trains[mg_id][k];
        line[0] = '+';
        n = 1 + num(line + 1, mg_xp[k]);
        n = cat(line, n, " XP  ");
        cat(line, n, stat_names[s]);
        t += text_draw(FONT_COMPACT, TEXT_CENTER, row++, line, t, TITLE_PAL_TEXT);
        if (mg_level_ups[k]) {
            n = cat(line, 0, stat_names[s]);
            n = cat(line, n, " UP! LV ");
            num(line + n, stat_lv[s]);
            t += text_draw(FONT_COMPACT, TEXT_CENTER, row++, line, t, TITLE_PAL_GOLD);
        }
    }
    n = cat(line, 0, "PLAYER LV ");
    num(line + n, player_level());
    t += text_draw(FONT_COMPACT, TEXT_CENTER, row + 1, line, t, TITLE_PAL_HINT);
    draw(FONT_COMPACT, TEXT_CENTER, 16, "A: AGAIN    B: TRAINING", t, TITLE_PAL_PROMPT);
}

static void card_update(void) {
    if (mg_card_delay) {
        mg_card_delay--;
        return;
    }
    if (joy_pressed & (J_A | J_START)) {
        game_seed = test_seed ? test_seed : frame_count;
        sfx_play(SFX_START);
        mg_phase = MG_PH_PLAY;
        state_restart();
    } else if (joy_pressed & J_B) {
        sfx_play(SFX_TOUCH);
        state_change(STATE_TRAINING);
    }
}

// --- playing ---
static void play_begin(void) {
    palettes_play();  // the match's sprite and HUD palettes; the game sets its background's
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    banked_sprite_data_vbk1(0, PLAYERS_TILE_COUNT, players_tiles, BANK(players));
    initrand(game_seed);
    mg_run = MG_RUN_READY;
    mg_timer = 0;
    mg_paused = mg_hud_dirty = mg_won = 0;
    mg_score = 0;
    mg_detail[0] = 0;
    move_win(7, HUD_WY);
    SHOW_WIN;
    gfx_sprite_split(HUD_WY);
    game_enter();
    banner("READY...");
}

// Score the game: bests, plays, XP, level ups, and save.
static void score_it(void) {
    uint8_t id = mg_id;
    mg_plays[id]++;
    mg_new_best = mg_score > mg_best[id];
    if (mg_new_best) mg_best[id] = mg_score;
    uint16_t xp = mg_score > XP_CAP ? XP_CAP : mg_score * xp_per[id];
    if (xp > XP_CAP) xp = XP_CAP;
    if (mg_new_best) xp += XP_BEST;
    if (mg_won) xp += XP_WIN;
    mg_xp[0] = xp;
    mg_xp[1] = xp >> 1;
    for (uint8_t k = 0; k < 2; k++) mg_level_ups[k] = progress_award(trains[id][k], mg_xp[k]);
    progress_save();
}

void mg_finish(uint16_t score) BANKED {
    if (mg_run == MG_RUN_OVER) return;
    mg_score = score;
    mg_run = MG_RUN_OVER;
    mg_timer = 0;
    banner(mg_won ? "YOU WIN!" : "FINISH!");
    sfx_play(SFX_GOAL);
}

void minigame_enter(void) BANKED {
    progress_load();
    if (mg_phase == MG_PH_PLAY) play_begin();
    else if (mg_phase == MG_PH_RESULT) result_enter();
    else brief_enter();
}

void minigame_update(void) BANKED {
    if (mg_phase != MG_PH_PLAY) {
        card_update();
        return;
    }
    if ((joy_pressed & J_START) && mg_run != MG_RUN_OVER) {
        mg_paused = !mg_paused;
        sfx_play(SFX_TOUCH);
        if (mg_paused) banner("PAUSED  SELECT:QUIT");
        else if (mg_run == MG_RUN_LIVE) mg_hud_dirty = 1;
        else banner("READY...");
        return;
    }
    if (mg_paused) {
        if (joy_pressed & J_SELECT) state_change(STATE_TRAINING);
        return;
    }
    mg_timer++;
    if (mg_run == MG_RUN_READY) {
        if (mg_timer == MG_READY_FRAMES - 30) {
            banner("GO!");
            sfx_play(SFX_WHISTLE);
        }
        if (mg_timer >= MG_READY_FRAMES) {
            mg_run = MG_RUN_LIVE;
            mg_timer = 0;
            mg_hud_dirty = 1;
        }
    } else if (mg_run == MG_RUN_OVER && mg_timer >= MG_OVER_FRAMES) {
        score_it();
        mg_phase = MG_PH_RESULT;
        state_restart();
        return;
    }
    game_update();
}

// --- drawing ---
static void place(uint8_t n, int16_t x, int16_t y) {
    if (x <= -8 || x >= 160 || y <= -16 || y >= 144) move_sprite(n, 0, 0);
    else move_sprite(n, (uint8_t)(x + 8), (uint8_t)(y + 16));
}

void mg_sprite(uint8_t n, uint8_t tile, uint8_t prop, int16_t x, int16_t y) BANKED {
    set_sprite_tile(n, tile);
    set_sprite_prop(n, prop);
    place(n, x, y);
}

void mg_hide(uint8_t first, uint8_t last) BANKED {
    for (uint8_t n = first; n <= last; n++) move_sprite(n, 0, 0);
}

void mg_player(uint8_t slot, uint8_t tile, int16_t cx, int16_t feet, uint8_t pal, uint8_t flip) BANKED {
    uint8_t n = OAM_MG_PLAYER + (slot << 2);
    uint8_t l = flip ? tile + 4 : tile, r = flip ? tile : tile + 4;
    int16_t x = cx - 8, y = feet - 23;
    pal |= S_BANK;
    if (flip) pal |= S_FLIPX;
    set_sprite_tile(n, l);
    set_sprite_tile(n + 1, l + 2);
    set_sprite_tile(n + 2, r);
    set_sprite_tile(n + 3, r + 2);
    for (uint8_t k = 0; k < 4; k++) set_sprite_prop(n + k, pal);
    place(n, x, y);
    place(n + 1, x, y + 16);
    place(n + 2, x + 8, y);
    place(n + 3, x + 8, y + 16);
}

#define T_LYING 160
void mg_player_lying(uint8_t slot, int16_t cx, int16_t cy, uint8_t pal, uint8_t flip) BANKED {
    uint8_t n = OAM_MG_PLAYER + (slot << 2);
    pal |= S_BANK;
    if (flip) pal |= S_FLIPX;
    for (uint8_t c = 0; c < 3; c++) {
        set_sprite_tile(n + c, T_LYING + ((flip ? 2 - c : c) << 1));
        set_sprite_prop(n + c, pal);
        place(n + c, cx - 12 + (c << 3), cy - 8);
    }
    move_sprite(n + 3, 0, 0);
}

void mg_player_hide(uint8_t slot) BANKED {
    uint8_t n = OAM_MG_PLAYER + (slot << 2);
    mg_hide(n, n + 3);
}

void mg_ball(int16_t x, int16_t gy, int16_t z, uint8_t roll, uint8_t pal) BANKED {
    uint8_t tile = MG_T_BALL + (roll && ((uint8_t)frame_count & 8) ? 2 : 0);
    mg_sprite(OAM_MG_BALL, tile, pal, x - 4, gy - z - 8);
    if (gy < 0) move_sprite(OAM_MG_SHADOW, 0, 0);
    else mg_sprite(OAM_MG_SHADOW, MG_T_SHADOW, SPR_PAL_UI, x - 4, gy - 6);
}

// --- loading ---
void mg_load_bg(const uint8_t *tiles, uint8_t n, const uint8_t *map, const uint8_t *attr,
                uint8_t w, uint8_t h, uint8_t bank) BANKED {
    uint8_t buf[20 * 21];
    uint16_t size = (uint16_t)w * h;
    if (size > sizeof(buf)) return;
    banked_bkg_data(SCENE_TILES, n, tiles, bank);
    banked_memcpy(buf, map, size, bank);
    set_bkg_tiles(0, 0, w, h, buf);
    banked_memcpy(buf, attr, size, bank);
    VBK_REG = 1;
    set_bkg_tiles(0, 0, w, h, buf);
    VBK_REG = 0;
}

void mg_load_spr(const uint8_t *tiles, uint8_t n, uint8_t bank) BANKED {
    banked_sprite_data(MG_SPR_TILES, n, tiles, bank);
}
