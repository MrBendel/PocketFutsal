#pragma bank 255
// Play: a 1-on-1 futsal match against the CPU, Mario Tennis style. You play the
// near half of a court seen in perspective, the CPU the far one; each defends the
// goal behind them, and a goal is a point: first to WIN_POINTS, by two.
//
// The ball is always loose: you strike it rather than carry it. With the ball
// coming your way (or loose in your half), A readies a power shot, straight where
// the d-pad points, and B a curve, bending in to the side the d-pad holds; B then
// A is a lob, A then B a drop shot. Readying plants you: the meter fills as you
// wait, each tap of the button again adds more, and the strike goes by itself as
// the ball arrives. Which strike depends on the ball's height: a kick on the
// ground, a volley at the waist, a header, or a bicycle kick for one that's just
// gone over your head. Planted out of reach, you slide for it, and lie there a
// while. A ball only just reached (a slide, a block, a ball on fire met with less
// than half a meter) pops up into the other half, where met in the air it's a
// slam, on fire. A full meter sets the ball on fire too. Unready, the ball pops
// up off you; on fire, it knocks you down and goes on.
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
#define T_LYING 160    // lying (a slide, knocked down): three 8x16 columns, head to the right
// Background tiles: the court from SCENE_TILES, then the meter's three.
#define T_METER (SCENE_TILES + PITCH_TILE_COUNT)  // empty, half, full

// Hardware sprites. Lower numbers draw on top: the ball over everyone, the
// shadow under everyone.
#define OAM_BALL 0
#define OAM_PLAYER 1  // 4 each, by side
#define OAM_SHADOW 9

// --- tuning ---
#define SPEED_YOU 22      // a frame, in SUB
#define SPEED_CPU 16
#define HOLD_MAX 90       // frames at a full meter before the player gives up the strike
#define REACH_X 10        // court px either side a strike reaches
#define REACH_FWD 8       // in front
#define REACH_BACK 4      // behind (a bicycle kick reaches further)
#define SLIDE_REACH 24    // px either side a slide goes for
#define SLIDE_SPEED 48
#define SLIDE_LUNGE 10    // frames of sliding
#define SLIDE_FRAMES 64   // a missed slide: on the floor until this runs out
#define SLIDE_REST 28     // a slide that got there: up sooner
#define POWER_SPEED 96    // a ball on fire
#define DRIVE_SPEED 56    // A: + the charge
#define CURVE_SPEED 50    // B: + the charge
#define CURVE_OFS 44      // px a curve bends by the time it gets there
#define HEADER_SPEED 32   // + half the charge
#define CHIP_LIFT 56      // a lob: about 32 px up, 37 frames in the air
#define POP_LIFT 64       // a pop-up: about 43 px up, 42 frames
#define GRAVITY 3
#define POWER_FADE 40     // a ball on fire burns out below this speed (|vx| + |vy|)
#define SLOW 16           // a ball slower than this (|vx| + |vy|) is loose
#define FIRE_MEET 24      // the charge it takes to meet a ball on fire cleanly
#define SCUFF 4           // a strike with less charge than this is scuffed into the net
#define KICK_FRAMES 14    // a strike's pose; no touches meanwhile
#define BICYCLE_FRAMES 24
#define BLOCK_FRAMES 16
#define DOWN_FRAMES 40    // knocked down by a ball on fire
#define TOSS_LIFT 54      // the serve's toss: about 30 px up
#define TOSS_STRIKE 20    // px: struck when it falls back to this
#define SERVE_FRAMES 50
#define GOAL_FRAMES 110
#define BALL_R 3          // px: the ball's radius on the court
#define HOME_CPU 24       // the CPU's depth, waiting
#define CPU_PLANT 96      // px: the CPU readies when the ball's this close and it's in place
#define CPU_LATE 56       // px: and this close, in place or not
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
uint8_t point_reason;
uint8_t ball_landings;
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
uint8_t pl_taps[2];
uint8_t pl_aim[2];
int8_t pl_slide_vx[2], pl_slide_vy[2];
int16_t cpu_tx, cpu_ty;
uint8_t cpu_aim;
uint8_t cpu_idle;  // non-zero: the CPU's player stands still (tests, the playtest page)

int16_t ball_x, ball_y, ball_z;
int16_t ball_vx, ball_vy, ball_vz;
uint8_t ball_power;
int8_t ball_curve;
uint8_t ball_curve_t;
int16_t ball_cacc;  // the curve's push, building up to whole SUBs of speed
uint8_t ball_pop;
uint8_t ball_drop;
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

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
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
        int32_t row = COURT_SY + floordiv((int32_t)PROJ_K0 * 448 * fy + (int32_t)(PROJ_K1 - PROJ_K0) * fy * fy, 114688L);  /* 448 * 256 */
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
        uint8_t you = last_scorer == SIDE_YOU;
        if (!goal_timer) win_text(5, 0, server == SIDE_YOU ? "YOU SERVE" : "CPU SERVES");
        else if (point_reason == POINT_GOAL) win_text(2, 0, you ? "GOAL! YOU SCORE" : "GOAL! CPU SCORES");
        else if (point_reason == POINT_NET) win_text(3, 0, you ? "NET! POINT YOU" : "NET! POINT CPU");
        else win_text(0, 0, you ? "2 BOUNCES! POINT YOU" : "2 BOUNCES! POINT CPU");
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
static void unready(uint8_t i) {
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = pl_taps[i] = 0;
    pl_aim[i] = 1;
}

static void ball_plain(void) {
    ball_power = ball_curve = ball_curve_t = ball_pop = ball_drop = 0;
    ball_cacc = 0;
}

static void serve_setup(uint8_t side) {
    server = side;
    phase = PHASE_SERVE;
    for (uint8_t i = 0; i < 2; i++) {
        pl_vx[i] = pl_vy[i] = 0;
        unready(i);
        pl_move[i] = pl_move_timer[i] = 0;
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
    ball_plain();
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
        else if (m == MOVE_VOLLEY || m == MOVE_BLOCK) f = F_VOLLEY;
        else if (m == MOVE_HEADER) f = F_HEADER, lift = 4;
        else if (m == MOVE_BICYCLE) f = F_BICYCLE, lift = 8;
        else if (m == MOVE_SLIDE) lie = 1, flip = !flip;  // feet first
        else if (m == MOVE_DOWN) lie = 1;
        else if (pl_shot[i] || (phase == PHASE_TOSS && server == i)) f = F_READY;
        else if (pl_vx[i] || pl_vy[i]) f = (frame_count & 8) ? F_RUN_A : F_RUN_B;
        uint8_t tile = lie ? T_LYING : (f << 3) + (i == SIDE_CPU ? VIEW_FRONT : 0);
        uint8_t pal = i == SIDE_YOU ? SPR_PAL_YOU : SPR_PAL_CPU;
        if (pl_shot[i]) {
            // Charging: flashing through white and fire, faster as the meter
            // fills (every 8 frames, 4 from half, 2 full).
            uint8_t c = pl_charge[i];
            uint8_t step = (uint8_t)(c == CHARGE_MAX ? frame_count >> 1 : c >= CHARGE_MAX / 2 ? frame_count >> 2 : frame_count >> 3);
            step %= 3;
            if (step == 1) pal = SPR_PAL_FLASH;
            else if (step == 2 && c >= CHARGE_MAX / 2) pal = SPR_PAL_POWER;
        }
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

// --- where the ball is ---
// The ball relative to player i: across, forward (toward the other half:
// positive in front of them), and up.
static int16_t rel_x, rel_fwd, rel_z;
static void relate(uint8_t i) {
    rel_x = PX(ball_x) - PX(pl_x[i]);
    rel_fwd = PX(pl_y[i]) - PX(ball_y);
    if (i == SIDE_CPU) rel_fwd = -rel_fwd;
    rel_z = PX(ball_z);
}

static int16_t ball_speed(void) {
    return abs16(ball_vx) + abs16(ball_vy);
}

// Coming this way (toward player i's goal).
static uint8_t coming(uint8_t i) {
    return i == SIDE_YOU ? ball_vy > 0 : ball_vy < 0;
}

// Slow, in player i's half: theirs to go and get.
static uint8_t loose_here(uint8_t i) {
    if (ball_speed() >= SLOW) return 0;
    return i == SIDE_YOU ? PX(ball_y) >= HALFWAY : PX(ball_y) < HALFWAY;
}

// Player i may ready a strike: mid-rally, the ball coming or loose in their half.
static uint8_t can_ready(uint8_t i) {
    return phase == PHASE_RALLY && !goal_timer && !pl_move_timer[i] && (coming(i) || loose_here(i));
}

// The depth of the other side's half that a lob or a pop-up drops into, by who struck it.
static int16_t other_y(uint8_t i, int16_t from_halfway) {
    return i == SIDE_YOU ? HALFWAY - from_halfway : HALFWAY + from_halfway;
}

// --- readying ---
// A press of A (b 0) or B (b 1): the first readies a power shot or a curve; the
// other button next makes it a drop shot (A, B) or a lob (B, A); after that, or
// the same button again, it's a tap: more charge.
static void ready(uint8_t i, uint8_t b) {
    uint8_t s = pl_shot[i];
    if (!s) {
        unready(i);
        pl_shot[i] = b ? SHOT_CURVE : SHOT_POWER;
        if (i == SIDE_YOU) pl_aim[i] = joy & J_LEFT ? 0 : joy & J_RIGHT ? 2 : 1;
        return;
    }
    if (s == SHOT_POWER && b) {
        pl_shot[i] = SHOT_DROP;
        return;
    }
    if (s == SHOT_CURVE && !b) {
        pl_shot[i] = SHOT_LOB;
        return;
    }
    if (pl_taps[i] >= TAP_MAX || pl_charge[i] == CHARGE_MAX) return;
    pl_taps[i]++;
    pl_charge[i] += TAP_CHARGE;
    if (pl_charge[i] >= CHARGE_MAX) {
        pl_charge[i] = CHARGE_MAX;
        if (i == SIDE_YOU) sfx_play(SFX_CHARGE);
    }
}

// --- strikes ---
// Set the ball going toward (dx, dy) px away at speed (SUB a frame, |vx| + |vy|,
// the same measure as ball_speed): exact, where the 32 directions would round a
// slight angle away. A division: only as it's struck.
static void launch(int16_t dx, int16_t dy, uint8_t speed) {
    int16_t sum = abs16(dx) + abs16(dy);
    if (!sum) sum = 1;
    ball_vx = (dx * speed) / sum;
    ball_vy = (dy * speed) / sum;
}

static void struck(uint8_t i, uint8_t move, uint8_t frames) {
    last_touch = i;
    ball_landings = 0;
    phase = PHASE_RALLY;
    if (move != MOVE_SLIDE) {
        pl_face[i] = PX(ball_x) < PX(pl_x[i]);
        pl_move[i] = move;
        pl_move_timer[i] = frames;
    }
    pl_last_move[i] = move;
    unready(i);
    pl_shots[i]++;
}

// Set ball_vz so the ball comes down at depth `land` (px): its time in the air t
// is the distance over its speed, and from height z0 it lands after t frames
// with vz = 1.5 t - z0 / t (gravity 3). Every strike arcs, so the ball is in the
// air, its shadow under it, and clears the net if it's meant to.
static void arc(int16_t land) {
    int16_t vy = abs16(ball_vy);
    if (!vy) vy = 1;
    int16_t t = clamp16((abs16(land - PX(ball_y)) << 4) / vy, 6, 80);
    ball_vz = ((3 * t) >> 1) - ball_z / t;
}

// A clean strike: the readied shot, shaped by the ball's height (move).
static void strike(uint8_t i, uint8_t move) {
    uint8_t was_pop = ball_pop;
    int16_t bx = PX(ball_x), by = PX(ball_y);
    // Toward the far goal: its middle, or a corner: the way you're holding, or
    // last held while planted (the CPU picks its corner as it readies).
    int16_t ty = i == SIDE_YOU ? COURT_FAR - 8 : COURT_NEAR + 8;
    uint8_t aim = pl_aim[i];
    if (i == SIDE_YOU) {
        if (joy & J_LEFT) aim = 0;
        else if (joy & J_RIGHT) aim = 2;
    } else aim = cpu_aim;
    int16_t tx = aim == 0 ? GOAL_X0 + 4 : aim == 2 ? GOAL_X1 - 4 : CENTER_X;
    uint8_t c = pl_charge[i], kind = pl_shot[i], speed, frames = KICK_FRAMES;
    if (phase != PHASE_RALLY && c == CHARGE_MAX) c--;  // no serves on fire
    uint8_t fire = c == CHARGE_MAX;
    // Where it comes down: deeper the more it's charged. Barely charged at all,
    // it's scuffed: it comes down on your own side, short of the net.
    int16_t land = c < SCUFF && phase == PHASE_RALLY ? other_y(i, -10) : other_y(i, 24 + c + (c >> 1));
    ball_plain();
    if (was_pop && move != MOVE_KICK) {
        // A pop-up met in the air: a slam, on fire, straight and deep.
        ball_power = 1;
        if (move == MOVE_HEADER) frames = KICK_FRAMES + 4;
        else if (move == MOVE_BICYCLE) frames = BICYCLE_FRAMES;
        launch(tx - bx, ty - by, POWER_SPEED);
        arc(other_y(i, 64));
    } else if (move == MOVE_HEADER || move == MOVE_BICYCLE) {
        // In the air: always straight.
        if (move == MOVE_BICYCLE && c >= CHARGE_MAX / 2) {
            speed = POWER_SPEED;
            ball_power = 1;
        } else speed = move == MOVE_HEADER ? HEADER_SPEED + (c >> 1) : DRIVE_SPEED + 16 + (c >> 1);
        frames = move == MOVE_HEADER ? KICK_FRAMES + 4 : BICYCLE_FRAMES;
        launch(tx - bx, ty - by, speed);
        arc(move == MOVE_HEADER ? other_y(i, 28 + (c >> 1)) : other_y(i, 56));
    } else if (kind == SHOT_LOB || kind == SHOT_DROP) {
        // Placed: a lob drops just behind the other player, a drop shot just past
        // halfway. A lob's time in the air is known, so its speed is the distance
        // over it (7/16 of it a frame); a drop shot goes at 4/5 of the distance a
        // frame (about 20 frames), up to 64, lifted to come down there.
        int16_t dist;
        if (kind == SHOT_LOB) {
            land = PX(pl_y[i ^ 1]) + (i == SIDE_YOU ? -12 : 12);
            dist = abs16(land - by);
            speed = (uint8_t)clamp16((dist * 7) >> 4, 16, 72);
            ball_vz = move == MOVE_VOLLEY ? CHIP_LIFT - 8 : CHIP_LIFT;
        } else {
            land = other_y(i, 18);
            dist = abs16(land - by);
            speed = (uint8_t)clamp16((dist * 4) / 5, 12, 64);
            ball_drop = 1;
        }
        launch(tx - bx, land - by, speed);
        if (kind == SHOT_DROP) arc(land);  // its speed is capped: the lift makes up the distance
    } else if (kind == SHOT_CURVE) {
        // Set off CURVE_OFS px wide of the target and bent back in: a steady push
        // across for its time in flight, t frames (at its starting speed: the ball
        // slows, so it drifts on after the push ends). 384 * CURVE_OFS / t^2
        // sixteenths of a SUB a frame, each frame, lands it on the target
        // (tuned: build/scratch/curve.mjs).
        int8_t side = aim == 0 ? -1 : aim == 2 ? 1 : PX(pl_x[i]) < CENTER_X ? 1 : -1;
        speed = fire ? POWER_SPEED - 16 : CURVE_SPEED + c;
        ball_power = fire;
        int16_t t = clamp16((abs16(ty - by) << 4) / speed, 16, 90);
        int16_t push = clamp16((384 * CURVE_OFS) / (t * t), 1, 60);
        ball_curve = side > 0 ? (int8_t)push : -(int8_t)push;
        ball_curve_t = (uint8_t)t;
        launch(tx - side * CURVE_OFS - bx, ty - by, speed);
        arc(land);
    } else {
        // Power: straight, a low fast arc.
        speed = fire ? POWER_SPEED : DRIVE_SPEED + c + (move == MOVE_VOLLEY ? 8 : 0);
        ball_power = fire;
        launch(tx - bx, ty - by, speed);
        arc(land);
    }
    sfx_play(ball_power ? SFX_POWER : SFX_KICK);
    struck(i, move, frames);
}

// Only just got there: it pops up high and drops just behind the other player,
// at head height as it reaches them, for them to slam.
static void pop_up(uint8_t i, uint8_t move, uint8_t frames) {
    int16_t bx = PX(ball_x), by = PX(ball_y);
    int16_t land = PX(pl_y[i ^ 1]) + (i == SIDE_YOU ? -10 : 10);
    int16_t tx = CENTER_X + (int16_t)(rand() & 31) - 16;
    ball_plain();
    ball_vz = POP_LIFT;
    ball_pop = 1;
    // 42 frames in the air: 3/8 of the distance a frame.
    launch(tx - bx, land - by, (uint8_t)clamp16((abs16(land - by) * 3) >> 3, 8, 60));
    sfx_play(SFX_TOUCH);
    struck(i, move, frames);
}

static void knock_down(uint8_t i) {
    // On fire, met unready or at full stretch: down they go, and it goes on, slowed.
    pl_move[i] = MOVE_DOWN;
    pl_move_timer[i] = DOWN_FRAMES;
    unready(i);
    ball_power = 0;
    ball_vx >>= 1;
    ball_vy >>= 1;
    sfx_play(SFX_PARRY);
}

// Planted and in reach: a clean strike, unless it's on fire and the meter isn't
// three quarters full, when it's all they can do to pop it up.
static void meet(uint8_t i, uint8_t move) {
    if (ball_power && pl_charge[i] < FIRE_MEET) pop_up(i, move, KICK_FRAMES);
    else strike(i, move);
}

static void slide(uint8_t i, int8_t vx, int8_t vy) {
    pl_slide_vx[i] = vx;
    pl_slide_vy[i] = vy;
    pl_face[i] = vx < 0;
    pl_move[i] = MOVE_SLIDE;
    pl_move_timer[i] = SLIDE_FRAMES;
    unready(i);
    sfx_play(SFX_CATCH);
}

// Keep player i in their half.
static void keep_in(uint8_t i) {
    int16_t y0 = i == SIDE_YOU ? TO_SUB(HALFWAY + 6) : TO_SUB(COURT_FAR + 4);
    int16_t y1 = i == SIDE_YOU ? TO_SUB(COURT_NEAR - 4) : TO_SUB(HALFWAY - 6);
    pl_x[i] = clamp16(pl_x[i], TO_SUB(COURT_X0 + 4), TO_SUB(COURT_X1 - 4));
    pl_y[i] = clamp16(pl_y[i], y0, y1);
}

// Planted, the ball where it can be struck now: which strike, by its height
// (MOVE_NONE: not in reach).
static uint8_t in_reach(void) {
    int16_t ax = abs16(rel_x);
    if (ax <= REACH_X && rel_fwd >= -REACH_BACK && rel_fwd <= REACH_FWD) {
        if (rel_z < 5) return MOVE_KICK;
        if (rel_z < 16) return MOVE_VOLLEY;
        if (rel_z < 28) return MOVE_HEADER;
    } else if (ax <= REACH_X - 2 && rel_fwd < -2 && rel_fwd >= -14 && rel_z >= 10 && rel_z < 32) {
        return MOVE_BICYCLE;  // just over your head: over it goes, backwards
    }
    return MOVE_NONE;
}

// A player's frame: move (dx, dy: -1, 0 or 1) unless planted, charge, then strike,
// slide, or take it unready.
static void player_update(uint8_t i, int8_t dx, int8_t dy) {
    pl_vx[i] = pl_vy[i] = 0;
    if (pl_move_timer[i]) {
        pl_move_timer[i]--;
        if (pl_move[i] != MOVE_SLIDE || pl_move_timer[i] < SLIDE_FRAMES - SLIDE_LUNGE) return;
        // The slide: along the court, reaching for it.
        pl_x[i] += pl_slide_vx[i];
        pl_y[i] += pl_slide_vy[i];
        keep_in(i);
        relate(i);
        if (abs16(rel_x) <= REACH_X && rel_fwd >= -REACH_BACK - 2 && rel_fwd <= REACH_FWD && rel_z < 16) {
            if (ball_power) {
                knock_down(i);
                return;
            }
            pop_up(i, MOVE_SLIDE, 0);
            pl_move_timer[i] = SLIDE_REST;  // got there: up sooner
        }
        return;
    }
    // Planted, readying a strike: still. Serving: along the line only.
    if (pl_shot[i] || (phase == PHASE_TOSS && server == i)) dx = dy = 0;
    if (phase != PHASE_RALLY && server == i) dy = 0;
    int8_t speed = i == SIDE_YOU ? SPEED_YOU : SPEED_CPU;
    int8_t vx = dx > 0 ? speed : dx < 0 ? -speed : 0;
    int8_t vy = dy > 0 ? speed : dy < 0 ? -speed : 0;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    pl_x[i] += vx;
    pl_y[i] += vy;
    keep_in(i);
    if (vx) pl_face[i] = vx < 0;

    if (pl_shot[i]) {
        if (pl_charge[i] < CHARGE_MAX) {
            if (frame_count & 1 && ++pl_charge[i] == CHARGE_MAX && i == SIDE_YOU) sfx_play(SFX_CHARGE);
        } else if (phase == PHASE_RALLY && ++pl_hold[i] > HOLD_MAX) {
            unready(i);  // waited too long: the strike is given up
        }
    }
    if (goal_timer || phase == PHASE_SERVE) return;
    if (phase == PHASE_TOSS) {
        // The serve: struck as the toss falls back.
        if (server == i && ball_vz < 0 && ball_z < TO_SUB(TOSS_STRIKE)) strike(i, MOVE_KICK);
        return;
    }
    relate(i);
    int16_t ax = abs16(rel_x);
    if (pl_shot[i]) {
        uint8_t move = in_reach();
        if (move) {
            meet(i, move);
            return;
        }
        if (loose_here(i)) {
            // Loose and out of reach: slide to it if it's near enough, else
            // stand down and go and get it.
            if (ax <= SLIDE_REACH && abs16(rel_fwd) <= SLIDE_REACH) {
                uint8_t d = dir_to(rel_x, PX(ball_y) - PX(pl_y[i]));
                slide(i, (int8_t)(((int16_t)cos_t[d] * SLIDE_SPEED) >> 6), (int8_t)(((int16_t)SIN(d) * SLIDE_SPEED) >> 6));
            } else unready(i);
        } else if (!coming(i)) {
            unready(i);  // it's going away: stand down
        } else if (ax > REACH_X && ax <= SLIDE_REACH && rel_fwd >= -REACH_BACK && rel_fwd <= REACH_FWD + 4 &&
                   rel_z < 16) {
            slide(i, rel_x < 0 ? -SLIDE_SPEED : SLIDE_SPEED, 0);  // about to go past: go for it
        }
        return;
    }
    // Unready, and it hits you: it pops up off you; on fire, it knocks you down.
    if (ax <= 6 && rel_fwd >= -3 && rel_fwd <= 4 && rel_z < 20 && coming(i) && ball_speed() >= SLOW) {
        if (ball_power) knock_down(i);
        else pop_up(i, MOVE_BLOCK, BLOCK_FRAMES);
    }
}

// --- the CPU ---
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

// Pick a strike: a lob over you if you're up near halfway, a drop shot if you're
// back on your line, else a curve one time in four, else power.
static void cpu_ready(void) {
    uint8_t r = rand();
    int16_t yy = PX(pl_y[SIDE_YOU]);
    if (yy < 160 && (r & 3) == 0) ready(SIDE_CPU, 1), ready(SIDE_CPU, 0);
    else if (yy > 196 && (r & 3) == 1) ready(SIDE_CPU, 0), ready(SIDE_CPU, 1);
    else ready(SIDE_CPU, ((r >> 2) & 3) == 0);
}

// The CPU: get across to where the ball's coming, plant in time, tap now and
// then, aim away from you. It slides by the same rules as you.
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
                unready(SIDE_CPU);
                pl_shot[SIDE_CPU] = (rand() & 3) == 0 ? SHOT_CURVE : SHOT_POWER;
                phase = PHASE_TOSS;
                ball_vz = TOSS_LIFT;
                sfx_play(SFX_TOUCH);
            }
        }
    } else if (phase == PHASE_RALLY && !goal_timer && !pl_move_timer[SIDE_CPU]) {
        uint8_t loose = loose_here(SIDE_CPU);
        if (loose || coming(SIDE_CPU)) {
            int16_t dist = by - cy;
            if (loose) {
                // Get behind it.
                tx = bx;
                ty = by - 6;
                dist = 0;
            } else {
                if (!(frame_count & 7)) cpu_tx = PX(predict_x(TO_SUB(HOME_CPU)));
                tx = cpu_tx;
                if (ball_z > TO_SUB(8) && ball_vz > 0 && !ball_pop) ty = HOME_CPU - 12;  // a lob: back up
            }
            uint8_t there = abs16(tx - cx) <= 3 && abs16(ty - cy) <= 3;
            if (!pl_shot[SIDE_CPU]) {
                if ((there && dist < CPU_PLANT) || (!loose && dist < CPU_LATE)) cpu_ready();
            } else if (!(frame_count & 7) && (rand() & 1)) {
                ready(SIDE_CPU, pl_shot[SIDE_CPU] == SHOT_CURVE);  // a tap
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
static void point(uint8_t side, uint8_t reason) {
    score[side]++;
    last_scorer = side;
    point_reason = reason;
    goal_timer = GOAL_FRAMES;
    ball_power = 0;
    sfx_play(reason == POINT_GOAL ? SFX_GOAL : SFX_WHISTLE);
    note(GOAL_FRAMES);
}

// The ball has come down (tennis rules): first on the striker's own side, it
// didn't get over (the other side's point); a second time, or dead, it's the
// striker's.
static void landed(void) {
    uint8_t s = last_touch;
    uint8_t half = PX(ball_y) < HALFWAY ? SIDE_CPU : SIDE_YOU;
    if (!ball_landings && half == s) {
        point(s ^ 1, POINT_NET);
        return;
    }
    if (++ball_landings >= 2 || !ball_vz) point(s, POINT_BOUNCE);
}

static void board(void) {
    bounces++;
    ball_curve_t = 0;
    sfx_play(SFX_BOARD);
}

static void ball_update(void) {
    if (phase == PHASE_SERVE) {
        // At the server's feet, in front of them and to the side.
        ball_x = pl_x[server] + TO_SUB(8);
        ball_y = pl_y[server] + (server == SIDE_YOU ? -TO_SUB(6) : TO_SUB(6));
        return;
    }
    // A curve: pushed across, a sixteenth of a SUB at a time.
    if (ball_curve_t) {
        ball_curve_t--;
        ball_cacc += ball_curve;
        ball_vx += ball_cacc >> 4;
        ball_cacc &= 15;
    }
    int16_t x = ball_x + ball_vx, y = ball_y + ball_vy;
    // The net: crossing halfway lower than its top, it's stopped and drops back.
    if ((PX(y) < HALFWAY) != (PX(ball_y) < HALFWAY) && ball_z < TO_SUB(NET_H)) {
        y = ball_y;
        ball_vy = -(ball_vy >> 2);
        ball_vx >>= 1;
        ball_curve_t = 0;
        sfx_play(SFX_BOARD);
    }
    // In the air: gravity, and a bounce as it lands. A drop shot barely bounces;
    // a pop-up can only be slammed before it lands.
    if (ball_z || ball_vz) {
        ball_vz -= GRAVITY;
        ball_z += ball_vz;
        if (ball_z <= 0) {
            ball_z = 0;
            ball_pop = 0;
            if (ball_drop) {
                ball_drop = 0;
                ball_vz = (-ball_vz) >> 2;
                if (ball_vz < 8) ball_vz = 8;
                ball_vx >>= 1;
                ball_vy >>= 1;
            } else ball_vz = ball_vz < -8 ? (-ball_vz) >> 1 : 0;
            if (phase == PHASE_RALLY && !goal_timer) landed();
        }
    }
    if (!ball_z) {
        if (!ball_curve_t) ball_vx = drag(ball_vx);  // a curve keeps its push across
        ball_vy = drag(ball_vy);
    }
    if (ball_power && ball_speed() < POWER_FADE) ball_power = 0;
    if (ball_vx || ball_vy) ball_roll++;

    int16_t px = PX(x), py = PX(y);
    uint8_t in_mouth = px > GOAL_X0 + BALL_R && px < GOAL_X1 - BALL_R;
    uint8_t in_net = PX(ball_y) < COURT_FAR || PX(ball_y) > COURT_NEAR;
    // Past a goal line: in through the mouth, or off the boards.
    if (py < COURT_FAR + BALL_R || py > COURT_NEAR - BALL_R) {
        if (in_net || (in_mouth && PX(ball_z) < CROSSBAR)) {
            // In the goal: the posts keep it in the mouth, the back of the net stops it.
            ball_curve_t = 0;
            if (px < GOAL_X0 + BALL_R) x = TO_SUB(GOAL_X0 + BALL_R), ball_vx = 0;
            if (px > GOAL_X1 - BALL_R) x = TO_SUB(GOAL_X1 - BALL_R), ball_vx = 0;
            if (py < COURT_FAR - NET_BACK) y = TO_SUB(COURT_FAR - NET_BACK), ball_vy = 0;
            if (py > COURT_NEAR + NET_BACK) y = TO_SUB(COURT_NEAR + NET_BACK), ball_vy = 0;
            if (PX(ball_z) > CROSSBAR - 2) ball_z = TO_SUB(CROSSBAR - 2), ball_vz = 0;
            if (!goal_timer) {
                if (py < COURT_FAR - BALL_R) point(SIDE_YOU, POINT_GOAL);
                else if (py > COURT_NEAR + BALL_R) point(SIDE_CPU, POINT_GOAL);
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

    // You: the d-pad runs; A and B ready a strike (and combine, and tap).
    if (joy_pressed & (J_A | J_B)) {
        uint8_t b = !(joy_pressed & J_A);
        if (phase == PHASE_SERVE) {
            if (server == SIDE_YOU && !serve_timer) {
                unready(SIDE_YOU);
                pl_shot[SIDE_YOU] = b ? SHOT_CURVE : SHOT_POWER;
                pl_aim[SIDE_YOU] = joy & J_LEFT ? 0 : joy & J_RIGHT ? 2 : 1;
                phase = PHASE_TOSS;
                ball_vz = TOSS_LIFT;
                sfx_play(SFX_TOUCH);
            }
        } else if (pl_shot[SIDE_YOU] || can_ready(SIDE_YOU)) {
            ready(SIDE_YOU, b);
        }
    }
    // Planted (or serving), LEFT or RIGHT sets your aim, and it sticks.
    if (pl_shot[SIDE_YOU]) {
        if (joy & J_LEFT) pl_aim[SIDE_YOU] = 0;
        else if (joy & J_RIGHT) pl_aim[SIDE_YOU] = 2;
    }
    int8_t dx = joy & J_RIGHT ? 1 : joy & J_LEFT ? -1 : 0;
    int8_t dy = joy & J_DOWN ? 1 : joy & J_UP ? -1 : 0;
    player_update(SIDE_YOU, dx, dy);
    cpu_update();
    ball_update();
    meter_update();
    draw();
}
