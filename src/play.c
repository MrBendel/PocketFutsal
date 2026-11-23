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
// A is a lob, A then B a drop shot. Readying plants you: each press fills the meter (only presses,
// and it stays; SELECT calls it off), and the strike goes by itself as
// the ball arrives. Which strike depends on the ball's height: a kick on the
// ground, a volley at the waist, a header, or a bicycle kick for one that's just
// gone over your head. Planted out of reach, you slide for it, and lie there a
// while. A ball only just reached (a slide, a ball on fire met with less than
// three eighths of a meter) pops up over the net, where met in the air with half a
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
#include "player_anim.h"
#include "pitch.h"
#include "play_hud.h"
#include "vfx.h"
#include "proj.h"
#include "hud.h"
#include "sprites.h"
#include "players.h"

// Sprite tiles (tools/tile-art.mjs). VRAM bank 0: the ball and its shadow, an
// 8x16 pair each. VRAM bank 1: the players, 8 tiles a frame.
#define S_BALL 0  // and S_BALL + 2, its other roll
#define S_SHADOW 4
#define S_AIM 6     // the aim marker
#define S_TRAIL 8   // a trail's dots, largest first: S_TRAIL, +2, +4, +6
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

// Hardware sprites. Lower numbers draw on top. Interleave the ball and
// player blocks by court depth; shadows remain underneath.
#define OAM_BALL ball_oam
uint8_t ball_oam;  // 0, 4 or 8: before, between or behind the players
#define OAM_PLAYER 1  // 4 each, by side
#define OAM_SHADOW 9
#define OAM_AIM 10    // where your queued strike will land
#define OAM_SLAM 19   // where to meet a lob or a pop-up coming down (flashing)
#define OAM_PLAYER_SHADOW 15  // two columns per player, below the body
#define OAM_TRAIL 11  // 4: the ball's trail
#define TRAIL_MAX 4

// --- tuning ---
// Running, as Mario Tennis's velocities show (build/scratch/mt/vel.mjs): to full speed
// in about 7 frames, a straight ramp; let go, a coast of about 20 frames, easing down
// evenly; turning round, a skid at the coasting rate for about 9 frames, then the full
// push the other way. Each player's speed, push and grip are their own (pl_top,
// pl_accel, pl_grip), to upgrade. The game running you to the ball, and the CPU, brake
// firmly instead (BRAKE): they stop where they mean to.
#define SPEED_YOU 22      // a frame, in SUB
#define SPEED_CPU 20
#define ACCEL 3           // SUB a frame each frame: about 8 frames to full speed
#define GRIP 1            // letting go: a coast of 22 frames from full speed (about 15 px)
#define BRAKE 4           // firm: about 6 frames to a stop
#define SKID_FRAMES 9     // turning round: this long at the grip, then the full push
#define HOLD_MAX 90       // frames at a full meter before the CPU gives up the strike
#define YOU_TAP 4         // your meter: each press adds this (it fills only by pressing, and stays)
#define REACH_X 16        // court px either side a strike reaches (a body width)
#define REACH_Z 36        // px: a header reaches this high (a jump); a bicycle kick 40
#define SLIDE_Z 24        // px: a slide gets a ball this high (a diving header)
#define REACH_FWD 8       // in front
#define REACH_BACK 4      // behind (a bicycle kick reaches further)
#define SLIDE_SPEED 48
#define SLIDE_LUNGE 12    // frames of sliding
// The furthest a slide gets you: your reach plus the lunge (16 + 36 px). No other
// limit: you're run to the ball from anywhere; out of this at the last moment,
// you just miss.
#define SLIDE_REACH (REACH_X + ((SLIDE_SPEED * SLIDE_LUNGE) >> 4))
#define SLIDE_FRAMES 64   // a missed slide: on the floor until this runs out
#define SLIDE_REST 28     // a slide that got there: up sooner
// The ball's pace: Mario Tennis's, a little over a second from one player to the
// other. Gravity is 3/4 SUB a frame each frame (3 frames in 4: ball_fall), so
// speeds and lifts are half what they'd be at gravity 3: the same arcs, twice as long.
#define POWER_SPEED 48    // a ball on fire
#define DRIVE_SPEED 28    // A: + half the charge
#define CURVE_SPEED 25    // B: + half the charge
#define CURVE_OFS 44      // px a curve bends by the time it gets there
#define HEADER_SPEED 24   // + a quarter of the charge: flat, not a loop up
#define CHIP_LIFT 28      // a lob: about 32 px up, 74 frames in the air
#define POP_LIFT 32       // a pop-up: about 43 px up, 84 frames
#define BOUNCE_MIN 15     // a first bounce gets up at least this: about 9 px, 40 frames
#define LOB_BOUNCE 20     // a lob sits up where it lands: about 16 px, 54 frames, slowed to a quarter
#define DROP_HOP 16       // a drop shot's: about 11 px, 43 frames
#define SLAM_Z 26         // px: a lob or a pop-up is met this high coming down (a header)
#define ARC_MAX 100       // frames: the longest a drop shot is in the air (about 60 px up)
#define ARC_FLAT 60       // frames: the longest anything else arcs (about 22 px up): a weak kick doesn't loop
#define BOUNCE_DEAD 4     // a bounce lower than this: it rolls
#define POWER_FADE 20     // a ball on fire burns out below this speed (|vx| + |vy|)
#define SLOW 8            // a ball slower than this (|vx| + |vy|) is loose
#define FIRE_MEET 12      // the charge it takes to meet a ball on fire cleanly (a press and a moment)
#define SCUFF 4           // a strike with less charge than this is scuffed into the net
#define KICK_FRAMES 14    // a strike's pose; no touches meanwhile
#define BICYCLE_FRAMES 24
#define BLOCK_FRAMES 16
#define DOWN_FRAMES 40    // knocked down by a ball on fire
#define TOSS_LIFT 27      // the serve's toss: about 30 px up
#define TOSS_STRIKE 20    // px: struck when it falls back to this
#define SERVE_FRAMES 50
#define BALL_R 3          // px: the ball's radius on the court
#define HOME_CPU 24       // the CPU's depth, waiting
#define CPU_PLANT 96      // px: the CPU readies when the ball's this close and it's in place
#define CPU_LATE 40       // px: and this close, in place or not
#define CONTROL_LIFT 28   // a control touch: about 32 px up, 74 frames
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
int8_t pl_top[2];                 // running speed (SUB a frame)
uint8_t pl_accel[2], pl_grip[2];  // speeding up; slowing (the coast, and a turn's skid)
uint8_t pl_skid[2];               // frames turning round so far
uint8_t pl_auto[2];          // the game's running player i to the ball this frame:
int8_t pl_wx[2], pl_wy[2];   // at this velocity (applied next frame, through the momentum)
uint8_t pl_face[2];
uint8_t pl_side[2];
uint8_t pl_shot[2];
uint8_t pl_charge[2];
uint8_t pl_hold[2];
uint8_t pl_move[2];
uint8_t pl_move_timer[2];
uint8_t pl_shots[2];
uint8_t pl_last_move[2];
uint8_t pl_taps[2];
uint8_t pl_combo[2];  // a lob or a drop shot armed: frames it stays armed
#define COMBO_FRAMES 24  // about 0.4 s: press the other button just before the ball arrives
uint8_t pl_flash[2];  // charging: 0 own colors, 1 white, 2 fire
uint8_t pl_aim[2];
uint8_t pl_depth[2];  // 0 short, 1 as charged, 2 deep: UP / DOWN while queued (it sticks)
uint8_t cpu_depth;
int16_t aim_tx, aim_land;  // aim_point()'s answer: where it's aimed to land (court px)
int8_t pl_slide_vx[2], pl_slide_vy[2];
int16_t cpu_tx, cpu_ty;
int16_t meet_x[2], meet_y[2];  // where each side means to meet the ball (court px)
uint8_t meet_out[2];  // the ball's landing out: that side lets it go
uint8_t ball_flight;    // counts each change in the ball's flight (a touch, a bounce, the net)
uint8_t flight_frame;   // the frame it last changed on
int16_t ball_hx, ball_hwy;  // the ball's centre as last drawn: hardware x, hardware y + camera (for effects)
uint8_t meet_slam[2];   // meet_* is the slam spot: a lob or a pop-up, met coming down
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
uint8_t ball_dropped;  // a drop shot that has bounced (its marker stays till it's met)
uint8_t ball_lob;
uint8_t ball_trail;   // the trail behind it: 0 none, TRAIL_POWER red, TRAIL_CURVE blue
uint8_t trail_len;    // dots in it (2..4, by charge); shrinks to nothing once it's over
uint8_t trail_n;      // frames recorded since the strike (older positions aren't its)
uint8_t trail_i;      // the ring's newest entry
uint8_t trail_blue;   // the last trail was a curve's (its color as it fades)
int16_t trail_x[16], trail_y[16];  // where the ball was drawn, a frame each (y on the court, not the screen)
uint8_t last_touch;
uint8_t sets[2];
uint8_t match_sets = 1;
uint8_t holder;
uint8_t touches;
uint8_t ball_netted;
uint8_t ball_serve;
uint8_t set_won;  // this point won the set
uint8_t ball_fall;  // counts frames in the air: gravity acts on 3 in 4
uint8_t ball_roll;  // counts up as the ball rolls: its two frames

uint8_t meter_shown;  // the meter's value on screen (0xFF: redraw)
// Your shot on the HUD, where it says YOU (a debugging aid): the one queued, or for a
// moment after a strike what went (SEEN_*), else YOU.
#define SEEN_POP 5
#define SEEN_CONTROL 6
#define SEEN_SLAM 7
#define SEEN_HEADER 8
#define SEEN_BICYCLE 9
#define SEEN_SCUFF 10     // a strike with next to no charge: mishit, short of the net
#define SEEN_FRAMES 60
static const char seen_names[] = "YOUPWRCRVLOBDRPPOPCTLSLMHDRBIKMIS";
uint8_t shot_seen;    // what your last touch was (SHOT_* or SEEN_*)
uint8_t shot_seen_t;  // frames left showing it
uint8_t shot_shown = 0xFF;  // the label on screen (0xFF: redraw)
static uint8_t shot_buf[3];
uint8_t meter_buf[8];  // its tiles, queued to the window's map
uint8_t note_timer;    // the HUD shows a note (GOAL!) while this counts down
uint8_t hud_pending;   // redraw the HUD at the start of the next frame

// The perspective, by depth from -16 to 239 (index depth + 16): the map row, and
// the width scale /128. The same formula as tools/tile-art.mjs, worked out once.
uint8_t proj_y[256];
uint8_t proj_s[256];

static int16_t abs16(int16_t v) {
    return v < 0 ? -v : v;
}

// The player running this frame (player_update), for approach().
static uint8_t mv_i, mv_firm, mv_turn;
// Player mv_i's velocity v toward t: speeding up at pl_accel; slowing at pl_grip (a
// long coast), and turning round too for the first SKID_FRAMES (a skid), then at
// pl_accel; firm (mv_firm), slowing or turning at BRAKE.
static int8_t approach(int8_t v, int8_t t) {
    if (v == t) return v;
    uint8_t turn = (v > 0 && t < 0) || (v < 0 && t > 0);
    uint8_t d = pl_accel[mv_i];
    mv_turn |= turn;
    if (turn || (v > 0 && t < v) || (v < 0 && t > v)) {
        if (mv_firm) d = BRAKE;
        else if (!turn || pl_skid[mv_i] < SKID_FRAMES) d = pl_grip[mv_i];
    }
    if (v < t) {
        v += d;
        if (v > t) v = t;
    } else {
        v -= d;
        if (v < t) v = t;
    }
    return v;
}

static int16_t max16(int16_t a, int16_t b) {
    return a > b ? a : b;
}

static int16_t min16(int16_t a, int16_t b) {
    return a < b ? a : b;
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
// (proj_init: proj.c.)

#define PROJ_I(fy) ((uint8_t)((fy) + PROJ_OFS))
// A court point -> screen x (px).
static int16_t proj_x(int16_t fx, int16_t fy) {
    return CENTER_X + (((fx - CENTER_X) * (int16_t)proj_s[PROJ_I(fy)]) >> 7);
}

// --- the HUD: YOU 0 [meter] 0 CPU (this set's points), or a note for a moment ---
// (hud_draw: play_hud.c.)

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

// Your shot's label, where it says YOU: queued to the window's map when it changes.
static void seen(uint8_t what) {
    shot_seen = what;
    shot_seen_t = SEEN_FRAMES;
}

static void shot_label_update(void) {
    if (paused || note_timer) return;
    uint8_t k = pl_shot[SIDE_YOU];
    if (shot_seen_t) {
        shot_seen_t--;
        if (!k) k = shot_seen;
    }
    if (k == shot_shown) return;
    shot_shown = k;
    const char *n = seen_names + (k << 1) + k;
    for (uint8_t j = 0; j < 3; j++) shot_buf[j] = FONT_TILES + (uint8_t)(n[j] - ' ');
    gfx_vram_copy(get_win_xy_addr(0, 0), shot_buf, 3, 0);
}

// --- setting up ---
static void unready(uint8_t i) {
    pl_shot[i] = SHOT_NONE;
    pl_combo[i] = 0;
    pl_charge[i] = pl_hold[i] = pl_taps[i] = 0;
    pl_aim[i] = 1;
    pl_depth[i] = 1;
}

static void ball_plain(void) {
    ball_power = ball_curve = ball_curve_t = ball_pop = ball_drop = ball_lob = ball_dropped = 0;
    ball_trail = trail_len = 0;
    ball_cacc = 0;
}

static void serve_setup(uint8_t side) {
    server = side;
    phase = PHASE_SERVE;
    for (uint8_t i = 0; i < 2; i++) {
        pl_vx[i] = pl_vy[i] = 0;
        pl_skid[i] = 0;
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
    pl_side[0] = pl_side[1] = 0;
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

static void aim_point(uint8_t i);  // where a strike is aimed: with the strikes, below

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
    uint8_t n = i ? (ball_oam == 8 ? 4 : 5) : (ball_oam ? 0 : 1);
    gfx_actor(n, tile, lie, sx, sy, pal | S_BANK, flip);
}

// The ball's trail: where it was 4, 8, 12 and 16 frames ago, smaller and dimmer
// further back. Over (a bounce, a touch), it shrinks a dot every other frame.
static void draw_trail(int16_t sx, int16_t wy) {
    trail_i = (trail_i + 1) & 15;
    trail_x[trail_i] = sx;
    trail_y[trail_i] = wy;
    if (trail_n < 16) trail_n++;
    if (!ball_trail && trail_len && !((uint8_t)frame_count & 1)) trail_len--;
    if (goal_timer) trail_len = 0;
    uint8_t pal = ball_trail == TRAIL_CURVE || (!ball_trail && trail_blue) ? SPR_PAL_TRAIL_CURVE : SPR_PAL_TRAIL_POWER;
    if (ball_trail) trail_blue = ball_trail == TRAIL_CURVE;
    for (uint8_t k = 0; k < TRAIL_MAX; k++) {
        uint8_t age = (k + 1) << 2, n = OAM_TRAIL + k;
        if (k >= trail_len || age >= trail_n) {
            move_sprite(n, 0, 0);
            continue;
        }
        uint8_t e = (trail_i - age) & 15;
        set_sprite_tile(n, S_TRAIL + (k << 1));
        set_sprite_prop(n, pal);
        place(n, trail_x[e], trail_y[e] - cam_y);
    }
}

static uint8_t coming(uint8_t i);  // (the rules, below)

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

    ball_oam = ball_y >= pl_y[SIDE_YOU] ? 0 : ball_y >= pl_y[SIDE_CPU] ? 4 : 8;
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t f = F_STAND, lie = 0, flip = pl_face[i], special = 0;
        int8_t lift = 0;
        uint8_t m = pl_move_timer[i] ? pl_move[i] : MOVE_NONE;
        if (goal_timer) f = last_scorer == i ? F_WIN : F_LOSE;
        else if (m == MOVE_KICK) f = F_KICK;
        else if (m == MOVE_VOLLEY || m == MOVE_CONTROL) f = F_VOLLEY;
        else if (m >= MOVE_HEADER && m <= MOVE_DOWN) special = 1;
        else if (pl_shot[i] || (phase == PHASE_TOSS && server == i)) f = F_READY;
        else if (pl_vx[i] || pl_vy[i]) f = (frame_count & 8) ? F_RUN_A : F_RUN_B;
        uint8_t tile = lie ? T_LYING : (f << 3) + (i == SIDE_CPU ? VIEW_FRONT : 0);
        if (special) {
            tile = player_move_pose(m, pl_move_timer[i], i == SIDE_CPU, &lie, &lift);
            if (m == MOVE_SLIDE && lie) flip = !flip;
        }
        // Side art follows real momentum, including the coast through a turn.
        // Existing action frames take priority over the locomotion view.
        if (abs16(pl_vx[i]) > abs16(pl_vy[i]) + 3) pl_side[i] = 1;
        else if (abs16(pl_vy[i]) > abs16(pl_vx[i]) + 3) pl_side[i] = 0;
        if (!goal_timer && !special && (f == F_STAND || f == F_RUN_A || f == F_RUN_B) && pl_side[i]) {
            uint8_t pose = (pl_vx[i] || pl_vy[i]) ? ((frame_count & 4) ? 0 : (frame_count & 8) ? 2 : 1) : 0;
            tile = 224 + (pose << 3);
            if (pl_vx[i]) flip = pl_vx[i] < 0;
        }
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
        // Ground position never inherits the body's jump lift.
        gfx_player_shadow(OAM_PLAYER_SHADOW + (i << 1), sx,
            (int16_t)proj_y[PROJ_I(py)] - cam_y);
    }
    // The ball, up at its height, and its shadow on the court.
    int16_t sx = proj_x(PX(ball_x), by) + 4;
    int16_t sy = (int16_t)proj_y[PROJ_I(by)] - cam_y + 10;
    set_sprite_tile(OAM_BALL, S_BALL + ((ball_roll & 8) ? 2 : 0));
    set_sprite_prop(OAM_BALL, ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
    place(OAM_BALL, sx, sy - bz);
    ball_hx = sx + 4;
    ball_hwy = sy - bz + 4 + cam_y;
    vfx_draw(cam_y);
    set_sprite_tile(OAM_SHADOW, S_SHADOW);
    set_sprite_prop(OAM_SHADOW, SPR_PAL_UI);
    place(OAM_SHADOW, sx, sy - 1);
    draw_trail(sx, sy + cam_y - bz);
    // A lob or a pop-up coming down to you, or a drop shot: a flashing marker where
    // to meet it (the slam spot; off a drop shot's bounce).
    if (meet_flight[SIDE_YOU] == ball_flight && !goal_timer && coming(SIDE_YOU) &&
        ((meet_slam[SIDE_YOU] && (ball_lob || ball_pop)) || ((ball_drop || ball_dropped) && !meet_out[SIDE_YOU]))) {
        set_sprite_tile(OAM_SLAM, S_AIM);
        set_sprite_prop(OAM_SLAM, ((uint8_t)frame_count & 4) ? SPR_PAL_POWER : SPR_PAL_UI);
        place(OAM_SLAM, proj_x(meet_x[SIDE_YOU], meet_y[SIDE_YOU]) + 4, (int16_t)proj_y[PROJ_I(meet_y[SIDE_YOU])] - cam_y + 12);
    } else move_sprite(OAM_SLAM, 0, 0);
    // Your strike queued: a marker where it's aimed to land.
    // (Every other frame, and not a strike's: it's another aim_point.)
    if (pl_shot[SIDE_YOU] && !goal_timer && (phase == PHASE_RALLY || server == SIDE_YOU)) {
        if ((frame_count & 1) || (uint8_t)frame_count == flight_frame) return;
        aim_point(SIDE_YOU);
        set_sprite_tile(OAM_AIM, S_AIM);
        set_sprite_prop(OAM_AIM, SPR_PAL_UI);
        place(OAM_AIM, proj_x(aim_tx, aim_land) + 4, (int16_t)proj_y[PROJ_I(aim_land)] - cam_y + 12);
    } else move_sprite(OAM_AIM, 0, 0);
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

// The ball's over player i's half: theirs to play (no reaching over the net).
static uint8_t in_half(uint8_t i) {
    return i == SIDE_YOU ? PX(ball_y) >= HALFWAY : PX(ball_y) < HALFWAY;
}

// Slow, in player i's half: theirs to go and get.
static uint8_t loose_here(uint8_t i) {
    if (ball_speed() >= SLOW) return 0;
    return in_half(i);
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
        if (i == SIDE_YOU) {
            pl_charge[i] = YOU_TAP;
            pl_aim[i] = joy & J_LEFT ? 0 : joy & J_RIGHT ? 2 : 1;
            pl_depth[i] = joy & J_UP ? 2 : joy & J_DOWN ? 0 : 1;
        }
        return;
    }
    // The other button arms a lob (B then A) or a drop shot (A then B), or keeps it
    // armed, for COMBO_FRAMES: it has to be just before the strike (player_update
    // lets it lapse back). The first button again taps the charge up, and disarms it.
    if ((s == SHOT_CURVE || s == SHOT_LOB) && !b) {
        pl_shot[i] = SHOT_LOB;
        pl_combo[i] = COMBO_FRAMES;
        return;
    }
    if ((s == SHOT_POWER || s == SHOT_DROP) && b) {
        pl_shot[i] = SHOT_DROP;
        pl_combo[i] = COMBO_FRAMES;
        return;
    }
    if (s == SHOT_LOB) pl_shot[i] = SHOT_CURVE;
    else if (s == SHOT_DROP) pl_shot[i] = SHOT_POWER;
    pl_combo[i] = 0;
    // You: each press, as many as you like. The CPU: a few taps on top of its wait.
    if (pl_charge[i] == CHARGE_MAX || (i != SIDE_YOU && pl_taps[i] >= TAP_MAX)) return;
    pl_taps[i]++;
    pl_charge[i] += i == SIDE_YOU ? YOU_TAP : TAP_CHARGE;
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
// A burst effect at the ball; dust at a player's feet.
static void burst(uint8_t kind) {
    vfx_start(kind, ball_hx, ball_hwy);
}

static void dust(uint8_t i) {
    int16_t py = PX(pl_y[i]);
    vfx_start(VFX_DUST, proj_x(PX(pl_x[i]), py) + 8, (int16_t)proj_y[PROJ_I(py)] + 20);
}

static void new_flight(void) {
    ball_flight++;
    ball_trail = 0;
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
// with vz = 3 (t + 1) / 8 - z0 / t (gravity 3/4, a frame at a time: after t
// frames it's z0 + vz t - 3 t (t + 1) / 8 up, near enough). Every strike arcs, so the ball is in the
// air, its shadow under it, and clears the net if it's meant to.
// If it's to go over (clear), it must be NET_H + 3 px up as it gets to the net,
// t_n frames on, so a fast flat strike is lifted, and lands deeper (or long).
static int16_t arc_t;  // arc()'s time in the air, frames
uint8_t arc_clear;     // the arc just struck is to clear the net: net_clear() next frame
// A curve's push for its time in the air t (by t / 4): 384 * CURVE_OFS / t^2, 1 to 60.
static const uint8_t curve_push[] = {
    17, 17, 17, 17, 17, 17, 17, 17, 15, 12, 10, 8, 7, 6, 5, 4, 4, 3, 3, 3, 3, 2, 2, 2,
    2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static void arc(int16_t land, uint8_t clear) {
    int16_t vy = abs16(ball_vy);
    if (!vy) vy = 1;
    int16_t t = clamp16((abs16(land - PX(ball_y)) << 4) / vy, 12, 400);
    // Too slow to get there in ARC_FLAT frames (ARC_MAX a drop shot): it goes
    // faster instead of ballooning up out of sight: a quarter faster at a time (no
    // division), each about 4/5 of the time.
    uint8_t most = ball_drop ? ARC_MAX : ARC_FLAT;
    while (t > most) {
        ball_vx += ball_vx >> 2;
        ball_vy += ball_vy >> 2;
        t -= (t >> 2) - (t >> 4);
    }
    arc_t = t;
    ball_vz = (3 * (t + 1)) >> 3;
    if (ball_z) ball_vz -= ball_z / t;  // (a division only off the floor)
    arc_clear = clear;  // the net: checked next frame (net_clear), off the strike's busy one
}

// An arc meant to go over: NET_H + 3 px up as it gets to the net, t_n frames on, or
// lifted to be (a fast flat strike lands deeper, or long). The frame after the strike.
static void net_clear(void) {
    int16_t vy = abs16(ball_vy);
    if (!vy || (ball_vy < 0 ? PX(ball_y) <= HALFWAY : PX(ball_y) >= HALFWAY)) return;
    int16_t tn = clamp16((abs16(HALFWAY - PX(ball_y)) << 4) / vy, 1, 160);
    int16_t drop = ((tn * (tn + 1)) >> 3) * 3;  // gravity's pull by then
    // High enough already (multiplying, not dividing): leave it.
    if (ball_z + ball_vz * tn - drop >= TO_SUB(NET_H + 3)) return;
    ball_vz = (TO_SUB(NET_H + 3) + drop - ball_z) / tn;
}

// Where player i's strike is aimed to land (aim_tx, aim_land): across, angled the
// way held (or last held while queued), else half as far across, away from the other
// player; in depth, deeper the more charged, UP deeper and DOWN shorter. A lob lands just behind the
// other player, a drop shot just over the net. The CPU picks its own as it readies.
static void aim_point(uint8_t i) {
    uint8_t aim = pl_aim[i], depth = pl_depth[i], c = pl_charge[i];
    if (i == SIDE_YOU) {
        if (joy & J_LEFT) aim = 0;
        else if (joy & J_RIGHT) aim = 2;
        if (joy & J_UP) depth = 2;
        else if (joy & J_DOWN) depth = 0;
    } else {
        aim = cpu_aim;
        depth = cpu_depth;
    }
    int16_t d = 24 + c + (c >> 1);  // past the net: 24 .. 72
    if (depth == 2) d += 24;
    else if (!depth) d = d > 40 ? d - 24 : 16;
    else if (i == SIDE_YOU) {
        // Neither UP nor DOWN: away from the CPU in depth too: short of it back at
        // its baseline, past it up near the net (charged deep is where it stands).
        int16_t back = HALFWAY - PX(pl_y[SIDE_CPU]);  // px past the net it stands
        if (back > 60) { if (d > 40) d = 40; }
        else if (d < back + 24) d = back + 24;
    }
    if (pl_shot[i] == SHOT_LOB) {
        // Over their head, and deep: 24 px behind them, at least 60 past the net, so
        // a lob at a player up at the net sends them back for it (a bicycle kick).
        aim_land = i == SIDE_YOU ? min16(PX(pl_y[SIDE_CPU]) - 24, other_y(i, 60)) : max16(PX(pl_y[SIDE_YOU]) + 24, other_y(i, 60));
        aim_land = clamp16(aim_land, COURT_FAR + 8, COURT_NEAR - 8);
    }
    else if (pl_shot[i] == SHOT_DROP) aim_land = other_y(i, 18);
    else aim_land = clamp16(other_y(i, d), COURT_FAR + 8, COURT_NEAR - 8);
    // A direction from where you are: as far across as a strike goes (half as far as
    // down the court) to the left or right; neither held, half that, away from the
    // other player (straight ahead is where they stand); kept in the court.
    int16_t x0 = PX(pl_x[i]), wide = abs16(aim_land - PX(pl_y[i])) >> 1;
    if (aim == 1 && i == SIDE_YOU) {
        wide >>= 1;
        aim = PX(pl_x[SIDE_CPU]) < x0 ? 2 : 0;
    }
    aim_tx = clamp16(aim == 0 ? x0 - wide : aim == 2 ? x0 + wide : x0, COURT_X0 + 6, COURT_X1 - 6);
}

// A clean strike: the readied shot, shaped by the ball's height (move).
static void strike(uint8_t i, uint8_t move) {
    uint8_t was_pop = ball_pop, was_lob = ball_lob;
    int16_t bx = PX(ball_x), by = PX(ball_y);
    aim_point(i);
    uint8_t aim = i == SIDE_YOU ? (aim_tx < CENTER_X ? 0 : aim_tx > CENTER_X ? 2 : 1) : cpu_aim;
    int16_t tx = aim_tx;
    uint8_t c = pl_charge[i], kind = pl_shot[i], speed, frames = KICK_FRAMES;
    if (phase != PHASE_RALLY && c == CHARGE_MAX) c--;  // no serves on fire
    uint8_t fire = c == CHARGE_MAX;
    // Where it comes down (aim_point). Barely charged at all, it's scuffed: it
    // comes down on your own side, short of the net.
    uint8_t scuff = c < SCUFF && phase == PHASE_RALLY;
    int16_t land = scuff ? other_y(i, -10) : aim_land;
    ball_plain();
    uint8_t slam = move != MOVE_KICK && (was_pop || was_lob) && c >= CHARGE_MAX / 2;
    if (slam) {
        // A pop-up (or your own control) or a lob met in the air before it lands,
        // half charged: a slam, on fire, deep. (Less, it's a header: still out of the air.)
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
        } else speed = move == MOVE_HEADER ? HEADER_SPEED + (c >> 2) : DRIVE_SPEED + 8 + (c >> 2);
        frames = move == MOVE_HEADER ? KICK_FRAMES + 4 : BICYCLE_FRAMES;
        launch(tx - bx, land - by, speed);
        arc(land, !scuff);
    } else if (kind == SHOT_LOB || kind == SHOT_DROP) {
        // Placed: a lob drops just behind the other player, a drop shot just past
        // the net. A lob's time in the air is known, so its speed is the distance
        // over it (7/32 of it a frame); a drop shot goes at 2/5 of the distance a
        // frame (about 40 frames), up to 32, lifted to come down there.
        int16_t dist;
        // The whole way, across too (launch's measure: across is at most half of down).
        dist = abs16(land - by);
        dist += min16(abs16(tx - bx), dist >> 1);
        if (kind == SHOT_LOB) {
            speed = (uint8_t)clamp16((dist * 7) >> 5, 8, 44);  // (44: a deep one, across)
            ball_vz = move == MOVE_VOLLEY ? CHIP_LIFT - 4 : CHIP_LIFT;
            ball_lob = 1;
        } else {
            speed = (uint8_t)clamp16((dist * 13) >> 5, 6, 32);
            ball_drop = 1;
        }
        launch(tx - bx, land - by, speed);
        if (kind == SHOT_DROP) arc(land, 1);  // its speed is capped: the lift makes up the distance
    } else if (kind == SHOT_CURVE) {
        // Set off CURVE_OFS px wide of the target and bent back in: a steady push
        // across for its time in flight, t frames (at its starting speed: the ball
        // slows, so it drifts on after the push ends). 384 * CURVE_OFS / t^2
        // sixteenths of a SUB a frame, each frame, brings it in to the target
        // (tuned: build/scratch/curve.mjs); t is arc()'s, the push from a table
        // (two divisions fewer on a busy frame).
        int8_t side = aim == 0 ? -1 : aim == 2 ? 1 : PX(pl_x[i]) < CENTER_X ? 1 : -1;
        speed = fire ? POWER_SPEED - 8 : CURVE_SPEED + (c >> 1);
        ball_power = fire;
        launch(tx - side * CURVE_OFS - bx, land - by, speed);
        arc(land, !scuff);
        uint8_t t = (uint8_t)clamp16(arc_t, 32, 180);
        uint8_t push = curve_push[t >> 2];
        ball_curve = side > 0 ? (int8_t)push : -(int8_t)push;
        ball_curve_t = t;
    } else {
        // Power: straight, a low fast arc.
        speed = fire ? POWER_SPEED : DRIVE_SPEED + (c >> 1) + (move == MOVE_VOLLEY ? 4 : 0);
        ball_power = fire;
        launch(tx - bx, land - by, speed);
        arc(land, !scuff);
    }
    sfx_play(ball_power ? SFX_POWER : SFX_KICK);
    struck(i, move, frames);
    if (slam) burst(VFX_SLAM);  // an impact star: big for a slam, small on fire
    else if (ball_power) burst(VFX_FIRE);
    if (i == SIDE_YOU)
        seen(scuff ? SEEN_SCUFF : slam ? SEEN_SLAM :
             move == MOVE_HEADER ? SEEN_HEADER : move == MOVE_BICYCLE ? SEEN_BICYCLE : kind);
    // A trail behind it: red a power shot (or anything on fire), blue a curve;
    // longer the more charged. Not a scuff, a lob or a drop shot.
    if (!scuff && (ball_power || kind == SHOT_POWER || kind == SHOT_CURVE)) {
        ball_trail = kind == SHOT_CURVE ? TRAIL_CURVE : TRAIL_POWER;
        trail_len = ball_power ? TRAIL_MAX : c >= CHARGE_MAX / 2 ? 3 : 2;
        trail_n = 0;
    }
}

// Only just got there: it pops up high and drops just behind the other player,
// at head height as it reaches them, for them to slam.
static void pop_up(uint8_t i, uint8_t move, uint8_t frames) {
    int16_t bx = PX(ball_x), by = PX(ball_y);
    int16_t land = PX(pl_y[i ^ 1]) + (i == SIDE_YOU ? -10 : 10);
    aim_point(i);  // the way you're aiming, even so
    int16_t tx = aim_tx;
    ball_plain();
    ball_vz = POP_LIFT;
    ball_pop = 1;
    // 84 frames in the air: 3/16 of the distance a frame (the whole way, across too).
    int16_t dist = abs16(land - by);
    dist += min16(abs16(tx - bx), dist >> 1);
    launch(tx - bx, land - by, (uint8_t)clamp16((dist * 3) >> 4, 4, 30));
    sfx_play(SFX_TOUCH);
    struck(i, move, frames);
    if (i == SIDE_YOU) seen(SEEN_POP);
}

// Reached unready: a first touch, the ball up in front of you, to play again with
// your second (ready a strike as it comes down: half a meter makes it a slam).
// (struck() first: with it last, SDCC 4.5's code kept i in L, overwrote HL with
// &ball_vy, and passed struck() the address's low byte as i.)
static void control(uint8_t i) {
    struck(i, MOVE_CONTROL, CONTROL_FRAMES);
    if (i == SIDE_YOU) seen(SEEN_CONTROL);
    ball_plain();
    ball_vx = 0;
    ball_vy = last_touch == SIDE_YOU ? -2 : 2;  // 74 frames: 9 px ahead, still in reach
    ball_vz = CONTROL_LIFT;
    ball_pop = 1;
    sfx_play(SFX_TOUCH);
}

static void knock_down(uint8_t i) {
    new_flight();
    dust(i);
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
// three eighths full, when it's all they can do to pop it up.
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
    dust(i);
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
        if (rel_z < REACH_Z) return MOVE_HEADER;
    } else if (ax <= REACH_X - 2 && rel_fwd < -2 && rel_fwd >= -14 && rel_z >= 10 && rel_z < REACH_Z + 4) {
        return MOVE_BICYCLE;  // just over your head: over it goes, backwards
    }
    return MOVE_NONE;
}

// A player's frame: move (dx, dy: -1, 0 or 1) unless planted, charge, then strike,
// slide, or take it unready.
static void player_update(uint8_t i, int8_t dx, int8_t dy) {
    // A lob or a drop shot not struck in time: the curve or the power shot again
    // (the CPU keeps its armed: it presses the combo as the ball arrives).
    if (pl_combo[i] && i == SIDE_YOU && !--pl_combo[i]) {
        if (pl_shot[i] == SHOT_LOB) pl_shot[i] = SHOT_CURVE;
        else if (pl_shot[i] == SHOT_DROP) pl_shot[i] = SHOT_POWER;
    }
    if (pl_move_timer[i]) {
        pl_vx[i] = pl_vy[i] = 0;  // mid-move: stopped (it starts again from still)
        pl_move_timer[i]--;
        if (pl_move[i] != MOVE_SLIDE || pl_move_timer[i] < SLIDE_FRAMES - SLIDE_LUNGE) return;
        // The slide: along the court, reaching for it.
        pl_x[i] += pl_slide_vx[i];
        pl_y[i] += pl_slide_vy[i];
        keep_in(i);
        relate(i);
        if (in_half(i) && abs16(rel_x) <= REACH_X && rel_fwd >= -REACH_BACK - 2 && rel_fwd <= REACH_FWD && rel_z < SLIDE_Z) {
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
    // Queued, the d-pad only aims: it doesn't walk you off toward a sideline.
    if ((pl_shot[i] && phase == PHASE_RALLY) || (phase == PHASE_TOSS && server == i)) dx = dy = 0;
    if (phase != PHASE_RALLY && server == i) dx = dy = 0;  // serving: the d-pad aims, it doesn't walk you
    // Momentum: toward the speed held (or the auto-run's), through approach().
    int8_t speed = pl_top[i];
    int8_t tx = dx > 0 ? speed : dx < 0 ? -speed : 0;
    int8_t ty = dy > 0 ? speed : dy < 0 ? -speed : 0;
    mv_firm = i;  // the CPU (SIDE_CPU, 1)
    if (pl_auto[i]) {
        tx = pl_wx[i];
        ty = pl_wy[i];
        pl_auto[i] = 0;
        mv_firm = 1;
    }
    mv_i = i;
    mv_turn = 0;
    int8_t vx = approach(pl_vx[i], tx), vy = approach(pl_vy[i], ty);
    if (!mv_turn) pl_skid[i] = 0;
    else if (pl_skid[i] < 255) pl_skid[i]++;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    pl_x[i] += vx;
    pl_y[i] += vy;
    keep_in(i);
    if (tx) pl_face[i] = tx < 0;

    // The CPU's meter fills as it waits (a point every 4 frames; taps add more), and it
    // gives up the strike held full too long. Yours fills only as you press (ready()).
    if (i == SIDE_CPU && pl_shot[i] && (phase != PHASE_RALLY || live_for(i))) {
        if (pl_charge[i] < CHARGE_MAX) {
            if (!((uint8_t)frame_count & 3)) ++pl_charge[i];
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
        uint8_t move = in_half(i) ? in_reach() : MOVE_NONE;
        if (move) {
            meet(i, move);
            return;
        }
        // Right over you, too high yet: wait for it.
        if (ax <= REACH_X && rel_fwd >= -14 && rel_fwd <= REACH_FWD && rel_z >= 16) return;
        uint8_t loose = loose_here(i);
        if (!loose && !coming(i)) return;  // queued: it waits for the ball to come
        // The game runs you to where to meet it, at running speed (with the same
        // momentum), easing in as you get there, anywhere in your half. Going out,
        // it leaves it.
        int16_t tx = loose ? PX(ball_x) : meet_x[i];
        int16_t ty = loose ? PX(ball_y) + (i == SIDE_YOU ? 6 : -6) : meet_y[i];  // loose: just behind it
        int16_t dx = tx - PX(pl_x[i]), dy = ty - PX(pl_y[i]);
        // (Only on a trace of this flight: an old one would send you the wrong way.)
        if (loose || (meet_flight[i] == ball_flight && !meet_out[i])) {
            int8_t sp = pl_top[i];
            pl_wx[i] = (int8_t)clamp16(dx << 2, -sp, sp);  // a quarter of the way (SUB) a frame, near
            pl_wy[i] = (int8_t)clamp16(dy << 2, -sp, sp);
            pl_auto[i] = 1;
        }
        // About to go past, out of reach: slide for it, as soon as the slide only
        // just gets there in time (the ball's frames to reach you, rel_fwd / |vy|,
        // down to the slide's frames across, (ax - REACH_X + 2) / SLIDE_SPEED).
        // Toward where it'll cross your line (its trace), not where it is now: an
        // angled ball keeps going across while you lunge.
        int16_t sx = meet_flight[i] == ball_flight ? meet_x[i] - PX(pl_x[i]) : rel_x, asx = abs16(sx);
        if (!loose && !meet_out[i] && asx > REACH_X && asx <= SLIDE_REACH && rel_fwd >= -REACH_BACK && rel_z < SLIDE_Z &&
            rel_fwd * SLIDE_SPEED <= (asx - REACH_X + 2) * abs16(ball_vy))
            slide(i, sx < 0 ? -SLIDE_SPEED : SLIDE_SPEED, 0);
        return;
    }
    // Unready, and it reaches you: you control it; on fire, it knocks you down.
    if (ax <= 6 && rel_fwd >= -3 && rel_fwd <= 4 && rel_z < 20 && coming(i) && in_half(i) && ball_speed() >= SLOW) {
        if (ball_power) knock_down(i);
        else control(i);
    }
}

// --- where to meet it ---
// Where side i should meet the ball: its flight traced on (gravity, a bounce, a
// lob sitting up, not the curve) to where it first lands, then 8 frames more,
// rising off the bounce; or where it gets to i's baseline first. Landing out (and
// not yet bounced in), i lets it go: meet_out[i]. 12 frames a step (gravity over 12
// frames: vz down 9, z up 12 vz - 58), up to 16 steps; run by want_predict.
static void predict(uint8_t i) {
    int16_t x = ball_x, y = ball_y, z = ball_z, vz = ball_vz;
    int16_t vx4 = (ball_vx << 3) + (ball_vx << 2), vy4 = (ball_vy << 3) + (ball_vy << 2);
    int16_t line = pl_y[i];  // your depth: meet it here if you can
    uint8_t down = ball_landings, out = 0, cross = 0;
    uint8_t up = ball_lob || ball_pop;  // a lob or a pop-up: met at the slam spot instead
    meet_flight[i] = ball_flight;
    // Still in the air, coming: it flies straight until it lands, so where (and how
    // high) it crosses your line is exact, t = (line - y) / vy frames on. Low enough
    // to play there, that's where to meet it.
    if (!up && (ball_z || ball_vz) && (i == SIDE_YOU ? ball_vy > 0 && ball_y < line : ball_vy < 0 && ball_y > line)) {
        int16_t t = (line - ball_y) / ball_vy;
        int16_t zt = t < 180 ? ball_z + ball_vz * t - ((t * (t + 1)) >> 3) * 3 : 0;
        if (zt > 0 && zt < TO_SUB(REACH_Z - 2)) {
            x = ball_x + ball_vx * t;
            y = line;
            cross = 1;
        }
    }
    for (uint8_t k = 0; k < 16 && !cross; k++) {
        int16_t x0 = x, y0 = y, z0 = z;
        x += vx4;
        y += vy4;
        // Crossing your line at a height you can play it (the step's middle):
        // meet it there, on its path.
        if (!up && (i == SIDE_YOU ? y0 < line && y >= line : y0 > line && y <= line) &&
            ((z0 + (z > 0 ? z : 0)) >> 1) < TO_SUB(REACH_Z - 2)) {
            x = x0 + ((x - x0) >> 1);
            y = line;
            cross = 1;
            break;
        }
        if (z || vz) {
            z += (vz << 3) + (vz << 2) - 58;
            vz -= 9;
            // A lob or a pop-up coming down through SLAM_Z over your half: meet it
            // there (the step's share of the way down to it), to slam it.
            if (up && !down && z0 >= TO_SUB(SLAM_Z) && z < TO_SUB(SLAM_Z) &&
                (i == SIDE_YOU ? y >= TO_SUB(HALFWAY) : y < TO_SUB(HALFWAY))) {
                int16_t f = ((z0 - TO_SUB(SLAM_Z)) << 4) / (z0 - z);  // sixteenths of the step
                x = x0 + (((x - x0) * f) >> 4);
                y = y0 + (((y - y0) * f) >> 4);
                cross = 1;
                break;
            }
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
        if (down) break;  // a step (12 frames) past the bounce: rising off it
        if (i == SIDE_CPU ? y <= TO_SUB(COURT_FAR + 4) : y >= TO_SUB(COURT_NEAR - 4)) break;
    }
    meet_out[i] = out;
    meet_slam[i] = cross && up;
    meet_x[i] = clamp16(PX(x), COURT_X0 + 4, COURT_X1 - 4);
    // Off a bounce, stand just behind where it'll be (it's in front of you then).
    if (!cross) y += i == SIDE_YOU ? TO_SUB(6) : -TO_SUB(6);
    meet_y[i] = i == SIDE_CPU ? clamp16(PX(y), COURT_FAR + 4, HALFWAY - 6) : clamp16(PX(y), HALFWAY + 6, COURT_NEAR - 4);
}

// Trace it again when its flight has changed (not on the very frame: that's a
// strike's, busy already), now and then anyway (a placed ball, the curve), each
// side on its own frames.
// Trace side i's flight if it's changed (or now and then anyway: a placed ball,
// the curve), not on a frame the flight changed (busy already).
static void want_predict(uint8_t i) {
    uint8_t f = (uint8_t)frame_count;
    if (f == flight_frame || phase != PHASE_RALLY) return;
    // Going away from i over the other half: nothing to meet (only a strike turns
    // it round, a new flight), so no trace: fewer traces, the busiest frames lighter.
    // (Over i's own half, a control touch going up and away: still traced.)
    if (!coming(i) && !in_half(i)) {
        meet_flight[i] = ball_flight;
        meet_slam[i] = meet_out[i] = 0;
        return;
    }
    if (meet_flight[i] != ball_flight || ((f >> 1) & 7) == 0) {
        predict_frame = f;
        predict(i);
    }
}

// --- the CPU ---
// Pick a strike: a lob over you if you're up near halfway, a drop shot if you're
// back on your line, else a curve one time in four, else power.
static void cpu_ready(void) {
    uint8_t r = rand();
    cpu_depth = (r >> 4) & 3;
    if (cpu_depth == 3) cpu_depth = 1;
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
    // Its aim: angled away from you (to the side of it you're not), now and then straight.
    if (!pl_shot[SIDE_CPU]) cpu_aim = (rand() & 3) == 0 ? 1 : PX(pl_x[SIDE_YOU]) < PX(pl_x[SIDE_CPU]) ? 2 : 0;
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
            } else if (!((uint8_t)frame_count & (ball_power ? 3 : 15)) && (rand() & 3)) {  // on fire: taps hard (to meet it clean)
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
    if (arc_clear && (uint8_t)frame_count != flight_frame) {
        arc_clear = 0;
        net_clear();
    }
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
            burst(VFX_SPARK);
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
        if (++ball_fall & 3) ball_vz--;  // gravity: 3/4 SUB a frame
        ball_z += ball_vz;
        if (ball_z <= 0) {
            int16_t up = (-ball_vz * 3) >> 2;
            uint8_t first = !ball_landings;
            ball_z = 0;
            ball_pop = 0;
            if (ball_drop) {
                ball_drop = 0;
                ball_dropped = 1;
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
            ball_vz = up > BOUNCE_DEAD ? up : 0;
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
    gfx_player_shadow_init(OAM_PLAYER_SHADOW);
    gfx_player_shadow_init(OAM_PLAYER_SHADOW + 2);
    banked_bkg_data(SCENE_TILES, PITCH_TILE_COUNT, pitch_tiles, BANK(pitch));
    banked_bkg_data((uint8_t)T_METER, HUD_TILE_COUNT, hud_tiles, BANK(hud));
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    vfx_load();
    banked_sprite_data_vbk1(0, PLAYERS_TILE_COUNT, players_tiles, BANK(players));
    load_pitch();
    proj_init();
    initrand(game_seed);
    for (uint8_t i = 0; i < 2; i++) {
        pl_top[i] = i ? SPEED_CPU : SPEED_YOU;
        pl_accel[i] = ACCEL;
        pl_grip[i] = GRIP;
    }
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

    // You: the d-pad runs; A and B ready a strike (and combine, and tap); SELECT
    // calls it off.
    if (joy_pressed & J_SELECT && phase == PHASE_RALLY) unready(SIDE_YOU);
    if (joy_pressed & (J_A | J_B)) {
        uint8_t b = !(joy_pressed & J_A);
        if (phase == PHASE_SERVE) {
            if (server == SIDE_YOU && !serve_timer) {
                unready(SIDE_YOU);
                pl_shot[SIDE_YOU] = b ? SHOT_CURVE : SHOT_POWER;
                pl_charge[SIDE_YOU] = YOU_TAP;
                pl_aim[SIDE_YOU] = joy & J_LEFT ? 0 : joy & J_RIGHT ? 2 : 1;
                pl_depth[SIDE_YOU] = joy & J_UP ? 2 : joy & J_DOWN ? 0 : 1;
                phase = PHASE_TOSS;
                ball_vz = TOSS_LIFT;
                sfx_play(SFX_TOUCH);
            }
        } else if (pl_shot[SIDE_YOU] || (phase == PHASE_RALLY && !pl_move_timer[SIDE_YOU])) {
            ready(SIDE_YOU, b);  // queue it any time: it charges once the ball's coming
        }
    }
    // Queued (or serving), LEFT or RIGHT sets your aim, UP or DOWN how deep, and
    // they stick.
    if (pl_shot[SIDE_YOU]) {
        if (joy & J_LEFT) pl_aim[SIDE_YOU] = 0;
        else if (joy & J_RIGHT) pl_aim[SIDE_YOU] = 2;
        if (joy & J_UP) pl_depth[SIDE_YOU] = 2;
        else if (joy & J_DOWN) pl_depth[SIDE_YOU] = 0;
    }
    int8_t dx = joy & J_RIGHT ? 1 : joy & J_LEFT ? -1 : 0;
    int8_t dy = joy & J_DOWN ? 1 : joy & J_UP ? -1 : 0;
    player_update(SIDE_YOU, dx, dy);
    cpu_update();
    ball_update();
    // The flight traces, last thing: after anything struck this frame (then none,
    // it's busy already), one side a frame.
    want_predict((uint8_t)frame_count & 1);
    // The meter and your shot's label: not on a strike's frame (it's busy, and both
    // change then): the next.
    if (flight_frame != (uint8_t)frame_count) {
        meter_update();
        shot_label_update();
    }
    draw();
}
