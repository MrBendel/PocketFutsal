#pragma bank 255
// The 2v2 match's drawing, moved out of futsal.c (its bank was full). The court is a
// fixed view: a point's screen x is its court x, its row fs_projection_y[depth].
#include <gb/gb.h>
#include <gb/cgb.h>
#include "gfx.h"
#include "game.h"
#include "futsal.h"
#include "futsal_fx.h"
#include "futsal_goals.h"
#include "futsal_keeper.h"
#include "futsal_draw.h"
#define team(i) ((uint8_t)((i) >> 1))
#define HUMAN(i) (!fs_spectator&&(i)==fs_ctrl)
extern uint8_t fs_cam_target,fs_ball_oam,fs_trail_n,fs_trail_i,fs_trail_x[16],fs_trail_y[16];
extern uint8_t fs_view[4],fs_flip[4],fs_side[4],fs_pose[4],fs_moving[4],fs_action[4],fs_tackling[4];
extern uint8_t fs_stumble_timer,fs_stumble_player,fs_down[4],fs_dodge[4],fs_shot_charge,fs_ctrl;
extern uint8_t fs_air_kind[4],fs_goal_refresh;
static void place(uint8_t n, int16_t x, int16_t y) {
    if (x < 0 || x > 168 || y < 0 || y > 152) move_sprite(n,0,0);
    else move_sprite(n,(uint8_t)x,(uint8_t)y);
}
void futsal_draw(void) BANKED {
    static uint8_t by,bx,ball_sx,ball_wy,marker_x;
    static int16_t marker_y;
    by=(uint16_t)fs_ball_y>>4;bx=(uint16_t)fs_ball_x>>4;
    // The camera follows the ball, a little ahead of whoever has it (the way
    // they're attacking), easing a quarter of the way there a frame.
    {
        int16_t t=(int16_t)fs_projection_y[by]-68;
        if(fs_owner!=NOBODY)t+=team(fs_owner)?FS_CAM_LEAD:-FS_CAM_LEAD;
        if(t<0)t=0;else if(t>FS_CAM_MAX)t=FS_CAM_MAX;
        fs_cam_target=(uint8_t)t;
        t=(int16_t)fs_cam_target-fs_cam_y;
        if(t>3||t<-3)fs_cam_y+=t>>2;else fs_cam_y=fs_cam_target;
    }
    scroll_y=fs_cam_y;
    ball_sx=bx+4;ball_wy=fs_projection_y[by]+12;
    // Assign nearer players lower OAM indexes so overlapping bodies sort by depth.
    static uint8_t order[4]={0,1,2,3};
    // Five comparisons, fixed indices: no nested loop/index bookkeeping.
#define DEPTH_SWAP(a,b) do { if(fs_pl_y[order[a]]<fs_pl_y[order[b]]){uint8_t t=order[a];order[a]=order[b];order[b]=t;} } while(0)
    DEPTH_SWAP(0,1);DEPTH_SWAP(2,3);DEPTH_SWAP(0,2);DEPTH_SWAP(1,3);DEPTH_SWAP(1,2);
    fs_ball_oam=2;
    for(uint8_t i=0;i<4;i++) if(fs_pl_y[i]>fs_ball_y) fs_ball_oam+=4;
    for(uint8_t slot=0;slot<4;slot++) {
        static uint8_t i,n,tile,pal,pose,y,x;
        static int16_t sy;
        i=order[slot];n=(slot<<2)+2;if(n>=fs_ball_oam)n++;
        y=(uint16_t)fs_pl_y[i]>>4;x=(uint16_t)fs_pl_x[i]>>4;sy=fs_projection_y[y]-fs_cam_y;
        if(i==fs_ctrl){marker_x=x+4;marker_y=sy-18;}
        move_sprite(23+i,x+4,sy+12);
        if(fs_aerial[i]){
            uint8_t t=fs_aerial[i];
            tile=(i<<6)+(fs_view[i]<<5)+24;pal=S_BANK|i;
            if(t>24||t<5){tile=28+i*24+(fs_view[i]<<3);pal=i;}
            else if(t<=20&&t>=10&&fs_air_kind[i]){gfx_bicycle(n,tile,x,sy-12,pal,fs_flip[i]);continue;}
            else sy-=t>=10?10:5;
            gfx_actor(n,tile,0,x,sy,pal,fs_flip[i]);continue;
        }
        if(fs_down[i]||(fs_stumble_timer&&i==fs_stumble_player)){
            // Reuse this character's connected crouch/head, then stand up.
            if((fs_down[i]?fs_down[i]:fs_stumble_timer)>8)gfx_actor(n,28+i*24+(fs_view[i]<<3),0,x,sy,i,fs_flip[i]);
            else gfx_side(n,i,0,x,(uint8_t)sy,fs_flip[i]);
            continue;
        }
        if(HUMAN(i)&&(fs_pass_charge>4||fs_shot_charge>4)){
            pal=(fs_pass_charge>=12||fs_shot_charge>=FS_CHARGE_MAX)&&(fs_sim_ticks&4)?SPR_PAL_POWER:0;
            gfx_actor(n,28+(fs_view[i]<<3),0,x,sy,pal,fs_flip[i]);continue;
        }
        if(fs_tackling[i]>4 && fs_tackling[i]<=14) {
            uint8_t base=20+i*24, flip=!fs_flip[i];
            gfx_actor(n,base,1,x,sy,i,flip);
            continue;
        }
        if(!fs_action[i]&&fs_side[i]){
            pose=fs_moving[i]?((fs_sim_ticks&4)?0:(fs_sim_ticks&8)?2:1):0;
            fs_pose[i]=pose;
            gfx_side(n,i,pose,x,(uint8_t)sy,fs_flip[i]);continue;
        }
        pose=fs_action[i]?3:(fs_moving[i]?(((fs_sim_ticks>>3)&1)?1:2):0);
        fs_pose[i]=pose;
        tile=(i<<6)+(fs_view[i]<<5)+(pose<<3);
        pal=S_BANK|i|(fs_flip[i]?S_FLIPX:0);
        if(fs_dodge[i]&&(fs_sim_ticks&2))pal=(pal&~7)|SPR_PAL_POWER;
        if(fs_tackling[i]) { tile=20+i*24+8+(fs_view[i]<<3); pal &= ~S_BANK; }
        gfx_actor(n,tile,0,x,sy,pal,fs_flip[i]);
    }
    set_sprite_tile(fs_ball_oam,gfx_ball_tile()); set_sprite_prop(fs_ball_oam,fs_power_trail?SPR_PAL_POWER:SPR_PAL_BALL);
    move_sprite(fs_ball_oam,ball_sx,(uint8_t)(ball_wy-fs_cam_y-(fs_ball_z>>4)));
    if(fs_power_trail||fs_trail_n) {
        // A short history in map coordinates keeps the trail stable while scrolling.
        fs_trail_i=(fs_trail_i+1)&15;
        fs_trail_x[fs_trail_i]=ball_sx;
        fs_trail_y[fs_trail_i]=ball_wy-(fs_ball_z>>4);
        if(fs_power_trail && !fs_goal_timer) {
            if(fs_trail_n<16) fs_trail_n++;
            fs_power_trail--;
        } else fs_trail_n=0;
        for(uint8_t k=0;k<3;k++) {
            uint8_t age=(k+1)<<1, entry=(fs_trail_i-age)&15;
            if(fs_trail_n>age) place(20+k,fs_trail_x[entry],fs_trail_y[entry]-fs_cam_y);
            else move_sprite(20+k,0,0);
        }
    }
    move_sprite(27,ball_sx,(uint8_t)(ball_wy-fs_cam_y+1));
    place(19,marker_x,marker_y);
    if(fs_lob&&fs_owner==NOBODY&&!fs_goal_timer){
        uint8_t zy=(uint16_t)fs_target_y>>4,zx=(uint16_t)fs_target_x>>4;
        uint8_t pal=fs_zone_ready&&(fs_sim_ticks&4)?SPR_PAL_POWER:SPR_PAL_UI;
        set_sprite_prop(0,pal);set_sprite_prop(1,pal);
        move_sprite(0,zx,fs_projection_y[zy]-fs_cam_y+12);
        move_sprite(1,zx+8,fs_projection_y[zy]-fs_cam_y+12);
    }else{move_sprite(0,0,0);move_sprite(1,0,0);}
    futsal_keeper_draw(fs_cam_y);
    if(fs_fx_timer||fs_fx_frame!=255)futsal_fx_draw(fs_cam_y);
    if(fs_goal_refresh){
        fs_goal_refresh=0;futsal_goals_draw(fs_cam_y);
    }
}