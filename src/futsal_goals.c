#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "futsal.h"
#include "futsal_goals.h"
#include "futsal_goal.h"
#include "futsal_goal_far_bg.h"
#include "futsal_goal_near_bg.h"
#define GOAL_CELLS 32
#if 4 * GOAL_CELLS > 128
#error Goal buffers exceed the safe signed BG range in bank one
#endif
#include "gfx.h"
#include "sfx.h"
#if 250 + FUTSAL_GOAL_TILE_COUNT > 256
#error Goal and shadow sprites exceed bank-zero sprite VRAM
#endif
uint8_t fs_frame_timer,fs_frame_goal;
int8_t fs_goal_dx[2],fs_goal_dy[2];
uint16_t fs_post_hits,fs_bar_hits;
static int8_t knock;
static uint8_t dirty,last_pose[2],pending,target_end,target_pose,chunk,buffer,move_tick,move_amount;
static uint8_t patch_map[GOAL_CELLS],patch_attrs[GOAL_CELLS];
static uint16_t target_offset;
static uint8_t target_bank;
static const uint8_t *target_tiles;
uint16_t fs_goal_uploads;
void futsal_goals_reset(void) BANKED{
    fs_frame_timer=move_tick=move_amount=0;fs_goal_dx[0]=fs_goal_dx[1]=fs_goal_dy[0]=fs_goal_dy[1]=0;dirty=1;
}
void futsal_goals_load(void) BANKED{
    static const uint16_t palettes[12]={
        RGB8(40,132,172),RGB8(24,20,32),RGB8(136,152,168),RGB8(248,248,232),
        RGB8(48,144,184),RGB8(24,20,32),RGB8(136,152,168),RGB8(248,248,232),
        RGB8(88,160,64),RGB8(24,20,32),RGB8(136,152,168),RGB8(248,248,232)
    };
    banked_sprite_data(254,2,futsal_goal_tiles+64,BANK(futsal_goal));
    memcpy(&pal_base_bg[16],palettes,sizeof(palettes));
    fs_post_hits=fs_bar_hits=fs_goal_uploads=0;
    last_pose[0]=last_pose[1]=255;pending=buffer=0;futsal_goals_reset();
    // Two normal goals and two motion buffers fill signed BG IDs 0..127,
    // at 0x9000..0x97ff, separate from the player OBJ character data.
    VBK_REG=1;
    banked_bkg_data(0,GOAL_CELLS,futsal_goal_far_bg_tiles,BANK(futsal_goal_far_bg));
    banked_bkg_data(GOAL_CELLS,GOAL_CELLS,futsal_goal_near_bg_tiles,BANK(futsal_goal_near_bg));
    VBK_REG=0;
    // All ten goal OBJ slots are free. Nothing in a goal uses the scanline list.
    for(uint8_t i=30;i<40;i++)move_sprite(i,0,0);
}
static void prepare_patch(uint8_t end,uint8_t pose,uint8_t base){
    target_offset=(uint16_t)pose*GOAL_CELLS;
    target_bank=end?BANK(futsal_goal_near_bg):BANK(futsal_goal_far_bg);
    target_tiles=end?futsal_goal_near_bg_tiles:futsal_goal_far_bg_tiles;
    banked_memcpy(patch_map,(end?futsal_goal_near_bg_map:futsal_goal_far_bg_map)+target_offset,GOAL_CELLS,target_bank);
    banked_memcpy(patch_attrs,(end?futsal_goal_near_bg_attr:futsal_goal_far_bg_attr)+target_offset,GOAL_CELLS,target_bank);
    for(uint8_t i=0;i<GOAL_CELLS;i++)if(patch_attrs[i]&8)patch_map[i]=base+patch_map[i]-end*GOAL_CELLS;
}
static void commit_patch(uint8_t end){
    VBK_REG=1;set_bkg_tiles(6,end?15:0,8,4,patch_attrs);
    VBK_REG=0;set_bkg_tiles(6,end?15:0,8,4,patch_map);
    fs_goal_uploads++;
}
void futsal_goals_draw(uint8_t cam) BANKED{
    static uint8_t g,amount,pose,base,count;
    (void)cam;
    if(!dirty&&!pending)return;
    dirty=0;
    for(g=0;g<2;g++){
        amount=fs_goal_dy[g]<0?-fs_goal_dy[g]:fs_goal_dy[g];
        pose=amount?1+(amount-1)*3+(fs_goal_dx[g]<0?0:fs_goal_dx[g]>0?2:1):0;
        if(!pose){
            if(pending&&target_end==g)pending=0;
            if(last_pose[g]){prepare_patch(g,0,g*GOAL_CELLS);commit_patch(g);last_pose[g]=0;}
        }else if(last_pose[g]!=pose&&!pending){
            pending=1;target_end=g;target_pose=pose;chunk=0;
            prepare_patch(g,pose,2*GOAL_CELLS+(buffer^1)*GOAL_CELLS);
        }
    }
    if(!pending)return;
    base=2*GOAL_CELLS+(buffer^1)*GOAL_CELLS;
    count=GOAL_CELLS-chunk;if(count>6)count=6;
    VBK_REG=1;
    banked_bkg_data(base+chunk,count,target_tiles+((target_offset+chunk)<<4),target_bank);
    VBK_REG=0;chunk+=count;
    if(chunk==GOAL_CELLS){commit_patch(target_end);last_pose[target_end]=target_pose;buffer^=1;pending=0;}
}
// Called only at an end-line crossing. Sweep back to the actual plane so a
// fast diagonal cannot tunnel through a post or below the crossbar.
// 0 = wall/miss, 1 = clean goal, 2 = frame knocked loose.
uint8_t futsal_goals_contact(int16_t old_x,int16_t old_y,int16_t old_z) BANKED{
    static int16_t plane,t,x,z;
    static uint8_t post,bar;
    fs_frame_goal=fs_ball_y<0?0:1;plane=fs_frame_goal?224*16:0;
    t=fs_ball_y==old_y?16:((plane-old_y)*16)/(fs_ball_y-old_y);
    if(t<0)t=0;else if(t>16)t=16;
    x=old_x+((fs_ball_x-old_x)*t)/16;
    z=old_z+((fs_ball_z-old_z)*t)/16;
    post=z<=17*16&&((x>=(FS_GOAL_LEFT-3)*16&&x<=(FS_GOAL_LEFT+3)*16)||(x>=(FS_GOAL_RIGHT-3)*16&&x<=(FS_GOAL_RIGHT+3)*16));
    bar=x>=FS_GOAL_LEFT*16&&x<=FS_GOAL_RIGHT*16&&z>=11*16&&z<=17*16;
    if(post||bar){
        if(post)fs_post_hits++;else fs_bar_hits++;
        fs_frame_timer=60;move_tick=move_amount=0;knock=post?(x<80*16?-1:1):(fs_ball_vx<0?-1:fs_ball_vx>0?1:0);
        fs_ball_x=x;fs_ball_y=plane+(fs_frame_goal?-4*16:4*16);fs_ball_z=z;
        fs_ball_vy=-(fs_ball_vy-fs_ball_vy/4);
        if(post)fs_ball_vx=x<80*16?-32:32;else fs_ball_vx-=fs_ball_vx/4;
        if(fs_ball_vz>0)fs_ball_vz=-fs_ball_vz;
        fs_owner=NOBODY;fs_receiver=NOBODY;fs_lob=0;
        sfx_play(SFX_BOARD);dirty=1;return 2;
    }
    return x>(FS_GOAL_LEFT+3)*16&&x<(FS_GOAL_RIGHT-3)*16&&z<11*16;
}
void futsal_goals_step(void) BANKED{
    uint8_t amount;
    if(++move_tick==6){move_tick=0;if(move_amount<5)move_amount++;}
    amount=move_amount;
    if(amount>5)amount=5;
    fs_goal_dx[fs_frame_goal]=knock<0?-(int8_t)amount:knock>0?amount:0;
    fs_goal_dy[fs_frame_goal]=fs_frame_goal?amount:-(int8_t)amount;dirty=1;
    // Let the rebound read for a few frames while players stop for the reset.
    if(fs_frame_timer>48){
        fs_ball_x+=fs_ball_vx/2;fs_ball_y+=fs_ball_vy/2;
        if(fs_ball_x<10*16)fs_ball_x=10*16;else if(fs_ball_x>150*16)fs_ball_x=150*16;
        if(fs_ball_y<0)fs_ball_y=0;else if(fs_ball_y>224*16)fs_ball_y=224*16;
    }
    if(fs_ball_z>0){fs_ball_z-=8;if(fs_ball_z<0)fs_ball_z=0;}
    fs_frame_timer--;
}
