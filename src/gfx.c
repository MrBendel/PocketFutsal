#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include <gb/metasprites.h>
#include "ball_extra.h"
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
// Sprite palettes changed one colour at a time (gfx_obj_color): a bit each, only
// those uploaded (8 bytes, not all 128).
volatile uint8_t pal_obj_mask;
void pal_write_obj(uint8_t mask);
void gfx_obj_color(uint8_t p, uint8_t k, uint16_t c) {
    uint8_t i = (p << 2) + k;
    pal_base_obj[i] = pal_obj[i] = c;
    pal_obj_mask |= 1 << p;
}

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
        pal_obj_mask = 0;
    } else if (pal_obj_mask) {
        pal_write_obj(pal_obj_mask);
        pal_obj_mask = 0;
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

// GBDK's assembly metasprite renderer writes the actor block without C's
// repeated pointer arithmetic. Tile order stays identical to the original ABI.
static const metasprite_t actor_right[]={{0,0,0,0},{16,0,2,0},{-16,8,4,0},{16,0,6,0},METASPR_TERM};
static const metasprite_t actor_left[]={{0,0,4,0},{16,0,6,0},{-16,8,0,0},{16,0,2,0},METASPR_TERM};
static const metasprite_t actor_slide_right[]={{0,0,0,0},{0,8,2,0},{0,8,4,0},METASPR_TERM};
static const metasprite_t actor_slide_left[]={{0,0,4,0},{0,8,2,0},{0,8,0,0},METASPR_TERM};
void gfx_actor(uint8_t first,uint8_t tile,uint8_t wide,int16_t x,int16_t y,uint8_t pal,uint8_t flip){
    const metasprite_t *pose;
    if(flip)pal|=S_FLIPX;
    if(wide){pose=flip?actor_slide_left:actor_slide_right;x-=4;y++;shadow_OAM[first+3].y=0;}
    else{pose=flip?actor_left:actor_right;y-=7;}
    move_metasprite_ex(pose,tile,pal,first,(uint8_t)x,(uint8_t)y);
}

// Shared lower bodies keep all twelve side poses in 28 tiles, outside the
// signed background tile range occupied by Club Court (bank 0: 128..213).
#define SIDE_R(h,b) {{0,0,h,0},{16,0,b,0},{-16,8,h+2,0},{16,0,b+2,0},METASPR_TERM}
#define SIDE_L(h,b) {{0,0,h+2,0},{16,0,b+2,0},{-16,8,h,0},{16,0,b,0},METASPR_TERM}
#define SIDE_DEFINE(id,h) \
static const metasprite_t sr##id##a[]=SIDE_R(h,16),sr##id##b[]=SIDE_R(h,20),sr##id##c[]=SIDE_R(h,24); \
static const metasprite_t sl##id##a[]=SIDE_L(h,16),sl##id##b[]=SIDE_L(h,20),sl##id##c[]=SIDE_L(h,24);
SIDE_DEFINE(0,0)
SIDE_DEFINE(1,4)
SIDE_DEFINE(2,8)
SIDE_DEFINE(3,12)
#define SIDE_PTR_ROW(M,id) M##id##a,M##id##b,M##id##c
static const metasprite_t *const side_right_ptr[]={SIDE_PTR_ROW(sr,0),SIDE_PTR_ROW(sr,1),SIDE_PTR_ROW(sr,2),SIDE_PTR_ROW(sr,3)};
static const metasprite_t *const side_left_ptr[]={SIDE_PTR_ROW(sl,0),SIDE_PTR_ROW(sl,1),SIDE_PTR_ROW(sl,2),SIDE_PTR_ROW(sl,3)};
void gfx_side(uint8_t first,uint8_t character,uint8_t pose,uint8_t x,uint8_t y,uint8_t flip){
    uint8_t index=character*3+pose,pal=character;
    if(flip)pal|=S_FLIPX;
    move_metasprite_ex(flip?side_left_ptr[index]:side_right_ptr[index],214,pal,first,x,(uint8_t)(y-7));
}
// Keep the locomotion view calculation outside the full Futnet ROM bank.
uint8_t gfx_running_side(uint8_t previous,int8_t vx,int8_t vy){
    uint8_t mx=vx<0?-vx:vx,my=vy<0?-vy:vy;
    int8_t delta=mx-my;
    if(delta>3)return 1;
    if(delta<-3)return 0;
    return previous;
}

uint8_t gfx_ball_phase,gfx_ball_first;
uint16_t gfx_ball_distance;
void gfx_ball_init(uint8_t extra_first){
    gfx_ball_first=extra_first;gfx_ball_phase=0;gfx_ball_distance=0;
    banked_sprite_data(extra_first,BALL_EXTRA_TILE_COUNT,ball_extra_tiles,BANK(ball_extra));
}
void gfx_ball_step(int16_t vx,int16_t vy){
    static uint16_t x,y,t;
    x=vx<0?-vx:vx;y=vy<0?-vy:vy;
    if(x<y){t=x;x=y;y=t;}
    // Approximate court distance, not elapsed time: four pixels per phase,
    // about one circumference per six-frame revolution. Rest holds its pose.
    gfx_ball_distance+=x+(y>>1);
    if(gfx_ball_distance>=64){
        // Keep fast shots readable instead of skipping poses and strobing.
        if(gfx_ball_distance>=128)gfx_ball_distance=127;
        // The frames roll the ball's face down the screen (coming toward you):
        // played backwards going up it, away from you (playtest: "it always rotates
        // the same direction").
        gfx_ball_distance-=64;
        if(vy<0){if(!gfx_ball_phase)gfx_ball_phase=6;gfx_ball_phase--;}
        else if(++gfx_ball_phase==6)gfx_ball_phase=0;
    }
}
uint8_t gfx_ball_tile(void){
    return gfx_ball_phase<2?gfx_ball_phase<<1:gfx_ball_first+((gfx_ball_phase-2)<<1);
}
// Reuse each character's own kick tiles, rotated through hardware flips.
// The blank half of the lower pair moves above the sprite, preserving the
// approved 16x24 silhouette without uploading tiles during an aerial move.
static const metasprite_t bicycle_right[]={{0,0,6,0},{16,0,4,0},{-16,8,2,0},{16,0,0,0},METASPR_TERM};
static const metasprite_t bicycle_left[]={{0,0,2,0},{16,0,0,0},{-16,8,6,0},{16,0,4,0},METASPR_TERM};
void gfx_bicycle(uint8_t first,uint8_t tile,uint8_t x,int16_t y,uint8_t pal,uint8_t flip){
    pal|=S_FLIPY;if(!flip)pal|=S_FLIPX;
    move_metasprite_ex(flip?bicycle_left:bicycle_right,tile,pal,first,x,(uint8_t)(y-15));
}
