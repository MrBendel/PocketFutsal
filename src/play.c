#pragma bank 255
// Play: futnet singles (football-tennis) against the CPU, Mario Tennis style. You
// play the near half of a court seen in perspective, the CPU the far one, a net
// between. Two touches a side, one bounce; the ball must go over and land in (a
// line is in). Sets are first to 11 by two (15 at most); the match is one set or
// the best of three.
//
// The ball is always loose: you strike it rather than carry it. With the ball
// coming your way (or loose in your half), A readies a power shot, straight where
// the d-pad points, and B a curve, bending in to the side the d-pad holds; B then
// A is a lob, A then B a drop shot. Readying plants you: the meter fills as you
// wait, each tap of the button again adds more, and the strike goes by itself as
// the ball arrives. Which strike depends on the ball's height: a kick on the
// ground, a volley at the waist, a header, or a bicycle kick for one that's just
// gone over your head. Planted out of reach, you slide for it, and lie there a
// while. A ball only just reached (a slide, a ball on fire met with less than
// three quarters of a meter) pops up over the net, where met in the air with half a
// meter it's a slam, on fire. A full meter sets the ball on fire too. Unready, your
// first touch controls it, up in front of you, to play with your second; on fire,
// it knocks you down and goes on.
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
#define SPEED_CPU 20
#define HOLD_MAX 90       // frames at a full meter before the player gives up the strike
#define REACH_X 12        // court px either side a strike reaches
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
#define BOUNCE_MIN 30     // a first bounce gets up at least this: about 9 px, 20 frames
#define LOB_BOUNCE 40     // a lob sits up where it lands: about 16 px, 27 frames, slowed to a quarter
#define DROP_HOP 24       // a drop shot's: about 6 px, 16 frames
#define POWER_FADE 40     // a ball on fire burns out below this speed (|vx| + |vy|)
#define SLOW 16           // a ball slower than this (|vx| + |vy|) is loose
#define FIRE_MEET 20      // the charge it takes to meet a ball on fire cleanly
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
#define CPU_LATE 40       // px: and this close, in place or not
#define CONTROL_LIFT 56   // a control touch: about 32 px up, 37 frames
#define CONTROL_FRAMES 8  // its pose: then you can ready your second touch

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
uint8_t pl_flash[2];  // charging: 0 own colors, 1 white, 2 fire
uint8_t pl_aim[2];
int8_t pl_slide_vx[2], pl_slide_vy[2];
int16_t cpu_tx, cpu_ty;
int16_t meet_x[2], meet_y[2];  // where each side means to meet the ball (court px)
uint8_t meet_out[2];  // the ball's landing out: that side lets it go
uint8_t ball_flight;    // counts each change in the ball's flight (a touch, a bounce, the net)
uint8_t flight_frame;   // the frame it last changed on
uint8_t meet_flight[2]; // the flight each side's meet_* was traced for
uint8_t predict_frame;  // the frame a trace last ran on
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
uint8_t ball_lob;
uint8_t last_touch;
uint8_t sets[2];
uint8_t match_sets = 1;
uint8_t holder;
uint8_t touches;
uint8_t ball_netted;
uint8_t ball_serve;
uint8_t set_won;  // this point won the set
uint8_t ball_roll;  // counts up as the ball rolls: its two frames

uint8_t meter_shown;  // the meter's value on screen (0xFF: redraw)
uint8_t meter_buf[8];  // its tiles, queued to the window's map
uint8_t note_timer;    // the HUD shows a note (GOAL!) while this counts down
uint8_t hud_pending;   // redraw the HUD at the start of the next frame

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

// --- the HUD: YOU 0 [meter] 0 CPU (this set's points), or a note for a moment ---
static void hud_draw(void) {
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
}

// The note's drawn at the start of the next frame (hud_pending): a point lands in
// the middle of a busy one, and drawing text is slow.
static void note(uint8_t frames) {
    note_timer = frames;
    hud_pending = 1;
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
    ball_power = ball_curve = ball_curve_t = ball_pop = ball_drop = ball_lob = 0;
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
    last_touch = holder = side;
    touches = ball_landings = ball_netted = ball_serve = 0;
    meet_flight[0] = meet_flight[1] = ball_flight - 1;  // no trace of this point yet
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
        else if (m == MOVE_VOLLEY || m == MOVE_CONTROL) f = F_VOLLEY;
        else if (m == MOVE_HEADER) f = F_HEADER, lift = 4;
        else if (m == MOVE_BICYCLE) f = F_BICYCLE, lift = 8;
        else if (m == MOVE_SLIDE) lie = 1, flip = !flip;  // feet first
        else if (m == MOVE_DOWN) lie = 1;
        else if (pl_shot[i] || (phase == PHASE_TOSS && server == i)) f = F_READY;
        else if (pl_vx[i] || pl_vy[i]) f = (frame_count & 8) ? F_RUN_A : F_RUN_B;
        uint8_t tile = lie ? T_LYING : (f << 3) + (i == SIDE_CPU ? VIEW_FRONT : 0);
        uint8_t pal = i == SIDE_YOU ? SPR_PAL_YOU : SPR_PAL_CPU;
        if (pl_shot[i]) {
            // Charging: flashing through white and fire, a step every 8 frames, 4
            // from half a meter, 2 full. (A counter, not frame_count % 3: SDCC's
            // modulo costs thousands of cycles.)
            uint8_t c = pl_charge[i];
            uint8_t mask = c == CHARGE_MAX ? 1 : c >= CHARGE_MAX / 2 ? 3 : 7;
            if (!((uint8_t)frame_count & mask) && ++pl_flash[i] > 2) pl_flash[i] = 0;
            if (pl_flash[i] == 1) pal = SPR_PAL_FLASH;
            else if (pl_flash[i] == 2 && c >= CHARGE_MAX / 2) pal = SPR_PAL_POWER;
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

// Coming this way (toward player i's baseline).
static uint8_t coming(uint8_t i) {
    return i == SIDE_YOU ? ball_vy > 0 : ball_vy < 0;
}

// Slow, in player i's half: theirs to go and get.
static uint8_t loose_here(uint8_t i) {
    if (ball_speed() >= SLOW) return 0;
    return i == SIDE_YOU ? PX(ball_y) >= HALFWAY : PX(ball_y) < HALFWAY;
}

// The ball's on its way to player i (or loose in their half): a queued strike
// charges, and the game runs them to it.
static uint8_t live_for(uint8_t i) {
    return coming(i) || loose_here(i);
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
static void point(uint8_t side, uint8_t reason);  // the ball's rules, below
static uint8_t in_court(int16_t px, int16_t py);
static void predict(uint8_t i);
static void want_predict(uint8_t i);

// Set the ball going toward (dx, dy) px away at speed (SUB a frame, |vx| + |vy|,
// the same measure as ball_speed): exact, where the 32 directions would round a
// slight angle away. A division: only as it's struck.
static void launch(int16_t dx, int16_t dy, uint8_t speed) {
    // At most half as far across as down the court (about 27 degrees): wider, and
    // a ball that lands in runs off the court faster than anyone can follow it.
    int16_t wide = abs16(dy) >> 1;
    if (dx > wide) dx = wide;
    else if (dx < -wide) dx = -wide;
    int16_t sum = abs16(dx) + abs16(dy);
    if (!sum) sum = 1;
    ball_vx = (dx * speed) / sum;
    int16_t vy = (int16_t)speed - abs16(ball_vx);  // the rest of it: no second division
    ball_vy = dy < 0 ? -vy : vy;
}

// A touch by player i (in their half: they're the holder). A third is a fault.
static void new_flight(void) {
    ball_flight++;
    flight_frame = (uint8_t)frame_count;
}

static void struck(uint8_t i, uint8_t move, uint8_t frames) {
    new_flight();
    if (phase != PHASE_RALLY) ball_serve = 1;  // the serve: it must go over
    last_touch = i;
    phase = PHASE_RALLY;
    if (++touches > 2 && !goal_timer) point(i ^ 1, POINT_TOUCHES);
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
// with vz = 1.5 (t + 1) - z0 / t (gravity 3, a frame at a time: after t frames
// it's z0 + vz t - 1.5 t (t + 1) up). Every strike arcs, so the ball is in the
// air, its shadow under it, and clears the net if it's meant to.
// If it's to go over (clear), it must be NET_H + 3 px up as it gets to the net,
// t_n frames on, so a fast flat strike is lifted, and lands deeper (or long).
static void arc(int16_t land, uint8_t clear) {
    int16_t vy = abs16(ball_vy);
    if (!vy) vy = 1;
    int16_t t = clamp16((abs16(land - PX(ball_y)) << 4) / vy, 6, 80);
    ball_vz = ((3 * (t + 1)) >> 1) - ball_z / t;
    if (!clear) return;
    int16_t tn = clamp16((abs16(HALFWAY - PX(ball_y)) << 4) / vy, 1, 80);
    int16_t drop = (3 * tn * (tn + 1)) >> 1;  // gravity's pull by then
    // High enough already (multiplying, not dividing): leave it.
    if (ball_z + ball_vz * tn - drop >= TO_SUB(NET_H + 3)) return;
    ball_vz = (TO_SUB(NET_H + 3) + drop - ball_z) / tn;
}

// A clean strike: the readied shot, shaped by the ball's height (move).
static void strike(uint8_t i, uint8_t move) {
    uint8_t was_pop = ball_pop;
    int16_t bx = PX(ball_x), by = PX(ball_y);
    // Down the middle, or toward a sideline: the way you're holding, or last held
    // while planted (the CPU picks its side as it readies).
    uint8_t aim = pl_aim[i];
    if (i == SIDE_YOU) {
        if (joy & J_LEFT) aim = 0;
        else if (joy & J_RIGHT) aim = 2;
    } else aim = cpu_aim;
    int16_t tx = aim == 0 ? COURT_X0 + 24 : aim == 2 ? COURT_X1 - 24 : CENTER_X;
    uint8_t c = pl_charge[i], kind = pl_shot[i], speed, frames = KICK_FRAMES;
    if (phase != PHASE_RALLY && c == CHARGE_MAX) c--;  // no serves on fire
    uint8_t fire = c == CHARGE_MAX;
    // Where it comes down: deeper the more it's charged. Barely charged at all,
    // it's scuffed: it comes down on your own side, short of the net.
    uint8_t scuff = c < SCUFF && phase == PHASE_RALLY;
    int16_t land = scuff ? other_y(i, -10) : other_y(i, 24 + c + (c >> 1));
    ball_plain();
    if (was_pop && move != MOVE_KICK && c >= CHARGE_MAX / 2) {
        // A pop-up (or your own control) met in the air, half charged: a slam, on
        // fire, deep.
        ball_power = 1;
        if (move == MOVE_HEADER) frames = KICK_FRAMES + 4;
        else if (move == MOVE_BICYCLE) frames = BICYCLE_FRAMES;
        land = other_y(i, 64);
        launch(tx - bx, land - by, POWER_SPEED);
        arc(land, 1);
    } else if (move == MOVE_HEADER || move == MOVE_BICYCLE) {
        // In the air: always straight. On fire at a full meter (a bicycle kick, at half).
        if (fire || (move == MOVE_BICYCLE && c >= CHARGE_MAX / 2)) {
            speed = POWER_SPEED;
            ball_power = 1;
        } else speed = move == MOVE_HEADER ? HEADER_SPEED + (c >> 1) : DRIVE_SPEED + 16 + (c >> 1);
        frames = move == MOVE_HEADER ? KICK_FRAMES + 4 : BICYCLE_FRAMES;
        if (!scuff) land = move == MOVE_HEADER ? other_y(i, 28 + (c >> 1)) : other_y(i, 56);
        launch(tx - bx, land - by, speed);
        arc(land, !scuff);
    } else if (kind == SHOT_LOB || kind == SHOT_DROP) {
        // Placed: a lob drops just behind the other player, a drop shot just past
        // the net. A lob's time in the air is known, so its speed is the distance
        // over it (7/16 of it a frame); a drop shot goes at 4/5 of the distance a
        // frame (about 20 frames), up to 64, lifted to come down there.
        int16_t dist;
        if (kind == SHOT_LOB) {
            land = PX(pl_y[i ^ 1]) + (i == SIDE_YOU ? -12 : 12);
            dist = abs16(land - by);
            speed = (uint8_t)clamp16((dist * 7) >> 4, 16, 72);
            ball_vz = move == MOVE_VOLLEY ? CHIP_LIFT - 8 : CHIP_LIFT;
            ball_lob = 1;
        } else {
            land = other_y(i, 18);
            dist = abs16(land - by);
            speed = (uint8_t)clamp16((dist * 4) / 5, 12, 64);
            ball_drop = 1;
        }
        launch(tx - bx, land - by, speed);
        if (kind == SHOT_DROP) arc(land, 1);  // its speed is capped: the lift makes up the distance
    } else if (kind == SHOT_CURVE) {
        // Set off CURVE_OFS px wide of the target and bent back in: a steady push
        // across for its time in flight, t frames (at its starting speed: the ball
        // slows, so it drifts on after the push ends). 384 * CURVE_OFS / t^2
        // sixteenths of a SUB a frame, each frame, brings it in to the target
        // (tuned: build/scratch/curve.mjs).
        int8_t side = aim == 0 ? -1 : aim == 2 ? 1 : PX(pl_x[i]) < CENTER_X ? 1 : -1;
        speed = fire ? POWER_SPEED - 16 : CURVE_SPEED + c;
        ball_power = fire;
        int16_t t = clamp16((abs16(land - by) << 4) / speed, 16, 90);
        int16_t push = clamp16((384 * CURVE_OFS) / (t * t), 1, 60);
        ball_curve = side > 0 ? (int8_t)push : -(int8_t)push;
        ball_curve_t = (uint8_t)t;
        launch(tx - side * CURVE_OFS - bx, land - by, speed);
        arc(land, !scuff);
    } else {
        // Power: straight, a low fast arc.
        speed = fire ? POWER_SPEED : DRIVE_SPEED + c + (move == MOVE_VOLLEY ? 8 : 0);
        ball_power = fire;
        launch(tx - bx, land - by, speed);
        arc(land, !scuff);
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

// Reached unready: a first touch, the ball up in front of you, to play again with
// your second (ready a strike as it comes down: half a meter makes it a slam).
// (struck() first: with it last, SDCC 4.5's code kept i in L, overwrote HL with
// &ball_vy, and passed struck() the address's low byte as i.)
static void control(uint8_t i) {
    struck(i, MOVE_CONTROL, CONTROL_FRAMES);
    ball_plain();
    ball_vx = 0;
    ball_vy = last_touch == SIDE_YOU ? -3 : 3;  // 37 frames: 7 px ahead, still in reach
    ball_vz = CONTROL_LIFT;
    ball_pop = 1;
    sfx_play(SFX_TOUCH);
}

static void knock_down(uint8_t i) {
    new_flight();
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
    // Queued with the ball on its way: the game moves you (below), not the d-pad.
    // Serving: still once it's tossed.
    if ((pl_shot[i] && phase == PHASE_RALLY && live_for(i)) || (phase == PHASE_TOSS && server == i)) dx = dy = 0;
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

    if (pl_shot[i] && (phase != PHASE_RALLY || live_for(i))) {
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
        // Your own control, still going up: wait for it to come down (charging).
        if (ball_pop && last_touch == i && ball_vz > 0) return;
        uint8_t move = in_reach();
        if (move) {
            meet(i, move);
            return;
        }
        // Right over you, too high yet: wait for it.
        if (ax <= REACH_X && rel_fwd >= -14 && rel_fwd <= REACH_FWD && rel_z >= 16) return;
        uint8_t loose = loose_here(i);
        if (!loose && !coming(i)) return;  // queued: it waits for the ball to come
        // The game runs you to where to meet it, half as fast again as running,
        // anywhere in your half: queue it up and you get there. Going out, it
        // leaves it.
        want_predict(i);
        int16_t tx = loose ? PX(ball_x) : meet_x[i];
        int16_t ty = loose ? PX(ball_y) : meet_y[i];
        ty += i == SIDE_YOU ? 6 : -6;  // stand just behind it: it's in front
        int16_t dx = tx - PX(pl_x[i]), dy = ty - PX(pl_y[i]);
        // (Only on a trace of this flight: an old one would send you the wrong way.)
        if (loose || (meet_flight[i] == ball_flight && !meet_out[i])) {
            int8_t sp = i == SIDE_YOU ? SPEED_YOU + (SPEED_YOU >> 1) : SPEED_CPU + (SPEED_CPU >> 1);
            if (dx > 1) pl_x[i] += sp, pl_vx[i] = sp;
            else if (dx < -1) pl_x[i] -= sp, pl_vx[i] = -sp;
            if (dy > 1) pl_y[i] += sp, pl_vy[i] = sp;
            else if (dy < -1) pl_y[i] -= sp, pl_vy[i] = -sp;
            keep_in(i);
        }
        // About to go past, out of reach: slide for it, as soon as the slide only
        // just gets there in time (the ball's frames to reach you, rel_fwd / |vy|,
        // down to the slide's frames across, (ax - REACH_X + 2) / SLIDE_SPEED).
        if (!loose && !meet_out[i] && ax > REACH_X && ax <= SLIDE_REACH && rel_fwd >= -REACH_BACK && rel_z < 16 &&
            rel_fwd * SLIDE_SPEED <= (ax - REACH_X + 2) * abs16(ball_vy))
            slide(i, rel_x < 0 ? -SLIDE_SPEED : SLIDE_SPEED, 0);
        return;
    }
    // Unready, and it reaches you: you control it; on fire, it knocks you down.
    if (ax <= 6 && rel_fwd >= -3 && rel_fwd <= 4 && rel_z < 20 && coming(i) && ball_speed() >= SLOW) {
        if (ball_power) knock_down(i);
        else control(i);
    }
}

// --- where to meet it ---
// Where side i should meet the ball: its flight traced on (gravity, a bounce, a
// lob sitting up, not the curve) to where it first lands, then 8 frames more,
// rising off the bounce; or where it gets to i's baseline first. Landing out (and
// not yet bounced in), i lets it go: meet_out[i]. 8 frames a step (gravity over 8
// frames: vz down 24, z up 8 vz - 108), up to 15 steps; run by want_predict.
static void predict(uint8_t i) {
    int16_t x = ball_x, y = ball_y, z = ball_z, vz = ball_vz;
    int16_t vx4 = ball_vx << 3, vy4 = ball_vy << 3;
    uint8_t down = ball_landings, out = 0;
    meet_flight[i] = ball_flight;
    for (uint8_t k = 0; k < 15; k++) {
        x += vx4;
        y += vy4;
        if (z || vz) {
            z += (vz << 3) - 108;
            vz -= GRAVITY * 8;
            if (z <= 0) {
                z = 0;
                vz = (-vz * 3) >> 2;
                if (!down) {
                    down = 1;
                    if (!in_court(PX(x), PX(y))) {
                        out = 1;
                        break;
                    }
                    if (ball_lob) vx4 >>= 2, vy4 >>= 2, vz = LOB_BOUNCE;
                    else {
                        if (vz < BOUNCE_MIN) vz = BOUNCE_MIN;
                        vx4 -= vx4 >> 2;
                        vy4 -= vy4 >> 2;
                    }
                }
            }
        }
        if (down) break;  // a step (8 frames) past the bounce: rising off it
        if (i == SIDE_CPU ? PX(y) <= COURT_FAR + 4 : PX(y) >= COURT_NEAR - 4) break;
    }
    meet_out[i] = out;
    meet_x[i] = clamp16(PX(x), COURT_X0 + 4, COURT_X1 - 4);
    meet_y[i] = i == SIDE_CPU ? clamp16(PX(y), COURT_FAR + 4, HALFWAY - 6) : clamp16(PX(y), HALFWAY + 6, COURT_NEAR - 4);
}

// Trace it again when its flight has changed (not on the very frame: that's a
// strike's, busy already), now and then anyway (a placed ball, the curve), each
// side on its own frames.
// One trace a frame at most (the other side's waits a frame).
static void want_predict(uint8_t i) {
    uint8_t f = (uint8_t)frame_count;
    if (f == predict_frame) return;
    if ((meet_flight[i] != ball_flight && f != flight_frame) || ((f + (i << 3)) & 15) == 0) {
        predict_frame = f;
        predict(i);
    }
}

// --- the CPU ---
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
                want_predict(SIDE_CPU);
                if (meet_out[SIDE_CPU]) {
                    // Going out: let it go (out of its way, back toward the middle).
                    tx = bx < CENTER_X ? CENTER_X + 24 : CENTER_X - 24;
                    ty = cy;
                    dist = 255;
                } else {
                    tx = meet_x[SIDE_CPU];
                    ty = meet_y[SIDE_CPU];
                    dist = abs16(by - cy);
                }
            }
            uint8_t there = abs16(tx - cx) <= 3 && abs16(ty - cy) <= 3;
            if (meet_out[SIDE_CPU]) {
                if (pl_shot[SIDE_CPU]) unready(SIDE_CPU);
            } else if (!pl_shot[SIDE_CPU]) {
                // By your rules: it queues as soon as the ball's on its way (the
                // game runs it there), with a moment's reaction.
                if (there || dist < CPU_PLANT || !(frame_count & 3)) cpu_ready();
            } else if (!(frame_count & 7) && (rand() & 3)) {
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
// A point to `side`. A set is first to SET_POINTS by two, or SET_CAP; set_won
// notes it for the end of the celebration.
static void point(uint8_t side, uint8_t reason) {
    uint8_t o = side ^ 1;
    score[side]++;
    last_scorer = side;
    point_reason = reason;
    goal_timer = GOAL_FRAMES;
    ball_power = 0;
    if ((score[side] >= SET_POINTS && score[side] >= score[o] + 2) || score[side] >= SET_CAP) {
        sets[side]++;
        set_won = 1;
    }
    sfx_play(SFX_GOAL);
    note(GOAL_FRAMES);
}

static uint8_t in_court(int16_t px, int16_t py) {
    return px >= COURT_X0 && px <= COURT_X1 && py >= COURT_FAR && py <= COURT_NEAR;  // lines are in
}

// The ball has come down (futnet singles: one bounce a side). Out on the full, it's
// the striker's fault; off the net, or a serve that didn't go over, it's dead on
// their side; in, it's a bounce in the holder's half, and a second (or a ball that
// dies there) is the other side's point.
static void landed(void) {
    if (!in_court(PX(ball_x), PX(ball_y))) {
        if (ball_landings) point(holder ^ 1, POINT_BOUNCE);  // in, then out: never played
        else point(last_touch ^ 1, POINT_OUT);
    } else if (ball_netted) {
        point(holder ^ 1, POINT_NET);
    } else if (ball_serve) {
        point(server ^ 1, POINT_FAULT);
    } else if (++ball_landings >= 2 || !ball_vz) {
        point(holder ^ 1, POINT_BOUNCE);
    }
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
    if ((PX(y) < HALFWAY) != (PX(ball_y) < HALFWAY)) {
        if (ball_z < TO_SUB(NET_H) && PX(x) >= COURT_X0 - 6 && PX(x) <= COURT_X1 + 5) {
            // Into the net: stopped, it drops back, dead.
            y = ball_y;
            ball_vy = -(ball_vy >> 2);
            ball_vx >>= 1;
            ball_curve_t = 0;
            ball_netted = 1;
            new_flight();
            sfx_play(SFX_BOARD);
        } else {
            // Over: the other side's ball now, their two touches and one bounce.
            holder = PX(y) < HALFWAY ? SIDE_CPU : SIDE_YOU;
            touches = ball_landings = 0;
            ball_serve = 0;
        }
    }
    // In the air: gravity, and a bounce as it lands: three quarters as high again.
    // The first bounce in a half always gets up a playable height (a futnet ball
    // bounces over half a metre); a drop shot's only just; a lob sits up. A pop-up can only be
    // slammed before it lands.
    if (ball_z || ball_vz) {
        ball_vz -= GRAVITY;
        ball_z += ball_vz;
        if (ball_z <= 0) {
            int16_t up = (-ball_vz * 3) >> 2;
            uint8_t first = !ball_landings;
            ball_z = 0;
            ball_pop = 0;
            if (ball_drop) {
                ball_drop = 0;
                up = (-ball_vz) >> 2;
                if (up < DROP_HOP) up = DROP_HOP;
                ball_vx >>= 1;
                ball_vy >>= 1;
            } else if (ball_lob) {
                // Backspin: it sits up where it lands, to be turned and reached.
                ball_lob = 0;
                if (up < LOB_BOUNCE) up = LOB_BOUNCE;
                ball_vx >>= 2;
                ball_vy >>= 2;
            } else {
                if (first && up < BOUNCE_MIN) up = BOUNCE_MIN;
                ball_vx -= ball_vx >> 2;  // the court takes a quarter of its pace
                ball_vy -= ball_vy >> 2;
            }
            ball_vz = up > 8 ? up : 0;
            new_flight();
            if (phase == PHASE_RALLY && !goal_timer) landed();
        }
    }
    if (!ball_z) {
        if (!ball_curve_t) ball_vx = drag(ball_vx);  // a curve keeps its push across
        ball_vy = drag(ball_vy);
    }
    if (ball_power && ball_speed() < POWER_FADE) ball_power = 0;
    if (ball_vx || ball_vy) ball_roll++;
    // The floor round the court ends somewhere: the ball stops at its edge.
    if (PX(x) < OUT_X0 || PX(x) > OUT_X1) x = ball_x, ball_vx = 0, ball_curve_t = 0;
    if (PX(y) < OUT_Y0 || PX(y) > OUT_Y1) y = ball_y, ball_vy = 0;
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
    sets[0] = sets[1] = 0;
    set_won = 0;
    pl_shots[0] = pl_shots[1] = 0;
    pl_last_move[0] = pl_last_move[1] = MOVE_NONE;
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
    if (hud_pending) {
        hud_pending = 0;
        hud_draw();
    }
    if (note_timer && !--note_timer) hud_draw();

    if (goal_timer) {
        // The celebration, then the serve: the next set's, or the end of the match.
        ball_update();
        if (--goal_timer == GOAL_FRAMES / 2 && set_won) note(GOAL_FRAMES / 2);  // SET TO ...
        if (!goal_timer) {
            uint8_t s = last_scorer;
            if (set_won) {
                set_won = 0;
                if (sets[s] >= match_sets) {
                    winner = s;
                    state_change(STATE_GAME_OVER);
                    return;
                }
                score[0] = score[1] = 0;
            }
            serve_setup(s ^ 1);
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
        } else if (pl_shot[SIDE_YOU] || (phase == PHASE_RALLY && !pl_move_timer[SIDE_YOU])) {
            ready(SIDE_YOU, b);  // queue it any time: it charges once the ball's coming
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
