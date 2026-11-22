#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "gfx_internal.h"
#include "state.h"

uint8_t scroll_x;
uint8_t scroll_y;

// Not static: vram_queue.s and gfx_strips.c read these.
Strip strips[STRIP_MAX];
uint8_t strip_count;

typedef struct {
    uint8_t *dst;
    const uint8_t *src;
    uint8_t len;
    uint8_t bank;  // ROM bank holding src (0: bank 0 or RAM)
} Copy;
Copy copies[COPY_MAX];
uint8_t copy_count;
void vram_queue_write(void);  // vram_queue.s

// The palettes on screen (gfx_set_fade fills them), and whether the vblank
// handler should upload them (pal_write, vram_queue.s).
uint16_t pal_bg[PAL_COUNT * 4], pal_obj[PAL_COUNT * 4];
volatile uint8_t pal_dirty;
void pal_write(void);

// Set by gfx_commit(); the vblank handler only touches the queues when set, so it
// never sees a half-built frame (e.g. when a frame runs long).
volatile uint8_t vram_ready;  // also cleared by gfx_clear (gfx_setup.c)

uint8_t *gfx_strip(uint8_t x, uint8_t y, uint8_t vertical, uint8_t len) {
    if (strip_count == STRIP_MAX || vram_ready) gfx_flush();
    Strip *s = &strips[strip_count++];
    s->x = x & 31;
    s->y = y & 31;
    s->vertical = vertical;
    s->len = len;
    s->attr = 0;
    return s->tiles;
}

uint8_t *gfx_strip_attr(uint8_t x, uint8_t y, uint8_t vertical, uint8_t len) {
    uint8_t *t = gfx_strip(x, y, vertical, len);
    strips[strip_count - 1].attr = 1;
    return t;
}

void gfx_vram_copy(uint8_t *dst, const uint8_t *src, uint8_t len, uint8_t bank) {
    if (copy_count == COPY_MAX || vram_ready) gfx_flush();
    Copy *c = &copies[copy_count++];
    c->dst = dst;
    c->src = src;
    c->len = len;
    c->bank = bank;
}

void gfx_commit(void) {
    if (strip_count || copy_count) vram_ready = 1;
}

// Write everything queued, outside vblank: directly with the display off, or with
// each byte waiting for VRAM access (set_vram_byte) when the display is on (the
// rare fallback when a queue overflows). In vblank, vram_queue.s does this.
static void write_queued(uint8_t wait) {
    uint8_t saved = CURRENT_BANK;
    for (uint8_t i = 0; i < copy_count; i++) {
        if (copies[i].bank) SWITCH_ROM(copies[i].bank);
        uint8_t *d = copies[i].dst;
        const uint8_t *src = copies[i].src;
        uint8_t n = copies[i].len;
        if (wait) {
            while (n--) set_vram_byte(d++, *src++);
        } else {
            while (n--) *d++ = *src++;
        }
    }
    SWITCH_ROM(saved);
    copy_count = 0;
    write_strips(wait);  // gfx_strips.c (banked: the rare slow path)
    strip_count = 0;
    vram_ready = 0;
}

void gfx_flush(void) {
    write_queued(LCDC_REG & LCDCF_ON);
}

// Vblank handler, installed by gfx_init (after GBDK's sprite DMA). Scroll first: it must land inside
// vblank or the screen tears. Then the queued VRAM writes, in assembly so even a
// full queue (a row, a column and a sprite frame) fits in the rest of vblank.
uint8_t obj_restore;

// LCD interrupt at the line above the HUD (gfx_sprite_split): wait for that
// line's horizontal blank, then sprites off for the rest of the screen.
void gfx_lcd(void) {
    if (!(LCDC_REG & LCDCF_OBJON)) return;
    while (STAT_REG & STATF_BUSY) {
    }
    LCDC_REG &= ~LCDCF_OBJON;
    obj_restore = 1;
}

// The dialogue box's split (gfx_box_split), the other way around: its own
// handlers, installed only while a box is up, so other frames pay nothing.
// The new frame starts in the box: window on, sprites off ...
void gfx_vbl_box(void) {
    LCDC_REG = (LCDC_REG | LCDCF_WINON) & ~LCDCF_OBJON;
}

// ... and at its bottom line, the window off and sprites on.
void gfx_lcd_box(void) {
    while (STAT_REG & STATF_BUSY) {
    }
    LCDC_REG = (LCDC_REG & ~LCDCF_WINON) | LCDCF_OBJON;
}

void gfx_sprites(uint8_t on) {
    obj_restore = 0;
    if (on) SHOW_SPRITES;
    else HIDE_SPRITES;
}

void gfx_vbl(void) {
    if (obj_restore) {
        LCDC_REG |= LCDCF_OBJON;  // back on for the new frame (gfx_lcd turned them off)
        obj_restore = 0;
    }
    SCX_REG = scroll_x;
    SCY_REG = scroll_y;
    if (pal_dirty) {
        pal_write();
        pal_dirty = 0;
    }
    if (vram_ready) {
        // The queue writes VRAM bank 0; a load into bank 1 (weapon art) may be
        // what this interrupted, so switch to 0 and back.
        uint8_t vbk = VBK_REG;
        VBK_REG = 0;
        vram_queue_write();
        VBK_REG = vbk;
        copy_count = 0;
        strip_count = 0;
        vram_ready = 0;
    }
}



static uint8_t glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c < ' ' || c > '_') c = ' ';
    return FONT_TILES + (uint8_t)(c - ' ');
}

void bkg_text(uint8_t x, uint8_t y, const char *s) {
    while (*s) set_bkg_tile_xy(x++, y, glyph(*s++));
}

void win_text(uint8_t x, uint8_t y, const char *s) {
    while (*s) set_win_tile_xy(x++, y, glyph(*s++));
}

void banked_memcpy(void *dst, const void *src, uint16_t n, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    memcpy(dst, src, n);
    SWITCH_ROM(saved);
}

void banked_sprite_data(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    set_sprite_data(first, n, data);
    SWITCH_ROM(saved);
}

void banked_sprite_data_vbk1(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    VBK_REG = 1;
    set_sprite_data(first, n, data);
    VBK_REG = 0;
    SWITCH_ROM(saved);
}

void banked_bkg_data(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    set_bkg_data(first, n, data);
    SWITCH_ROM(saved);
}

// Shared by futnet and futsal; keeping this in bank 0 avoids growing the
// already full match bank. OAM slots are allocated behind each mode's players.
void gfx_player_shadow_init(uint8_t first) {
    set_sprite_tile(first, 16); set_sprite_prop(first, SPR_PAL_UI);
    set_sprite_tile(first + 1, 18); set_sprite_prop(first + 1, SPR_PAL_UI);
}
void gfx_player_shadow(uint8_t first, int16_t x, int16_t y) {
    // Both modes constrain feet to the projected court (hardware y 12..156).
    // The existing HUD sprite split clips the lower edge.
    uint8_t ground_y = (uint8_t)(y + 12);
    move_sprite(first, (uint8_t)x, ground_y);
    move_sprite(first + 1, (uint8_t)(x + 8), ground_y);
}

// Write each actor's contiguous shadow-OAM block through one pointer. This
// avoids repeated sprite-index multiplication and per-column function calls.
void gfx_actor(uint8_t first, uint8_t tile, uint8_t wide, int16_t x, int16_t y, uint8_t pal, uint8_t flip) {
    volatile OAM_item_t *o = &shadow_OAM[first];
    pal |= S_BANK;
    if (flip) pal |= S_FLIPX;
    o[0].prop = pal; o[1].prop = pal; o[2].prop = pal;
    if (wide) {
        o[0].tile = tile + (flip ? 4 : 0); o[1].tile = tile + 2;
        o[2].tile = tile + (flip ? 0 : 4);
        uint8_t visible = y >= -1 && y <= 159;
        o[0].y = o[1].y = o[2].y = visible ? (uint8_t)(y + 1) : 0;
        o[0].x = (uint8_t)(x - 4); o[1].x = (uint8_t)(x + 4); o[2].x = (uint8_t)(x + 12);
        o[3].y = 0;
    } else {
        uint8_t l = flip ? tile + 4 : tile, r = flip ? tile : tile + 4;
        o[0].tile = l; o[1].tile = l + 2; o[2].tile = r; o[3].tile = r + 2;
        o[3].prop = pal;
        o[0].x = o[1].x = (uint8_t)x;
        o[2].x = o[3].x = (uint8_t)(x + 8);
        o[0].y = o[2].y = y >= 7 && y <= 167 ? (uint8_t)(y - 7) : 0;
        o[1].y = o[3].y = y >= -9 && y <= 151 ? (uint8_t)(y + 9) : 0;
    }
}
