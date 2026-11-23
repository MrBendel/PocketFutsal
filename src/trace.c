#pragma bank 255
// Where each side means to meet the ball: its flight traced on (predict), out of
// play.c's bank. Run by play.c's want_predict, a side a frame.
#include <gb/gb.h>
#include "game.h"
#include "trace.h"

extern uint8_t ball_flight, meet_flight[2], meet_slam[2];
extern int16_t ball_vx, ball_vy, ball_vz;

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

// Where side i should meet the ball: its flight traced on (gravity, a bounce, a
// lob sitting up, not the curve) to where it first lands, then 8 frames more,
// rising off the bounce; or where it gets to i's baseline first. Landing out (and
// not yet bounced in), i lets it go: meet_out[i]. 12 frames a step (gravity over 12
// frames: vz down 9, z up 12 vz - 58), up to 16 steps; run by want_predict.
void predict(uint8_t i) BANKED {
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
                    if (!IN_COURT(PX(x), PX(y))) {
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
