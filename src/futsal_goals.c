#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "futsal.h"
#include "futsal_goals.h"
#include "futsal_goal.h"
#include "futsal_goal_bg.h"
#include "gfx.h"
#include "sfx.h"
#if 250 + FUTSAL_GOAL_TILE_COUNT > 256
#error Goal and shadow sprites exceed bank-zero sprite VRAM
#endif
uint8_t fs_frame_timer,fs_frame_goal;
int8_t fs_goal_dx[2],fs_goal_dy[2];
uint16_t fs_post_hits,fs_bar_hits;
static int8_t knock;
static uint8_t dirty,last_pose[2],pending,target_end,target_pose,chunk,buffer;
static uint8_t patch_map[24],patch_attrs[24];
static uint16_t target_offset;
uint16_t fs_goal_uploads;
void futsal_goals_reset(void) BANKED{
    fs_frame_timer=0;fs_goal_dx[0]=fs_goal_dx[1]=fs_goal_dy[0]=fs_goal_dy[1]=0;dirty=1;
}
void futsal_goals_load(void) BANKED{
    static const uint16_t palettes[12]={
        RGB8(40,132,172),RGB8(24,20,32),RGB8(88,160,64),RGB8(248,248,232),
        RGB8(48,144,184),RGB8(24,20,32),RGB8(88,160,64),RGB8(248,248,232),
        RGB8(88,160,64),RGB8(24,20,32),RGB8(40,88,56),RGB8(248,248,232)
    };
    banked_sprite_data(250,FUTSAL_GOAL_TILE_COUNT,futsal_goal_tiles,BANK(futsal_goal));
    memcpy(&pal_base_bg[16],palettes,sizeof(palettes));
    fs_post_hits=fs_bar_hits=fs_goal_uploads=0;
    last_pose[0]=last_pose[1]=255;pending=buffer=0;futsal_goals_reset();
    // Normal poses stay resident. Two shared motion buffers at 48..95 let
    // us stream a displaced frame without touching the currently visible one.
    VBK_REG=1;
    banked_bkg_data(0,24,futsal_goal_bg_tiles,BANK(futsal_goal_bg));
    banked_bkg_data(24,24,futsal_goal_bg_tiles+16*24*16,BANK(futsal_goal_bg));
    VBK_REG=0;
    // All ten goal OBJ slots are free. Nothing in a goal uses the scanline list.
    for(uint8_t i=30;i<40;i++)move_sprite(i,0,0);
}
static void prepare_patch(uint8_t end,uint8_t pose,uint8_t base){
    target_offset=((uint16_t)end*16+pose)*24;
    banked_memcpy(patch_map,futsal_goal_bg_map+target_offset,24,BANK(futsal_goal_bg));
    banked_memcpy(patch_attrs,futsal_goal_bg_attr+target_offset,24,BANK(futsal_goal_bg));
    for(uint8_t i=0;i<24;i++)if(patch_attrs[i]&8)patch_map[i]=base+patch_map[i]-end*24;
}
static void commit_patch(uint8_t end){
    VBK_REG=1;set_bkg_tiles(6,end?16:1,8,3,patch_attrs);
    VBK_REG=0;set_bkg_tiles(6,end?16:1,8,3,patch_map);
    fs_goal_uploads++;
}
void futsal_goals_draw(uint8_t cam) BANKED{
    static uint8_t g,amount,pose,base;
    (void)cam; // BG goals scroll with the court automatically.
    if(!dirty&&!pending)return;
    dirty=0;
    for(g=0;g<2;g++){
        amount=fs_goal_dy[g]<0?-fs_goal_dy[g]:fs_goal_dy[g];
        pose=amount?1+(amount-1)*3+(fs_goal_dx[g]<0?0:fs_goal_dx[g]>0?2:1):0;
        if(!pose){
            if(pending&&target_end==g)pending=0;
            if(last_pose[g]){prepare_patch(g,0,g*24);commit_patch(g);last_pose[g]=0;}
        }else if(last_pose[g]!=pose&&!pending){
            pending=1;target_end=g;target_pose=pose;chunk=0;
            prepare_patch(g,pose,48+(buffer^1)*24);
        }
    }
    if(!pending)return;
    base=48+(buffer^1)*24;
    // Six tiles per frame keeps VRAM writes inside the frame budget. The map
    // switches only after all four chunks are ready, so no partial net appears.
    VBK_REG=1;
    banked_bkg_data(base+chunk,6,futsal_goal_bg_tiles+((target_offset+chunk)<<4),BANK(futsal_goal_bg));
    VBK_REG=0;chunk+=6;
    if(chunk==24){commit_patch(target_end);last_pose[target_end]=target_pose;buffer^=1;pending=0;}
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
        fs_frame_timer=60;knock=post?(x<80*16?-1:1):(fs_ball_vx<0?-1:fs_ball_vx>0?1:0);
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
    uint8_t amount=(60-fs_frame_timer)>>2;
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
