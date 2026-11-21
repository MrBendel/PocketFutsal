#pragma bank 255
// Play: a 1-on-1 futsal match against the CPU, Mario Tennis style. You play the
// near half of a court seen in perspective, the CPU the far one; each defends the
// goal behind them, and a goal is a point: first to WIN_POINTS, by two.
//
// The ball is always loose: you strike it rather than carry it. Press A (a
// drive) or B (a chip, a lob) before the ball reaches you: the strike charges
// for as long as it takes to arrive, and fires by itself the moment the ball is in
// reach. Which strike depends on the ball's height: a kick on the ground, a volley
// at the waist, a header, or a bicycle kick for one that's just gone over your
// head. While charging you only shuffle. A full meter on a drive is a power shot,
// on fire: met with less than half a meter, it knocks the player down and goes on.
// LEFT or RIGHT as the strike goes aims it at that corner. A or B with the ball
// just out of reach, coming past, is a dive. Without a strike ready, the ball
// bounces off you.
//
// Each point starts with a serve, by the side that conceded (you first): A or B
// tosses the ball up, and it's struck on the way down. START pauses.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "pitch.h"
#include "hud.h"
#include "sprites.h"
#include "players.h"

// Sprite tiles (tools/tile-art.mjs). VRAM bank 0: the ball and its shadow, an
// 8x16 pair each. VRAM bank 1: the players, 8 tiles a frame.
#define S_BALL 0  // and S_BALL + 2, its other roll
#define S_SHADOW 4
#define F_STAND 0
#define F_RUN_A 1
#define F_RUN_B 2
#define F_READY 3
#define F_KICK 4
#define F_VOLLEY 5
#define F_HEADER 6
#define F_BICYCLE 7
#define F_WIN 8
#define F_LOSE 9
#define VIEW_FRONT 80  // the front view's tiles (the CPU, facing you)
#define T_DIVE 160     // lying: three 8x16 columns
// Background tiles: the court from SCENE_TILES, then the meter's three.
#define T_METER (SCENE_TILES + PITCH_TILE_COUNT)  // empty, half, full

// Hardware sprites. Lower numbers draw on top: the ball over everyone, the
// shadow under everyone.
#define OAM_BALL 0
#define OAM_PLAYER 1  // 4 each, by side
#define OAM_SHADOW 9

// --- tuning ---
#define SPEED_YOU 22      // a frame, in SUB
#define SPEED_CPU 18
#define SPEED_CHARGE 8    // shuffling while a strike charges
#define HOLD_MAX 90       // frames at a full meter before the player gives up the strike
#define REACH_X 12        // court px either side a strike reaches
#define REACH_FWD 8       // in front
#define REACH_BACK 4      // behind (a bicycle kick reaches further)
#define DRIVE_SPEED 40    // + the charge
#define POWER_SPEED 96
#define CHIP_SPEED 16     // + three quarters of the charge
#define CHIP_LIFT 56       // a lob: about 32 px up
#define DRIVE_LIFT 24      // + half the charge: a drive's low arc (6-16 px up), so it
                           // comes at the waist or bounces in
#define HEADER_SPEED 32   // + half the charge
#define GRAVITY 3
#define POWER_FADE 40     // a power shot burns out below this speed (|vx| + |vy|)
#define KICK_FRAMES 14    // a strike's pose; no touches meanwhile
#define BICYCLE_FRAMES 24
#define DIVE_FRAMES 44    // a dive: the lunge, then on the floor
#define DIVE_LUNGE 10
#define DIVE_SPEED 40
#define DOWN_FRAMES 40    // knocked down by a power shot
#define TOSS_LIFT 54      // the serve's toss: about 30 px up
#define TOSS_STRIKE 20    // px: struck when it falls back to this
#define SERVE_FRAMES 50
#define GOAL_FRAMES 110
#define BALL_R 3          // px: the ball's radius on the court
#define HOME_CPU 24       // the CPU's depth, waiting
#define NET_BACK 10       // px: the back of a goal's net behind its line

uint16_t game_seed;
uint16_t test_seed;

uint8_t score[2];
uint8_t winner;
uint8_t last_scorer;
uint8_t server;
uint8_t phase;
uint8_t serve_timer;
uint8_t goal_timer;
uint8_t paused;
uint8_t cam_y;

int16_t pl_x[2], pl_y[2];
int8_t pl_vx[2], pl_vy[2];
uint8_t pl_face[2];
uint8_t pl_shot[2];
uint8_t pl_charge[2];
uint8_t pl_hold[2];
uint8_t pl_move[2];
uint8_t pl_move_timer[2];
uint8_t pl_shots[2];
uint8_t pl_last_move[2];
int16_t cpu_tx, cpu_ty;
uint8_t cpu_aim;
uint8_t cpu_idle;  // non-zero: the CPU's player stands still (tests, the playtest page)

int16_t ball_x, ball_y, ball_z;
int16_t ball_vx, ball_vy, ball_vz;
uint8_t ball_power;
uint8_t last_touch;
uint8_t bounces;
uint8_t ball_roll;  // counts up as the ball rolls: its two frames

uint8_t meter_shown;  // the meter's value on screen (0xFF: redraw)
uint8_t meter_buf[8];  // its tiles, queued to the window's map
uint8_t note_timer;    // the HUD shows a note (GOAL!) while this counts down

// The perspective, by depth from -16 to 239 (index depth + 16): the map row, and
// the width scale /128. The same formula as tools/tile-art.mjs, worked out once.
#define PROJ_OFS 16
uint8_t proj_y[256];
uint8_t proj_s[256];

// 32 directions, clockwise from right (y grows toward you): cos * 64. The sine
// is the cosine 8 steps back.
static const int8_t cos_t[32] = {
    64, 63, 59, 53, 45, 36, 24, 12, 0, -12, -24, -36, -45, -53, -59, -63,
    -64, -63, -59, -53, -45, -36, -24, -12, 0, 12, 24, 36, 45, 53, 59, 63,
};
#define SIN(d) cos_t[((d) + 24) & 31]
// tan of 5.625, 16.875, 28.125 and 39.375 degrees, * 64: the boundaries between
// the directions of the first octant.
static const uint8_t tan_t[4] = {6, 19, 34, 53};

static uint8_t octant(uint16_t a, uint16_t b) {  // b <= a: steps 0..4 from the a axis
    uint8_t k = 0;
    uint16_t b64 = b << 6;
    while (k < 4 && a * tan_t[k] < b64) k++;
    return k;
}

// The direction (0-31) from a vector, in px. Only when a strike is struck.
static uint8_t dir_to(int16_t dx, int16_t dy) {
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t k = ay <= ax ? octant(ax, ay) : 8 - octant(ay, ax);
    if (dx >= 0) return dy >= 0 ? k : (32 - k) & 31;
    return dy >= 0 ? 16 - k : 16 + k;
}

static int16_t abs16(int16_t v) {
    return v < 0 ? -v : v;
}

// Slow a rolling ball: a sixteenth of a unit every other frame (a steady
// slowing), so a good drive crosses the court and a slow ball stops.
static int16_t drag(int16_t v) {
    if (frame_count & 1) return v;
    if (v > 0) v--;
    else if (v < 0) v++;
    return v;
}

// floor(a / b), b > 0, for the tables (once, at the start).
static int32_t floordiv(int32_t a, int32_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

static void proj_init(void) {
    for (int16_t i = 0; i < 256; i++) {
        int32_t fy = i - PROJ_OFS;
        int32_t row = COURT_SY + floordiv((int32_t)PROJ_K0 * 448 * fy + (int32_t)(PROJ_K1 - PROJ_K0) * fy * fy, 114688L)  /* 448 * 256 */;
        proj_y[i] = (uint8_t)row;
        proj_s[i] = (uint8_t)(PROJ_S0 + floordiv((int32_t)(128 - PROJ_S0) * fy, COURT_NEAR));
    }
}

#define PROJ_I(fy) ((uint8_t)((fy) + PROJ_OFS))
// A court point -> screen x (px).
static int16_t proj_x(int16_t fx, int16_t fy) {
    return CENTER_X + (((fx - CENTER_X) * (int16_t)proj_s[PROJ_I(fy)]) >> 7);
}

// --- the HUD: YOU 0 [meter] 0 CPU, or a note for a moment ---
static void hud_draw(void) {
    fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
    if (paused) {
        win_text(7, 0, "PAUSED");
        return;
    }
    if (note_timer) {
        if (goal_timer) win_text(2, 0, last_scorer == SIDE_YOU ? "GOAL! YOU SCORE" : "GOAL! CPU SCORES");
        else win_text(5, 0, server == SIDE_YOU ? "YOU SERVE" : "CPU SERVES");
        return;
    }
    win_text(0, 0, "YOU");
    win_number(4, 0, score[SIDE_YOU], 1, 0);
    win_number(15, 0, score[SIDE_CPU], 1, 0);
    win_text(17, 0, "CPU");
    meter_shown = 0xFF;  // the meter's tiles were cleared: draw them again
}

static void note(uint8_t frames) {
    note_timer = frames;
    hud_draw();
}

// The meter: 8 tiles, 2 steps each, for your charge. Queued to the window's map
// when it changes.
static void meter_update(void) {
    if (paused || note_timer) return;
    uint8_t m = pl_charge[SIDE_YOU] >> 1;  // 0 .. 16
    if (m == meter_shown) return;
    meter_shown = m;
    for (uint8_t k = 0; k < 8; k++) {
        uint8_t full = (k << 1) + 2;
        meter_buf[k] = T_METER + (m >= full ? 2 : m + 1 == full ? 1 : 0);
    }
    gfx_vram_copy(get_win_xy_addr(6, 0), meter_buf, 8, 0);
}

// --- setting up ---
static void serve_setup(uint8_t side) {
    server = side;
    phase = PHASE_SERVE;
    for (uint8_t i = 0; i < 2; i++) {
        pl_vx[i] = pl_vy[i] = 0;
        pl_shot[i] = SHOT_NONE;
        pl_charge[i] = pl_hold[i] = pl_move[i] = pl_move_timer[i] = 0;
    }
    pl_x[SIDE_YOU] = TO_SUB(CENTER_X);
    pl_x[SIDE_CPU] = TO_SUB(CENTER_X);
    if (side == SIDE_YOU) {
        pl_y[SIDE_YOU] = TO_SUB(COURT_NEAR - 16);
        pl_y[SIDE_CPU] = TO_SUB(HOME_CPU);
    } else {
        pl_y[SIDE_YOU] = TO_SUB(COURT_NEAR - 24);
        pl_y[SIDE_CPU] = TO_SUB(16);
        pl_x[SIDE_CPU] = TO_SUB(56 + (rand() & 31) + (rand() & 15));
    }
    pl_face[SIDE_YOU] = 0;
    pl_face[SIDE_CPU] = 1;
    ball_z = ball_vx = ball_vy = ball_vz = 0;
    ball_power = 0;
    last_touch = side;
    serve_timer = SERVE_FRAMES;
    note(SERVE_FRAMES);
}

static void load_pitch(void) {
    uint8_t buf[PITCH_MAP_W * PITCH_MAP_H];
    banked_memcpy(buf, pitch_map, sizeof(buf), BANK(pitch));
    set_bkg_tiles(0, 0, PITCH_MAP_W, PITCH_MAP_H, buf);
    banked_memcpy(buf, pitch_attr, sizeof(buf), BANK(pitch));
    VBK_REG = 1;
    set_bkg_tiles(0, 0, PITCH_MAP_W, PITCH_MAP_H, buf);
    VBK_REG = 0;
}

// --- drawing ---
// Hardware coordinates are 8 and 16 px off the screen's; anything off the
// playfield is parked at y 0 (hidden).
static void place(uint8_t n, int16_t x, int16_t y) {
    if (y < 0 || y > 160 || x < 0 || x > 168) {
        move_sprite(n, 0, 0);
        return;
    }
    move_sprite(n, (uint8_t)x, (uint8_t)y);
}

// A player: 16 x 24 standing (two columns, each an 8x16 sprite over the top
// half of another), or 24 x 16 lying (three 8x16 columns). (sx, sy): the screen
// point under its feet, or under the middle of it lying.
static void draw_player(uint8_t i, uint8_t tile, uint8_t lie, int16_t sx, int16_t sy, uint8_t pal, uint8_t flip) {
    uint8_t n = OAM_PLAYER + (i << 2);
    pal |= S_BANK;
    if (flip) pal |= S_FLIPX;
    if (lie) {
        // Columns at sx-12, -4, +4; flipped, their order reverses.
        for (uint8_t c = 0; c < 3; c++) {
            set_sprite_tile(n + c, tile + ((flip ? 2 - c : c) << 1));
            set_sprite_prop(n + c, pal);
            place(n + c, sx - 4 + (c << 3), sy + 1);
        }
        move_sprite(n + 3, 0, 0);
        return;
    }
    uint8_t l = flip ? tile + 4 : tile, r = flip ? tile : tile + 4;
    set_sprite_tile(n, l);
    set_sprite_tile(n + 1, l + 2);
    set_sprite_tile(n + 2, r);
    set_sprite_tile(n + 3, r + 2);
    for (uint8_t k = 0; k < 4; k++) set_sprite_prop(n + k, pal);
    place(n, sx, sy - 7);
    place(n + 1, sx, sy + 9);
    place(n + 2, sx + 8, sy - 7);
    place(n + 3, sx + 8, sy + 9);
}

static void draw(void) {
    // The camera: after the ball, a pixel or two a frame.
    int16_t by = PX(ball_y), bz = PX(ball_z);
    int16_t t = (int16_t)proj_y[PROJ_I(by)] - (bz >> 1) - 72;
    if (t < 0) t = 0;
    if (t > CAM_MAX) t = CAM_MAX;
    if (cam_y + 1 < t) cam_y += 2;
    else if (cam_y < t) cam_y++;
    else if (cam_y > t + 1) cam_y -= 2;
    else if (cam_y > t) cam_y--;
    scroll_y = cam_y;

    for (uint8_t i = 0; i < 2; i++) {
        uint8_t f = F_STAND, lie = 0, flip = pl_face[i];
        int8_t lift = 0;
        uint8_t m = pl_move_timer[i] ? pl_move[i] : MOVE_NONE;
        if (goal_timer) f = last_scorer == i ? F_WIN : F_LOSE;
        else if (m == MOVE_KICK) f = F_KICK;
        else if (m == MOVE_VOLLEY) f = F_VOLLEY;
        else if (m == MOVE_HEADER) f = F_HEADER, lift = 4;
        else if (m == MOVE_BICYCLE) f = F_BICYCLE, lift = 8;
        else if (m == MOVE_DIVE || m == MOVE_DOWN) {
            lie = 1;
            if (m == MOVE_DIVE && pl_move_timer[i] > DIVE_FRAMES - DIVE_LUNGE) lift = 4;
        } else if (pl_shot[i] || (phase == PHASE_TOSS && server == i)) f = F_READY;
        else if (pl_vx[i] || pl_vy[i]) f = (frame_count & 8) ? F_RUN_A : F_RUN_B;
        uint8_t tile = lie ? T_DIVE : (f << 3) + (i == SIDE_CPU ? VIEW_FRONT : 0);
        uint8_t pal = i == SIDE_YOU ? SPR_PAL_YOU : SPR_PAL_CPU;
        // A full meter: flashing, the power shot is ready.
        if (pl_charge[i] == CHARGE_MAX && (frame_count & 4)) pal = SPR_PAL_POWER;
        int16_t py = PX(pl_y[i]);
        int16_t sx = proj_x(PX(pl_x[i]), py);
        int16_t sy = (int16_t)proj_y[PROJ_I(py)] - cam_y - lift;
        draw_player(i, tile, lie, sx, sy, pal, flip);
    }
    // The ball, up at its height, and its shadow on the court.
    int16_t sx = proj_x(PX(ball_x), by) + 4;
    int16_t sy = (int16_t)proj_y[PROJ_I(by)] - cam_y + 10;
    set_sprite_tile(OAM_BALL, S_BALL + ((ball_roll & 8) ? 2 : 0));
    set_sprite_prop(OAM_BALL, ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
    place(OAM_BALL, sx, sy - bz);
    set_sprite_tile(OAM_SHADOW, S_SHADOW);
    set_sprite_prop(OAM_SHADOW, SPR_PAL_UI);
    place(OAM_SHADOW, sx, sy - 1);
}

// --- strikes ---
static void strike(uint8_t i, uint8_t move) {
    // Toward the far goal: its middle, or a corner if LEFT or RIGHT is held (the
    // CPU picks its corner as it readies the strike).
    int16_t ty = i == SIDE_YOU ? COURT_FAR - 8 : COURT_NEAR + 8;
    uint8_t aim = 1;
    if (i == SIDE_YOU) {
        if (joy & J_LEFT) aim = 0;
        else if (joy & J_RIGHT) aim = 2;
    } else aim = cpu_aim;
    int16_t tx = aim == 0 ? GOAL_X0 + 12 : aim == 2 ? GOAL_X1 - 12 : CENTER_X;
    int16_t bx = PX(ball_x);
    uint8_t d = dir_to(tx - bx, ty - PX(ball_y));
    uint8_t c = pl_charge[i], speed;
    uint8_t chip = pl_shot[i] == SHOT_CHIP;
    uint8_t frames = KICK_FRAMES;
    ball_power = 0;
    if (phase != PHASE_RALLY && c == CHARGE_MAX) c--;  // no power serves
    if (move == MOVE_DIVE) {
        // At full stretch: a pop up, whatever was readied.
        speed = 28;
        ball_vz = 24;
        sfx_play(SFX_TOUCH);
    } else if (move == MOVE_HEADER) {
        speed = HEADER_SPEED + (c >> 1);
        ball_vz = -8;
        sfx_play(SFX_KICK);
        frames = KICK_FRAMES + 4;
    } else if (move == MOVE_BICYCLE) {
        speed = DRIVE_SPEED + 16 + (c >> 1);
        ball_vz = 8;
        if (c >= CHARGE_MAX / 2) {
            ball_power = 1;
            speed = POWER_SPEED;
        }
        sfx_play(ball_power ? SFX_POWER : SFX_KICK);
        frames = BICYCLE_FRAMES;
    } else if (chip) {
        speed = CHIP_SPEED + (c >> 1) + (c >> 2);
        if (i == SIDE_CPU) {
            // The CPU's lob drops just behind you: about 37 frames in the air, so
            // 7/16 of the distance a frame.
            int16_t dist = PX(pl_y[SIDE_YOU]) + 12 - PX(ball_y);
            speed = dist < 37 ? 16 : dist > 160 ? 72 : (uint8_t)((dist * 7) >> 4);
        }
        ball_vz = move == MOVE_VOLLEY ? CHIP_LIFT - 8 : CHIP_LIFT;
        sfx_play(SFX_KICK);
    } else if (c == CHARGE_MAX) {
        speed = POWER_SPEED;
        ball_vz = move == MOVE_VOLLEY ? 4 : 0;
        ball_power = 1;
        sfx_play(SFX_POWER);
    } else {
        speed = DRIVE_SPEED + c + (move == MOVE_VOLLEY ? 8 : 0);
        ball_vz = move == MOVE_VOLLEY ? 8 : DRIVE_LIFT + (c >> 1);
        sfx_play(SFX_KICK);
    }
    ball_vx = ((int16_t)cos_t[d] * speed) >> 6;
    ball_vy = ((int16_t)SIN(d) * speed) >> 6;
    last_touch = i;
    phase = PHASE_RALLY;
    if (move != MOVE_DIVE) {
        pl_face[i] = bx < PX(pl_x[i]);
        pl_move[i] = move;
        pl_move_timer[i] = frames;
    }
    pl_last_move[i] = move;
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = 0;
    pl_shots[i]++;
}

// The ball relative to player i: across, forward (toward the other half:
// positive in front of them), and up.
static int16_t rel_x, rel_fwd, rel_z;
static void relate(uint8_t i) {
    rel_x = PX(ball_x) - PX(pl_x[i]);
    rel_fwd = PX(pl_y[i]) - PX(ball_y);
    if (i == SIDE_CPU) rel_fwd = -rel_fwd;
    rel_z = PX(ball_z);
}

// Coming this way (toward player i's goal).
static uint8_t coming(uint8_t i) {
    return i == SIDE_YOU ? ball_vy > 0 : ball_vy < 0;
}

// Just out of reach and coming past, low enough: a dive would get there.
static uint8_t dive_range(uint8_t i) {
    relate(i);
    int16_t ax = abs16(rel_x);
    return phase == PHASE_RALLY && coming(i) && ax > REACH_X && ax <= REACH_X + 22 && rel_fwd >= -4 &&
           rel_fwd <= 16 && rel_z < 16;
}

static void dive(uint8_t i) {
    relate(i);
    pl_face[i] = rel_x < 0;
    pl_move[i] = MOVE_DIVE;
    pl_move_timer[i] = DIVE_FRAMES;
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = 0;
    sfx_play(SFX_CATCH);
}

static void knock_down(uint8_t i) {
    // Too hot to handle: down they go, and the ball goes on, slowed.
    pl_move[i] = MOVE_DOWN;
    pl_move_timer[i] = DOWN_FRAMES;
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = 0;
    ball_power = 0;
    ball_vx >>= 1;
    ball_vy >>= 1;
    last_touch = i;
    sfx_play(SFX_PARRY);
}

// A player's frame: move (dx, dy: -1, 0 or 1), charge, then strike or block.
static void player_update(uint8_t i, int8_t dx, int8_t dy) {
    pl_vx[i] = pl_vy[i] = 0;
    if (pl_move_timer[i]) {
        uint8_t m = pl_move[i];
        pl_move_timer[i]--;
        if (m != MOVE_DIVE || pl_move_timer[i] < DIVE_FRAMES - DIVE_LUNGE) return;
        // The lunge: across, reaching for it.
        pl_x[i] += pl_face[i] ? -DIVE_SPEED : DIVE_SPEED;
        if (pl_x[i] < TO_SUB(COURT_X0)) pl_x[i] = TO_SUB(COURT_X0);
        if (pl_x[i] > TO_SUB(COURT_X1)) pl_x[i] = TO_SUB(COURT_X1);
        relate(i);
        if (abs16(rel_x) <= REACH_X + 2 && rel_fwd >= -REACH_BACK - 2 && rel_fwd <= REACH_FWD && rel_z < 16) {
            strike(i, MOVE_DIVE);
            pl_move_timer[i] = DIVE_FRAMES - DIVE_LUNGE;  // got it: that's the lunge over
        }
        return;
    }
    // Serving: along the line only, and still once it's tossed.
    if (phase != PHASE_RALLY && server == i) {
        dy = 0;
        if (phase == PHASE_TOSS) dx = 0;
    }
    int8_t speed = pl_shot[i] ? SPEED_CHARGE : i == SIDE_YOU ? SPEED_YOU : SPEED_CPU;
    int8_t vx = dx > 0 ? speed : dx < 0 ? -speed : 0;
    int8_t vy = dy > 0 ? speed : dy < 0 ? -speed : 0;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    int16_t x = pl_x[i] + vx, y = pl_y[i] + vy;
    if (x < TO_SUB(COURT_X0 + 4)) x = TO_SUB(COURT_X0 + 4);
    if (x > TO_SUB(COURT_X1 - 4)) x = TO_SUB(COURT_X1 - 4);
    // Each in their own half.
    int16_t y0 = i == SIDE_YOU ? TO_SUB(HALFWAY + 6) : TO_SUB(COURT_FAR + 4);
    int16_t y1 = i == SIDE_YOU ? TO_SUB(COURT_NEAR - 4) : TO_SUB(HALFWAY - 6);
    if (y < y0) y = y0;
    if (y > y1) y = y1;
    pl_x[i] = x;
    pl_y[i] = y;
    if (vx) pl_face[i] = vx < 0;

    if (pl_shot[i]) {
        if (pl_charge[i] < CHARGE_MAX) {
            if (++pl_charge[i] == CHARGE_MAX && i == SIDE_YOU) sfx_play(SFX_CHARGE);
        } else if (phase == PHASE_RALLY && ++pl_hold[i] > HOLD_MAX) {
            pl_shot[i] = SHOT_NONE;  // waited too long: the strike is given up
            pl_charge[i] = pl_hold[i] = 0;
        }
    }
    if (goal_timer) return;
    if (phase == PHASE_SERVE) return;
    if (phase == PHASE_TOSS) {
        // The serve: struck as the toss falls back.
        if (server == i && ball_vz < 0 && ball_z < TO_SUB(TOSS_STRIKE)) strike(i, MOVE_KICK);
        return;
    }
    relate(i);
    int16_t ax = abs16(rel_x);
    if (pl_shot[i]) {
        // In reach: which strike is up to the ball's height.
        uint8_t move = MOVE_NONE;
        if (ax <= REACH_X && rel_fwd >= -REACH_BACK && rel_fwd <= REACH_FWD) {
            if (rel_z < 5) move = MOVE_KICK;
            else if (rel_z < 16) move = MOVE_VOLLEY;
            else if (rel_z < 28) move = MOVE_HEADER;
        } else if (ax <= REACH_X - 2 && rel_fwd < -2 && rel_fwd >= -14 && rel_z >= 10 && rel_z < 32) {
            move = MOVE_BICYCLE;  // just over your head: over it goes, backwards
        }
        if (move) {
            if (ball_power && pl_charge[i] < CHARGE_MAX / 2) knock_down(i);
            else strike(i, move);
        }
        return;
    }
    // No strike ready: it bounces off you (a power shot knocks you down).
    if (ax <= 6 && rel_fwd >= -3 && rel_fwd <= 4 && rel_z < 20 && coming(i)) {
        if (ball_power) {
            knock_down(i);
            return;
        }
        ball_vy = -(ball_vy >> 1);
        ball_vx >>= 1;
        last_touch = i;
        sfx_play(SFX_TOUCH);
        return;
    }
    // A slow ball at your feet: walking pushes it along.
    if (ax <= 6 && rel_fwd >= -4 && rel_fwd <= 5 && rel_z < 8 && (vx || vy) &&
        abs16(ball_vx) + abs16(ball_vy) < 24) {
        ball_vx = vx + (vx >> 1);
        ball_vy = vy + (vy >> 1);
        last_touch = i;
    }
}

// Where the ball will cross depth `y` (court px, SUB), folded off the boards.
// A division: every 8 frames at most.
static int16_t predict_x(int16_t y) {
    if (!ball_vy) return ball_x;
    int16_t t = (y - ball_y) / ball_vy;  // frames
    if (t < 0) return ball_x;
    if (t > 120) t = 120;
    int16_t x = ball_x + ball_vx * t;
    for (uint8_t k = 0; k < 3; k++) {
        if (x < TO_SUB(COURT_X0)) x = TO_SUB(COURT_X0 * 2) - x;
        else if (x > TO_SUB(COURT_X1)) x = TO_SUB(COURT_X1 * 2) - x;
    }
    return x;
}

// The CPU: get across to where the ball's coming, ready a strike as it comes,
// aim away from you. A dive for one it can't reach.
static void cpu_update(void) {
    if (cpu_idle) {
        player_update(SIDE_CPU, 0, 0);
        return;
    }
    int16_t cx = PX(pl_x[SIDE_CPU]), cy = PX(pl_y[SIDE_CPU]);
    int16_t bx = PX(ball_x), by = PX(ball_y);
    int16_t tx = CENTER_X, ty = HOME_CPU;
    // Aim at the corner away from you, now and then down the middle.
    if (!pl_shot[SIDE_CPU]) cpu_aim = (rand() & 3) == 0 ? 1 : PX(pl_x[SIDE_YOU]) < CENTER_X ? 2 : 0;
    if (phase == PHASE_SERVE) {
        if (server == SIDE_CPU) {
            tx = cx;
            ty = cy;
            if (!serve_timer) {
                pl_shot[SIDE_CPU] = (rand() & 3) == 0 ? SHOT_CHIP : SHOT_DRIVE;
                pl_charge[SIDE_CPU] = 0;
                phase = PHASE_TOSS;
                ball_vz = TOSS_LIFT;
                sfx_play(SFX_TOUCH);
            }
        }
    } else if (phase == PHASE_RALLY && !goal_timer) {
        uint8_t slow = abs16(ball_vx) + abs16(ball_vy) < 16;
        if (by < HALFWAY && slow) {
            // Loose in its half: go and get behind it.
            tx = bx;
            ty = by - 6;
            if (!pl_shot[SIDE_CPU] && abs16(bx - cx) < 20 && abs16(by - cy) < 20) pl_shot[SIDE_CPU] = SHOT_DRIVE;
        } else if (ball_vy < 0) {
            if (!(frame_count & 7)) cpu_tx = PX(predict_x(TO_SUB(HOME_CPU)));
            tx = cpu_tx;
            // A lob on the way up: back up for it.
            if (ball_z > TO_SUB(8) && ball_vz > 0) ty = HOME_CPU - 12;
            if (!pl_shot[SIDE_CPU] && !pl_move_timer[SIDE_CPU] && by - cy < 100) {
                pl_shot[SIDE_CPU] = (rand() & 3) == 0 ? SHOT_CHIP : SHOT_DRIVE;
                pl_charge[SIDE_CPU] = 0;
            }
            if (pl_shot[SIDE_CPU] && dive_range(SIDE_CPU)) {
                dive(SIDE_CPU);
                player_update(SIDE_CPU, 0, 0);
                return;
            }
        }
    }
    cpu_tx = tx;
    cpu_ty = ty;
    int8_t dx = tx > cx + 1 ? 1 : tx < cx - 1 ? -1 : 0;
    int8_t dy = ty > cy + 1 ? 1 : ty < cy - 1 ? -1 : 0;
    player_update(SIDE_CPU, dx, dy);
}

// --- the ball ---
static void goal(uint8_t side) {
    score[side]++;
    last_scorer = side;
    goal_timer = GOAL_FRAMES;
    ball_power = 0;
    sfx_play(SFX_GOAL);
    note(GOAL_FRAMES);
}

static void board(void) {
    bounces++;
    sfx_play(SFX_BOARD);
}

static void ball_update(void) {
    if (phase == PHASE_SERVE) {
        // At the server's feet, in front of them and to the side.
        ball_x = pl_x[server] + TO_SUB(8);
        ball_y = pl_y[server] + (server == SIDE_YOU ? -TO_SUB(6) : TO_SUB(6));
        return;
    }
    int16_t x = ball_x + ball_vx, y = ball_y + ball_vy;
    // In the air: gravity, and a bounce or two as it lands.
    if (ball_z || ball_vz) {
        ball_vz -= GRAVITY;
        ball_z += ball_vz;
        if (ball_z <= 0) {
            ball_z = 0;
            ball_vz = ball_vz < -12 ? (-ball_vz) >> 1 : 0;
        }
    }
    if (!ball_z) {
        ball_vx = drag(ball_vx);
        ball_vy = drag(ball_vy);
    }
    if (ball_power && abs16(ball_vx) + abs16(ball_vy) < POWER_FADE) ball_power = 0;
    if (ball_vx || ball_vy) ball_roll++;

    int16_t px = PX(x), py = PX(y);
    uint8_t in_mouth = px > GOAL_X0 + BALL_R && px < GOAL_X1 - BALL_R;
    uint8_t in_net = PX(ball_y) < COURT_FAR || PX(ball_y) > COURT_NEAR;
    // Past a goal line: in through the mouth, or off the boards.
    if (py < COURT_FAR + BALL_R || py > COURT_NEAR - BALL_R) {
        if (in_net || (in_mouth && PX(ball_z) < CROSSBAR)) {
            // In the goal: the posts keep it in the mouth, the back of the net stops it.
            if (px < GOAL_X0 + BALL_R) x = TO_SUB(GOAL_X0 + BALL_R), ball_vx = 0;
            if (px > GOAL_X1 - BALL_R) x = TO_SUB(GOAL_X1 - BALL_R), ball_vx = 0;
            if (py < COURT_FAR - NET_BACK) y = TO_SUB(COURT_FAR - NET_BACK), ball_vy = 0;
            if (py > COURT_NEAR + NET_BACK) y = TO_SUB(COURT_NEAR + NET_BACK), ball_vy = 0;
            if (PX(ball_z) > CROSSBAR - 2) ball_z = TO_SUB(CROSSBAR - 2), ball_vz = 0;
            if (!goal_timer) {
                if (py < COURT_FAR - BALL_R) goal(SIDE_YOU);
                else if (py > COURT_NEAR + BALL_R) goal(SIDE_CPU);
            }
        } else {
            y = py < HALFWAY ? TO_SUB(COURT_FAR + BALL_R) : TO_SUB(COURT_NEAR - BALL_R);
            ball_vy = -(ball_vy - (ball_vy >> 2));
            board();
        }
    }
    if (px < COURT_X0 + BALL_R) {
        x = TO_SUB(COURT_X0 + BALL_R);
        ball_vx = -(ball_vx - (ball_vx >> 2));
        board();
    } else if (px > COURT_X1 - BALL_R) {
        x = TO_SUB(COURT_X1 - BALL_R);
        ball_vx = -(ball_vx - (ball_vx >> 2));
        board();
    }
    ball_x = x;
    ball_y = y;
}

// --- the scene ---
void play_enter(void) BANKED {
    palettes_play();
    banked_bkg_data(SCENE_TILES, PITCH_TILE_COUNT, pitch_tiles, BANK(pitch));
    banked_bkg_data((uint8_t)T_METER, HUD_TILE_COUNT, hud_tiles, BANK(hud));
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    banked_sprite_data_vbk1(0, PLAYERS_TILE_COUNT, players_tiles, BANK(players));
    load_pitch();
    proj_init();
    initrand(game_seed);
    score[0] = score[1] = 0;
    pl_shots[0] = pl_shots[1] = 0;
    pl_last_move[0] = pl_last_move[1] = MOVE_NONE;
    bounces = 0;
    goal_timer = 0;
    paused = 0;
    winner = NOBODY;
    last_scorer = NOBODY;
    cam_y = CAM_MAX;
    // The HUD on the window, at the bottom (gfx.h HUD_WY). Its attributes are
    // BG_PAL_UI everywhere (gfx_init).
    move_win(7, HUD_WY);
    SHOW_WIN;
    gfx_sprite_split(HUD_WY);  // sprites stop above the HUD
    serve_setup(SIDE_YOU);
    ball_update();
    sfx_play(SFX_WHISTLE);
    draw();
}

void play_update(void) BANKED {
    if (joy_pressed & J_START && !goal_timer) {
        paused = !paused;
        hud_draw();
        return;
    }
    if (paused) return;
    if (note_timer && !--note_timer) hud_draw();

    if (goal_timer) {
        // The celebration: the ball settles in the net, then the serve (or the end).
        ball_update();
        if (!--goal_timer) {
            uint8_t s = last_scorer, o = s ^ 1;
            if (score[s] >= WIN_POINTS && score[s] >= score[o] + 2) {
                winner = s;
                state_change(STATE_GAME_OVER);
                return;
            }
            serve_setup(o);
        }
        draw();
        return;
    }
    if (serve_timer) serve_timer--;

    // You: the d-pad runs; A readies a drive, B a chip (either swaps to the
    // other while charging). With the ball just out of reach coming past, a dive.
    if (!pl_move_timer[SIDE_YOU] && joy_pressed & (J_A | J_B)) {
        uint8_t shot = joy_pressed & J_A ? SHOT_DRIVE : SHOT_CHIP;
        if (phase == PHASE_SERVE) {
            if (server == SIDE_YOU && !serve_timer) {
                pl_shot[SIDE_YOU] = shot;
                pl_charge[SIDE_YOU] = 0;
                phase = PHASE_TOSS;
                ball_vz = TOSS_LIFT;
                sfx_play(SFX_TOUCH);
            }
        } else if (phase == PHASE_RALLY && dive_range(SIDE_YOU)) {
            dive(SIDE_YOU);
        } else if (pl_shot[SIDE_YOU] != shot && !(phase == PHASE_TOSS && server == SIDE_YOU)) {
            if (!pl_shot[SIDE_YOU]) pl_charge[SIDE_YOU] = 0;
            pl_shot[SIDE_YOU] = shot;
        }
    }
    int8_t dx = joy & J_RIGHT ? 1 : joy & J_LEFT ? -1 : 0;
    int8_t dy = joy & J_DOWN ? 1 : joy & J_UP ? -1 : 0;
    player_update(SIDE_YOU, dx, dy);
    cpu_update();
    ball_update();
    meter_update();
    draw();
}
