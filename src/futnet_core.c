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

// 512 / t for t / 4 (the middle of each step; 12 at least).
static const uint8_t inv_t[] = {43, 43, 43, 38, 29, 24, 20, 17, 15, 14, 12, 11, 10, 10, 9, 8, 8, 7, 7, 7, 6, 6, 6, 5, 5, 5};

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
    // Mario Tennis's way round (playtest 2026-11-27): A, B a lob; B, A a drop shot.
    if ((s == SHOT_POWER || s == SHOT_LOB) && b) {
        pl_shot[i] = SHOT_LOB;
        pl_combo[i] = FC_COMBO_FRAMES;
        return;
    }
    if ((s == SHOT_CURVE || s == SHOT_DROP) && !b) {
        pl_shot[i] = SHOT_DROP;
        pl_combo[i] = FC_COMBO_FRAMES;
        return;
    }
    if (s == SHOT_LOB) pl_shot[i] = SHOT_POWER;
    else if (s == SHOT_DROP) pl_shot[i] = SHOT_CURVE;
    pl_combo[i] = 0;
    if (pl_charge[i] == CHARGE_MAX || (i != SIDE_YOU && pl_taps[i] >= TAP_MAX)) return;
    pl_taps[i]++;
    uint8_t was = pl_charge[i];
    pl_charge[i] += i == SIDE_YOU ? FC_YOU_TAP : TAP_CHARGE;
    if (i == SIDE_YOU && was < FC_SLAM_CHARGE && pl_charge[i] >= FC_SLAM_CHARGE) sfx_play(SFX_HALF);
    if (pl_charge[i] >= CHARGE_MAX) {
        pl_charge[i] = CHARGE_MAX;
        if (i == SIDE_YOU) sfx_play(SFX_CHARGE);
    }
}

int16_t fc_min_x, fc_max_x, fc_min_y, fc_max_y;

#define STEP_AXIS(v, t) do { \
    int8_t _v = (v), _t = (t); \
    if (_v != _t) { \
        uint8_t _turn = (_v > 0 && _t < 0) || (_v < 0 && _t > 0); \
        uint8_t _d = accel; \
        turn |= _turn; \
        if (_turn || (_v > 0 && _t < _v) || (_v < 0 && _t > _v)) { \
            if (firm == 1 || (firm && !_turn)) _d = FC_BRAKE; /* 2: firm, but a turn skids */ \
            else if (!_turn || skid < FC_SKID_FRAMES) _d = grip; \
        } \
        if (_v < _t) { _v += _d; if (_v > _t) _v = _t; } \
        else { _v -= _d; if (_v < _t) _v = _t; } \
        (v) = _v; \
    } \
} while (0)

void fc_step_move(uint8_t i, int8_t tx, int8_t ty, uint8_t firm) {
    uint8_t accel = pl_accel[i], grip = pl_grip[i], skid = pl_skid[i], turn = 0;
    int8_t vx = pl_vx[i], vy = pl_vy[i];
    STEP_AXIS(vx, tx);
    STEP_AXIS(vy, ty);
    pl_skid[i] = !turn ? 0 : skid < 255 ? skid + 1 : 255;
    pl_vx[i] = vx;
    pl_vy[i] = vy;
    if (vx) {
        int16_t x = pl_x[i] + vx;
        if (x < fc_min_x) x = fc_min_x;
        else if (x > fc_max_x) x = fc_max_x;
        pl_x[i] = x;
    }
    if (vy) {
        int16_t y = pl_y[i] + vy;
        if (y < fc_min_y) y = fc_min_y;
        else if (y > fc_max_y) y = fc_max_y;
        pl_y[i] = y;
    }
    if (tx) pl_face[i] = tx < 0;
}

uint8_t fc_charge_pal(uint8_t i, uint8_t base_pal) {
    if (!pl_shot[i]) return base_pal;
    uint8_t c = pl_charge[i];
    if (c < CHARGE_MAX / 2) return base_pal;  // a press or two: your own colors (the flash is for power)
    uint8_t mask = c == CHARGE_MAX ? 1 : c >= CHARGE_MAX / 2 ? 3 : 7;
    if (!((uint8_t)frame_count & mask)) {
        uint8_t f = pl_flash[i] + 1;
        pl_flash[i] = f > 2 ? 0 : f;
    }
    uint8_t f = pl_flash[i];
    if (f == 1) return SPR_PAL_FLASH;
    if (f == 2 && c >= CHARGE_MAX / 2) return SPR_PAL_POWER;
    return base_pal;
}

uint8_t fc_wide = 4;

void fc_launch_vel(int16_t dx, int16_t dy, uint8_t speed) {
    uint16_t ady = dy < 0 ? (uint16_t)(-dy) : (uint16_t)dy;
    int16_t wide = (int16_t)((ady * fc_wide) >> 3);
    if (dx > wide) dx = wide;
    else if (dx < -wide) dx = -wide;
    uint16_t adx = dx < 0 ? (uint16_t)(-dx) : (uint16_t)dx;
    uint16_t sum = adx + ady;
    if (!sum) sum = 1;
    uint16_t avx = (adx * (uint16_t)speed) / sum;
    ball_vx = dx < 0 ? -(int16_t)avx : (int16_t)avx;
    int16_t vy = (int16_t)speed - (int16_t)avx;
    ball_vy = dy < 0 ? -vy : vy;
}

void fc_arc(int16_t land, uint8_t clear) {
    int16_t vy = ball_vy < 0 ? -ball_vy : ball_vy;
    if (!vy) vy = 1;
    int16_t dy = land - PX(ball_y);
    uint16_t ady = dy < 0 ? (uint16_t)(-dy) : (uint16_t)dy;
    uint16_t ut = (ady << 4) / (uint16_t)vy;
    int16_t t = ut < 12 ? 12 : ut > 400 ? 400 : (int16_t)ut;
    uint8_t most = ball_drop ? FC_ARC_MAX : FC_ARC_FLAT;
    if (t > most) {
        int16_t vx = ball_vx < 0 ? -ball_vx : ball_vx;
        do {
            vx += vx >> 2;
            vy += vy >> 2;
            t -= (t >> 2) - (t >> 4) + (t >> 7);
        } while (t > most);
        ball_vx = ball_vx < 0 ? -vx : vx;
        ball_vy = ball_vy < 0 ? -vy : vy;
    }
    arc_t = t;
    ball_vz = (3 * (t + 1)) >> 3;
    if (ball_z > 0) ball_vz -= (int16_t)(((uint16_t)ball_z * inv_t[t > 103 ? 25 : t >> 2]) >> 9);
    arc_clear = clear;
}

void fc_net_clear(void) {
    int16_t vy = ball_vy < 0 ? -ball_vy : ball_vy;
    if (!vy || (ball_vy < 0 ? PX(ball_y) <= HALFWAY : PX(ball_y) >= HALFWAY)) return;
    int16_t dy = HALFWAY - PX(ball_y);
    uint16_t ady = dy < 0 ? (uint16_t)(-dy) : (uint16_t)dy;
    uint16_t utn = (ady << 4) / (uint16_t)vy;
    uint8_t tn8 = utn < 6 ? 6 : utn > 160 ? 160 : (uint8_t)utn;
    int16_t d = (int16_t)(((uint16_t)tn8 * (uint8_t)(tn8 + 1)) >> 3);
    int16_t drop = d + (d << 1);
    if (ball_z + ball_vz * (int16_t)tn8 - drop >= TO_SUB(NET_H + 3)) return;
    ball_vz = (TO_SUB(NET_H + 3) + drop - ball_z) / tn8;
}

uint8_t fc_gold_speed = FC_GOLD_SPEED;  // a gold strike's speed (play.c's power slam sets it for a frame)

uint8_t fc_strike_ball(uint8_t i, uint8_t move, int16_t tx, int16_t land, uint8_t flags) {
    uint8_t aim = flags & 3;
    uint8_t scuff = (flags & FC_F_SCUFF) != 0;
    uint8_t slam = (flags & FC_F_SLAM) != 0;
    uint8_t is_serve = (flags & FC_F_SERVE) != 0;
    int16_t bx = PX(ball_x), by = PX(ball_y);
    uint8_t c = pl_charge[i], kind = pl_shot[i], speed;
    if (is_serve && c == CHARGE_MAX) c--;
    uint8_t fire = c == CHARGE_MAX;
    // Your slam is your weapon: faster than anything; the CPU's goes at fire's pace.
    uint8_t slam_speed = i == SIDE_YOU ? FC_SLAM_SPEED : FC_POWER_SPEED;
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
            speed = (uint8_t)clamp16((dist * 45) >> 8, 8, 40);  // 91 frames in the air
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
        speed = slam ? slam_speed : fire ? FC_POWER_SPEED - 8 : FC_CURVE_SPEED + (c >> 1);
        ball_power = fire;
        fc_launch_vel(tx - side * FC_CURVE_OFS - bx, land - by, speed);
        fc_arc(land, !scuff);
        uint8_t t = (uint8_t)clamp16(arc_t, 32, 180);
        uint8_t push = curve_push[t >> 2];
        ball_curve = side > 0 ? (int8_t)push : -(int8_t)push;
        ball_curve_t = t;
    } else {
        speed = kind == SHOT_GOLD ? fc_gold_speed : slam ? slam_speed : fire ? FC_POWER_SPEED : FC_DRIVE_SPEED + (c >> 1) + (move == MOVE_VOLLEY ? 4 : 0);
        ball_power = fire;
        ball_gold = kind == SHOT_GOLD;
        fc_launch_vel(tx - bx, land - by, speed);
        fc_arc(land, !scuff);
    }
    sfx_play(ball_power ? SFX_POWER : SFX_KICK);
    // Trails mark the shots that earned them: on fire, a slam, gold, a curve charged past half.
    if (!scuff && (ball_power || ball_gold || (kind == SHOT_CURVE && c >= CHARGE_MAX / 2))) {
        ball_trail = kind == SHOT_CURVE ? TRAIL_CURVE : kind == SHOT_GOLD ? TRAIL_GOLD : TRAIL_POWER;
        trail_len = ball_power || ball_gold ? FC_TRAIL_MAX : 3;
        trail_n = 0;
    }
    return move;
}

static uint8_t trail_clear;

void fc_trail_draw(uint8_t oam_first, int16_t sx, int16_t wy, int16_t cam_y) {
    trail_i = (trail_i + 1) & 15;
    trail_x[trail_i] = sx;
    trail_y[trail_i] = wy;
    if (trail_n < 16) trail_n++;
    if (!ball_trail && trail_len && !((uint8_t)frame_count & 1)) trail_len--;
    if (!trail_len || trail_n <= 4) {
        if (trail_clear) return;
        for (uint8_t k = 0; k < FC_TRAIL_MAX; k++) shadow_OAM[oam_first + k].y = 0;
        trail_clear = 1;
        return;
    }
    trail_clear = 0;
    if (ball_trail)
        trail_pal = ball_trail == TRAIL_CURVE ? SPR_PAL_TRAIL_CURVE
                  : ball_trail == TRAIL_GOLD  ? SPR_PAL_POWER
                                              : SPR_PAL_TRAIL_POWER;
    uint8_t pal = trail_pal;
    for (uint8_t k = (uint8_t)frame_count & 1; k < FC_TRAIL_MAX; k += 2) {
        uint8_t age = (k + 1) << 2, n = oam_first + k;
        if (k >= trail_len || age >= trail_n) {
            shadow_OAM[n].y = 0;
            continue;
        }
        uint8_t e = (trail_i - age) & 15;
        int16_t px = trail_x[e], py = trail_y[e] - cam_y;
        if (py < 0 || py > 160 || px < 0 || px > 168) {
            shadow_OAM[n].y = 0;
        } else {
            OAM_item_t *o = &shadow_OAM[n];
            o->y = (uint8_t)py;
            o->x = (uint8_t)px;
            o->tile = FC_S_TRAIL + (k << 1);
            o->prop = pal;
        }
    }
}
