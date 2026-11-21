#pragma bank 255
// Play: a 1-on-1 futsal match against the CPU, Mario Tennis style. Each side has
// a player and a keeper on a one-screen pitch with boards all round; first to
// WIN_GOALS wins.
//
// The ball is always loose: you strike it rather than carry it. Press A (a
// driven shot along the ground) or B (a chip, up and over the keeper) before
// the ball reaches you: the shot charges for as long as it takes to arrive, and
// fires by itself the moment the ball is in reach. While charging, the player
// slows to a shuffle, so charging early is a gamble on where the ball goes. A
// full meter is a power shot: the ball catches fire, and a keeper can only beat
// it away. UP or DOWN as the shot goes aims it at that corner of the goal.
// Without a shot ready, running into the ball dribbles it ahead of you, and
// standing still traps it.
//
// Keepers slide along their goal line after the ball, and hold anything they
// reach that isn't on fire (and isn't above the crossbar), then throw it out.
// START pauses.
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

// Sprite tiles (tools/tile-art.mjs SPRITE_TILES): 16x16 frames are 4 tiles,
// the 8x8 pieces 2 (an 8x16 pair with a blank bottom).
#define S_STAND 0
#define S_RUN_A 4
#define S_RUN_B 8
#define S_KICK 12
#define S_SAVE 16
#define S_BALL 20  // and S_BALL + 2, its other roll
#define S_SHADOW 24
#define S_MARKER 26
// Background tiles: the pitch from SCENE_TILES, then the meter's three.
#define T_METER (SCENE_TILES + PITCH_TILE_COUNT)  // empty, half, full

// Hardware sprites. Lower numbers draw on top: the ball over everyone, the
// shadow under everyone.
#define OAM_BALL 0
#define OAM_MARKER 1
#define OAM_PLAYER 2  // 2 each, by side
#define OAM_KEEPER 6  // 2 each, by goal
#define OAM_SHADOW 10

// --- tuning ---
#define SPEED_YOU 20      // a frame, in SUB: 1.25 px
#define SPEED_CPU 16
#define SPEED_CHARGE 8    // shuffling while a shot charges
#define HOLD_MAX 90       // frames at a full meter before the player gives up the shot
#define KICK_FRAMES 14    // the kick pose; the kicker can't touch the ball again meanwhile
#define DRIVE_SPEED 40    // + the charge
#define POWER_SPEED 96
#define CHIP_SPEED 16     // + three quarters of the charge: charge to chip further
#define CHIP_LIFT 40
#define GRAVITY 3
#define POWER_FADE 40     // a power shot burns out below this speed (|vx| + |vy|)
#define KEEPER_SPEED 12
#define KEEPER_RANGE 8    // px either side of the middle of the goal it keeps to
#define KEEPER_REACH_Z 10 // px: a ball higher than this goes over the keeper's head
#define KEEPER_X 6        // px in front of the goal line
#define KEEPER_HOLD 40    // frames a keeper holds the ball before throwing it out
#define SAVE_FRAMES 20
#define THROW_SPEED 36
#define KICKOFF_FRAMES 50
#define GOAL_FRAMES 110
#define BALL_R 3          // px: the ball's radius on the ground

uint16_t game_seed;
uint16_t test_seed;

uint8_t score[2];
uint8_t winner;
uint8_t last_scorer;
uint8_t kickoff_timer;
uint8_t goal_timer;
uint8_t paused;

int16_t pl_x[2], pl_y[2];
int8_t pl_vx[2], pl_vy[2];
uint8_t pl_face[2];
uint8_t pl_shot[2];
uint8_t pl_charge[2];
uint8_t pl_hold[2];
uint8_t pl_kick_timer[2];
uint8_t pl_shots[2];
uint8_t pl_touching[2];  // touching the ball last frame (a dribble's tick plays once)
uint8_t cpu_aim;
uint8_t cpu_idle;  // non-zero: the CPU's player stands still (tests, the playtest page)

int16_t kp_x[2], kp_y[2];
uint8_t kp_hold;
uint8_t kp_save_timer[2];

int16_t ball_x, ball_y, ball_z;
int16_t ball_vx, ball_vy, ball_vz;
uint8_t ball_power;
uint8_t ball_owner;
uint8_t last_touch;
uint8_t bounces;
uint8_t ball_roll;  // counts up as the ball rolls: its two frames

uint8_t meter_shown;  // the meter's value on screen (0xFF: redraw)
uint8_t meter_buf[8];  // its tiles, queued to the window's map
uint8_t note_timer;    // the HUD shows a note (GOAL!) while this counts down

// 32 directions, clockwise from right (screen y points down): cos * 64. The
// sine is the cosine 8 steps back.
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

// The direction (0-31) from a vector, in px. Only when a shot is struck.
static uint8_t dir_to(int16_t dx, int16_t dy) {
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t k = ay <= ax ? octant(ax, ay) : 8 - octant(ay, ax);
    if (dx >= 0) return dy >= 0 ? k : (32 - k) & 31;
    return dy >= 0 ? 16 - k : 16 + k;
}

static int16_t abs16(int16_t v) {
    return v < 0 ? -v : v;
}

// Slow a rolling ball: a thirty-second of its speed, and a little more, so it
// stops rather than creeping forever.
static int16_t drag(int16_t v) {
    if (v > 0) {
        v -= (v >> 5) + 1;
        if (v < 0) v = 0;
    } else if (v < 0) {
        v += ((-v) >> 5) + 1;
        if (v > 0) v = 0;
    }
    return v;
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
        else win_text(7, 0, "READY");
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
static void kickoff(uint8_t conceded) {
    pl_x[SIDE_YOU] = TO_SUB(conceded == SIDE_YOU ? 66 : 40);
    pl_x[SIDE_CPU] = TO_SUB(conceded == SIDE_CPU ? 94 : 120);
    for (uint8_t i = 0; i < 2; i++) {
        pl_y[i] = TO_SUB(CENTER_Y);
        pl_vx[i] = pl_vy[i] = 0;
        pl_shot[i] = SHOT_NONE;
        pl_charge[i] = pl_hold[i] = pl_kick_timer[i] = pl_touching[i] = 0;
        kp_y[i] = TO_SUB(CENTER_Y);
        kp_save_timer[i] = 0;
    }
    pl_face[SIDE_YOU] = 0;
    pl_face[SIDE_CPU] = 1;
    kp_x[0] = TO_SUB(FIELD_X0 + KEEPER_X);
    kp_x[1] = TO_SUB(FIELD_X1 - KEEPER_X);
    ball_x = TO_SUB(CENTER_X);
    ball_y = TO_SUB(CENTER_Y);
    ball_z = ball_vx = ball_vy = ball_vz = 0;
    ball_power = 0;
    ball_owner = NOBODY;
    last_touch = NOBODY;
    kickoff_timer = KICKOFF_FRAMES;
    note(KICKOFF_FRAMES);
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
// A 16x16 character (two 8x16 sprites) with its feet at (x, y) px.
static void draw16(uint8_t n, uint8_t tile, int16_t x, int16_t y, uint8_t pal, uint8_t flip) {
    if (flip) {
        set_sprite_tile(n, tile + 2);
        set_sprite_tile(n + 1, tile);
        pal |= S_FLIPX;
    } else {
        set_sprite_tile(n, tile);
        set_sprite_tile(n + 1, tile + 2);
    }
    set_sprite_prop(n, pal);
    set_sprite_prop(n + 1, pal);
    // Hardware coordinates are 8 and 16 px off the screen's.
    move_sprite(n, (uint8_t)x, (uint8_t)(y + 1));
    move_sprite(n + 1, (uint8_t)(x + 8), (uint8_t)(y + 1));
}

static void draw(void) {
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t tile = S_STAND;
        if (pl_kick_timer[i]) tile = S_KICK;
        else if (pl_vx[i] || pl_vy[i]) tile = (frame_count & 8) ? S_RUN_A : S_RUN_B;
        uint8_t pal = i == SIDE_YOU ? SPR_PAL_YOU : SPR_PAL_CPU;
        // A full meter: flashing, the power shot is ready.
        if (pl_charge[i] == CHARGE_MAX && (frame_count & 4)) pal = SPR_PAL_POWER;
        draw16(OAM_PLAYER + (i << 1), tile, PX(pl_x[i]), PX(pl_y[i]), pal, pl_face[i]);
        draw16(OAM_KEEPER + (i << 1), kp_save_timer[i] ? S_SAVE : S_STAND, PX(kp_x[i]), PX(kp_y[i]),
               i ? SPR_PAL_KEEP_CPU : SPR_PAL_KEEP_YOU, i);
    }
    // The marker over your head.
    set_sprite_tile(OAM_MARKER, S_MARKER);
    set_sprite_prop(OAM_MARKER, SPR_PAL_UI);
    move_sprite(OAM_MARKER, (uint8_t)(PX(pl_x[SIDE_YOU]) + 4), (uint8_t)(PX(pl_y[SIDE_YOU]) - 5));
    // The ball, up at its height, and its shadow on the ground.
    uint8_t bx = (uint8_t)PX(ball_x), by = (uint8_t)PX(ball_y), bz = (uint8_t)PX(ball_z);
    set_sprite_tile(OAM_BALL, S_BALL + ((ball_roll & 8) ? 2 : 0));
    set_sprite_prop(OAM_BALL, ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
    move_sprite(OAM_BALL, bx + 4, by + 12 - bz);
    set_sprite_tile(OAM_SHADOW, S_SHADOW);
    set_sprite_prop(OAM_SHADOW, SPR_PAL_UI);
    move_sprite(OAM_SHADOW, bx + 4, by + 11);
}

// --- shots ---
static void kick(uint8_t i) {
    // Toward the far goal: its middle, or a corner if UP or DOWN is held (the
    // CPU picks its corner as it readies the shot).
    int16_t tx = i == SIDE_YOU ? FIELD_X1 + 4 : FIELD_X0 - 4;
    int16_t ty = CENTER_Y;
    uint8_t aim = 1;
    if (i == SIDE_YOU) {
        if (joy & J_UP) aim = 0;
        else if (joy & J_DOWN) aim = 2;
    } else aim = cpu_aim;
    if (aim == 0) ty = MOUTH_Y0 + 5;
    else if (aim == 2) ty = MOUTH_Y1 - 5;
    uint8_t d = dir_to(tx - PX(ball_x), ty - PX(ball_y));
    uint8_t c = pl_charge[i], speed;
    ball_power = 0;
    if (pl_shot[i] == SHOT_CHIP) {
        speed = CHIP_SPEED + (c >> 1) + (c >> 2);
        ball_vz = CHIP_LIFT;
        sfx_play(SFX_KICK);
    } else if (c == CHARGE_MAX) {
        speed = POWER_SPEED;
        ball_vz = 0;
        ball_power = 1;
        sfx_play(SFX_POWER);
    } else {
        speed = DRIVE_SPEED + c;
        ball_vz = 0;
        sfx_play(SFX_KICK);
    }
    ball_vx = ((int16_t)cos_t[d] * speed) >> 6;
    ball_vy = ((int16_t)SIN(d) * speed) >> 6;
    last_touch = i;
    pl_face[i] = ball_vx < 0;
    pl_kick_timer[i] = KICK_FRAMES;
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = 0;
    pl_shots[i]++;
}

// A player's frame: move (dx, dy: -1, 0 or 1), charge, then strike or dribble.
static void player_update(uint8_t i, int8_t dx, int8_t dy) {
    int8_t speed = pl_shot[i] ? SPEED_CHARGE : i == SIDE_YOU ? SPEED_YOU : SPEED_CPU;
    int8_t vx = dx > 0 ? speed : dx < 0 ? -speed : 0;
    int8_t vy = dy > 0 ? speed : dy < 0 ? -speed : 0;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    int16_t x = pl_x[i] + vx, y = pl_y[i] + vy;
    if (x < TO_SUB(FIELD_X0 + 4)) x = TO_SUB(FIELD_X0 + 4);
    if (x > TO_SUB(FIELD_X1 - 4)) x = TO_SUB(FIELD_X1 - 4);
    if (y < TO_SUB(FIELD_Y0 + 6)) y = TO_SUB(FIELD_Y0 + 6);
    if (y > TO_SUB(FIELD_Y1 - 2)) y = TO_SUB(FIELD_Y1 - 2);
    pl_x[i] = x;
    pl_y[i] = y;
    if (vx) pl_face[i] = vx < 0;

    if (pl_kick_timer[i]) {
        pl_kick_timer[i]--;
        return;
    }
    if (pl_shot[i]) {
        if (pl_charge[i] < CHARGE_MAX) {
            if (++pl_charge[i] == CHARGE_MAX && i == SIDE_YOU) sfx_play(SFX_CHARGE);
        } else if (++pl_hold[i] > HOLD_MAX) {
            pl_shot[i] = SHOT_NONE;  // waited too long: the shot is given up
            pl_charge[i] = pl_hold[i] = 0;
        }
    }
    if (ball_owner != NOBODY) return;
    int16_t bdx = PX(ball_x) - PX(x), bdy = PX(ball_y) - PX(y);
    uint8_t low = PX(ball_z) < 12;
    // In reach with a shot ready: strike it.
    if (pl_shot[i] && low && abs16(bdx) <= 10 && abs16(bdy) <= 7) {
        kick(i);
        return;
    }
    // Touching it: running pushes it on ahead; standing still traps it.
    uint8_t touching = PX(ball_z) < 8 && abs16(bdx) <= 6 && abs16(bdy) <= 5;
    if (touching) {
        uint8_t pushed = 0;
        if ((vx > 0 && bdx >= -2) || (vx < 0 && bdx <= 2)) {
            ball_vx = vx + (vx >> 1);
            pushed = 1;
        }
        if ((vy > 0 && bdy >= -2) || (vy < 0 && bdy <= 2)) {
            ball_vy = vy + (vy >> 1);
            pushed = 1;
        }
        if (!pushed) {
            ball_vx >>= 2;
            ball_vy >>= 2;
        }
        ball_power = 0;
        last_touch = i;
        if (!pl_touching[i]) sfx_play(SFX_TOUCH);
    }
    pl_touching[i] = touching;
}

// The CPU: get goal-side of the ball (to its right: it attacks the left goal),
// ready a shot when close, and let the charge build as it closes in.
static void cpu_update(void) {
    if (cpu_idle) {
        player_update(SIDE_CPU, 0, 0);
        return;
    }
    int16_t bx = PX(ball_x), by = PX(ball_y), cx = PX(pl_x[SIDE_CPU]), cy = PX(pl_y[SIDE_CPU]);
    int16_t tx = bx + 9, ty = by;
    if (ball_owner == SIDE_CPU) tx = CENTER_X + 30, ty = CENTER_Y;  // its keeper has it: wait for the throw
    else if (ball_owner == SIDE_YOU) tx = CENTER_X + 10, ty = CENTER_Y;
    else if (cx < bx + 4) {
        // On the wrong side of the ball: go round it, not through it.
        tx = bx + 12;
        ty = cy < by ? by - 14 : by + 14;
    }
    int8_t dx = tx > cx + 1 ? 1 : tx < cx - 1 ? -1 : 0;
    int8_t dy = ty > cy + 1 ? 1 : ty < cy - 1 ? -1 : 0;
    if (!pl_shot[SIDE_CPU] && !pl_kick_timer[SIDE_CPU] && ball_owner == NOBODY &&
        cx > bx + 2 && abs16(bx - cx) < 30 && abs16(by - cy) < 14) {
        uint8_t r = rand();
        pl_shot[SIDE_CPU] = (r & 3) == 0 ? SHOT_CHIP : SHOT_DRIVE;
        r = (r >> 2) & 3;
        cpu_aim = r == 3 ? 1 : r;
    }
    player_update(SIDE_CPU, dx, dy);
}

// Players don't stand inside each other: overlapping, they step apart.
static void players_separate(void) {
    int16_t dx = pl_x[SIDE_CPU] - pl_x[SIDE_YOU], dy = pl_y[SIDE_CPU] - pl_y[SIDE_YOU];
    if (abs16(dx) >= TO_SUB(10) || abs16(dy) >= TO_SUB(6)) return;
    if (dx >= 0) {
        pl_x[SIDE_CPU] += SUB;
        pl_x[SIDE_YOU] -= SUB;
    } else {
        pl_x[SIDE_CPU] -= SUB;
        pl_x[SIDE_YOU] += SUB;
    }
}

// --- the keepers ---
static void keeper_update(uint8_t g) {
    if (kp_save_timer[g]) kp_save_timer[g]--;
    // After the ball when it's coming this way or in this half; else back to the middle.
    int16_t ty = CENTER_Y;
    uint8_t coming = g ? ball_vx > 0 || PX(ball_x) > CENTER_X : ball_vx < 0 || PX(ball_x) < CENTER_X;
    if (coming || ball_owner == g) ty = PX(ball_y);
    if (ty < CENTER_Y - KEEPER_RANGE) ty = CENTER_Y - KEEPER_RANGE;
    if (ty > CENTER_Y + KEEPER_RANGE) ty = CENTER_Y + KEEPER_RANGE;
    ty = TO_SUB(ty);
    int16_t y = kp_y[g];
    if (y < ty - KEEPER_SPEED) y += KEEPER_SPEED;
    else if (y > ty + KEEPER_SPEED) y -= KEEPER_SPEED;
    else y = ty;
    kp_y[g] = y;

    if (ball_owner != NOBODY || goal_timer) return;
    int16_t dx = PX(ball_x) - PX(kp_x[g]), dy = PX(ball_y) - PX(y);
    // Its reach: a stretch for a slow ball, less the harder it's hit. A power
    // shot it can only parry, and only one straight at it.
    uint8_t speed = (uint8_t)(abs16(ball_vx) >> 4);
    uint8_t reach = ball_power ? 3 : 11 - speed;
    // (Not behind it: once past the keeper, a ball is beyond saving.)
    if ((g ? dx > 2 : dx < -2) || abs16(dx) > 6 || abs16(dy) > reach || PX(ball_z) >= KEEPER_REACH_Z) return;
    kp_save_timer[g] = SAVE_FRAMES;
    last_touch = g;
    if (ball_power) {
        // On fire: too hot to hold. Beaten back out, up, and off at an angle.
        ball_power = 0;
        ball_vx = -(ball_vx >> 1);
        ball_vy += (int16_t)(rand() & 31) - 16;
        ball_vz = 20;
        ball_x = kp_x[g] + (g ? -TO_SUB(9) : TO_SUB(9));
        sfx_play(SFX_PARRY);
        return;
    }
    ball_owner = g;
    kp_hold = KEEPER_HOLD;
    ball_vx = ball_vy = ball_vz = 0;
    sfx_play(SFX_CATCH);
}

// A keeper holding the ball keeps it at its chest, then throws it out.
static void held_update(void) {
    uint8_t g = ball_owner;
    ball_x = kp_x[g] + (g ? -TO_SUB(5) : TO_SUB(5));
    ball_y = kp_y[g];
    ball_z = TO_SUB(6);
    if (--kp_hold) return;
    ball_owner = NOBODY;
    ball_x = kp_x[g] + (g ? -TO_SUB(10) : TO_SUB(10));
    ball_vx = g ? -THROW_SPEED : THROW_SPEED;
    ball_vy = (int16_t)(rand() & 31) - 16;
    ball_vz = 20;
    kp_save_timer[g] = SAVE_FRAMES >> 1;
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
    if (ball_owner != NOBODY) {
        held_update();
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
    uint8_t mouth = py > MOUTH_Y0 + BALL_R && py < MOUTH_Y1 - BALL_R && PX(ball_z) < CROSSBAR;
    // Past a goal line: in through the mouth, or off the end boards.
    if (px < FIELD_X0 + BALL_R || px > FIELD_X1 - BALL_R) {
        uint8_t in_net = PX(ball_x) < FIELD_X0 || PX(ball_x) > FIELD_X1;
        if (mouth || in_net) {
            // In the goal: the posts keep it in the mouth, the back of the net stops it.
            if (py < MOUTH_Y0 + BALL_R) y = TO_SUB(MOUTH_Y0 + BALL_R), ball_vy = 0;
            if (py > MOUTH_Y1 - BALL_R) y = TO_SUB(MOUTH_Y1 - BALL_R), ball_vy = 0;
            if (px < BALL_R) x = TO_SUB(BALL_R), ball_vx = 0;
            if (px > 159 - BALL_R) x = TO_SUB(159 - BALL_R), ball_vx = 0;
            if (!goal_timer) {
                if (px < FIELD_X0 - BALL_R) goal(SIDE_CPU);
                else if (px > FIELD_X1 + BALL_R) goal(SIDE_YOU);
            }
        } else {
            x = px < CENTER_X ? TO_SUB(FIELD_X0 + BALL_R) : TO_SUB(FIELD_X1 - BALL_R);
            ball_vx = -(ball_vx - (ball_vx >> 2));
            board();
        }
    }
    if (py < FIELD_Y0 + BALL_R) {
        y = TO_SUB(FIELD_Y0 + BALL_R);
        ball_vy = -(ball_vy - (ball_vy >> 2));
        board();
    } else if (py > FIELD_Y1 - BALL_R) {
        y = TO_SUB(FIELD_Y1 - BALL_R);
        ball_vy = -(ball_vy - (ball_vy >> 2));
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
    load_pitch();
    initrand(game_seed);
    score[0] = score[1] = 0;
    pl_shots[0] = pl_shots[1] = 0;
    bounces = 0;
    goal_timer = 0;
    paused = 0;
    winner = NOBODY;
    last_scorer = NOBODY;
    // The HUD on the window, at the bottom (gfx.h HUD_WY). Its attributes are
    // BG_PAL_UI everywhere (gfx_init).
    move_win(7, HUD_WY);
    SHOW_WIN;
    gfx_sprite_split(HUD_WY);  // sprites stop above the HUD
    kickoff(NOBODY);
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
        // The celebration: the ball settles in the net, then the kickoff (or the end).
        ball_update();
        if (!--goal_timer) {
            if (score[last_scorer] >= WIN_GOALS) {
                winner = last_scorer;
                state_change(STATE_GAME_OVER);
                return;
            }
            kickoff(last_scorer ^ 1);
        }
        draw();
        return;
    }
    if (kickoff_timer) {
        if (!--kickoff_timer) sfx_play(SFX_WHISTLE);
        draw();
        return;
    }

    // You: the d-pad runs; A readies a driven shot, B a chip (either swaps
    // to the other while charging).
    if (!pl_kick_timer[SIDE_YOU]) {
        if (joy_pressed & J_A && pl_shot[SIDE_YOU] != SHOT_DRIVE) {
            if (!pl_shot[SIDE_YOU]) pl_charge[SIDE_YOU] = 0;
            pl_shot[SIDE_YOU] = SHOT_DRIVE;
        } else if (joy_pressed & J_B && pl_shot[SIDE_YOU] != SHOT_CHIP) {
            if (!pl_shot[SIDE_YOU]) pl_charge[SIDE_YOU] = 0;
            pl_shot[SIDE_YOU] = SHOT_CHIP;
        }
    }
    int8_t dx = joy & J_RIGHT ? 1 : joy & J_LEFT ? -1 : 0;
    int8_t dy = joy & J_DOWN ? 1 : joy & J_UP ? -1 : 0;
    player_update(SIDE_YOU, dx, dy);
    cpu_update();
    players_separate();
    keeper_update(0);
    keeper_update(1);
    ball_update();
    meter_update();
    draw();
}
