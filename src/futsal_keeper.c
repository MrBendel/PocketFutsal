#pragma bank 255
// The 2v2 goalkeepers (2026-11-27, Mario Strikers: "charged shots" mean something
// only against a keeper; without one, nearly every shot on target went in: 48 of 52
// in AI matches). One on each goal line, AI for both teams: it shadows the ball
// across the goal mouth, and dives as a shot is struck. Whether it gets there is
// decided then, from how far it has to go, the ticks it has (the shot's flight) and
// the shot's pace: a full-charge shot from close into a corner beats it, a slow or a
// long one doesn't. A save parries the ball back out, loose.
// Art: the Shootout keeper (artwork/players/keeper-dive), its ready frame trimmed to
// 16 px and its low and high save poses (tools/shootout-player-art.mjs FUTSAL_KEEPER).
#include <gb/gb.h>
#include <rand.h>
#include "gfx.h"
#include "game.h"
#include "sfx.h"
#include "futsal.h"
#include "futsal_keeper.h"
#include "futsal_goals.h"
#define FS_MOUTH_L (FS_GOAL_LEFT+3)
#define FS_MOUTH_R (FS_GOAL_RIGHT-3)
#include "futsal_keeper_art.h"
#if FS_KP_TILE + FUTSAL_KEEPER_ART_TILE_COUNT > 214
#error Keeper tiles overlap the side sprite atlas
#endif
#define KP_SPEED 16   // SUB a tick, shadowing the ball
#define KP_DIVE 24    // SUB a tick, diving (futsal_keeper_shot multiplies by it with shifts)
#define KP_MOUTH 14   // px either side of the middle it shadows the ball over
#define KP_HIGH (12*16)  // a shot crossing above this: the high save
#define KP_OVER (22*16)  // ... above this: over its head
static const int16_t kp_y[2]={5*16,219*16};  // court depth of each keeper
int16_t fs_kp_x[2],fs_kp_to[2];
uint8_t fs_kp_dive[2],fs_kp_save[2],kp_high[2];
uint16_t fs_saves;
// What to draw, worked out each tick (futsal_keeper_step) in bytes, so drawing,
// every frame, only compares and writes: its screen x (the sprites' left), pose
// (0 ready, 1 low save, 2 high save) and facing; its map row (futsal_keeper_load).
static uint8_t kp_px[2],kp_pose[2],kp_flip[2],kp_row[2];
static uint8_t kp_fresh[2];  // the dive decided this tick: its pose drawn from the next (one frame's work each)
// What the sprites show now.
static uint8_t shown_g=255,shown_pose,shown_flip,shown_x,shown_y;
#define kp_redraw() (shown_g=255)
void futsal_keeper_load(void) BANKED{
    banked_sprite_data(FS_KP_TILE,FUTSAL_KEEPER_ART_TILE_COUNT,futsal_keeper_art_tiles,BANK(futsal_keeper_art));
    fs_saves=0;
    futsal_keeper_reset();
}
void futsal_keeper_reset(void) BANKED{
    for(uint8_t g=0;g<2;g++){fs_kp_x[g]=fs_kp_to[g]=80*16;fs_kp_dive[g]=fs_kp_save[g]=0;}
    for(uint8_t n=0;n<8;n++)move_sprite(FS_KP_OAM+n,0,0);
    // (Here, not on loading: futsal_enter fills the depth table after loading us.)
    kp_row[0]=fs_projection_y[kp_y[0]>>4];kp_row[1]=fs_projection_y[kp_y[1]>>4];
    kp_redraw();
}
void futsal_keeper_shot(uint8_t g,int16_t tx,uint8_t t,uint8_t speed) BANKED{
    static int16_t dist,cover;
    if(tx<(FS_MOUTH_L)*16||tx>(FS_MOUTH_R)*16){fs_kp_save[g]=0;return;}  // wide: let it go
    dist=tx-fs_kp_x[g];if(dist<0)dist=-dist;
    cover=(int16_t)(((uint16_t)t<<4)+((uint16_t)t<<3))+((int16_t)(((128-speed)>>3)+3)<<4);  // t*KP_DIVE (24), by shifts
    fs_kp_save[g]=dist<=cover;
    // The dive: all the way if it gets there, else as far as it can.
    if(fs_kp_save[g])fs_kp_to[g]=tx;
    else fs_kp_to[g]=fs_kp_x[g]+(tx<fs_kp_x[g]?-cover:cover);
    fs_kp_dive[g]=t+12;kp_high[g]=0;kp_fresh[g]=1;
}
void futsal_keeper_step(void) BANKED{
    static int16_t to,d;
    for(uint8_t g=0;g<2;g++){
        if(fs_kp_dive[g]){fs_kp_dive[g]--;to=fs_kp_to[g];d=KP_DIVE;}
        else{
            to=80*16+((fs_ball_x-80*16)>>1);
            if(to<(80-KP_MOUTH)*16)to=(80-KP_MOUTH)*16;else if(to>(80+KP_MOUTH)*16)to=(80+KP_MOUTH)*16;
            d=KP_SPEED;
        }
        if(fs_kp_x[g]<to){fs_kp_x[g]+=d;if(fs_kp_x[g]>to)fs_kp_x[g]=to;}
        else if(fs_kp_x[g]>to){fs_kp_x[g]-=d;if(fs_kp_x[g]<to)fs_kp_x[g]=to;}
        // For drawing: diving (or down after it), the save pose toward the shot.
        kp_px[g]=(uint8_t)(fs_kp_x[g]>>4);
        if(kp_fresh[g])kp_fresh[g]=0;
        else if(fs_kp_dive[g]){
            kp_pose[g]=kp_high[g]?2:1;kp_px[g]-=8;
            kp_flip[g]=fs_kp_to[g]<fs_kp_x[g]||(fs_kp_x[g]==fs_kp_to[g]&&fs_kp_x[g]<80*16);
        }else kp_pose[g]=kp_flip[g]=0;
    }
}
uint8_t futsal_keeper_check(int16_t old_y) BANKED{
    uint8_t g=fs_ball_vy>0;
    if(!fs_kp_save[g]||!fs_is_shot)return 0;
    if(g?(old_y>=kp_y[1]||fs_ball_y<kp_y[1]):(old_y<=kp_y[0]||fs_ball_y>kp_y[0]))return 0;
    fs_kp_save[g]=0;
    if(fs_ball_z>KP_OVER)return 0;  // over its head
    kp_high[g]=fs_ball_z>KP_HIGH;
    // Parried: back out, slow, off to a side.
    fs_ball_y=kp_y[g];fs_ball_vy=-(fs_ball_vy>>2);
    fs_ball_vx=(fs_ball_vx>>2)+((rand()&1)?12:-12);
    if(fs_ball_vz<0)fs_ball_vz=-fs_ball_vz>>1;
    fs_is_shot=0;fs_flight_left=0;fs_power_trail=0;fs_receiver=NOBODY;
    fs_saves++;sfx_play(SFX_PARRY);
    return 1;
}
// One goal shows at a time (the court's 256 rows, 136 on screen): its keeper. Every
// frame (the camera moves most of them), so in bytes, at fixed OAM addresses: tiles
// and flags only as its pose changes, positions as they do.
#define KO(n) shadow_OAM[FS_KP_OAM+(n)]
void futsal_keeper_draw(uint8_t cam) BANKED{
    static uint8_t g,y,x,pose,flip,t,k,p;
    static int16_t top;
    g=cam<56?0:1;
    top=(int16_t)kp_row[g]-cam-7;  // the OAM y of its top, as gfx_actor places a player
    if(top<1||top>159){
        if(shown_g!=254){KO(0).y=KO(1).y=KO(2).y=KO(3).y=KO(4).y=KO(5).y=KO(6).y=KO(7).y=0;shown_g=254;}
        return;  // off screen: hidden once
    }
    y=(uint8_t)top;x=kp_px[g];pose=kp_pose[g];flip=kp_flip[g];
    if(g!=shown_g||pose!=shown_pose||flip!=shown_flip){
        shown_g=g;shown_pose=pose;shown_flip=flip;
        // Columns left to right (right to left flipped): tiles t+4k, its top and bottom.
        t=FS_KP_TILE+(pose==2?24:pose?8:0);
        k=flip?(pose?12:4):0;
        KO(0).tile=t+k;KO(1).tile=t+k+2;k=flip?k-4:k+4;
        KO(2).tile=t+k;KO(3).tile=t+k+2;k=flip?k-4:k+4;
        KO(4).tile=t+k;KO(5).tile=t+k+2;k=flip?k-4:k+4;
        KO(6).tile=t+k;KO(7).tile=t+k+2;
        p=(g?1:3)|(flip?S_FLIPX:0);  // blue's keeper at the bottom, red's at the top
        KO(0).prop=KO(1).prop=KO(2).prop=KO(3).prop=KO(4).prop=KO(5).prop=KO(6).prop=KO(7).prop=p;
        shown_x=shown_y=0;  // every position again
        if(!pose)KO(4).y=KO(5).y=KO(6).y=KO(7).y=0;
    }
    if(y!=shown_y){
        shown_y=y;
        KO(0).y=KO(2).y=y;KO(1).y=KO(3).y=y+8;
        if(pose){KO(4).y=KO(6).y=y;KO(5).y=KO(7).y=y+8;}
    }
    if(x!=shown_x){
        shown_x=x;
        KO(0).x=KO(1).x=x;KO(2).x=KO(3).x=x+8;
        if(pose){KO(4).x=KO(5).x=x+16;KO(6).x=KO(7).x=x+24;}
    }
}
