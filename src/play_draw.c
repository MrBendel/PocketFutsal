#pragma bank 255
// The match's drawing (from play.c, 2026-11-26, to make room in its bank for the
// RPG's stats): the camera, the players (their poses, Codex's slide, bicycle and
// point reactions streamed in), the ball, its shadow and trail, the slam spot and
// zone. play.c's state is all global (game.h); the aim marker stays there, with
// aim_point.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "game.h"
#include "futnet_core.h"
#include "player_anim.h"
#include "slide_art.h"
#include "reaction_art.h"
#include "play_hud.h"
#include "play_draw.h"
#include "vfx.h"
#include "proj.h"
#include "state.h"
#include "sprites.h"

static int16_t abs16(int16_t v) { return v < 0 ? -v : v; }
#define PROJ_I(fy) ((uint8_t)((fy) + PROJ_OFS))
static int16_t proj_x(int16_t fx, int16_t fy) {
    return CENTER_X + (((fx - CENTER_X) * (int16_t)proj_s[PROJ_I(fy)]) >> 7);
}
// Coming this way (toward player i's baseline).
static uint8_t coming(uint8_t i) {
    return i == SIDE_YOU ? ball_vy > 0 : ball_vy < 0;
}

// Sprite tiles (tools/tile-art.mjs). VRAM bank 0: the ball and its shadow, an
// 8x16 pair each. VRAM bank 1: the players, 8 tiles a frame.
#define S_BALL 0  // first two spin frames; gfx_ball_tile selects the six-frame cycle
#define S_SHADOW 4
#define S_TRAIL 8   // a trail's dots, largest first: S_TRAIL, +2, +4, +6
#define S_RING 28   // the slam spot: a 16x8 ring's left half (the right, flipped); past
                    // the ball's extra spin tiles (20-27), before the bursts' (32 on)
// Its tiles (an 8x16 sprite: the ring, then blank): 1 bright, 3 inside, as
// tools/tile-art.mjs's ASCII would be ('....1111', '..113333', '.13.....', '13......').
static const uint8_t ring_tiles[32] = {
    0x0f, 0x00, 0x3f, 0x0f, 0x60, 0x20, 0xc0, 0x40, 0xc0, 0x40, 0x60, 0x20, 0x3f, 0x0f, 0x0f, 0x00,
};
// The slam zone: three rings, one inside the other, 1 px apart, 16x11
// (build/scratch/zoneart3.mjs), 1 the outer, 2 the middle, 3 the inner (the spot).
// Its left column; the right is it flipped. Two sprites. It moves by palette, not
// by moving: the lit ring steps in, outer to the spot, over and over (2026-11-25,
// playtest: "a smaller circle with 3 concentric rings ... cycle the palette to make
// it look like it's moving inward", then "a smaller ring where the rings are all
// connected, 1px apart"), quicker as the ball comes down.
#define S_ZONE 148   // 2 tiles (sprite tiles past the gauges: play_hud.h)
#define OAM_ZONE 29  // 2
#define ZONE_N 2
#define ZONE_PAL SPR_PAL_TRAIL_CURVE  // borrowed: a lob or a pop-up leaves no trail
static const uint8_t zone_tiles[32] = {
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x1c, 0x00, 0x20, 0x03, 0x40, 0x0c, 0x43, 0x13, 0x8c, 0x2c,
    0x43, 0x13, 0x40, 0x0c, 0x20, 0x03, 0x1c, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#define ZONE_DIM RGB8(32, 96, 112)   // the rings not lit: a shade darker than the court
#define ZONE_LIT RGB8(255, 232, 64)  // the lit one
#define ZONE_HOT RGB8(255, 144, 32)  // the one it just left (outside it): a trail in
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
#define OAM_SLAM 19   // where to meet a lob or a pop-up coming down (flashing): a ring,
#define OAM_SLAM_R 28 // its right half
// OAM_PLAYER_SHADOW 15, two a player (with the power gauge in them): play_hud.h. 31-34 the slide's two more a player (draw_slide); 37-39 free.
#define OAM_TRAIL 11  // 4: the ball's trail
#define TRAIL_MAX 4

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

// The slide (2026-11-26: Codex's feet-first study, artwork/players/slide/study-v1, by
// tools/export-slide.mjs): 32x24 frames facing right (mirrored for left), up to six
// 8x16 sprites, the player's four and two more (OAM 31-32 you, 33-34 the CPU). Their
// tiles are loaded as the frame changes, into sprite tiles 192-203 (you) and 208-219
// (the CPU), VRAM bank 0 (free in Futnet: past the bursts' 160-175), by DMA in vblank
// from a buffer here. Brake, plant, extend as the lunge starts, contact through the
// lunge and lying there, then decelerate, kneel, ready to get up (the move's timers,
// and what a slide reaches, as they were). All the frames' tiles are copied to WRAM
// bank 2 as the match loads (a frame's 192 bytes copied as it changed took ~15k
// cycles). The bicycle kick's are with them (2026-11-26: the library's generated
// tuck, strike, recover, 24x24, your back and the CPU's front): tucked past 20 of
// its 24 frames, striking past 10, recovering past 4, then ready as before.
#define S_SLIDE 192        // 16 a player
#define OAM_SLIDE_X 31     // 2 a player
uint8_t slide_shown[2];    // the frame whose tiles are loaded (0xFF: none)
static uint8_t slide_first[SLIDE_ART_FRAMES], slide_count[SLIDE_ART_FRAMES];
static uint8_t slide_meta[SLIDE_ART_META];
static uint16_t slide_at[SLIDE_ART_FRAMES * 4];
static uint8_t slide_frame(uint8_t t) {
    return t > 61 ? SLIDE_BRAKE : t > 58 ? SLIDE_PLANT : t > 54 ? SLIDE_EXTEND : t > 20 ? SLIDE_CONTACT :
        t > 12 ? SLIDE_DECELERATE : t > 4 ? SLIDE_KNEEL : SLIDE_READY;
}
static void draw_slide(uint8_t i, uint8_t k, int16_t sx, int16_t sy, uint8_t pal, uint8_t left) {
    uint8_t tile = S_SLIDE + (i << 4), c = slide_count[k];
    if (slide_shown[i] != k) {
        if (gfx_dma((uint8_t *)(0x8000 + ((uint16_t)tile << 4)), SLIDE_ART_RAM + ((uint16_t)slide_first[k] << 5), c << 1))
            slide_shown[i] = k;
    }
    // By metasprite (generated, both ways round): a loop over the six in C cost ~4k
    // cycles a frame, twice the lying pose's. (sx, sy): under the middle of the
    // frame's bottom.
    uint8_t *m = slide_meta + slide_at[(k << 2) + (left << 1)];
    uint8_t x0 = (uint8_t)(sx - 8), y0 = (uint8_t)(sy - 7), o = i ? OAM_SLIDE_X + 2 : OAM_SLIDE_X;
    if (left) pal |= S_FLIPX;
    move_metasprite_ex((const metasprite_t *)m, tile, pal, i ? (ball_oam == 8 ? 4 : 5) : (ball_oam ? 0 : 1), x0, y0);
    if (c > 4) move_metasprite_ex((const metasprite_t *)(slide_meta + slide_at[(k << 2) + (left << 1) + 1]), tile, pal, o, x0, y0);
    else shadow_OAM[o].y = shadow_OAM[o + 1].y = 0;
}

// A point's reactions (2026-11-26: the library's generated win and loss cycles,
// artwork/players/roster/spiky/complete.json, by tools/export-reactions.mjs): six
// 16x24 front-view frames each, both players turned to face you. The winner's cycle
// (anticipate, raise a fist, take off, the top, come down, land) loops, its lift
// doubled; the loser stands, crouches, then shakes their head (turn, centre, turn
// back, settle) to the end. A step every 8 frames of the point's 110. Streamed into
// the slide's tiles (no slide during one), the same way: all twelve frames in RAM.
static const uint8_t react_win[14] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1 };
static const uint8_t react_loss[14] = { 6, 7, 8, 9, 10, 11, 8, 9, 10, 11, 8, 9, 10, 11 };
static const uint8_t react_lift[REACTION_ART_FRAMES] = REACTION_ART_LIFTS;
static void draw_reaction(uint8_t i, uint8_t won, int16_t sx, int16_t sy, uint8_t pal) {
    uint8_t step = (uint8_t)(GOAL_FRAMES - goal_timer) >> 3;
    if (step > 13) step = 13;
    uint8_t r = won ? react_win[step] : react_loss[step], tile = S_SLIDE + (i << 4);
    if (slide_shown[i] != (0x80 | r)) {
        if (gfx_dma((uint8_t *)(0x8000 + ((uint16_t)tile << 4)), REACTION_ART_RAM + ((uint16_t)r << 7), 8))
            slide_shown[i] = 0x80 | r;
    }
    gfx_actor(i ? (ball_oam == 8 ? 4 : 5) : (ball_oam ? 0 : 1), tile, 0, sx, sy - (react_lift[r] << 1), pal, 0);
}

static void draw_trail(int16_t sx, int16_t wy) {
    if (goal_timer) trail_len = 0;
    fc_trail_draw(OAM_TRAIL, sx, wy, cam_y);
}


// The slam zone (playtest: "something bigger that acts like a target to say get
// here now"): three rings round the spot. The lit ring steps in, outer, middle, the
// spot, and round again, a step every 8 frames while the ball's more than 48 px deep
// from the spot, 6 past 20, then 4; the ring it left glows orange, the third dims.
// Its palette's the curve trail's, borrowed while it shows and given back after.
uint8_t zone_on;        // showing (the palette borrowed)
uint8_t zone_lit;       // the lit ring: 0 outer, 1 middle, 2 the spot
static uint8_t zone_tick;  // frames to the next step in
static uint8_t zone_key;   // the palette as set: zone_lit
static uint16_t zone_saved[4];
static void zone_colors(const uint16_t *c) {
    for (uint8_t k = 1; k < 4; k++) gfx_obj_color(ZONE_PAL, k, c[k]);
}
static void zone_off(void) {
    for (uint8_t k = 0; k < ZONE_N; k++) move_sprite(OAM_ZONE + k, 0, 0);
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
        zone_lit = 0;
        zone_tick = 0;
    }
    if (zone_tick) zone_tick--;
    else {
        int16_t d = abs16(PX(ball_y) - meet_y[SIDE_YOU]);
        if (zone_key != 0xFF) zone_lit = zone_lit == 2 ? 0 : zone_lit + 1;
        zone_tick = d > 48 ? 7 : d > 20 ? 5 : 3;
    }
    if (zone_lit != zone_key) {
        uint16_t c[4];
        zone_key = zone_lit;
        c[1] = c[2] = c[3] = ZONE_DIM;
        c[zone_lit ? zone_lit : 3] = ZONE_HOT;  // the one outside it (the spot's, round from the outer)
        c[zone_lit + 1] = ZONE_LIT;
        zone_colors(c);
    }
    place(OAM_ZONE, rx, ry - 4);
    place(OAM_ZONE + 1, rx + 8, ry - 4);
}

// Lying and lifted, for the player being drawn: globals, not locals. As locals with
// their addresses passed to player_move_pose, SDCC zeroed them once before the loop,
// not each pass, so the CPU inherited your slide or knockdown: drawn lying, or split
// in three (standing side-view tiles in the lying layout) when running sideways.
uint8_t draw_lie;
int8_t draw_lift;

void play_draw(void) BANKED {
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
        uint8_t t = pl_move_timer[i];
        if (m == MOVE_SLIDE && !goal_timer) draw_slide(i, slide_frame(t), sx, sy, pal, pl_slide_vx[i] < 0);
        else if (m == MOVE_BICYCLE && t > 4 && !goal_timer)
            draw_slide(i, (i == SIDE_YOU ? SLIDE_BIKE_BACK_TUCK : SLIDE_BIKE_FRONT_TUCK) + (t > 20 ? 0 : t > 10 ? 1 : 2), sx, sy, pal, 0);
        else {
            if (goal_timer) draw_reaction(i, last_scorer == i, sx, sy, pal);
            else draw_player(i, tile, draw_lie, sx, sy, pal, flip);
            // (Constant addresses: shadow_OAM[OAM_SLIDE_X + (i << 1)] took ~1.9k cycles.)
            if (i) shadow_OAM[OAM_SLIDE_X + 2].y = shadow_OAM[OAM_SLIDE_X + 3].y = 0;
            else shadow_OAM[OAM_SLIDE_X].y = shadow_OAM[OAM_SLIDE_X + 1].y = 0;
        }
        if (i == SIDE_YOU) you_sx = sx, you_sy = sy;
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
}


// The sprites' tiles and the streamed art, as the match loads (play_enter).
void play_draw_load(void) BANKED {
    set_sprite_data(S_RING, 2, ring_tiles);
    set_sprite_data(S_ZONE, 2, zone_tiles);
    for (uint8_t k = 0; k < ZONE_N; k++) {  // set once: a frame only moves them
        set_sprite_tile(OAM_ZONE + k, S_ZONE);
        set_sprite_prop(OAM_ZONE + k, k ? ZONE_PAL | S_FLIPX : ZONE_PAL);
        move_sprite(OAM_ZONE + k, 0, 0);
    }
    zone_on = 0;
    slide_shown[0] = slide_shown[1] = 0xFF;
    slide_art_load(slide_first, slide_count, slide_meta, slide_at);
    reaction_art_load();
}
