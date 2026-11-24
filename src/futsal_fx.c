#pragma bank 255
// The pack's original 16x16 hit animation, compacted with hardware flips.
// Sprite slots 28/29 and tiles 242..249 are unused by players, ball and zone.
#include <gb/gb.h>
#include "gfx.h"
#include "futsal.h"
#include "futsal_fx.h"
#define FUTSAL_FX_TABLES
#include "futsal_fx_tiles.h"
#if FS_FX_FIRST_TILE + FS_FX_TILE_COUNT > 256
#error Futsal contact effects exceed sprite VRAM
#endif

uint8_t fs_fx_timer,fs_fx_frame;
int16_t fs_fx_x,fs_fx_y;
static uint8_t hold,left,wait;
static int16_t hit_x,hit_y,hit_z;
static void hide(void){move_sprite(28,0,0);move_sprite(29,0,0);}
void futsal_fx_load(void) BANKED{
    banked_sprite_data(FS_FX_FIRST_TILE,FS_FX_TILE_COUNT,futsal_fx_tiles,BANK(futsal_fx_tiles));
    fs_fx_timer=0;fs_fx_frame=255;hide();
}
void futsal_fx_start(int16_t x,int16_t y,int16_t z,uint8_t aerial) BANKED{
    hit_x=x;hit_y=y;hit_z=z;
    if(fs_fx_timer)hide();
    hold=aerial?3:2;left=hold;fs_fx_frame=0;fs_fx_timer=hold*4;wait=2;
}
void futsal_fx_draw(uint8_t cam) BANKED{
    if(!fs_fx_timer){if(fs_fx_frame!=255){hide();fs_fx_frame=255;}return;}
    if(wait==2){wait=1;return;} // Keep setup off the busy strike frame.
    if(wait){
        uint8_t px=(uint16_t)hit_x>>4,py=(uint16_t)hit_y>>4;
        fs_fx_x=80+(((px-80)*(int16_t)fs_projection_s[py])>>7);
        fs_fx_y=(int16_t)fs_projection_y[py]+8-(hit_z>>4);wait=0;
    }
    int16_t y=fs_fx_y-cam;
    if(y<0||y>160||fs_fx_x<0||fs_fx_x>160)hide();
    else{
        const uint8_t *cell=fs_fx_cells[fs_fx_frame];
        OAM_item_t *o=&shadow_OAM[28];
        o->x=fs_fx_x;o->y=y;o->tile=cell[0];o->prop=SPR_PAL_POWER|cell[1];
        o++;o->x=fs_fx_x+8;o->y=y;o->tile=cell[2];o->prop=SPR_PAL_POWER|cell[3];
    }
    fs_fx_timer--;if(!--left){left=hold;fs_fx_frame++;}
}
