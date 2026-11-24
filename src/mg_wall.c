#pragma bank 255
// TARGET WALL (see minigame.h for the contract): a training wall, like the wall
// practice in Mario Tennis on the Game Boy Advance. Six panels on the wall (3
// across, 2 high); one lights up. Strike the ball back at the wall: only the lit
// panel scores, and hits in a row score more. The ball rebounds off the wall,
// bounces once on the floor, and you strike it again; let it bounce twice and it's
// fed again from the wall after a pause. A 60 second clock.
//
//   d-pad      move; held at the strike, it aims: LEFT / nothing / RIGHT picks the
//              column, UP the top row (else the bottom row)
//   A          with the ball in reach: strike it. Held before it arrives: charge,
//              and the strike goes as it comes into reach; a full charge is a
//              power shot (faster there and back, worth a point more on the lit panel)
//
// Stats: AIM narrows the spread round the panel you aim at; POWER fills the charge
// faster and makes a power shot quicker. Late in the clock the lit panel moves on
// by itself if you're slow (never while a shot is on its way).
//
// The scene: tools/art/mg_wall.mjs (the perspective there matches gy_tab/s_tab).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include <rand.h>
#include "gfx.h"
#include "state.h"
#include "input.h"
#include "sfx.h"
#include "game.h"
#include "futnet_core.h"
#include "minigame.h"
#include "progress.h"
#include "mg_wall_bg.h"
#include "mg_wall_spr.h"

// --- the court, in world px (x across, 0..160 at the near end; d in depth from
// the wall's foot) and SUB = 16ths. Heights z in px above the floor (unscaled).
#define GRAV 2            // SUB a frame each frame
#define D_MAX 126         // the tables' depth
#define PL_D_MIN 60       // you stay behind the strike line (depth 56)
#define PL_D_MAX 104
#define PL_X_MIN 0
#define PL_X_MAX 160
#define PL_SPEED 20       // SUB a frame
#define BALL_X_MIN -8     // off the screen's edges: the ball comes back off the side walls
#define BALL_X_MAX 168
#define REACH_X 14        // px either side of you
#define REACH_FWD 10      // px in front (toward the wall)
#define REACH_BACK 6
#define REACH_Z 30
#define SWEET_X 6         // a ball struck this close to your middle goes truer
#define KICK_FRAMES 12
#define CHARGE_FULL 64
#define T_DRIVE 36        // frames to the wall: a tap; less with charge, down to 20
#define T_POWER 18        // a power shot (less with POWER)
#define FEED_T 62
#define FEED_VD 22
#define MISS_WAIT 50      // frames from a missed ball to the next feed
#define FLASH_FRAMES 24   // a hit panel flashes, then another lights
#define POP_FRAMES 36
#define GAME_SECONDS 60
#define LATE_SECONDS 40   // from here on the lit panel moves on by itself
#define MULT_MAX 4

// The wall's panels: a column by world x at the wall, a row by height there.
#define COL_X1 48         // world x: 3/4 scale at the wall (screen 56 and 104)
#define COL_X2 112
#define ROW_Z_LOW 11      // below: the kickboard
#define ROW_Z_MID 32      // the top row from here
#define ROW_Z_TOP 54      // above: bricks
#define NO_PANEL 0xFF
#define PAN_UNLIT 1       // background palettes
#define PAN_LIT 2
#define PAN_HIT 3

// Ball states.
#define BALL_HELD 0       // waiting to be fed (hidden at the wall)
#define BALL_OUT 1        // on its way to the wall
#define BALL_BACK 2       // off the wall (or fed): yours to strike
#define BALL_DEAD 3       // missed: rolling away

// Sprites (MG_SPR_TILES + WT_*, tools/art/mg_wall.mjs).
#define WT_PLUS 0
#define WT_DIGIT 2
#define WT_M 22
#define WT_I 24
#define WT_S 26
#define OAM_AIM OAM_MG_FREE        // the aim marker on the wall, while charging
#define OAM_POP (OAM_MG_FREE + 1)  // "+3": 2
#define OAM_MISS (OAM_MG_FREE + 3) // "MISS": 4
#define OAM_TRAIL (OAM_MG_FREE + 7) // 4: shared Futnet red/blue shot trail

// The perspective (tools/art/mg_wall.mjs gy, sc), every 2 px of depth.
static const uint8_t gy_tab[64] = {
    56, 57, 58, 59, 60, 61, 62, 63, 65, 66, 67, 68, 69, 70, 72, 73, 74, 75, 77, 78, 79, 81,
    82, 83, 85, 86, 87, 89, 90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 106, 107, 109, 111,
    112, 114, 116, 117, 119, 121, 123, 124, 126, 128, 130, 132, 134, 135, 137, 139, 141, 143,
    145, 147, 149, 151};
static const uint8_t s_tab[64] = {
    96, 97, 97, 98, 98, 99, 100, 100, 101, 102, 102, 103, 103, 104, 105, 105, 106, 106, 107,
    108, 108, 109, 110, 110, 111, 111, 112, 113, 113, 114, 114, 115, 116, 116, 117, 118, 118,
    119, 119, 120, 121, 121, 122, 122, 123, 124, 124, 125, 126, 126, 127, 127, 128, 129, 129,
    130, 130, 131, 132, 132, 133, 134, 134, 135};
// 2048 / T for T = 16 .. 64: v (SUB a frame) = px * rcp >> 7 covers px in T frames.
static const uint8_t rcp_tab[49] = {
    128, 120, 114, 108, 102, 98, 93, 89, 85, 82, 79, 76, 73, 71, 68, 66, 64, 62, 60, 59, 57,
    55, 54, 53, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 39, 38, 37, 37, 36, 35,
    35, 34, 34, 33, 33, 32};
#define RCP(t) rcp_tab[(t) - 16]

static const int16_t col_x[3] = {16, 80, 144};  // the panels' middles, world x at the wall
static const int8_t row_z[2] = {43, 21};        // top, bottom: heights at the wall
static const uint8_t col_sx[3] = {32, 80, 128}; // ... on screen
static const uint8_t row_sy[2] = {24, 40};

// --- watchable state ---
int16_t mg_wall_bx, mg_wall_bd, mg_wall_bz;  // the ball: world x, depth, height (SUB)
int16_t mg_wall_vx, mg_wall_vd, mg_wall_vz;
uint8_t mg_wall_ball;      // BALL_*
uint8_t mg_wall_bounces;   // on the floor since it left the wall
uint8_t mg_wall_fire;      // a power shot in flight
int16_t mg_wall_px, mg_wall_pd;  // you: world x, depth (SUB)
uint8_t mg_wall_charge;    // 0 .. CHARGE_FULL
uint8_t mg_wall_charging;  // A held, waiting for the ball
uint8_t mg_wall_kick;      // the strike's pose, counting down
uint8_t mg_wall_aim_col, mg_wall_aim_row;  // the last strike's aim
uint8_t mg_wall_lit;       // the lit panel (row * 3 + col; NO_PANEL during a flash)
uint8_t mg_wall_hit_panel; // the panel the ball last struck (NO_PANEL: the kickboard or bricks)
uint8_t mg_wall_flash;     // frames left of a hit panel's flash
uint8_t mg_wall_flash_panel;
uint16_t mg_wall_lit_age;  // frames the lit panel has been lit
uint16_t mg_wall_points;
uint8_t mg_wall_mult;      // a lit hit now scores this (+1 a power shot)
uint8_t mg_wall_hits;      // lit panels hit
uint8_t mg_wall_shots;     // balls that reached the wall
uint8_t mg_wall_time;      // seconds left
uint8_t mg_wall_tick;      // frames into this second
uint8_t mg_wall_wait;      // frames to the next feed
uint8_t mg_wall_pop;       // frames left of the "+N"
uint8_t mg_wall_pop_n;
uint8_t mg_wall_miss;      // frames left of "MISS"
uint8_t mg_wall_err;       // px: the spread of a strike (AIM)
uint8_t mg_wall_charge_rate;  // charge a frame (POWER)
uint8_t mg_wall_walk;      // the run cycle
uint8_t mg_wall_shown_time, mg_wall_shown_mult;
uint16_t mg_wall_shown_points;

// The closed shutter panels' tiles (rows 2-5, cols 1-18) and the 6x2 open goal-hole tiles.
static uint8_t panel_tiles[4][18];
static uint8_t open_tiles[2][6];

static const uint16_t pals[6][4] = {
    {RGB8(32, 168, 80), RGB8(20, 136, 64), RGB8(12, 84, 44), RGB8(248, 252, 248)},      // 0: green court
    {RGB8(64, 128, 192), RGB8(40, 84, 144), RGB8(16, 28, 56), RGB8(176, 224, 255)},     // 1: closed steel shutter
    {RGB8(14, 22, 42), RGB8(128, 152, 184), RGB8(8, 10, 20), RGB8(255, 212, 48)},       // 2: OPEN goal hole (net + gold rim)
    {RGB8(24, 216, 104), RGB8(160, 255, 196), RGB8(12, 84, 44), RGB8(255, 255, 255)},   // 3: GOAL! flash
    {RGB8(192, 48, 64), RGB8(248, 192, 48), RGB8(24, 24, 48), RGB8(248, 248, 240)},     // 4: stadium rail
    {RGB8(196, 208, 224), RGB8(120, 136, 160), RGB8(20, 24, 40), RGB8(252, 252, 252)},  // 5: white futsal goalposts & crossbar
};
#define LIT_FACE RGB8(255, 212, 48)
#define LIT_BLINK RGB8(255, 252, 208)

extern uint16_t pal_bg[PAL_COUNT * 4];
extern volatile uint8_t pal_dirty;

static void set_lit_color(uint16_t c) {
    pal_base_bg[PAN_LIT * 4 + 3] = c;
    pal_bg[PAN_LIT * 4 + 3] = c;
    pal_dirty = 1;
}

// --- drawing ---
static uint8_t didx(int16_t d) {  // depth (SUB) -> table index
    d >>= 5;
    if (d < 0) return 0;
    if (d > 63) return 63;
    return (uint8_t)d;
}
static int16_t proj_x(int16_t x, uint8_t i) {  // world x (SUB) -> screen x
    return 80 + (int16_t)(((x >> 4) - 80) * s_tab[i] >> 7);
}

static void set_panel(uint8_t p, uint8_t pal) {
    uint8_t row = (p >= 3 ? 2 : 0), col = (uint8_t)((p >= 3 ? p - 3 : p) * 6);
    uint8_t open = (pal != PAN_UNLIT);
    for (uint8_t r = 0; r < 2; r++) {
        uint8_t *b = gfx_strip_attr(1 + col, 2 + row + r, 0, 6);
        memcpy(b, open ? open_tiles[r] : &panel_tiles[row + r][col], 6);
        memset(b + STRIP_MAX_LEN, pal, 6);
    }
}

static void light_new(void) {
    uint8_t old = mg_wall_lit == NO_PANEL ? mg_wall_flash_panel : mg_wall_lit;
    uint8_t n;
    do n = (uint8_t)rand() & 7; while (n >= 6 || n == old);
    if (mg_wall_lit != NO_PANEL) set_panel(mg_wall_lit, PAN_UNLIT);
    mg_wall_lit = n;
    mg_wall_lit_age = 0;
    set_lit_color(LIT_FACE);
    set_panel(n, PAN_LIT);
}

static void hud_labels(void) {
    fill_win_rect(0, 0, SCREEN_TW, 1, 0);
    win_text(0, 0, "TIME");
    win_text(9, 0, "PTS");
    win_text(17, 0, "X");
    mg_wall_shown_time = mg_wall_shown_mult = 0xFF;
    mg_wall_shown_points = 0xFFFF;
}
static void hud_numbers(void) {
    if (mg_wall_shown_time != mg_wall_time) win_number(5, 0, mg_wall_shown_time = mg_wall_time, 2, 0);
    if (mg_wall_shown_points != mg_wall_points) win_number(13, 0, mg_wall_shown_points = mg_wall_points, 3, 0);
    if (mg_wall_shown_mult != mg_wall_mult) win_number(18, 0, mg_wall_shown_mult = mg_wall_mult, 1, 0);
}

// --- the ball ---
static void feed(void) {
    uint8_t c = (uint8_t)rand() & 3;
    if (c == 3) c = 1;
    mg_wall_bx = col_x[c] << 4;
    mg_wall_bd = 0;
    mg_wall_bz = 24 << 4;
    mg_wall_vd = FEED_VD;
    mg_wall_vx = (int16_t)(((mg_wall_px >> 4) - col_x[c]) * RCP(FEED_T)) >> 7;
    mg_wall_vz = 14;
    mg_wall_ball = BALL_BACK;
    mg_wall_bounces = 0;
    mg_wall_fire = 0;
    sfx_play(SFX_TOUCH);
}

// A random offset in -err .. err px.
static int16_t spread(uint8_t err) {
    int16_t r = (int16_t)((uint8_t)rand() & 31) - 16;
    return (r * err) >> 4;
}

static void strike(void) {
    uint8_t col = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : 1;
    uint8_t row = (joy & J_UP) ? 0 : 1;
    uint8_t lv = stat_lv[STAT_POWER];
    uint8_t kind = pl_shot[SIDE_YOU] ? pl_shot[SIDE_YOU] : SHOT_POWER;
    uint8_t curve = (kind == SHOT_CURVE);
    uint8_t power = mg_wall_charge >= CHARGE_FULL || pl_charge[SIDE_YOU] >= CHARGE_MAX;
    uint8_t t = power ? T_POWER - (lv >> 3) : T_DRIVE - (mg_wall_charge >> 2);
    uint8_t err = mg_wall_err;
    int16_t dx = (mg_wall_bx - mg_wall_px) >> 4;
    if (dx > -SWEET_X && dx < SWEET_X) err = (err * 3) >> 2;  // struck in the middle: truer
    if (power) err += err >> 1;
    int16_t tx = col_x[col] + spread(err);
    int16_t tz = row_z[row] + spread(err >> 1);
    if (t < 16) t = 16;
    mg_wall_aim_col = col;
    mg_wall_aim_row = row;
    fc_ball_plain();
    if (curve) {
        // Curve shot (B): launches wide of the target and bends back into the panel!
        int8_t side = col == 0 ? -1 : col == 2 ? 1 : (mg_wall_px < TO_SUB(80) ? 1 : -1);
        int16_t wide_tx = tx - side * 26;
        mg_wall_vx = (int16_t)((wide_tx - (mg_wall_bx >> 4)) * RCP(t)) >> 7;
        ball_curve = side > 0 ? 12 : -12;
        ball_curve_t = t;
    } else {
        mg_wall_vx = (int16_t)((tx - (mg_wall_bx >> 4)) * RCP(t)) >> 7;
    }
    mg_wall_vd = -((int16_t)((mg_wall_bd >> 4) * RCP(t)) >> 7);
    // z after t frames (vz first, then z): z0 + vz t - GRAV t (t + 1) / 2 = tz
    mg_wall_vz = ((int16_t)((tz - (mg_wall_bz >> 4)) * RCP(t)) >> 7) + t + 1;
    mg_wall_ball = BALL_OUT;
    mg_wall_fire = power;
    ball_power = power;
    ball_trail = curve ? TRAIL_CURVE : TRAIL_POWER;
    trail_len = power ? FC_TRAIL_MAX : mg_wall_charge >= (CHARGE_FULL >> 1) ? 3 : 2;
    trail_n = 0;
    mg_wall_charge = 0;
    mg_wall_charging = 0;
    fc_unready(SIDE_YOU);
    mg_wall_kick = KICK_FRAMES;
    sfx_play(power ? SFX_POWER : SFX_KICK);
}

static void miss(void) {
    mg_wall_ball = BALL_DEAD;
    mg_wall_wait = MISS_WAIT;
    mg_wall_mult = 1;
    mg_wall_miss = 40;
    mg_wall_charging = 0;
    mg_wall_charge = 0;
    fc_unready(SIDE_YOU);
    ball_trail = 0;
    ball_curve_t = 0;
    sfx_play(SFX_PARRY);
}

static void wall_hit(void) {
    int16_t x = mg_wall_bx >> 4, z = mg_wall_bz >> 4;
    uint8_t col = x < COL_X1 ? 0 : x < COL_X2 ? 1 : 2;
    uint8_t p = NO_PANEL;
    if (z >= ROW_Z_MID && z < ROW_Z_TOP) p = col;
    else if (z >= ROW_Z_LOW && z < ROW_Z_MID) p = col + 3;
    mg_wall_hit_panel = p;
    mg_wall_shots++;
    if (p != NO_PANEL && p == mg_wall_lit) {
        // GOAL! The ball rips through the open hole into the net!
        uint8_t n = mg_wall_mult + mg_wall_fire;
        mg_wall_points += n;
        mg_wall_hits++;
        if (mg_wall_mult < MULT_MAX) mg_wall_mult++;
        mg_wall_flash_panel = p;
        mg_wall_flash = FLASH_FRAMES;
        mg_wall_lit = NO_PANEL;
        set_panel(p, PAN_HIT);
        mg_wall_pop = POP_FRAMES;
        mg_wall_pop_n = n;
        mg_wall_ball = BALL_HELD;
        mg_wall_wait = FLASH_FRAMES + 34;  // breather after goal so you see the next open hole before the feed
        ball_trail = 0;
        ball_curve_t = 0;
        sfx_play(SFX_GOAL);
        return;
    }
    // Closed steel panel or post: CLANG! Rebound back onto the pitch.
    mg_wall_mult = 1;
    sfx_play(SFX_BOARD);
    mg_wall_bd = 0;
    mg_wall_vd = (-mg_wall_vd * 3) >> 2;
    mg_wall_vx >>= 2;
    mg_wall_vz = mg_wall_vz < 0 ? (-mg_wall_vz * 3) >> 3 : mg_wall_vz >> 1;
    mg_wall_ball = BALL_BACK;
    mg_wall_bounces = 0;
    ball_trail = 0;
    ball_curve_t = 0;
}

static void ball_move(void) {
    if (mg_wall_ball == BALL_HELD) return;
    if (mg_wall_ball == BALL_OUT && ball_curve_t) {
        ball_vx = mg_wall_vx;
        fc_curve_step();
        mg_wall_vx = ball_vx;
    }
    mg_wall_bx += mg_wall_vx;
    mg_wall_bd += mg_wall_vd;
    mg_wall_vz -= GRAV;
    mg_wall_bz += mg_wall_vz;
    if (mg_wall_bx < (BALL_X_MIN << 4)) {
        mg_wall_bx = BALL_X_MIN << 4;
        mg_wall_vx = -mg_wall_vx;
    } else if (mg_wall_bx > (BALL_X_MAX << 4)) {
        mg_wall_bx = BALL_X_MAX << 4;
        mg_wall_vx = -mg_wall_vx;
    }
    if (mg_wall_bz < 0) {
        mg_wall_bz = 0;
        if (mg_wall_vz < -6) {
            mg_wall_vz = (-mg_wall_vz * 5) >> 3;
            if (mg_wall_ball != BALL_DEAD) sfx_play(SFX_TOUCH);
        } else {
            mg_wall_vz = 0;  // rolling
        }
        if (mg_wall_ball == BALL_BACK && ++mg_wall_bounces >= 2) miss();
        if (mg_wall_ball == BALL_DEAD) {  // the floor slows it
            mg_wall_vx -= mg_wall_vx >> 3;
            mg_wall_vd -= mg_wall_vd >> 3;
        }
    }
    if (mg_wall_ball == BALL_OUT && mg_wall_bd <= 0) wall_hit();
    if (mg_wall_ball == BALL_BACK && mg_wall_bd > ((PL_D_MAX + 16) << 4)) miss();
    if (mg_wall_ball == BALL_DEAD && mg_wall_bd < 0) {  // a dead ball rolls back off the wall
        mg_wall_bd = 0;
        mg_wall_vd = -mg_wall_vd;
    }
}

static uint8_t in_reach(void) {
    if (mg_wall_ball != BALL_BACK || mg_wall_vd < 0) return 0;
    int16_t dx = (mg_wall_bx - mg_wall_px) >> 4, dd = (mg_wall_bd - mg_wall_pd) >> 4;
    return dx >= -REACH_X && dx <= REACH_X && dd >= -REACH_FWD && dd <= REACH_BACK &&
           (mg_wall_bz >> 4) <= REACH_Z;
}

// --- you (shared Futnet movement, charge-up, power/curve strikes) ---
static void you_move(void) {
    pl_x[SIDE_YOU] = mg_wall_px;
    pl_y[SIDE_YOU] = mg_wall_pd;
    if (mg_wall_kick) {
        pl_vx[SIDE_YOU] = pl_vy[SIDE_YOU] = 0;
        return;
    }
    int8_t sp = mg_wall_charging ? (PL_SPEED >> 1) : PL_SPEED;
    int8_t tx = (joy & J_RIGHT) ? sp : (joy & J_LEFT) ? -sp : 0;
    int8_t ty = (joy & J_DOWN) ? sp : (joy & J_UP) ? -sp : 0;
    fc_min_x = PL_X_MIN << 4; fc_max_x = PL_X_MAX << 4;
    fc_min_y = PL_D_MIN << 4; fc_max_y = PL_D_MAX << 4;
    fc_step_move(SIDE_YOU, tx, ty, 1);
    mg_wall_px = pl_x[SIDE_YOU];
    mg_wall_pd = pl_y[SIDE_YOU];
    if (pl_vx[SIDE_YOU] || pl_vy[SIDE_YOU]) mg_wall_walk++;
    else mg_wall_walk = 0;
}

static void you_strike(void) {
    if (mg_wall_kick) {
        mg_wall_kick--;
        return;
    }
    if (joy_pressed & (J_A | J_B)) {
        uint8_t b = (joy_pressed & J_B) != 0;
        fc_ready(SIDE_YOU, b, 0);
        if (in_reach()) {
            strike();
            return;
        }
        mg_wall_charging = 1;
        // Tap boosts charge too (like Futnet), while holding keeps filling smoothly!
        uint8_t tap_c = pl_charge[SIDE_YOU] << 1;
        if (tap_c > mg_wall_charge) mg_wall_charge = tap_c > CHARGE_FULL ? CHARGE_FULL : tap_c;
    }
    if (!mg_wall_charging) return;
    if (!(joy & (J_A | J_B))) {  // let go before it came: no strike
        mg_wall_charging = 0;
        mg_wall_charge = 0;
        fc_unready(SIDE_YOU);
        return;
    }
    if (mg_wall_charge < CHARGE_FULL) {
        uint8_t c = mg_wall_charge + mg_wall_charge_rate;
        if (c >= CHARGE_FULL) {
            c = CHARGE_FULL;
            sfx_play(SFX_CHARGE);
        }
        mg_wall_charge = c;
    }
    // Keep Futnet's pl_charge synced (0..32) so fc_charge_pal flashes white -> fire!
    pl_charge[SIDE_YOU] = mg_wall_charge >> 1;
    if (in_reach()) strike();
}

// --- the panels ---
static void panels_update(void) {
    if (mg_wall_flash) {
        if (--mg_wall_flash == 0) {
            set_panel(mg_wall_flash_panel, PAN_UNLIT);
            light_new();
        }
        return;
    }
    if (mg_wall_lit == NO_PANEL) return;
    mg_wall_lit_age++;
    // Late on, a lit panel left too long moves on (not with a shot on its way).
    uint16_t limit = 0xFFFF;
    if (mg_wall_time <= LATE_SECONDS) limit = 120 + ((uint16_t)mg_wall_time << 2);
    if (mg_wall_lit_age >= limit && mg_wall_ball != BALL_OUT) {
        light_new();
        sfx_play(SFX_TOUCH);
    }
    // Blink: steady, faster for its last second.
    uint8_t rate = (limit != 0xFFFF && mg_wall_lit_age + 60 >= limit) ? 4 : 16;
    if (((uint8_t)mg_wall_lit_age & (rate - 1)) == 0) {
        set_lit_color(((uint8_t)mg_wall_lit_age & rate) ? LIT_BLINK : LIT_FACE);
    }
}

// --- drawing ---
static void draw(void) {
    uint8_t i = didx(mg_wall_pd);
    int16_t sx = proj_x(mg_wall_px, i), feet = gy_tab[i];
    uint8_t frame = MG_F_STAND, flip = pl_face[SIDE_YOU];
    if (mg_wall_kick) {
        int16_t bz = mg_wall_bz >> 4;
        frame = bz > 18 ? MG_F_HEADER : bz > 6 ? MG_F_VOLLEY : MG_F_KICK;
        flip = mg_wall_bx < mg_wall_px;
    } else if (mg_wall_charging) {
        frame = MG_F_READY;
    } else if (mg_wall_walk) {
        frame = (mg_wall_walk & 8) ? MG_F_RUN_B : MG_F_RUN_A;
    }
    uint8_t tile = MG_FRAME(MG_VIEW_BACK, frame);
    pl_side[SIDE_YOU] = gfx_running_side(pl_side[SIDE_YOU], pl_vx[SIDE_YOU], pl_vy[SIDE_YOU]);
    if (!mg_wall_kick && !mg_wall_charging && pl_side[SIDE_YOU]) {
        uint8_t pose = (pl_vx[SIDE_YOU] || pl_vy[SIDE_YOU]) ? ((frame_count & 4) ? 0 : (frame_count & 8) ? 2 : 1) : 0;
        tile = 224 + (pose << 3);
        if (pl_vx[SIDE_YOU]) flip = pl_vx[SIDE_YOU] < 0;
    }
    uint8_t pal = fc_charge_pal(SIDE_YOU, SPR_PAL_YOU);
    mg_player(0, tile, sx, feet, pal, flip);

    if (mg_wall_ball == BALL_HELD) {
        mg_hide(OAM_MG_BALL, OAM_MG_SHADOW);
        mg_hide(OAM_TRAIL, OAM_TRAIL + 3);
    } else {
        uint8_t j = didx(mg_wall_bd);
        int16_t bx = proj_x(mg_wall_bx, j);
        int16_t z = (int16_t)((mg_wall_bz >> 4) * s_tab[j]) >> 7;
        mg_ball(bx, gy_tab[j], z, mg_wall_ball == BALL_DEAD || mg_wall_bz == 0,
                mg_wall_fire ? SPR_PAL_POWER : SPR_PAL_BALL);
        fc_trail_draw(OAM_TRAIL, bx + 4, gy_tab[j] + 8 - z, 0);
    }

    // The aim marker on the panel you'd strike at, while charging.
    if (mg_wall_charging) {
        uint8_t col = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : 1;
        uint8_t row = (joy & J_UP) ? 0 : 1;
        mg_sprite(OAM_AIM, MG_T_AIM, SPR_PAL_UI, col_sx[col] - 4, row_sy[row] - 4);
    } else {
        mg_hide(OAM_AIM, OAM_AIM);
    }

    // "+N" rising off the panel just hit.
    if (mg_wall_pop) {
        uint8_t p = mg_wall_flash_panel;
        int16_t x = col_sx[p >= 3 ? p - 3 : p], y = row_sy[p >= 3 ? 1 : 0] - 12 - ((POP_FRAMES - mg_wall_pop) >> 2);
        mg_wall_pop--;
        mg_sprite(OAM_POP, MG_SPR_TILES + WT_PLUS, SPR_PAL_UI, x - 7, y);
        mg_sprite(OAM_POP + 1, MG_SPR_TILES + WT_DIGIT + (mg_wall_pop_n << 1), SPR_PAL_UI, x, y);
    } else {
        mg_hide(OAM_POP, OAM_POP + 1);
    }

    // "MISS" over you.
    if (mg_wall_miss) {
        int16_t y = feet - 36;
        mg_wall_miss--;
        mg_sprite(OAM_MISS, MG_SPR_TILES + WT_M, SPR_PAL_TRAIL_POWER, sx - 14, y);
        mg_sprite(OAM_MISS + 1, MG_SPR_TILES + WT_I, SPR_PAL_TRAIL_POWER, sx - 7, y);
        mg_sprite(OAM_MISS + 2, MG_SPR_TILES + WT_S, SPR_PAL_TRAIL_POWER, sx - 1, y);
        mg_sprite(OAM_MISS + 3, MG_SPR_TILES + WT_S, SPR_PAL_TRAIL_POWER, sx + 6, y);
    } else {
        mg_hide(OAM_MISS, OAM_MISS + 3);
    }
}

// "TARGETS 9 OF 21" for the results.
static uint8_t put_num(uint8_t at, uint8_t n) {
    if (n >= 100) mg_detail[at++] = '0' + n / 100, n %= 100, mg_detail[at++] = '0' + n / 10;
    else if (n >= 10) mg_detail[at++] = '0' + n / 10;
    mg_detail[at++] = '0' + n % 10;
    return at;
}
static void finish(void) {
    uint8_t at = 0;
    const char *s = "TARGETS ";
    while (*s) mg_detail[at++] = *s++;
    at = put_num(at, mg_wall_hits);
    s = " OF ";
    while (*s) mg_detail[at++] = *s++;
    at = put_num(at, mg_wall_shots);
    mg_detail[at] = 0;
    mg_finish(mg_wall_points);
}

void mg_wall_enter(void) BANKED {
    memcpy(pal_base_bg, pals, sizeof(pals));
    // Load only rows 0..17 onto the screen; rows 18..19 hold the open-hole template!
    mg_load_bg(mg_wall_bg_tiles, MG_WALL_BG_TILE_COUNT, mg_wall_bg_map, mg_wall_bg_attr,
               MG_WALL_BG_MAP_W, 18, BANK(mg_wall_bg));
    mg_load_spr(mg_wall_spr_tiles, MG_WALL_SPR_TILE_COUNT, BANK(mg_wall_spr));
    for (uint8_t r = 0; r < 4; r++)
        banked_memcpy(panel_tiles[r], mg_wall_bg_map + (2 + r) * MG_WALL_BG_MAP_W + 1, 18, BANK(mg_wall_bg));
    for (uint8_t r = 0; r < 2; r++)
        banked_memcpy(open_tiles[r], mg_wall_bg_map + (18 + r) * MG_WALL_BG_MAP_W, 6, BANK(mg_wall_bg));

    mg_wall_px = 80 << 4;
    mg_wall_pd = 88 << 4;
    mg_wall_ball = BALL_HELD;
    mg_wall_bx = 80 << 4;
    mg_wall_bd = mg_wall_bz = 0;
    mg_wall_vx = mg_wall_vd = mg_wall_vz = 0;
    mg_wall_bounces = mg_wall_fire = 0;
    mg_wall_charge = mg_wall_charging = mg_wall_kick = mg_wall_walk = 0;
    fc_unready(SIDE_YOU);
    fc_ball_plain();
    pl_vx[SIDE_YOU] = pl_vy[SIDE_YOU] = 0;
    pl_accel[SIDE_YOU] = FC_ACCEL;
    pl_grip[SIDE_YOU] = FC_GRIP;
    pl_skid[SIDE_YOU] = pl_side[SIDE_YOU] = pl_flash[SIDE_YOU] = 0;
    pl_face[SIDE_YOU] = 0;
    mg_wall_aim_col = 1;
    mg_wall_aim_row = 1;
    mg_wall_points = 0;
    mg_wall_mult = 1;
    mg_wall_hits = mg_wall_shots = 0;
    mg_wall_time = GAME_SECONDS;
    mg_wall_tick = 0;
    mg_wall_wait = 0;
    mg_wall_pop = mg_wall_miss = mg_wall_flash = 0;
    mg_wall_flash_panel = 1;
    mg_wall_hit_panel = NO_PANEL;
    // AIM: the spread, 29 px either way at level 1 down to 10 at 20 (a panel is 64 wide).
    mg_wall_err = 30 - stat_lv[STAT_AIM];
    // POWER: a full charge in 32 frames at level 1, 9 at 20.
    mg_wall_charge_rate = 2 + (stat_lv[STAT_POWER] >> 2);
    // The first open goal hole: middle of the bottom row (display is off).
    mg_wall_lit = 4;
    mg_wall_lit_age = 0;
    {
        uint8_t a[6];
        memset(a, PAN_LIT, 6);
        set_bkg_tiles(7, 4, 6, 1, open_tiles[0]);
        set_bkg_tiles(7, 5, 6, 1, open_tiles[1]);
        VBK_REG = 1;
        set_bkg_tiles(7, 4, 6, 1, a);
        set_bkg_tiles(7, 5, 6, 1, a);
        VBK_REG = 0;
    }
    mg_hide(OAM_MG_FREE, OAM_MG_COUNT - 1);
    draw();
}

void mg_wall_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw();
        return;
    }
    if (mg_run != MG_RUN_LIVE) return;
    if (mg_hud_dirty) {
        mg_hud_dirty = 0;
        hud_labels();
    }
    if (mg_wall_ball == BALL_HELD) {
        feed();
    } else if (mg_wall_ball == BALL_DEAD && --mg_wall_wait == 0) {
        feed();
    }
    you_move();
    you_strike();
    ball_move();
    panels_update();
    draw();
    if (++mg_wall_tick >= 60) {
        mg_wall_tick = 0;
        if (--mg_wall_time == 0) {
            hud_numbers();
            finish();
            return;
        }
        if (mg_wall_time <= 5) sfx_play(SFX_TOUCH);
    }
    hud_numbers();
}
