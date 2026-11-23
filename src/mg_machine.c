#pragma bank 255
// BALL MACHINE (see minigame.h for the shell's contract): a ball machine at the far
// end of the futnet court fires balls over the net into your half; run to each
// and press A to strike it back over, inside the far half's lines. A ball that
// lands in is a point; in the gold ring (it moves every shot), three. In the net,
// out, or not reached before its second bounce: a miss. Three misses end it.
//
// Trains REFLEX (getting to the ball: your run speed and reach rise with it) and
// CONTROL (a cleaner return: less scatter, fewer mishits into the net).
//
// The court and its perspective are the match's (game.h, play.c): flat court
// coordinates (x across, y in depth from the far baseline, 0, to yours, 224),
// in SUBs where they move, projected onto the screen through proj_y / proj_s.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <string.h>
#include "state.h"
#include "gfx.h"
#include "input.h"
#include "sfx.h"
#include "game.h"
#include "futnet_core.h"
#include "proj.h"
#include "player_anim.h"
#include "minigame.h"
#include "progress.h"
#include "mg_machine_bg.h"
#include "mg_machine_spr.h"

#define PROJ_I(fy) ((uint8_t)((fy) + PROJ_OFS))

// --- the scene ---
#define CAM 16             // matches tools/art/mg_machine.mjs (screen_y = proj_y - 16, scroll_y = 0)
#define MACHINE_Y (-4)     // court px: the machine stands just behind the far baseline
#define MACHINE_Z 6        // px: its barrel's height
#define MX_MIN 40          // where it may roll to along the baseline
#define MX_MAX 120
#define PL_X_MIN 0         // where you may run: your half and a little round it
#define PL_X_MAX 160
#define PL_Y_MIN (HALFWAY + 8)
#define PL_Y_MAX 228

// Sprite tiles (tools/art/mg_machine.mjs SPR), from MG_SPR_TILES.
#define T_MACHINE (MG_SPR_TILES + 0)       // three 8x16 columns
#define T_MACHINE_TURN (MG_SPR_TILES + 6)  // turned to your left (flipped: your right)
#define T_RING (MG_SPR_TILES + 12)         // three columns
#define T_FLASH (MG_SPR_TILES + 18)
// OAM (lower numbers draw on top).
#define OAM_FLASH (OAM_MG_FREE + 0)
#define OAM_MACHINE (OAM_MG_FREE + 1)  // 3
#define OAM_AIM (OAM_MG_FREE + 4)
#define OAM_RING (OAM_MG_FREE + 5)     // 3
#define OAM_TRAIL (OAM_MG_FREE + 8)    // 4: shared Futnet red/blue/gold shot trail
#define OAM_LAST (OAM_MG_FREE + 11)
// Keep SPR_PAL_TRAIL_POWER (red) & SPR_PAL_TRAIL_CURVE (blue) for real shot trails!
#define PAL_MACHINE SPR_PAL_CPU
#define PAL_RING SPR_PAL_POWER

// --- tuning ---
#define SPEED_BASE 21      // your run, SUB a frame, + REFLEX level / 2
#define ACCEL 3            // as in the match: a few frames to start, stop, turn
#define BRAKE 4
#define REACH_BASE 14      // px either side a strike reaches, + REFLEX level / 3
#define REACH_FWD 12       // px in front of you
#define REACH_BACK 6       // and behind
#define REACH_Z 34         // a header, this high
#define SWING_BUF 8        // A strikes the ball if it comes in reach within these frames
#define STRIKE_FRAMES 14   // a strike's pose: stopped meanwhile
#define WHIFF_FRAMES 18    // a swing at nothing: stopped a little longer
#define RET_KICK_T 58      // your return's frames in the air, by the strike
#define RET_VOLLEY_T 64
#define RET_HEADER_T 74
#define BOUNCE_MIN 15      // a first bounce gets up at least this (vz, SUB)
#define BOUNCE_DEAD 4
#define NOTE_FRAMES 60
#define DEAD_FRAMES 48     // a dead ball rolls on this long before it's cleared
#define END_FRAMES 45      // the third miss shows, then the game ends
#define LEVEL_MAX 10
#define LEVEL_RETURNS 4    // returns in for each step up in pace
// The return's aim spots on the far half (court px): LEFT / straight / RIGHT,
// UP deep / short with DOWN. The ring sits on one of these nine.
#define AIM_X_L 32
#define AIM_X_R 128
#define AIM_Y_DEEP 22
#define AIM_Y_MID 52
#define AIM_Y_SHORT 80
#define RING_X 14          // px either side of its middle the ring counts
#define RING_Y 12

// The ball's states.
#define BS_NONE 0   // none out
#define BS_IN 1     // the machine's shot, coming at you
#define BS_OUT 2    // your return, on its way
#define BS_DEAD 3   // decided: rolling on, out of play
// The machine's.
#define MS_WAIT 0   // between shots
#define MS_TELL 1   // rolling to its spot, turning, flashing: about to fire
#define MS_FLIGHT 2 // a ball is out
// How a ball turned out (mg_machine_result).
#define R_NONE 0
#define R_IN 1      // your return landed in: 1 point
#define R_TARGET 2  // in the ring: 3
#define R_NET 3     // misses:
#define R_OUT 4
#define R_MISS 5    // not reached before its second bounce
#define R_LONG 6    // the machine's shot went out: no harm done
#define PTS_IN 1
#define PTS_TARGET 3
#define MISSES_MAX 3

// --- watchable state ---
uint16_t mg_machine_points;
uint16_t mg_machine_returns;   // returns in (the pace goes up every LEVEL_RETURNS)
uint16_t mg_machine_targets;   // of them, in the ring
uint8_t mg_machine_misses;
uint8_t mg_machine_level;      // 0 .. LEVEL_MAX: the pace
uint8_t mg_machine_result;     // R_*: the last ball's
uint8_t mg_machine_state;      // MS_*
uint8_t mg_machine_timer;      // its countdown
uint8_t mg_machine_aim;        // where it's turned: 0 your left, 1 straight, 2 right
uint8_t mg_machine_recoil;     // frames of the kick as it fires
int16_t mg_machine_mx, mg_machine_mx_to;  // its x along the baseline (px), and where it's rolling
uint8_t mg_machine_shot_t;     // its next shot's frames in the air
int16_t mg_machine_shot_x, mg_machine_shot_y;  // where it'll land (court px)
// The ball (SUB; z up from the court).
uint8_t mg_machine_ball;       // BS_*
int16_t mg_machine_bx, mg_machine_by, mg_machine_bz;
int16_t mg_machine_bvx, mg_machine_bvy, mg_machine_bvz;
uint8_t mg_machine_bounces;    // in the half it's in
uint8_t mg_machine_fall;       // gravity acts 3 frames in 4
uint8_t mg_machine_roll;
uint8_t mg_machine_dead;       // a dead ball's countdown
// You.
int16_t mg_machine_px, mg_machine_py;  // SUB
int8_t mg_machine_pvx, mg_machine_pvy;
uint8_t mg_machine_face;       // 1: facing left
uint8_t mg_machine_swing;      // A pressed: frames left for the ball to come in reach
uint8_t mg_machine_move;       // MOVE_* (game.h): the pose of the last strike
uint8_t mg_machine_move_t;     // ... while this counts down
uint8_t mg_machine_speed;      // your run speed (REFLEX)
uint8_t mg_machine_reach;      // your reach across (REFLEX)
uint8_t mg_machine_bad;        // the last strike's contact: 0 clean .. (scatter, the net)
uint8_t mg_machine_netted;     // the last return was mishit into the net
// The ring and your return.
int16_t mg_machine_tx, mg_machine_ty;          // the ring's middle (court px)
int16_t mg_machine_land_x, mg_machine_land_y;  // where the return is going (court px)
uint8_t mg_machine_note;       // the HUD shows a note while this counts down
uint8_t mg_machine_end;        // after the third miss: counts down to the end

static char note_buf[21];

// Background palettes 0..4 (tools/art/mg_machine.mjs): Mario Tennis GBC training court.
//   0: gold & crimson stadium wall, 1: steel-blue far deck + net, 2/3: coral/amber deck LEDs,
//   4: vibrant emerald green player half with crisp white lines.
static const uint16_t bg_pals[5 * 4] = {
    RGB8(248, 200, 56),  RGB8(216, 72, 96),   RGB8(88, 24, 48),   RGB8(255, 248, 232),
    RGB8(120, 200, 248), RGB8(56, 136, 208),  RGB8(24, 48, 96),   RGB8(240, 248, 255),
    RGB8(120, 200, 248), RGB8(56, 136, 208),  RGB8(24, 48, 96),   RGB8(255, 64, 104),
    RGB8(120, 200, 248), RGB8(56, 136, 208),  RGB8(24, 48, 96),   RGB8(255, 192, 40),
    RGB8(40, 184, 96),   RGB8(30, 152, 78),   RGB8(16, 96, 52),   RGB8(244, 252, 244),
};
// Sprite palettes: machine (3), target ring (4), and crisp aim/shadow marks (7).
static const uint16_t machine_pal[4] = {0, RGB8(248, 240, 216), RGB8(232, 96, 32), RGB8(24, 20, 32)};
static const uint16_t ring_pal[4] = {0, RGB8(255, 240, 80), RGB8(248, 152, 16), RGB8(120, 32, 16)};
static const uint16_t marks_pal[4] = {0, RGB8(255, 240, 72), RGB8(32, 80, 96), RGB8(16, 32, 44)};

// --- helpers ---
static int16_t abs16(int16_t v) {
    return v < 0 ? -v : v;
}
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
// A random number from -r to r (r < 128): a multiply, no division.
static int16_t spread(uint8_t r) {
    return (int16_t)(((uint16_t)rand() * (uint16_t)(2 * r + 1)) >> 8) - r;
}
// A random number from 0 to n - 1.
static uint8_t roll(uint8_t n) {
    return (uint8_t)(((uint16_t)rand() * n) >> 8);
}
static int8_t approach(int8_t v, int8_t t) {
    if (v < t) {
        v += v < 0 ? BRAKE : ACCEL;
        if (v > t) v = t;
    } else if (v > t) {
        v -= v > 0 ? BRAKE : ACCEL;
        if (v < t) v = t;
    }
    return v;
}
static int16_t proj_x(int16_t fx, int16_t fy) {
    return CENTER_X + (((fx - CENTER_X) * (int16_t)proj_s[PROJ_I(fy)]) >> 7);
}
static int16_t proj_sy(int16_t fy) {  // a court depth -> its screen line
    return (int16_t)proj_y[PROJ_I(fy)] - CAM;
}
static uint8_t in_far_half(int16_t x, int16_t y) {  // lines are in
    return x >= COURT_X0 && x <= COURT_X1 && y >= COURT_FAR && y < HALFWAY;
}
static uint8_t in_near_half(int16_t x, int16_t y) {
    return x >= COURT_X0 && x <= COURT_X1 && y >= HALFWAY && y <= COURT_NEAR;
}

// --- the HUD: PTS 12   MISS X - - , or a note for a moment ---
static void hud_draw(void) {
    fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
    win_text(0, 0, "PTS");
    win_number(4, 0, mg_machine_points, 3, 0);
    win_text(10, 0, "MISS");
    for (uint8_t k = 0; k < MISSES_MAX; k++) win_text(15 + (k << 1), 0, k < mg_machine_misses ? "X" : "-");
}
static void note(const char *s, uint8_t faster) {
    uint8_t n = 0;
    while (s[n]) note_buf[n] = s[n], n++;
    if (faster) {
        const char *f = " FASTER!";
        while (*f) note_buf[n++] = *f++;
    }
    note_buf[n] = 0;
    fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
    win_text((SCREEN_TW - n) >> 1, 0, note_buf);
    mg_machine_note = NOTE_FRAMES;
}

// --- the ball's flight ---
// Height at the net: for a ball now at (y, z) going at vy, vz, its frames to the
// net tn and gravity's pull by then (3/4 SUB a frame each frame).
static int16_t net_tn, net_drop;
static void to_net(int16_t vy) {
    int16_t d = abs16(TO_SUB(HALFWAY) - mg_machine_by);
    int16_t avy = abs16(vy);
    if (!avy) avy = 1;
    net_tn = clamp16(d / avy, 1, 160);
    net_drop = ((net_tn * (net_tn + 1)) >> 3) * 3;
}
// Set vz to come down after t frames (from the ball's height now), and make sure
// it's NET_H + 3 px up at the net (or, mishit, low enough to hit the tape).
static void arc(int16_t t, uint8_t into_net) {
    int16_t z = mg_machine_bz;
    if (t < 12) t = 12;
    mg_machine_bvz = (3 * (t + 1)) >> 3;
    if (z) mg_machine_bvz -= z / t;
    to_net(mg_machine_bvy);
    int16_t at_net = z + mg_machine_bvz * net_tn - net_drop;
    if (into_net) {
        if (at_net > TO_SUB(NET_H - 5)) mg_machine_bvz = (TO_SUB(NET_H - 5) + net_drop - z) / net_tn;
    } else if (at_net < TO_SUB(NET_H + 3)) {
        mg_machine_bvz = (TO_SUB(NET_H + 3) + net_drop - z) / net_tn;
    }
}

// Where a flight from the ball's position now first comes down (court px), run
// on a frame at a time as ball_update would: once a shot, before it fires.
static int16_t trace_x, trace_y;
static void trace(void) {
    int16_t x = mg_machine_bx, y = mg_machine_by, z = mg_machine_bz, vz = mg_machine_bvz;
    uint8_t f = mg_machine_fall;
    for (uint8_t k = 0; k < 220; k++) {
        if (++f & 3) vz--;
        z += vz;
        x += mg_machine_bvx;
        y += mg_machine_bvy;
        if (z <= 0) break;
    }
    trace_x = PX(x);
    trace_y = PX(y);
}

// --- the machine ---
// Its next shot: where it lands, how long it's in the air (the pace), and the ring.
static void plan_shot(void) {
    uint8_t lv = mg_machine_level;
    int16_t wide = 24 + lv * 6;  // across: from the middle, wider as it goes
    if (wide > 60) wide = 60;
    uint8_t t = 84 - (lv << 2);  // frames in the air: 84 .. 44
    uint8_t kind = lv >= 2 ? roll(4) : 1;
    int16_t ly = 150 + roll(36 + lv * 3);
    if (kind == 0) {
        t += 34;  // a lob: high, deep, dropping steeply: it sits up off the bounce
        ly = 188 + roll(20);
    } else if (kind == 3 && lv >= 4) {
        t -= 12;  // a skimmer: low and fast
        ly = 140 + roll(30);
    }
    t += roll(16);
    mg_machine_shot_t = t;
    mg_machine_shot_x = clamp16(CENTER_X + spread((uint8_t)wide), COURT_X0 + 10, COURT_X1 - 10);
    mg_machine_shot_y = clamp16(ly, HALFWAY + 24, COURT_NEAR - 16);
    // It rolls along the baseline to fire from somewhere new (from level 3).
    mg_machine_mx_to = lv >= 3 ? MX_MIN + roll(MX_MAX - MX_MIN) : CENTER_X;
    int16_t d = mg_machine_shot_x - mg_machine_mx_to;
    mg_machine_aim = d < -14 ? 0 : d > 14 ? 2 : 1;
    // The ring: one of the nine aim spots, not where it just was.
    int16_t ox = mg_machine_tx, oy = mg_machine_ty;
    do {
        uint8_t r = roll(9);
        uint8_t c = r % 3, w = r / 3;
        mg_machine_tx = c == 0 ? AIM_X_L : c == 1 ? CENTER_X : AIM_X_R;
        mg_machine_ty = w == 0 ? AIM_Y_DEEP : w == 1 ? AIM_Y_MID : AIM_Y_SHORT;
    } while (mg_machine_tx == ox && mg_machine_ty == oy);
}

static void begin_tell(void) {
    plan_shot();
    mg_machine_state = MS_TELL;
    int16_t tell = 44 - 3 * mg_machine_level;  // 44 .. 14 frames of warning
    int16_t roll_far = abs16(mg_machine_mx_to - mg_machine_mx);
    if (tell < roll_far) tell = roll_far;  // time to get there (a px a frame)
    mg_machine_timer = (uint8_t)tell;
    sfx_play(SFX_TOUCH);
}

static void fire(void) {
    uint8_t t = mg_machine_shot_t;
    mg_machine_bx = TO_SUB(mg_machine_mx);
    mg_machine_by = TO_SUB(MACHINE_Y);
    mg_machine_bz = TO_SUB(MACHINE_Z);
    mg_machine_fall = 0;
    mg_machine_bvx = TO_SUB(mg_machine_shot_x - mg_machine_mx) / t;
    mg_machine_bvy = TO_SUB(mg_machine_shot_y - MACHINE_Y) / t;
    arc(t, 0);
    // Lifted over the net, a fast one flies on further: keep it in.
    trace();
    if (trace_y > COURT_NEAR - 6) {
        mg_machine_bvy -= mg_machine_bvy >> 2;
        trace();
    }
    mg_machine_shot_x = trace_x;
    mg_machine_shot_y = trace_y;
    mg_machine_ball = BS_IN;
    mg_machine_bounces = 0;
    mg_machine_state = MS_FLIGHT;
    mg_machine_recoil = 12;
    mg_machine_result = R_NONE;
    sfx_play(SFX_KICK);
}

static void machine_update(void) {
    if (mg_machine_recoil) mg_machine_recoil--;
    if (mg_machine_state == MS_WAIT) {
        if (mg_machine_timer) mg_machine_timer--;
        else if (!mg_machine_end) begin_tell();
    } else if (mg_machine_state == MS_TELL) {
        if (mg_machine_mx < mg_machine_mx_to) mg_machine_mx++;
        else if (mg_machine_mx > mg_machine_mx_to) mg_machine_mx--;
        if (!--mg_machine_timer) fire();
    }
}

// --- a ball decided ---
static void decide(uint8_t r) {
    uint8_t faster = 0;
    mg_machine_result = r;
    mg_machine_ball = BS_DEAD;
    mg_machine_dead = DEAD_FRAMES;
    if (r == R_IN || r == R_TARGET) {
        mg_machine_points += r == R_TARGET ? PTS_TARGET : PTS_IN;
        mg_machine_returns++;
        if (r == R_TARGET) mg_machine_targets++;
        if (mg_machine_level < LEVEL_MAX && mg_machine_returns >= (uint16_t)(mg_machine_level + 1) * LEVEL_RETURNS) {
            mg_machine_level++;
            faster = 1;
        }
        sfx_play(r == R_TARGET ? SFX_GOAL : SFX_CHARGE);
        note(r == R_TARGET ? "TARGET! +3" : "IN! +1", faster);
    } else if (r == R_LONG) {
        note("LONG! NO MISS", 0);
    } else {
        mg_machine_misses++;
        sfx_play(r == R_NET ? SFX_BOARD : SFX_PARRY);
        note(r == R_NET ? "NET! MISS" : r == R_OUT ? "OUT! MISS" : "MISSED IT!", 0);
        if (mg_machine_misses >= MISSES_MAX) mg_machine_end = END_FRAMES;
    }
    mg_machine_state = MS_WAIT;
    int16_t gap = 50 - (mg_machine_level << 2);  // 50 .. 10 frames between shots
    mg_machine_timer = (uint8_t)gap;
}

// --- the ball ---
static void ball_update(void) {
    if (mg_machine_ball == BS_NONE) return;
    if (mg_machine_ball == BS_OUT) {
        if (arc_clear) {
            arc_clear = 0;
            ball_x = mg_machine_bx; ball_y = mg_machine_by; ball_z = mg_machine_bz;
            ball_vx = mg_machine_bvx; ball_vy = mg_machine_bvy; ball_vz = mg_machine_bvz;
            fc_net_clear();
            mg_machine_bvz = ball_vz;
        }
        if (ball_curve_t) {
            ball_vx = mg_machine_bvx;
            fc_curve_step();
            mg_machine_bvx = ball_vx;
        }
    }
    int16_t x = mg_machine_bx + mg_machine_bvx, y = mg_machine_by + mg_machine_bvy;
    // Your return, across the net: over it, or into it.
    if (mg_machine_ball == BS_OUT && PX(y) < HALFWAY && PX(mg_machine_by) >= HALFWAY) {
        if (mg_machine_bz < TO_SUB(NET_H) && PX(x) >= COURT_X0 - 6 && PX(x) <= COURT_X1 + 5) {
            y = mg_machine_by;
            mg_machine_bvy = -(mg_machine_bvy >> 2);
            mg_machine_bvx >>= 1;
            ball_curve_t = 0;
            ball_trail = 0;
            decide(R_NET);
        } else {
            mg_machine_bounces = 0;
        }
    }
    // In the air: gravity, and a bounce as it lands, three quarters as high again.
    if (mg_machine_bz || mg_machine_bvz) {
        if (++mg_machine_fall & 3) mg_machine_bvz--;
        mg_machine_bz += mg_machine_bvz;
        if (mg_machine_bz <= 0) {
            int16_t up = (-mg_machine_bvz * 3) >> 2;
            uint8_t first = !mg_machine_bounces;
            mg_machine_bz = 0;
            if (first && up < BOUNCE_MIN) up = BOUNCE_MIN;
            mg_machine_bvx -= mg_machine_bvx >> 2;
            mg_machine_bvy -= mg_machine_bvy >> 2;
            mg_machine_bvz = up > BOUNCE_DEAD ? up : 0;
            mg_machine_bounces++;
            ball_trail = 0;
            ball_power = 0;
            ball_curve_t = 0;
            int16_t lx = PX(x), ly = PX(y);
            if (mg_machine_ball == BS_IN) {
                if (first && !in_near_half(lx, ly)) decide(R_LONG);
                else if (!first || !mg_machine_bvz) decide(R_MISS);
            } else if (mg_machine_ball == BS_OUT) {
                if (!in_far_half(lx, ly)) decide(R_OUT);
                else if (abs16(lx - mg_machine_tx) <= RING_X && abs16(ly - mg_machine_ty) <= RING_Y) decide(R_TARGET);
                else decide(R_IN);
            }
        }
    } else if (!(frame_count & 1)) {
        // On the ground: rolling, slowing.
        if (mg_machine_bvx > 0) mg_machine_bvx--;
        else if (mg_machine_bvx < 0) mg_machine_bvx++;
        if (mg_machine_bvy > 0) mg_machine_bvy--;
        else if (mg_machine_bvy < 0) mg_machine_bvy++;
    }
    // Past you on the full, or rolled off the back: gone.
    if (mg_machine_ball == BS_IN && PX(y) > OUT_Y1) decide(mg_machine_bounces ? R_MISS : R_LONG);
    if (mg_machine_bvx || mg_machine_bvy) mg_machine_roll++;
    if (PX(x) < OUT_X0 || PX(x) > OUT_X1) x = mg_machine_bx, mg_machine_bvx = 0;
    if (PX(y) < OUT_Y0 || PX(y) > OUT_Y1) y = mg_machine_by, mg_machine_bvy = 0;
    mg_machine_bx = x;
    mg_machine_by = y;
    if (mg_machine_ball == BS_DEAD && mg_machine_dead && !--mg_machine_dead) mg_machine_ball = BS_NONE;
}

// --- you (shared Futnet movement, charge-up, power/curve/lob/drop strikes) ---
static void strike(uint8_t move, int16_t rel_x, int16_t rel_fwd) {
    uint8_t aim = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : pl_aim[SIDE_YOU];
    uint8_t dep = (joy & J_UP) ? 2 : (joy & J_DOWN) ? 0 : pl_depth[SIDE_YOU];
    int16_t ax = aim == 0 ? AIM_X_L : aim == 2 ? AIM_X_R : CENTER_X;
    int16_t ay = dep == 2 ? AIM_Y_DEEP : dep == 0 ? AIM_Y_SHORT : AIM_Y_MID;
    if (pl_shot[SIDE_YOU] == SHOT_LOB) ay = AIM_Y_DEEP;
    else if (pl_shot[SIDE_YOU] == SHOT_DROP) ay = AIM_Y_SHORT;
    uint8_t ctl = (stat_lv[STAT_CONTROL] - 1) >> 1;  // 0 .. 9
    int16_t bad = (abs16(rel_x) << 1) + (rel_fwd < 0 ? -rel_fwd * 3 : 0) + (move == MOVE_HEADER ? 6 : 0);
    int16_t rx = 6 + bad - ctl, ry = 4 + (bad >> 1) - (ctl >> 1);
    if (rx < 4) rx = 4;
    if (ry < 3) ry = 3;
    mg_machine_bad = (uint8_t)bad;
    mg_machine_land_x = ax + spread((uint8_t)rx);
    mg_machine_land_y = ay + spread((uint8_t)ry);
    int16_t netc = bad > 18 ? ((bad - 18) << 2) - (ctl << 1) : 0;
    mg_machine_netted = netc > 0 && rand() < (uint8_t)netc;
    // Hand off to Futnet's shared strike engine (power, curve, lob, drop, trails, SFX).
    pl_x[SIDE_YOU] = mg_machine_px;
    pl_y[SIDE_YOU] = mg_machine_py;
    ball_x = mg_machine_bx;
    ball_y = mg_machine_by;
    ball_z = mg_machine_bz;
    move = fc_strike_ball(SIDE_YOU, move, mg_machine_land_x, mg_machine_land_y,
                          aim, mg_machine_netted, 0, 0);
    mg_machine_bvx = ball_vx;
    mg_machine_bvy = ball_vy;
    mg_machine_bvz = ball_vz;
    mg_machine_fall = 0;
    mg_machine_ball = BS_OUT;
    mg_machine_bounces = 0;
    mg_machine_move = move;
    mg_machine_move_t = STRIKE_FRAMES + (move == MOVE_HEADER ? 4 : 0);
    mg_machine_face = rel_x < 0;
    fc_unready(SIDE_YOU);
}

// The machine's ball in reach: strike it (the height picks the strike).
static uint8_t try_strike(void) {
    if (mg_machine_ball != BS_IN || PX(mg_machine_by) < HALFWAY) return 0;
    int16_t rel_x = PX(mg_machine_bx) - PX(mg_machine_px);
    int16_t rel_fwd = PX(mg_machine_py) - PX(mg_machine_by);
    int16_t z = PX(mg_machine_bz);
    if (abs16(rel_x) > mg_machine_reach || rel_fwd > REACH_FWD || rel_fwd < -REACH_BACK || z > REACH_Z) return 0;
    strike(z < 5 ? MOVE_KICK : z < 16 ? MOVE_VOLLEY : MOVE_HEADER, rel_x, rel_fwd);
    return 1;
}

static void player_update(void) {
    fc_combo_tick(SIDE_YOU);
    // Sync watchable mg_machine_px/py <-> shared pl_x/pl_y in case tests write mg_machine_px directly.
    pl_x[SIDE_YOU] = mg_machine_px;
    pl_y[SIDE_YOU] = mg_machine_py;
    pl_vx[SIDE_YOU] = mg_machine_pvx;
    pl_vy[SIDE_YOU] = mg_machine_pvy;
    pl_face[SIDE_YOU] = mg_machine_face;
    pl_top[SIDE_YOU] = (int8_t)mg_machine_speed;

    if (mg_machine_move_t) {
        mg_machine_move_t--;
        mg_machine_pvx = mg_machine_pvy = 0;
        pl_vx[SIDE_YOU] = pl_vy[SIDE_YOU] = 0;
        return;
    }

    // Queue / tap-charge with Futnet's exact A (power) / B (curve) / B->A (lob) / A->B (drop).
    if (joy_pressed & J_A) {
        fc_ready(SIDE_YOU, 0, 0);
        mg_machine_swing = SWING_BUF;
    } else if (joy_pressed & J_B) {
        fc_ready(SIDE_YOU, 1, 0);
        mg_machine_swing = SWING_BUF;
    }
    if (joy_pressed & J_SELECT) fc_unready(SIDE_YOU);
    if (pl_shot[SIDE_YOU]) {
        if (joy & J_LEFT) pl_aim[SIDE_YOU] = 0;
        else if (joy & J_RIGHT) pl_aim[SIDE_YOU] = 2;
        if (joy & J_UP) pl_depth[SIDE_YOU] = 2;
        else if (joy & J_DOWN) pl_depth[SIDE_YOU] = 0;
    }

    int8_t sp = (int8_t)mg_machine_speed;
    int8_t tx = 0, ty = 0;
    uint8_t firm = 0;
    if (pl_shot[SIDE_YOU] && mg_machine_ball == BS_IN) {
        // Queued while ball is incoming: D-pad only aims, game auto-runs you to the ball!
        int16_t dx = PX(mg_machine_bx) - PX(mg_machine_px);
        int16_t dy = (PX(mg_machine_by) + 6) - PX(mg_machine_py);
        tx = (int8_t)clamp16(dx << 2, -sp, sp);
        ty = (int8_t)clamp16(dy << 2, -sp, sp);
        firm = 1;
    } else if (!pl_shot[SIDE_YOU]) {
        tx = joy & J_RIGHT ? sp : joy & J_LEFT ? -sp : 0;
        ty = joy & J_DOWN ? sp : joy & J_UP ? -sp : 0;
    }
    fc_step_move(SIDE_YOU, tx, ty, firm,
                 TO_SUB(PL_X_MIN), TO_SUB(PL_X_MAX), TO_SUB(PL_Y_MIN), TO_SUB(PL_Y_MAX));
    mg_machine_px = pl_x[SIDE_YOU];
    mg_machine_py = pl_y[SIDE_YOU];
    mg_machine_pvx = pl_vx[SIDE_YOU];
    mg_machine_pvy = pl_vy[SIDE_YOU];
    mg_machine_face = pl_face[SIDE_YOU];

    if (pl_shot[SIDE_YOU]) {
        if (try_strike()) {
            mg_machine_swing = 0;
        }
    } else if (mg_machine_swing) {
        mg_machine_swing--;
    }
}

// --- drawing ---
static void draw(void) {
    // You: shared Futnet poses, side-running, and 3-stage charge-up flash!
    uint8_t f = MG_F_STAND, lie = 0, special = 0;
    int8_t lift = 0;
    if (mg_machine_move_t) {
        if (mg_machine_move == MOVE_HEADER || mg_machine_move == MOVE_BICYCLE) special = 1;
        else f = mg_machine_move == MOVE_VOLLEY ? MG_F_VOLLEY : MG_F_KICK;
    } else if (mg_run == MG_RUN_OVER && mg_machine_misses >= MISSES_MAX) {
        f = MG_F_LOSE;
    } else if (pl_shot[SIDE_YOU]) {
        f = MG_F_READY;
    } else if (mg_machine_pvx || mg_machine_pvy) {
        f = (frame_count & 8) ? MG_F_RUN_A : MG_F_RUN_B;
    }
    uint8_t ptile = MG_FRAME(MG_VIEW_BACK, f);
    uint8_t pflip = mg_machine_face;
    if (special) {
        ptile = player_move_pose(mg_machine_move, mg_machine_move_t, 0, &lie, &lift);
    } else {
        pl_side[SIDE_YOU] = gfx_running_side(pl_side[SIDE_YOU], mg_machine_pvx, mg_machine_pvy);
        if ((f == MG_F_STAND || f == MG_F_RUN_A || f == MG_F_RUN_B) && pl_side[SIDE_YOU]) {
            uint8_t pose = (mg_machine_pvx || mg_machine_pvy) ? ((frame_count & 4) ? 0 : (frame_count & 8) ? 2 : 1) : 0;
            ptile = 224 + (pose << 3);
            if (mg_machine_pvx) pflip = mg_machine_pvx < 0;
        }
    }
    uint8_t ppal = fc_charge_pal(SIDE_YOU, SPR_PAL_YOU);
    int16_t py = PX(mg_machine_py);
    mg_player(0, ptile, proj_x(PX(mg_machine_px), py), proj_sy(py) - lift, ppal, pflip);

    // The ball, its shadow, and its red/blue shot trail.
    if (mg_machine_ball == BS_NONE) {
        mg_hide(OAM_MG_BALL, OAM_MG_SHADOW);
        mg_hide(OAM_TRAIL, OAM_TRAIL + 3);
    } else {
        int16_t by = PX(mg_machine_by), bz = PX(mg_machine_bz);
        int16_t bx_s = proj_x(PX(mg_machine_bx), by), by_s = proj_sy(by);
        mg_ball(bx_s, by_s, bz, mg_machine_bvx || mg_machine_bvy,
                ball_power ? SPR_PAL_POWER : SPR_PAL_BALL);
        fc_trail_draw(OAM_TRAIL, bx_s + 4, by_s + 8 - bz, 0);
    }
    // The machine: turned the way it'll fire, flashing as it's about to, kicking
    // back as it fires, a burst of fire at its mouth.
    int16_t cx = proj_x(mg_machine_mx, MACHINE_Y), top = proj_sy(MACHINE_Y) - 12;
    uint8_t aim = mg_machine_state == MS_TELL || mg_machine_recoil ? mg_machine_aim : 1;
    uint8_t tile = aim == 1 ? T_MACHINE : T_MACHINE_TURN, flip = aim == 2 ? S_FLIPX : 0;
    uint8_t pal = PAL_MACHINE;
    if (mg_machine_state == MS_TELL && (mg_machine_timer & 4)) pal = SPR_PAL_FLASH;
    if (mg_machine_recoil > 6) top -= 2, pal = SPR_PAL_POWER;
    else if (mg_machine_recoil) top -= 1;
    for (uint8_t c = 0; c < 3; c++) {
        uint8_t col = flip ? 2 - c : c;
        mg_sprite(OAM_MACHINE + c, tile + (col << 1), pal | flip, cx - 12 + (c << 3), top);
    }
    if (mg_machine_recoil > 4) {
        int16_t mouth = cx + (aim == 0 ? -4 : aim == 2 ? 4 : 0);
        mg_sprite(OAM_FLASH, T_FLASH, SPR_PAL_POWER, mouth - 4, top + 6);
    } else {
        move_sprite(OAM_FLASH, 0, 0);
    }
    // The ring, where it's worth three (blinking as it moves to a new spot).
    if (mg_machine_state == MS_TELL && (mg_machine_timer & 2)) {
        mg_hide(OAM_RING, OAM_RING + 2);
    } else {
        int16_t rx = proj_x(mg_machine_tx, mg_machine_ty) - 12, ry = proj_sy(mg_machine_ty) - 6;
        for (uint8_t c = 0; c < 3; c++) mg_sprite(OAM_RING + c, T_RING + (c << 1), PAL_RING, rx + (c << 3), ry);
    }
    // While a ball comes at you, a marker where the d-pad / queued aim sends your return.
    if (mg_machine_ball == BS_IN && mg_run == MG_RUN_LIVE) {
        uint8_t aim = (joy & J_LEFT) ? 0 : (joy & J_RIGHT) ? 2 : pl_shot[SIDE_YOU] ? pl_aim[SIDE_YOU] : 1;
        uint8_t dep = (joy & J_UP) ? 2 : (joy & J_DOWN) ? 0 : pl_shot[SIDE_YOU] ? pl_depth[SIDE_YOU] : 1;
        int16_t ax = aim == 0 ? AIM_X_L : aim == 2 ? AIM_X_R : CENTER_X;
        int16_t ay = dep == 2 ? AIM_Y_DEEP : dep == 0 ? AIM_Y_SHORT : AIM_Y_MID;
        mg_sprite(OAM_AIM, MG_T_AIM, SPR_PAL_UI, proj_x(ax, ay) - 4, proj_sy(ay) - 4);
    } else {
        move_sprite(OAM_AIM, 0, 0);
    }
}

// --- the game ---
void mg_machine_enter(void) BANKED {
    mg_load_bg(mg_machine_bg_tiles, MG_MACHINE_BG_TILE_COUNT, mg_machine_bg_map, mg_machine_bg_attr,
               MG_MACHINE_BG_MAP_W, MG_MACHINE_BG_MAP_H, BANK(mg_machine_bg));
    scroll_y = 0;
    memcpy(pal_base_bg, bg_pals, sizeof(bg_pals));
    mg_load_spr(mg_machine_spr_tiles, MG_MACHINE_SPR_TILE_COUNT, BANK(mg_machine_spr));
    memcpy(&pal_base_obj[PAL_MACHINE << 2], machine_pal, sizeof(machine_pal));
    memcpy(&pal_base_obj[SPR_PAL_UI << 2], marks_pal, sizeof(marks_pal));
    proj_init();
    fc_unready(SIDE_YOU);
    fc_ball_plain();
    pl_accel[SIDE_YOU] = FC_ACCEL;
    pl_grip[SIDE_YOU] = FC_GRIP;
    pl_skid[SIDE_YOU] = pl_side[SIDE_YOU] = pl_flash[SIDE_YOU] = 0;
    arc_clear = 0;

    mg_machine_points = mg_machine_returns = mg_machine_targets = 0;
    mg_machine_misses = mg_machine_level = 0;
    mg_machine_result = R_NONE;
    mg_machine_state = MS_WAIT;
    mg_machine_timer = 30;  // a moment after GO!
    mg_machine_aim = 1;
    mg_machine_recoil = 0;
    mg_machine_mx = mg_machine_mx_to = CENTER_X;
    mg_machine_ball = BS_NONE;
    mg_machine_bvx = mg_machine_bvy = mg_machine_bvz = mg_machine_bz = 0;
    mg_machine_px = TO_SUB(CENTER_X);
    mg_machine_py = TO_SUB(COURT_NEAR - 24);
    mg_machine_pvx = mg_machine_pvy = 0;
    mg_machine_face = 0;
    mg_machine_swing = mg_machine_move_t = 0;
    mg_machine_move = MOVE_NONE;
    mg_machine_note = mg_machine_end = 0;
    mg_machine_tx = CENTER_X;
    mg_machine_ty = AIM_Y_MID;
    // REFLEX: quicker to the ball, a longer reach.
    uint8_t rf = stat_lv[STAT_REFLEX];
    mg_machine_speed = SPEED_BASE + (rf >> 1);
    mg_machine_reach = REACH_BASE + rf / 3;
    mg_hide(OAM_MG_FREE, OAM_LAST);
    draw();
}

void mg_machine_update(void) BANKED {
    if (mg_run == MG_RUN_READY) {
        draw();
        return;
    }
    if (mg_run == MG_RUN_LIVE) {
        if (mg_hud_dirty) {
            mg_hud_dirty = 0;
            if (!mg_machine_note) {
                hud_draw();
            } else {
                fill_win_rect(0, 0, SCREEN_TW, HUD_ROWS, 0);
                win_text((SCREEN_TW - (uint8_t)strlen(note_buf)) >> 1, 0, note_buf);
            }
        }
        if (mg_machine_note && !--mg_machine_note) hud_draw();
        if (mg_machine_end && !--mg_machine_end) {
            // RETURNS 12  RING 3
            char *d = mg_detail;
            const char *s = "RETURNS ";
            while (*s) *d++ = *s++;
            uint16_t n = mg_machine_returns;
            char t[5];
            uint8_t k = 0;
            do t[k++] = '0' + n % 10, n /= 10; while (n && k < 4);
            while (k) *d++ = t[--k];
            s = "  RING ";
            while (*s) *d++ = *s++;
            n = mg_machine_targets;
            do t[k++] = '0' + n % 10, n /= 10; while (n && k < 4);
            while (k) *d++ = t[--k];
            *d = 0;
            mg_finish(mg_machine_points);
        } else {
            player_update();
            machine_update();
        }
    } else if (mg_machine_move_t) {
        mg_machine_move_t--;
    }
    ball_update();
    draw();
}
