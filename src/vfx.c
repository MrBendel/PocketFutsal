#pragma bank 255
// Burst effects: an impact star for a slam or a strike on fire, dust for a slide or a
// knockdown, sparks off the net. Frames of 8x16 columns (src/vfx_tiles.h, made by
// tools/export-vfx.mjs), one effect at a time in sprites OAM_VFX on; a new one
// replaces it. Banked: it costs play.c's bank nothing but the calls.
#include <gb/gb.h>
#include "gfx.h"
#define VFX_TABLES
#include "vfx.h"

uint8_t vfx_kind = 0xFF;  // the effect showing (VFX_*), or 0xFF
uint8_t vfx_frame, vfx_hold;
int16_t vfx_hx, vfx_wy;   // its top-left corner: hardware x, hardware y plus the camera
static const uint8_t *vfx_cell;  // this frame's cells
static int16_t vfx_cam;          // the camera it was last placed for
static uint8_t vfx_rows;         // rows still to place (bit r)
static uint8_t vfx_row;          // the row placed last
static uint8_t vfx_wait;         // started this frame: drawn from the next

static void hide(void) {
    for (uint8_t k = 0; k < VFX_SPRITES_MAX; k++) move_sprite(OAM_VFX + k, 0, 0);
}

void vfx_load(void) BANKED {
    banked_sprite_data(VFX_FIRST_TILE, VFX_TILE_COUNT, vfx_tiles, BANK(vfx_tiles));
    vfx_kind = 0xFF;
    hide();
}

void vfx_start(uint8_t kind, int16_t hx, int16_t wy) BANKED {
    const uint8_t *e = vfx_fx[kind];
    if (vfx_kind != 0xFF) hide();
    vfx_kind = kind;
    vfx_frame = 0;
    vfx_hold = e[4];
    vfx_cell = vfx_cells + e[0];
    vfx_rows = 3;
    vfx_wait = 1;  // not on a strike's frame, the busiest
    vfx_hx = hx - (e[2] << 2);  // half its width: columns * 4
    vfx_wy = wy - (e[3] << 3);  // half its height: rows * 8
}

// One row of sprites placed a frame at most, when its frame or the camera has changed
// (this compiler makes a sprite loop dear: a slam's two rows take two frames).
void vfx_draw(int16_t cam_y) BANKED {
    if (vfx_kind == 0xFF) return;
    if (vfx_wait) {
        vfx_wait = 0;
        return;
    }
    const uint8_t *e = vfx_fx[vfx_kind];
    uint8_t cols = e[2], rows = e[3];
    if (cam_y != vfx_cam) {
        vfx_cam = cam_y;
        vfx_rows = 3;
    }
    if (rows == 1) vfx_rows &= 1;
    if (vfx_rows) {
        uint8_t r = vfx_row ^ (rows - 1);  // the other row (of two), if it's waiting
        if (!(vfx_rows & (1 << r))) r ^= 1;
        vfx_row = r;
        vfx_rows &= ~(1 << r);
        OAM_item_t *o = &shadow_OAM[OAM_VFX];
        const uint8_t *cell = vfx_cell;
        int16_t y = vfx_wy - cam_y;
        if (r) {
            o += cols;
            cell += cols;
            y += 16;
        }
        uint8_t pal = e[5], yok = (uint16_t)y <= 160, yy = (uint8_t)y;
        int16_t x = vfx_hx;
        for (uint8_t c = cols; c; c--, x += 8, o++) {
            uint8_t t = *cell++;
            // Empty, or off the playfield (above it, or under the HUD): hidden.
            if (t == 0xFF || !yok || (uint16_t)x > 168) {
                o->y = 0;
                continue;
            }
            o->y = yy;
            o->x = (uint8_t)x;
            o->tile = t;
            o->prop = pal;
        }
    }
    if (--vfx_hold) return;
    vfx_hold = e[4];
    vfx_cell += rows == 2 ? cols << 1 : cols;  // (1 or 2 rows: no multiply, it's dear)
    vfx_rows = 3;
    if (++vfx_frame >= e[1]) {
        vfx_kind = 0xFF;
        hide();
    }
}
