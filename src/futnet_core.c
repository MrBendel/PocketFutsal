// Shared Futnet movement, charge-up, power/curve/lob/drop strikes, and shot
// trail engine (futnet_core.h). Lives in bank 0 (hot per-frame code shared by
// src/play.c and court mini games with zero bank-switch overhead).
#include <gb/gb.h>
#include "futnet_core.h"
#include "input.h"
#include "sfx.h"
#include "gfx.h"
#include "state.h"

static int16_t abs16(int16_t v) { return v < 0 ? -v : v; }
static int16_t min16(int16_t a, int16_t b) { return a < b ? a : b; }
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) { return v < lo ? lo : v > hi ? hi : v; }

// Curve push table by arc_t / 4: 384 * CURVE_OFS / t^2.
static const uint8_t curve_push[] = {
    17, 17, 17, 17, 17, 17, 17, 17, 15, 12, 10, 8, 7, 6, 5, 4, 4, 3, 3, 3, 3, 2, 2, 2,
    2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

static int16_t fc_arc_t;

void fc_unready(uint8_t i) {
    pl_shot[i] = SHOT_NONE;
    pl_charge[i] = pl_hold[i] = pl_taps[i] = 0;
    pl_combo[i] = 0;
    pl_aim[i] = 1;
    pl_depth[i] = 1;
}

void fc_ball_plain(void) {
    ball_power = ball_gold = ball_curve = ball_curve_t = ball_pop = ball_drop = ball_lob = ball_dropped = 0;
    ball_trail = trail_len = 0;
    ball_cacc = 0;
}

void fc_ready(uint8_t i, uint8_t b, uint8_t allow_gold) {
    uint8_t s = pl_shot[i];
    if (i == SIDE_YOU) sfx_play(SFX_TICK);
    if (!s) {
        pl_shot[i] = b ? SHOT_CURVE : SHOT_POWER;
        pl_charge[i] = pl_hold[i] = pl_taps[i] = pl_combo[i] = 0;
        pl_aim[i] = 1;
        pl_depth[i] = 1;
        if (i == SIDE_YOU) {
            pl_charge[i] = FC_YOU_TAP;
            pl_aim[i] = joy & J_LEFT ? 0 : joy & J_RIGHT ? 2 : 1;
            pl_depth[i] = joy & J_UP ? 2 : joy & J_DOWN ? 0 : 1;
        }
        return;
    }
    if (i == SIDE_YOU && allow_gold && ((s == SHOT_POWER && b) || (s == SHOT_CURVE && !b))) {
        pl_shot[i] = SHOT_GOLD;
        return;
    }
    if ((s == SHOT_CURVE || s == SHOT_LOB) && !b) {
        pl_shot[i] = SHOT_LOB;
        pl_combo[i] = FC_COMBO_FRAMES;
        return;
    }
    if ((s == SHOT_POWER || s == SHOT_DROP) && b) {
        pl_shot[i] = SHOT_DROP;
        pl_combo[i] = FC_COMBO_FRAMES;
        return;
    }
    if (s == SHOT_LOB) pl_shot[i] = SHOT_CURVE;
    else if (s == SHOT_DROP) pl_shot[i] = SHOT_POWER;
    pl_combo[i] = 0;
    if (pl_charge[i] == CHARGE_MAX || (i != SIDE_YOU && pl_taps[i] >= TAP_MAX)) return;
    pl_taps[i]++;
    uint8_t was = pl_charge[i];
    pl_charge[i] += i == SIDE_YOU ? FC_YOU_TAP : TAP_CHARGE;
    if (i == SIDE_YOU && was < CHARGE_MAX / 2 && pl_charge[i] >= CHARGE_MAX / 2) sfx_play(SFX_HALF);
    if (pl_charge[i] >= CHARGE_MAX) {
        pl_charge[i] = CHARGE_MAX;
        if (i == SIDE_YOU) sfx_play(SFX_CHARGE);
    }
}

void fc_combo_tick(uint8_t i) {
    if (pl_combo[i] && i == SIDE_YOU && !--pl_combo[i]) {
        if (pl_shot[i] == SHOT_LOB) pl_shot[i] = SHOT_CURVE;
        else if (pl_shot[i] == SHOT_DROP) pl_shot[i] = SHOT_POWER;
    }
}

static uint8_t mv_i, mv_firm, mv_turn;
static int8_t approach(int8_t v, int8_t t) {
    if (v == t) return v;
    uint8_t turn = (v > 0 && t < 0) || (v < 0 && t > 0);
    uint8_t d = pl_accel[mv_i];
    mv_turn |= turn;
    if (turn || (v > 0 && t < v) || (v < 0 && t > v)) {
        if (mv_firm) d = FC_BRAKE;
        else if (!turn || pl_skid[mv_i] < FC_SKID_FRAMES) d = pl_grip[mv_i];
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

void fc_step_move(uint8_t i, int8_t tx, int8_t ty, uint8_t firm,
                  int16_t x0, int16_t x1, int16_t y0, int16_t y1) {
    mv_i = i;
    mv_firm = firm;
    mv_turn = 0;
    int8_t vx = approach(pl_vx[i], tx);
    int8_t vy = approach(pl_vy[i], ty);
    if (!mv_turn) pl_skid[i] = 0;
    else if (pl_skid[i] < 255) pl_skid[i]++;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    pl_x[i] = clamp16(pl_x[i] + vx, x0, x1);
    pl_y[i] = clamp16(pl_y[i] + vy, y0, y1);
    if (tx) pl_face[i] = tx < 0;
}

uint8_t fc_charge_pal(uint8_t i, uint8_t base_pal) {
    uint8_t pal = base_pal;
    if (pl_shot[i]) {
        uint8_t c = pl_charge[i];
        uint8_t mask = c == CHARGE_MAX ? 1 : c >= CHARGE_MAX / 2 ? 3 : 7;
        if (!((uint8_t)frame_count & mask) && ++pl_flash[i] > 2) pl_flash[i] = 0;
        if (pl_flash[i] == 1) pal = SPR_PAL_FLASH;
        else if (pl_flash[i] == 2 && c >= CHARGE_MAX / 2) pal = SPR_PAL_POWER;
    }
    return pal;
}

void fc_launch_vel(int16_t dx, int16_t dy, uint8_t speed) {
    int16_t wide = abs16(dy) >> 1;
    if (dx > wide) dx = wide;
    else if (dx < -wide) dx = -wide;
    int16_t sum = abs16(dx) + abs16(dy);
    if (!sum) sum = 1;
    ball_vx = (dx * speed) / sum;
    int16_t vy = (int16_t)speed - abs16(ball_vx);
    ball_vy = dy < 0 ? -vy : vy;
}

void fc_arc(int16_t land, uint8_t clear) {
    int16_t vy = abs16(ball_vy);
    if (!vy) vy = 1;
    int16_t t = clamp16((abs16(land - PX(ball_y)) << 4) / vy, 12, 400);
    uint8_t most = ball_drop ? FC_ARC_MAX : FC_ARC_FLAT;
    while (t > most) {
        ball_vx += ball_vx >> 2;
        ball_vy += ball_vy >> 2;
        t -= (t >> 2) - (t >> 4);
    }
    fc_arc_t = t;
    ball_vz = (3 * (t + 1)) >> 3;
    if (ball_z) ball_vz -= ball_z / t;
    arc_clear = clear;
}

void fc_net_clear(void) {
    int16_t vy = abs16(ball_vy);
    if (!vy || (ball_vy < 0 ? PX(ball_y) <= HALFWAY : PX(ball_y) >= HALFWAY)) return;
    int16_t tn = clamp16((abs16(HALFWAY - PX(ball_y)) << 4) / vy, 6, 160);
    int16_t drop = ((tn * (tn + 1)) >> 3) * 3;
    if (ball_z + ball_vz * tn - drop >= TO_SUB(NET_H + 3)) return;
    ball_vz = (TO_SUB(NET_H + 3) + drop - ball_z) / tn;
}

uint8_t fc_strike_ball(uint8_t i, uint8_t move, int16_t tx, int16_t land,
                       uint8_t aim, uint8_t scuff, uint8_t slam, uint8_t is_serve) {
    int16_t bx = PX(ball_x), by = PX(ball_y);
    uint8_t c = pl_charge[i], kind = pl_shot[i], speed;
    if (is_serve && c == CHARGE_MAX) c--;
    uint8_t fire = c == CHARGE_MAX;
    fc_ball_plain();
    if (slam) {
        fire = 1;
        move = MOVE_BICYCLE;
        if (kind != SHOT_CURVE && kind != SHOT_GOLD) kind = SHOT_POWER;
    } else if (kind == SHOT_GOLD) {
        kind = SHOT_POWER;
    }
    if (!slam && (move == MOVE_HEADER || move == MOVE_BICYCLE)) {
        if (fire || (move == MOVE_BICYCLE && c >= CHARGE_MAX / 2)) {
            speed = FC_POWER_SPEED;
            ball_power = 1;
        } else {
            speed = move == MOVE_HEADER ? FC_HEADER_SPEED + (c >> 2) : FC_DRIVE_SPEED + 8 + (c >> 2);
        }
        fc_launch_vel(tx - bx, land - by, speed);
        fc_arc(land, !scuff);
    } else if (kind == SHOT_LOB || kind == SHOT_DROP) {
        int16_t dist = abs16(land - by);
        dist += min16(abs16(tx - bx), dist >> 1);
        if (kind == SHOT_LOB) {
            speed = (uint8_t)clamp16((dist * 7) >> 5, 8, 44);
            ball_vz = move == MOVE_VOLLEY ? FC_CHIP_LIFT - 4 : FC_CHIP_LIFT;
            ball_lob = 1;
        } else {
            speed = (uint8_t)clamp16((dist * 13) >> 5, 6, 32);
            ball_drop = 1;
        }
        fc_launch_vel(tx - bx, land - by, speed);
        if (kind == SHOT_DROP) fc_arc(land, 1);
    } else if (kind == SHOT_CURVE) {
        int8_t side = aim == 0 ? -1 : aim == 2 ? 1 : PX(pl_x[i]) < CENTER_X ? 1 : -1;
        speed = slam ? FC_POWER_SPEED : fire ? FC_POWER_SPEED - 8 : FC_CURVE_SPEED + (c >> 1);
        ball_power = fire;
        fc_launch_vel(tx - side * FC_CURVE_OFS - bx, land - by, speed);
        fc_arc(land, !scuff);
        uint8_t t = (uint8_t)clamp16(fc_arc_t, 32, 180);
        uint8_t push = curve_push[t >> 2];
        ball_curve = side > 0 ? (int8_t)push : -(int8_t)push;
        ball_curve_t = t;
    } else {
        speed = kind == SHOT_GOLD ? FC_GOLD_SPEED : fire ? FC_POWER_SPEED : FC_DRIVE_SPEED + (c >> 1) + (move == MOVE_VOLLEY ? 4 : 0);
        ball_power = fire;
        ball_gold = kind == SHOT_GOLD;
        fc_launch_vel(tx - bx, land - by, speed);
        fc_arc(land, !scuff);
    }
    sfx_play(ball_power ? SFX_POWER : SFX_KICK);
    if (!scuff && (ball_power || kind == SHOT_POWER || kind == SHOT_CURVE)) {
        ball_trail = kind == SHOT_CURVE ? TRAIL_CURVE : kind == SHOT_GOLD ? TRAIL_GOLD : TRAIL_POWER;
        trail_len = ball_power ? FC_TRAIL_MAX : c >= CHARGE_MAX / 2 ? 3 : 2;
        trail_n = 0;
    }
    return move;
}

void fc_curve_step(void) {
    if (!ball_curve_t) return;
    ball_curve_t--;
    ball_cacc += ball_curve;
    ball_vx += ball_cacc >> 4;
    ball_cacc &= 15;
}

static void place_oam(uint8_t n, int16_t x, int16_t y) {
    if (y < 0 || y > 160 || x < 0 || x > 168) {
        move_sprite(n, 0, 0);
        return;
    }
    move_sprite(n, (uint8_t)x, (uint8_t)y);
}

void fc_trail_draw(uint8_t oam_first, int16_t sx, int16_t wy, int16_t cam_y) {
    trail_i = (trail_i + 1) & 15;
    trail_x[trail_i] = sx;
    trail_y[trail_i] = wy;
    if (trail_n < 16) trail_n++;
    if (!ball_trail && trail_len && !((uint8_t)frame_count & 1)) trail_len--;
    if (ball_trail)
        trail_pal = ball_trail == TRAIL_CURVE ? SPR_PAL_TRAIL_CURVE
                  : ball_trail == TRAIL_GOLD  ? SPR_PAL_POWER
                                              : SPR_PAL_TRAIL_POWER;
    uint8_t pal = trail_pal;
    for (uint8_t k = (uint8_t)frame_count & 1; k < FC_TRAIL_MAX; k += 2) {
        uint8_t age = (k + 1) << 2, n = oam_first + k;
        if (k >= trail_len || age >= trail_n) {
            move_sprite(n, 0, 0);
            continue;
        }
        uint8_t e = (trail_i - age) & 15;
        set_sprite_tile(n, FC_S_TRAIL + (k << 1));
        set_sprite_prop(n, pal);
        place_oam(n, trail_x[e], trail_y[e] - cam_y);
    }
}
