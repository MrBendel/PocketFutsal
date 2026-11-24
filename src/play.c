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
#include "futnet_core.h"
#include "player_anim.h"
#include "pitch.h"
#include "play_hud.h"
#include "vfx.h"
#include "proj.h"
#include "trace.h"
#include "sprites.h"
#include "players.h"

// Sprite tiles (tools/tile-art.mjs). VRAM bank 0: the ball and its shadow, an
// 8x16 pair each. VRAM bank 1: the players, 8 tiles a frame.
#define S_BALL 0  // first two spin frames; gfx_ball_tile selects the six-frame cycle
#define S_SHADOW 4
#define S_AIM 6     // the aim marker
#define S_TRAIL 8   // a trail's dots, largest first: S_TRAIL, +2, +4, +6
#define S_RING 28   // the slam spot: a 16x8 ring's left half (the right, flipped); past
                    // the ball's extra spin tiles (20-27), before the bursts' (32 on)
// Its tiles (an 8x16 sprite: the ring, then blank): 1 bright, 3 inside, as
// tools/tile-art.mjs's ASCII would be ('....1111', '..113333', '.13.....', '13......').
static const uint8_t ring_tiles[32] = {
    0x0f, 0x00, 0x3f, 0x0f, 0x60, 0x20, 0xc0, 0x40, 0xc0, 0x40, 0x60, 0x20, 0x3f, 0x0f, 0x0f, 0x00,
};
// The slam zone: three rings, one inside the other, 48x16 (build/scratch/zoneart.mjs),
// 1 the outer, 2 the middle, 3 the inner (the spot). Its left three columns; the
// right three are them flipped. Six sprites, one row of them: six on its lines, where
// a ring and 8 dots were ten. It closes by palette, not by moving: the lit ring steps
// in as the ball comes down, the others dim.
#define S_ZONE 148   // 6 tiles (sprite tiles past the gauges: play_hud.h)
#define OAM_ZONE 29  // 6
#define ZONE_PAL SPR_PAL_TRAIL_CURVE  // borrowed: a lob or a pop-up leaves no trail
static const uint8_t zone_tiles[96] = {
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00, 0x18, 0x00, 0x60, 0x00, 0x40, 0x00, 0x80, 0x00,
    0x40, 0x00, 0x60, 0x00, 0x18, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x30, 0x00, 0xc0, 0x01, 0x81,
    0x00, 0xc0, 0x00, 0x30, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x3f, 0x3f, 0xc0, 0xc0, 0x00, 0x00,
    0xc0, 0xc0, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#define ZONE_DIM RGB8(32, 96, 112)   // the rings not lit: a shade darker than the court
#define ZONE_LIT RGB8(255, 232, 64)  // the lit one, flashing to ...
#define ZONE_HOT RGB8(255, 144, 32)
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

// Hardware sprites. Lower numbers draw on top. Interleave the ball and
// player blocks by court depth; shadows remain underneath.
#define OAM_BALL ball_oam
uint8_t ball_oam;  // 0, 4 or 8: before, between or behind the players
#define OAM_PLAYER 1  // 4 each, by side
#define OAM_SHADOW 9
#define OAM_AIM 10    // where your queued strike will land
#define OAM_SLAM 19   // where to meet a lob or a pop-up coming down (flashing): a ring,
#define OAM_SLAM_R 28 // its right half
// OAM_PLAYER_SHADOW 15, two a player (with the power gauge in them): play_hud.h. 37-39 free.
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
#define SLIDE_Z 24        // px: a slide gets a ball this high (a diving header)
#define REACH_FWD 8       // in front
#define REACH_BACK 4      // behind (a bicycle kick reaches further)
#define SLIDE_SPEED 48
#define SLIDE_LUNGE 15    // frames of sliding
#define SLIDE_STRETCH 10  // got there within this many of them: a return; later, a pop-up
// The furthest a slide gets you: your reach plus the lunge (16 + 36 px). No other
// limit: you're run to the ball from anywhere; out of this at the last moment,
// you just miss.
#define SLIDE_REACH (REACH_X + ((SLIDE_SPEED * SLIDE_LUNGE) >> 4))
#define SLIDE_FRAMES 64   // a missed slide: on the floor until this runs out
#define SLIDE_REST 28     // a slide that got there: up sooner
// The ball's pace: Mario Tennis's, a little over a second from one player to the
// other. Gravity is 3/4 SUB a frame each frame (3 frames in 4: ball_fall), so
// speeds and lifts are half what they'd be at gravity 3: the same arcs, twice as long.
// The strikes' speeds and arcs (a drive, fire, a slam, a lob) are futnet_core.h's FC_*.
#define POP_LIFT 32       // a pop-up: about 43 px up, 84 frames
#define DROP_HOP 16       // a drop shot's: about 11 px, 43 frames
#define BOUNCE_DEAD 4     // a bounce lower than this: it rolls
#define POWER_FADE 20     // a ball on fire burns out below this speed (|vx| + |vy|)
#define SLOW 8            // a ball slower than this (|vx| + |vy|) is loose
#define FIRE_MEET 12      // the charge it takes to meet a ball on fire cleanly (the CPU's)
#define FIRE_MEET_YOU 8   // yours: two presses (playtests: people press two or three times before it arrives)
#define SLAM_CHARGE FC_SLAM_CHARGE  // your charge for a slam off a pop-up or a lob: four presses (the CPU's: half a meter)
#define SLAM_LEAP_X 28    // px across: your slam's reach, charged, at a lob or a pop-up coming down (a strike's 16)
#define SLAM_LEAP_FWD 20  // px either way in depth (a strike's 4 behind, 8 in front); 12 px of it a leap
#define AIM_IN 16         // px: aimed this far inside the lines (a long shot lands up to 10 px past its aim)
// The power meter (pl_power): a clean strike in a rally adds POWER_GAIN; full, A+B
// together arms the power slam, struck at the next clean strike: SPECIAL_SPEED, gold,
// deep. Only a press timed to within SPECIAL_WINDOW frames of it reaching you (your
// first press after it's struck: no mashing) returns it, on fire; else it knocks you
// down. The CPU times it CPU_SP_TIMED times in 8.
#define POWER_GAIN 1
#define SPECIAL_SPEED 88
#define SPECIAL_WINDOW 5
#define CPU_SP_TIMED 2
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
#define HOME_YOU 196
#define CPU_PLANT 96      // px: the CPU readies when the ball's this close and it's in place
#define CPU_LATE 40       // px: and this close, in place or not
#define CPU_START 8       // its meter as it readies
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
uint8_t you_leap;  // you leapt at a slam last frame: strike it now
uint8_t meet_slam[2];   // meet_* is the slam spot: a lob or a pop-up, met coming down
uint8_t meet_flight[2]; // the flight each side's meet_* was traced for
uint8_t predict_frame;  // the frame a trace last ran on
uint8_t cpu_aim;
uint8_t pl_power[2];    // the power meter (game.h)
uint8_t pl_special[2];  // the power slam armed
uint8_t ball_special;   // the ball's a power slam
uint8_t sp_try[2];      // pressed since the power slam was struck at you (one try)
uint8_t sp_age[2];      // frames since that press
uint8_t sp_timed;       // the last power slam met was timed (tests)
uint8_t cpu_idle;  // non-zero: the CPU's player stands still (tests, the playtest page)

int16_t ball_x, ball_y, ball_z;
int16_t ball_vx, ball_vy, ball_vz;
uint8_t ball_power;
uint8_t ball_gold;    // a gold slam: on fire, and only a full meter meets it clean
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
uint8_t trail_pal;    // the last trail's palette (its color as it fades)
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

// Your shot on the HUD, where it says YOU (a debugging aid): the one queued, or for a
// moment after a strike what went (SEEN_*), else YOU.
#define SEEN_POP 6
#define SEEN_CONTROL 7
#define SEEN_SLAM 8
#define SEEN_HEADER 9
#define SEEN_BICYCLE 10
#define SEEN_SCUFF 11     // a strike with next to no charge: mishit, short of the net
#define SEEN_GO 12        // the CPU's struck it to you and nothing's queued: press!
#define SEEN_FRAMES 60
uint8_t go_cue;       // the ball's coming to you, nothing queued (watched by tests: no blink now)
// Under pressure the CPU mishits: how far it ran for your shot (from where it stood
// as you struck it) and how fast the shot came, past PRESSURE_FREE, out of 64.
#define PRESSURE_FREE 30
int16_t cpu_from_x, cpu_from_y;
uint8_t cpu_rattled;  // the CPU's mishits so far (a pop-up, or scuffed)
uint8_t shot_seen;    // what your last touch was (SHOT_* or SEEN_*)
uint8_t shot_seen_t;  // frames left showing it
uint8_t note_timer;    // the HUD shows a note (GOAL!) while this counts down
uint8_t hud_pending;   // redraw the HUD at the start of the next frame

// The perspective, by depth from -16 to 239 (index depth + 16): the map row, and
// the width scale /128. The same formula as tools/tile-art.mjs, worked out once.
uint8_t proj_y[256];
uint8_t proj_s[256];

static int16_t abs16(int16_t v) {
    return v < 0 ? -v : v;
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

// Your shot's label, where it says YOU: queued to the window's map when it changes.
static void seen(uint8_t what) {
    shot_seen = what;
    shot_seen_t = SEEN_FRAMES;
}

// --- setting up ---
static void unready(uint8_t i) {
    fc_unready(i);
    if (i == SIDE_YOU) you_leap = 0;
}
static void ball_plain(void) { fc_ball_plain(); }

static void serve_setup(uint8_t side) {
    server = side;
    ball_special = 0;
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

static void draw_trail(int16_t sx, int16_t wy) {
    if (goal_timer) trail_len = 0;
    fc_trail_draw(OAM_TRAIL, sx, wy, cam_y);
}

static uint8_t coming(uint8_t i);  // (the rules, below)

// The slam zone (playtest: "something bigger that acts like a target to say get
// here now"): three rings round the spot, the outer as wide as most of your leap.
// The lit ring steps in as the ball comes (by its depth from the spot: past 48 px the
// outer, past 20 the middle, then the spot), flashing; the others dim. Its palette's
// the curve trail's, borrowed while it shows and given back after.
extern uint16_t pal_obj[];
extern volatile uint8_t pal_dirty;
uint8_t zone_on;        // showing (the palette borrowed)
uint8_t zone_lit;       // the lit ring: 0 outer, 1 middle, 2 the spot
static uint8_t zone_key;  // the palette as set: zone_lit and the flash
static uint16_t zone_saved[4];
static void zone_colors(const uint16_t *c) {
    uint16_t *b = &pal_base_obj[ZONE_PAL << 2], *o = &pal_obj[ZONE_PAL << 2];
    for (uint8_t k = 1; k < 4; k++) b[k] = o[k] = c[k];
    pal_dirty = 1;
}
static void zone_off(void) {
    for (uint8_t k = 0; k < 6; k++) move_sprite(OAM_ZONE + k, 0, 0);
    zone_colors(zone_saved);
    zone_on = 0;
}
static void zone_draw(int16_t rx, int16_t ry) {
    if (!zone_on) {
        uint16_t *b = &pal_base_obj[ZONE_PAL << 2];
        for (uint8_t k = 1; k < 4; k++) zone_saved[k] = b[k];
        trail_len = 0;  // a last trail fading out (its palette's ours now): gone
        zone_on = 1;
        zone_key = 0xFF;
    }
    int16_t d = abs16(PX(ball_y) - meet_y[SIDE_YOU]);
    zone_lit = d > 48 ? 0 : d > 20 ? 1 : 2;
    uint8_t key = zone_lit | ((uint8_t)frame_count & 8);
    if (key != zone_key) {
        uint16_t c[4];
        zone_key = key;
        c[1] = c[2] = c[3] = ZONE_DIM;
        c[zone_lit + 1] = (frame_count & 8) ? ZONE_HOT : ZONE_LIT;
        zone_colors(c);
    }
    // You on its lines (your body and shadow, feet 18 px above its middle to 32
    // below, on the screen), the outer ring no longer lit: its two ends go (its sides;
    // its top and bottom stay), the zone four sprites on those lines, not six: with
    // you, your shadow, the ball and its shadow, ten.
    int16_t dy = (int16_t)proj_y[PROJ_I(PX(pl_y[SIDE_YOU]))] - (int16_t)proj_y[PROJ_I(meet_y[SIDE_YOU])];
    uint8_t there = zone_lit && dy >= -18 && dy <= 32;
    for (uint8_t k = 0; k < 6; k++) {
        if (there && (k == 0 || k == 5)) shadow_OAM[OAM_ZONE + k].y = 0;
        else place(OAM_ZONE + k, rx - 16 + (k << 3), ry - 4);
    }
}

// Lying and lifted, for the player being drawn: globals, not locals. As locals with
// their addresses passed to player_move_pose, SDCC zeroed them once before the loop,
// not each pass, so the CPU inherited your slide or knockdown: drawn lying, or split
// in three (standing side-view tiles in the lying layout) when running sideways.
uint8_t draw_lie;
int8_t draw_lift;

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
        uint8_t f = F_STAND, flip = pl_face[i], special = 0;
        draw_lie = 0;
        draw_lift = 0;
        uint8_t m = pl_move_timer[i] ? pl_move[i] : MOVE_NONE;
        if (goal_timer) f = last_scorer == i ? F_WIN : F_LOSE;
        else if (m == MOVE_KICK) f = F_KICK;
        else if (m == MOVE_VOLLEY || m == MOVE_CONTROL) f = F_VOLLEY;
        else if (m >= MOVE_HEADER && m <= MOVE_DOWN) special = 1;
        else if ((pl_shot[i] && !(pl_vx[i] | pl_vy[i])) || (phase == PHASE_TOSS && server == i)) f = F_READY;
        else if (pl_vx[i] || pl_vy[i]) f = (frame_count & 8) ? F_RUN_A : F_RUN_B;
        uint8_t tile = (f << 3) + (i == SIDE_CPU ? VIEW_FRONT : 0);
        if (special) {
            tile = player_move_pose(m, pl_move_timer[i], i == SIDE_CPU, &draw_lie, &draw_lift);
            if (m == MOVE_SLIDE && draw_lie) flip = !flip;
        }
        // Side art follows real momentum, including the coast through a turn.
        // Existing action frames take priority over the locomotion view.
        pl_side[i] = gfx_running_side(pl_side[i], pl_vx[i], pl_vy[i]);
        if (!goal_timer && !special && (f == F_STAND || f == F_RUN_A || f == F_RUN_B) && pl_side[i]) {
            uint8_t pose = (pl_vx[i] || pl_vy[i]) ? ((frame_count & 4) ? 0 : (frame_count & 8) ? 2 : 1) : 0;
            tile = 224 + (pose << 3);
            if (pl_vx[i]) flip = pl_vx[i] < 0;
        }
        uint8_t pal = i == SIDE_YOU ? SPR_PAL_YOU : SPR_PAL_CPU;
        if (pl_shot[i]) pal = fc_charge_pal(i, pal);
        int16_t py = PX(pl_y[i]);
        int16_t sx = proj_x(PX(pl_x[i]), py);
        int16_t sy = (int16_t)proj_y[PROJ_I(py)] - cam_y - draw_lift;
        draw_player(i, tile, draw_lie, sx, sy, pal, flip);
        // Ground position never inherits the body's jump lift.
        gfx_player_shadow(OAM_PLAYER_SHADOW + (i << 1), sx,
            (int16_t)proj_y[PROJ_I(py)] - cam_y);
    }
    // The overlay, and the power gauges under the shadows just placed (play_hud.c;
    // their bars redrawn only off a strike's busy frame).
    hud_update(flight_frame == (uint8_t)frame_count);
    // The ball, up at its height, and its shadow on the court.
    int16_t sx = proj_x(PX(ball_x), by) + 4;
    int16_t sy = (int16_t)proj_y[PROJ_I(by)] - cam_y + 10;
    set_sprite_tile(OAM_BALL, gfx_ball_tile());
    set_sprite_prop(OAM_BALL, ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
    place(OAM_BALL, sx, sy - bz);
    ball_hx = sx + 4;
    ball_hwy = sy - bz + 4 + cam_y;
    if (flight_frame != (uint8_t)frame_count) vfx_draw(cam_y);  // (not a strike's busy frame: a slam's burst shows a frame on)
    set_sprite_tile(OAM_SHADOW, S_SHADOW);
    set_sprite_prop(OAM_SHADOW, SPR_PAL_UI);
    place(OAM_SHADOW, sx, sy - 1);
    if (flight_frame != (uint8_t)frame_count) draw_trail(sx, sy + cam_y - bz);  // (a new flight's starts empty: not on its busy first frame)
    // A lob or a pop-up coming down to you, or a drop shot: a flashing marker where
    // to meet it (the slam spot; off a drop shot's bounce).
    if (meet_flight[SIDE_YOU] == ball_flight && !goal_timer && coming(SIDE_YOU) &&
        ((meet_slam[SIDE_YOU] && (ball_lob || ball_pop)) || ((ball_drop || ball_dropped) && !meet_out[SIDE_YOU]))) {
        uint8_t pal = ((uint8_t)frame_count & 4) ? SPR_PAL_POWER : SPR_PAL_UI;
        int16_t rx = proj_x(meet_x[SIDE_YOU], meet_y[SIDE_YOU]), ry = (int16_t)proj_y[PROJ_I(meet_y[SIDE_YOU])] - cam_y + 12;
        if (meet_slam[SIDE_YOU] && (ball_lob || ball_pop)) {
            move_sprite(OAM_SLAM, 0, 0);  // the zone's inner ring is the spot
            move_sprite(OAM_SLAM_R, 0, 0);
            zone_draw(rx, ry);
        } else {
            set_sprite_tile(OAM_SLAM, S_RING);
            set_sprite_prop(OAM_SLAM, pal);
            place(OAM_SLAM, rx, ry);
            set_sprite_tile(OAM_SLAM_R, S_RING);
            set_sprite_prop(OAM_SLAM_R, pal | S_FLIPX);
            place(OAM_SLAM_R, rx + 8, ry);
            if (zone_on) zone_off();
        }
    } else {
        move_sprite(OAM_SLAM, 0, 0);
        move_sprite(OAM_SLAM_R, 0, 0);
        if (zone_on) zone_off();
    }
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
    fc_ready(i, b, meet_slam[i] && (ball_lob || ball_pop));
}

// --- strikes ---
static void point(uint8_t side, uint8_t reason);  // the ball's rules, below
static uint8_t in_court(int16_t px, int16_t py);
static void want_predict(uint8_t i);

// Set the ball going toward (dx, dy) px away at speed (SUB a frame, |vx| + |vy|,
// the same measure as ball_speed): exact, where the 32 directions would round a
// slight angle away. A division: only as it's struck.
static void launch(int16_t dx, int16_t dy, uint8_t speed) { fc_launch_vel(dx, dy, speed); }

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
    if (i == SIDE_YOU) {  // where the CPU stood as you struck it (its run, if it mishits)
        cpu_from_x = PX(pl_x[SIDE_CPU]);
        cpu_from_y = PX(pl_y[SIDE_CPU]);
    }
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
    ball_special = 0;
}

// Set ball_vz so the ball comes down at depth `land` (px): its time in the air t
// is the distance over its speed, and from height z0 it lands after t frames
// with vz = 3 (t + 1) / 8 - z0 / t (gravity 3/4, a frame at a time: after t
// frames it's z0 + vz t - 3 t (t + 1) / 8 up, near enough). Every strike arcs, so the ball is in the
// air, its shadow under it, and clears the net if it's meant to.
// If it's to go over (clear), it must be NET_H + 3 px up as it gets to the net,
// t_n frames on, so a fast flat strike is lifted, and lands deeper (or long).
int16_t arc_t;         // arc()'s time in the air, frames
uint8_t arc_clear;     // the arc just struck is to clear the net: net_clear() next frame
static uint8_t clear_frame;  // frame net_clear() ran on (spread predict() off it)
static void arc(int16_t land, uint8_t clear) { fc_arc(land, clear); }
static void net_clear(void) { fc_net_clear(); }

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
    int16_t d;
    if (i == SIDE_YOU) {
        // Yours (playtest: "it should always aim to shoot deep, deeper if there's more
        // power... B shots should curve mid field"): A deep, 64 .. 80 past the net by
        // the meter; B mid-court, 48 .. 56. UP 16 deeper (to 96: the baseline's 112,
        // AIM_IN inside it), DOWN 24 shorter.
        d = pl_shot[i] == SHOT_CURVE ? 48 + (c >> 2) : 64 + (c >> 1);
        if (depth == 2) d += 16;
        else if (!depth) d -= 24;
    } else {
        d = 24 + c + (c >> 1);  // the CPU's, past the net: 24 .. 72
        if (depth == 2) d += 24;
        else if (!depth) d = d > 40 ? d - 24 : 16;
    }
    if (pl_shot[i] == SHOT_LOB) {
        // Over their head, and deep: 24 px behind them, at least 60 past the net, so
        // a lob at a player up at the net sends them back for it (a bicycle kick).
        aim_land = i == SIDE_YOU ? min16(PX(pl_y[SIDE_CPU]) - 24, other_y(i, 60)) : max16(PX(pl_y[SIDE_YOU]) + 24, other_y(i, 60));
        aim_land = clamp16(aim_land, COURT_FAR + AIM_IN, COURT_NEAR - AIM_IN);
    }
    else if (pl_shot[i] == SHOT_DROP) aim_land = other_y(i, 18);
    else aim_land = clamp16(other_y(i, d), COURT_FAR + AIM_IN, COURT_NEAR - AIM_IN);
    // A direction from where you are: as far across as a strike goes (half as far as
    // down the court) to the left or right; neither held, half that, away from the
    // other player (straight ahead is where they stand); kept in the court.
    int16_t x0 = PX(pl_x[i]), wide = abs16(aim_land - PX(pl_y[i])) >> 1;
    if (aim == 1 && i == SIDE_YOU) {
        wide >>= 1;
        aim = PX(pl_x[SIDE_CPU]) < x0 ? 2 : 0;
    }
    aim_tx = clamp16(aim == 0 ? x0 - wide : aim == 2 ? x0 + wide : x0, COURT_X0 + AIM_IN, COURT_X1 - AIM_IN);
}

// A clean strike: the readied shot, shaped by the ball's height (move).
static void strike(uint8_t i, uint8_t move) {
    uint8_t was_pop = ball_pop, was_lob = ball_lob;
    aim_point(i);
    uint8_t aim = i == SIDE_YOU ? (aim_tx < CENTER_X ? 0 : aim_tx > CENTER_X ? 2 : 1) : cpu_aim;
    uint8_t c = pl_charge[i], kind = pl_shot[i], frames = KICK_FRAMES;
    uint8_t scuff = c < SCUFF && phase == PHASE_RALLY;
    int16_t land = scuff ? other_y(i, -10) : aim_land;
    uint8_t slam = move != MOVE_KICK && (was_pop || was_lob) && c >= (i == SIDE_YOU ? SLAM_CHARGE : CHARGE_MAX / 2);
    uint8_t special = pl_special[i] && phase == PHASE_RALLY;
    if (special) {
        // The power slam: gold, at SPECIAL_SPEED, deep (a bicycle kick, whatever the height).
        pl_shot[i] = kind = SHOT_GOLD;
        pl_charge[i] = c = CHARGE_MAX;
        scuff = 0;
        slam = 1;
        land = clamp16(other_y(i, 88), COURT_FAR + AIM_IN, COURT_NEAR - AIM_IN);
        fc_gold_speed = SPECIAL_SPEED;
        frames = BICYCLE_FRAMES;
    } else if (slam) {
        land = other_y(i, 64);
        frames = BICYCLE_FRAMES;
    } else if (move == MOVE_HEADER) frames = KICK_FRAMES + 4;
    else if (move == MOVE_BICYCLE) frames = BICYCLE_FRAMES;
    move = fc_strike_ball(i, move, aim_tx, land, FC_FLAGS(aim, scuff, slam, phase != PHASE_RALLY));
    uint8_t trail = ball_trail;  // struck() calls new_flight() which clears ball_trail
    struck(i, move, frames);
    ball_trail = trail;
    if (special) {
        fc_gold_speed = FC_GOLD_SPEED;
        pl_special[i] = 0;
        ball_special = 1;
        sp_try[i ^ 1] = 0;
    } else if (phase == PHASE_RALLY && !scuff && pl_power[i] < POWER_MAX) {
        pl_power[i] += POWER_GAIN;  // a clean strike in a rally fills the power meter
        if (pl_power[i] >= POWER_MAX) {
            pl_power[i] = POWER_MAX;
            if (i == SIDE_YOU) sfx_play(SFX_CHARGE);
        }
    }
    if (slam) burst(VFX_SLAM);  // an impact star: big for a slam, small on fire
    else if (ball_power) burst(VFX_FIRE);
    if (i == SIDE_YOU)
        seen(scuff ? SEEN_SCUFF : slam ? SEEN_SLAM :
             move == MOVE_HEADER ? SEEN_HEADER : move == MOVE_BICYCLE ? SEEN_BICYCLE : kind);
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
    ball_special = 0;
    ball_vx >>= 1;
    ball_vy >>= 1;
    sfx_play(SFX_PARRY);
}

// Planted and in reach: a clean strike, unless it's on fire and the meter isn't
// three eighths full, when it's all they can do to pop it up.
// The CPU, meeting your shot, may mishit it under pressure: the further it ran and
// the faster it came, the likelier: out of 64, a quarter of the run (px) plus half
// the speed, past PRESSURE_FREE. (It runs about 74 px for a shot at the median, at
// 28: none; a wide fast one, 100 px at 40, about a quarter.) Mostly it pops up (a
// slam for you); a quarter of the time, scuffed into its own half.
// The power slam reaching player i: timed (your first press since it was struck,
// within SPECIAL_WINDOW frames; the CPU by chance), they meet it with a full meter.
static uint8_t special_timed(uint8_t i) {
    sp_timed = i == SIDE_CPU ? (rand() & 7) < CPU_SP_TIMED : sp_try[i] && sp_age[i] <= SPECIAL_WINDOW;
    if (sp_timed) pl_charge[i] = CHARGE_MAX;
    return sp_timed;
}

static void meet(uint8_t i, uint8_t move) {
    if (ball_special) {
        if (special_timed(i)) strike(i, move);  // a counter, on fire
        else knock_down(i);
        return;
    }
    if (ball_power && pl_charge[i] < (ball_gold ? CHARGE_MAX : i == SIDE_YOU ? FIRE_MEET_YOU : FIRE_MEET)) {
        pop_up(i, move, KICK_FRAMES);
        return;
    }
    if (i == SIDE_CPU && last_touch == SIDE_YOU) {
        int16_t p = ((abs16(PX(pl_x[i]) - cpu_from_x) + abs16(PX(pl_y[i]) - cpu_from_y)) >> 2) +
                    (ball_speed() >> 1) - PRESSURE_FREE;
        uint8_t r = rand() & 63;
        if (p > (int16_t)r) {
            cpu_rattled++;
            if (r >= (p >> 2)) {
                pop_up(i, move, KICK_FRAMES);
                return;
            }
            pl_charge[i] = 0;  // scuffed
        }
    }
    strike(i, move);
}

static void slide(uint8_t i, int8_t vx, int8_t vy) {
    pl_slide_vx[i] = vx;
    pl_slide_vy[i] = vy;
    pl_face[i] = vx < 0;
    pl_move[i] = MOVE_SLIDE;
    pl_move_timer[i] = SLIDE_FRAMES;  // the strike stays readied: slide and return
    if (i == SIDE_YOU) you_leap = 0;
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
    fc_combo_tick(i);
    if (pl_move_timer[i]) {
        pl_vx[i] = pl_vy[i] = 0;  // mid-move: stopped (it starts again from still)
        pl_move_timer[i]--;
        if (pl_move[i] != MOVE_SLIDE) return;
        if (pl_move_timer[i] < SLIDE_FRAMES - SLIDE_LUNGE) {
            if (pl_shot[i]) unready(i);  // the lunge over, not there: missed
            return;
        }
        // The slide: along the court, reaching for it, the strike still readied.
        pl_x[i] += pl_slide_vx[i];
        pl_y[i] += pl_slide_vy[i];
        keep_in(i);
        relate(i);
        if (in_half(i) && abs16(rel_x) <= REACH_X && rel_fwd >= -REACH_BACK - 2 && rel_fwd <= REACH_FWD && rel_z < SLIDE_Z) {
            // Got there: slide and return it, the strike by its height; at full
            // stretch (the lunge's last third), only a pop-up (on fire, without
            // the meter to meet it: down you go).
            if (ball_special ? !special_timed(i) : ball_power && pl_charge[i] < (i == SIDE_YOU ? FIRE_MEET_YOU : FIRE_MEET)) {
                knock_down(i);
                return;
            }
            if (pl_move_timer[i] < SLIDE_FRAMES - SLIDE_STRETCH) pop_up(i, MOVE_SLIDE, 0);
            else meet(i, rel_z < 5 ? MOVE_KICK : rel_z < 16 ? MOVE_VOLLEY : MOVE_HEADER);
            pl_move[i] = MOVE_SLIDE;
            pl_move_timer[i] = SLIDE_REST;  // got there: up sooner
        }
        return;
    }
    // Planted, readying a strike: still, once the ball's over the net into your half
    // (before, queued early, you still run: playtest, "stuck from the power up before
    // I think I should"); the d-pad then only aims. Serving: along the line only.
    if ((pl_shot[i] && phase == PHASE_RALLY && in_half(i)) || (phase == PHASE_TOSS && server == i)) dx = dy = 0;
    if (phase != PHASE_RALLY && server == i) dx = dy = 0;  // serving: the d-pad aims, it doesn't walk you
    // Momentum: toward the speed held (or the auto-run's), through approach().
    int8_t speed = pl_top[i];
    int8_t tx = dx > 0 ? speed : dx < 0 ? -speed : 0;
    int8_t ty = dy > 0 ? speed : dy < 0 ? -speed : 0;
    uint8_t firm = i;  // the CPU (SIDE_CPU, 1)
    if (pl_auto[i]) {
        tx = pl_wx[i];
        ty = pl_wy[i];
        pl_auto[i] = 0;
        // Run for: it stops firmly where it means to, but yours still skids turning
        // round (firm 2), as when you steer: side to side costs you.
        firm = i == SIDE_YOU ? 2 : 1;
    }
    fc_min_x = TO_SUB(OUT_X0 + 8);
    fc_max_x = TO_SUB(OUT_X1 - 8);
    fc_min_y = i == SIDE_YOU ? TO_SUB(HALFWAY + 10) : TO_SUB(OUT_Y0 + 8);
    fc_max_y = i == SIDE_YOU ? TO_SUB(OUT_Y1 - 4) : TO_SUB(HALFWAY - 10);
    fc_step_move(i, tx, ty, firm);

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
        // Leapt last frame (below): the slam now, a frame on (a strike and a leap in
        // one frame were the busiest frame of a match).
        if (i == SIDE_YOU && you_leap) {
            you_leap = 0;
            if ((ball_lob || ball_pop) && rel_z < REACH_Z + 8) {
                meet(i, MOVE_HEADER);
                return;
            }
        }
        uint8_t move = in_half(i) ? in_reach() : MOVE_NONE;
        if (move) {
            meet(i, move);
            return;
        }
        // Your slam, charged: a leap at a lob or a pop-up coming down near you (the
        // ring's a zone, not a spot: playtest, "near impossible to get there").
        if (i == SIDE_YOU && (ball_lob || ball_pop) && ball_vz < 0 && pl_charge[i] >= SLAM_CHARGE && in_half(i) &&
            rel_z >= 4 && rel_z < REACH_Z + 8 && ax <= SLAM_LEAP_X && abs16(rel_fwd) <= SLAM_LEAP_FWD) {
            pl_x[i] += TO_SUB(clamp16(rel_x, -12, 12));
            pl_y[i] -= TO_SUB(clamp16(rel_fwd, -12, 12));
            keep_in(i);
            you_leap = 1;
            return;
        }
        // Right over you, too high yet: wait for it.
        if (ax <= REACH_X && rel_fwd >= -14 && rel_fwd <= REACH_FWD && rel_z >= 16) return;
        uint8_t loose = loose_here(i);
        if (!loose && !coming(i)) return;  // queued: it waits for the ball to come
        // Queued, you're locked where you stood (playtest: "position after the kick
        // to where the ball's landing, power up, locked; near it you return, a bit
        // further you slide and return, too far you miss"): only a loose ball in
        // your half walks you to it. The CPU too: it gets there, then readies.
        if (loose) {
            int16_t dx = PX(ball_x) - PX(pl_x[i]);
            int16_t dy = PX(ball_y) + (i == SIDE_YOU ? 6 : -6) - PX(pl_y[i]);  // just behind it
            int8_t sp = pl_top[i];
            pl_wx[i] = (int8_t)clamp16(dx << 2, -sp, sp);  // a quarter of the way (SUB) a frame, near
            pl_wy[i] = (int8_t)clamp16(dy << 2, -sp, sp);
            pl_auto[i] = 1;
        }
        // About to go past, out of reach: slide for it, as soon as the slide only
        // just gets there in time (the ball's frames to reach you, rel_fwd / |vy|,
        // down to the slide's frames across, (ax - REACH_X + 2) / SLIDE_SPEED).
        // Toward where it'll cross your line (its trace), not where it is now: an
        // angled ball keeps going across while you lunge. Too far for a slide to
        // get there, you still try, at full stretch (playtest: "the player should
        // always make the attempt even if they're too far"); a ball going out, you
        // let go.
        int16_t sx = meet_flight[i] == ball_flight ? meet_x[i] - PX(pl_x[i]) : rel_x, asx = abs16(sx);
        if (!loose && !meet_out[i] && asx > REACH_X && rel_fwd >= -REACH_BACK && rel_z < SLIDE_Z &&
            rel_fwd * SLIDE_SPEED <= (min16(asx, SLIDE_REACH) - REACH_X + 2) * abs16(ball_vy))
            slide(i, sx < 0 ? -SLIDE_SPEED : SLIDE_SPEED, 0);
        return;
    }
    // Unready, and it reaches you: you control it; on fire, it knocks you down.
    if (ax <= 6 && rel_fwd >= -3 && rel_fwd <= 4 && rel_z < 20 && coming(i) && in_half(i) && ball_speed() >= SLOW) {
        if (ball_power) knock_down(i);
        else control(i);
    }
}

// --- where to meet it: predict() (trace.c) ---

// Trace it again when its flight has changed (not on the very frame: that's a
// strike's, busy already), now and then anyway (a placed ball, the curve), each
// side on its own frames.
// Trace side i's flight if it's changed (or now and then anyway: a placed ball,
// the curve), not on a frame the flight changed (busy already).
static void want_predict(uint8_t i) {
    uint8_t f = (uint8_t)frame_count;
    if (f == flight_frame || f == clear_frame || phase != PHASE_RALLY) return;
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
    // Readied as it gets there (locked, as you are), not as the ball sets off: two
    // of your presses' worth to start (with less it netted half its strikes).
    if (pl_charge[SIDE_CPU] < CPU_START) pl_charge[SIDE_CPU] = CPU_START;
    // Its power meter full: the power slam, one time in two as it readies.
    if (pl_power[SIDE_CPU] == POWER_MAX && (r & 0x80)) {
        pl_power[SIDE_CPU] = 0;
        pl_special[SIDE_CPU] = 1;
        pl_charge[SIDE_CPU] = CHARGE_MAX;
    }
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
            // There: lined up across, near enough in depth (where to meet it moves
            // with where it stands: exactly there, it readied late, scuffing).
            uint8_t there = abs16(tx - cx) <= 3 && abs16(ty - cy) <= 10;
            if (meet_out[SIDE_CPU]) {
                if (pl_shot[SIDE_CPU]) unready(SIDE_CPU);
            } else if (!pl_shot[SIDE_CPU]) {
                // By your rules: it gets there first, then readies (locked, as you
                // are); the ball close and not there yet, it readies anyway and
                // slides for it if it must.
                if (there || dist < CPU_LATE) cpu_ready();
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

// --- the AI on your side (the title's PLAYER: AI, to see how fair the match is) ---
// It plays with your buttons and nothing else, as a fair human might: 12-27 frames
// after the CPU strikes it picks where it'll meet the ball (the game's trace, from
// where it stands; a ball going out it lets go), walks there on the d-pad, and once
// there (or with the ball 40 px off) presses A, or B one time in four, then once or
// twice more, holding the d-pad away from the CPU (its aim) three times in four.
uint8_t you_ai;      // non-zero: the AI plays your side
uint8_t ai_seen;     // the CPU's strikes when it last looked
uint8_t ai_wait;     // frames until it reacts (0: not waiting)
uint8_t ai_plan;     // non-zero: it's walking to ai_tx, ai_ty
int16_t ai_tx, ai_ty;
uint8_t ai_btn;      // J_A or J_B
uint8_t ai_aim;      // J_LEFT, J_RIGHT or 0, held once queued
uint8_t ai_presses;  // presses still to make
uint8_t ai_next;     // frames to the next
uint8_t ai_sp_t;     // the power slam: it presses this many frames out (0-7; within 5 is timed)

static void ai_input(void) {
    uint8_t pad = 0, press = 0;
    if (pl_shots[SIDE_CPU] != ai_seen) {
        ai_seen = pl_shots[SIDE_CPU];
        ai_wait = 12 + (rand() & 15);
        ai_plan = ai_presses = 0;
        ai_btn = (rand() & 3) ? J_A : J_B;
        ai_aim = !(rand() & 3) ? 0 : PX(pl_x[SIDE_CPU]) < CENTER_X ? J_RIGHT : J_LEFT;
        ai_sp_t = rand() & 7;
    }
    if (phase == PHASE_SERVE) {
        if (server == SIDE_YOU && !serve_timer && !((uint8_t)frame_count & 15)) press = J_A;
    } else if (phase == PHASE_RALLY) {
        // The power slam coming: one press, timed by its distance over its speed.
        if (ball_special && last_touch == SIDE_CPU && !sp_try[SIDE_YOU]) {
            int16_t d = PX(pl_y[SIDE_YOU]) - PX(ball_y) - REACH_FWD;
            if ((d << 4) <= (int16_t)ai_sp_t * abs16(ball_vy)) press = ai_btn;
        }
        if (ai_wait && !--ai_wait && coming(SIDE_YOU) && !meet_out[SIDE_YOU]) {
            ai_plan = 1;
            ai_tx = meet_x[SIDE_YOU];
            ai_ty = clamp16(meet_y[SIDE_YOU], 140, 205);
        }
        if (pl_shot[SIDE_YOU]) {
            if (in_half(SIDE_YOU)) pad = ai_aim;  // aims once planted (before, it would run)
            if (ai_presses && !--ai_next && !ball_special) press = ai_btn, ai_presses--, ai_next = 8;
        } else if (loose_here(SIDE_YOU) && !pl_move_timer[SIDE_YOU] && !ball_special) {
            press = ai_btn;  // a loose ball: queue, and the game runs you to it
        } else if (ai_plan) {
            if (meet_out[SIDE_YOU] || !coming(SIDE_YOU)) {
                ai_plan = 0;
            } else {
                int16_t dx = ai_tx - PX(pl_x[SIDE_YOU]), dy = ai_ty - PX(pl_y[SIDE_YOU]);
                if (dx > 3) pad = J_RIGHT; else if (dx < -3) pad = J_LEFT;
                if (dy > 3) pad |= J_DOWN; else if (dy < -3) pad |= J_UP;
                // (The power slam coming, it saves its press for the timing, above.)
                if (!ball_special && ((abs16(dx) <= 4 && abs16(dy) <= 6) || abs16(PX(ball_y) - PX(pl_y[SIDE_YOU])) < 40)) {
                    pad = 0;
                    press = pl_power[SIDE_YOU] == POWER_MAX ? J_A | J_B : ai_btn;  // full: the power slam
                    ai_presses = 1 + (rand() & 1);
                    ai_next = 8;
                    ai_plan = 0;
                }
            }
        }
    }
    joy = pad | press;
    joy_pressed = press;
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
        clear_frame = (uint8_t)frame_count;
        net_clear();
    }
    if (phase == PHASE_SERVE) {
        // At the server's feet, in front of them and to the side.
        ball_x = pl_x[server] + TO_SUB(8);
        ball_y = pl_y[server] + (server == SIDE_YOU ? -TO_SUB(6) : TO_SUB(6));
        return;
    }
    // A curve: pushed across, a sixteenth of a SUB at a time.
    fc_curve_step();
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
    gfx_ball_step(ball_vx,ball_vy);
    // The floor round the court ends somewhere: the ball stops at its edge.
    if (PX(x) < OUT_X0 || PX(x) > OUT_X1) x = ball_x, ball_vx = 0, ball_curve_t = 0;
    if (PX(y) < OUT_Y0 || PX(y) > OUT_Y1) y = ball_y, ball_vy = 0;
    ball_x = x;
    ball_y = y;
}

// --- the scene ---
void play_enter(void) BANKED {
    palettes_play();
    gfx_ball_init(20);
    gfx_player_shadow_init(OAM_PLAYER_SHADOW);
    gfx_player_shadow_init(OAM_PLAYER_SHADOW + 2);
    banked_bkg_data(SCENE_TILES, PITCH_TILE_COUNT, pitch_tiles, BANK(pitch));
    banked_sprite_data(0, SPRITES_TILE_COUNT, sprites_tiles, BANK(sprites));
    vfx_load();
    set_sprite_data(S_RING, 2, ring_tiles);
    set_sprite_data(S_ZONE, 6, zone_tiles);
    for (uint8_t k = 0; k < 6; k++) {  // set once: a frame only moves them
        set_sprite_tile(OAM_ZONE + k, S_ZONE + ((k < 3 ? k : 5 - k) << 1));
        set_sprite_prop(OAM_ZONE + k, k < 3 ? ZONE_PAL : ZONE_PAL | S_FLIPX);
        move_sprite(OAM_ZONE + k, 0, 0);
    }
    zone_on = 0;
    banked_sprite_data_vbk1(0, PLAYERS_TILE_COUNT, players_tiles, BANK(players));
    load_pitch();
    hud_init();  // the score board in the court art, the faces, the gauges
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
    ai_seen = ai_wait = ai_plan = ai_presses = 0;
    pl_power[0] = pl_power[1] = pl_special[0] = pl_special[1] = 0;
    pl_last_move[0] = pl_last_move[1] = MOVE_NONE;
    goal_timer = 0;
    paused = 0;
    winner = NOBODY;
    last_scorer = NOBODY;
    cam_y = CAM_MAX;
    // No HUD row: the window is play_hud.c's overlay, shown before each serve and
    // for a point's call, sprites free to draw over it.
    HIDE_WIN;
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
    if (you_ai) ai_input();  // the AI's buttons in place of yours

    // You: the d-pad runs; A and B ready a strike (and combine, and tap); SELECT
    // calls it off.
    if (joy_pressed & J_SELECT && phase == PHASE_RALLY) unready(SIDE_YOU);
    if (sp_age[SIDE_YOU] != 255) sp_age[SIDE_YOU]++;
    if ((joy_pressed & (J_A | J_B)) && ball_special && last_touch == SIDE_CPU && !sp_try[SIDE_YOU]) {
        sp_try[SIDE_YOU] = 1;  // your one try at timing the power slam
        sp_age[SIDE_YOU] = 0;
    }
    // The power meter full, A and B together (one pressed, the other held): the power
    // slam, armed for your next clean strike.
    if ((joy_pressed & (J_A | J_B)) && (joy & J_A) && (joy & J_B) && pl_power[SIDE_YOU] == POWER_MAX &&
        phase == PHASE_RALLY && !pl_move_timer[SIDE_YOU]) {
        pl_power[SIDE_YOU] = 0;
        pl_special[SIDE_YOU] = 1;
        if (!pl_shot[SIDE_YOU]) ready(SIDE_YOU, 0);
        pl_shot[SIDE_YOU] = SHOT_POWER;
        pl_charge[SIDE_YOU] = CHARGE_MAX;
        sfx_play(SFX_POWER);
    } else if (joy_pressed & (J_A | J_B)) {
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
            if (!b && (joy_pressed & J_B)) ready(SIDE_YOU, 1);  // A and B at once: both
        }
    }
    // Queued and planted (the ball in your half) or serving, LEFT or RIGHT sets your
    // aim, UP or DOWN how deep, and they stick. Before, the d-pad runs you.
    if (pl_shot[SIDE_YOU] && (phase != PHASE_RALLY || in_half(SIDE_YOU))) {
        if (joy & J_LEFT) pl_aim[SIDE_YOU] = 0;
        else if (joy & J_RIGHT) pl_aim[SIDE_YOU] = 2;
        if (joy & J_UP) pl_depth[SIDE_YOU] = 2;
        else if (joy & J_DOWN) pl_depth[SIDE_YOU] = 0;
    }
    int8_t dx = joy & J_RIGHT ? 1 : joy & J_LEFT ? -1 : 0;
    int8_t dy = joy & J_DOWN ? 1 : joy & J_UP ? -1 : 0;
    // Between shots (nothing queued, the d-pad let go, the ball going away), you jog
    // back to the middle, HOME_YOU deep, as the CPU goes home: a rally doesn't creep
    // you up to the net, where everything goes over your head. (Coming to you,
    // nothing queued, you stay put: the game running you there, the overnight
    // loop's idea, played like an autopilot, a control touch that never went over.)
    if (!(dx | dy) && !pl_shot[SIDE_YOU] && phase == PHASE_RALLY && !coming(SIDE_YOU)) {
        int16_t hx = CENTER_X - PX(pl_x[SIDE_YOU]), hy = HOME_YOU - PX(pl_y[SIDE_YOU]);
        pl_wx[SIDE_YOU] = (int8_t)clamp16(hx, -SPEED_YOU / 2, SPEED_YOU / 2);  // a jog
        pl_wy[SIDE_YOU] = (int8_t)clamp16(hy, -SPEED_YOU / 2, SPEED_YOU / 2);
        pl_auto[SIDE_YOU] = 1;
    }
    // No autopilot after a strike (playtest: "the player auto moves after kicking"):
    // you stand where you are till the d-pad moves you, the CPU going home meanwhile.
    player_update(SIDE_YOU, dx, dy);
    cpu_update();
    ball_update();
    // The flight traces, last thing: after anything struck this frame (then none,
    // it's busy already), one side a frame.
    want_predict((uint8_t)frame_count & 1);
    go_cue = last_touch == SIDE_CPU && !pl_shot[SIDE_YOU] && !pl_move_timer[SIDE_YOU] && phase == PHASE_RALLY &&
             !goal_timer && coming(SIDE_YOU);
    draw();
}
