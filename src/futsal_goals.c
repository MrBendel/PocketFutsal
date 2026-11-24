#pragma bank 255
#include <gb/gb.h>
#include "futsal.h"
#include "futsal_goals.h"
#include "futsal_goal.h"
#include "gfx.h"
#include "sfx.h"
#if 250 + FUTSAL_GOAL_TILE_COUNT > 256
#error Goal and shadow sprites exceed bank-zero sprite VRAM
#endif
uint8_t fs_frame_timer,fs_frame_goal;
int8_t fs_goal_dx[2],fs_goal_dy[2];
uint16_t fs_post_hits,fs_bar_hits;
static int8_t knock;
static uint8_t last_cam,dirty;
void futsal_goals_reset(void) BANKED{
    fs_frame_timer=0;fs_goal_dx[0]=fs_goal_dx[1]=fs_goal_dy[0]=fs_goal_dy[1]=0;dirty=1;
}
void futsal_goals_load(void) BANKED{
    banked_sprite_data(250,FUTSAL_GOAL_TILE_COUNT,futsal_goal_tiles,BANK(futsal_goal));
    fs_post_hits=fs_bar_hits=0;last_cam=255;futsal_goals_reset();
    for(uint8_t i=0;i<10;i++){
        uint8_t col=i%5;
        set_sprite_tile(30+i,col==0||col==4?250:252);
        set_sprite_prop(30+i,SPR_PAL_BALL|(col==4?S_FLIPX:0));
    }
}
void futsal_goals_draw(uint8_t cam) BANKED{
    static uint8_t g,j,x,y;
    static OAM_item_t *o;
    if(last_cam==cam&&!dirty)return;
    last_cam=cam;dirty=0;o=&shadow_OAM[30];
    for(g=0;g<2;g++){
        x=64+fs_goal_dx[g];y=fs_projection_y[g?224:0]+2-cam+fs_goal_dy[g];
        for(j=0;j<5;j++){o->x=x;o->y=y;o++;x+=8;}
    }
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
