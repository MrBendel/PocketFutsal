#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "game.h"
#include "futsal.h"
#include "sfx.h"
#include "futsal_pitch.h"
#include "sprites.h"
#include "players.h"



uint8_t fs_spectator = 1, fs_paused, fs_owner, fs_goal_timer, fs_cam_y;
int16_t fs_pl_x[4], fs_pl_y[4], fs_ball_x, fs_ball_y, fs_ball_vx, fs_ball_vy;
uint16_t fs_sim_ticks, fs_pass_count, fs_shot_count, fs_tackle_count;
uint8_t fs_cooldown[4], fs_action[4], fs_burst[4], fs_held_ticks, fs_pickup_lock, fs_last_kicker;
uint8_t fs_role[4]; // 0 chase, 1 carry, 2 support, 3 cover
uint8_t fs_projection_y[225], fs_projection_s[225];

static int16_t ab(int16_t n) { return n < 0 ? -n : n; }
static int16_t clamp(int16_t n, int16_t lo, int16_t hi) { return n < lo ? lo : n > hi ? hi : n; }
static uint8_t team(uint8_t i) { return i >> 1; }
static int16_t distance(uint8_t i, int16_t x, int16_t y) { return ab(fs_pl_x[i]-x)+ab(fs_pl_y[i]-y); }
static int16_t sx(int16_t x, int16_t y) { return 80 + (((x-80)*(int16_t)fs_projection_s[y]) >> 7); }
static void place(uint8_t n, int16_t x, int16_t y) {
    if (x < 0 || x > 168 || y < 0 || y > 152) move_sprite(n,0,0);
    else move_sprite(n,(uint8_t)x,(uint8_t)y);
}
static void hud(void) {
    char line[] = "BLUE 0 - 0 RED  WATCH";
    line[5] += score[0]; line[9] += score[1];
    if (!fs_spectator) { line[15]='P'; line[16]='L'; line[17]='A'; line[18]='Y'; line[19]=' '; }
    win_text(0,0,line);
    if (fs_paused) win_text(7,0,"PAUSED");
    else if (fs_goal_timer) win_text(6,0,"GOAL!");
}
static void kickoff(uint8_t side) {
    fs_pl_x[0]=52*16; fs_pl_y[0]=142*16;
    fs_pl_x[1]=108*16; fs_pl_y[1]=178*16;
    fs_pl_x[2]=108*16; fs_pl_y[2]=82*16;
    fs_pl_x[3]=52*16; fs_pl_y[3]=46*16;
    fs_owner=side*2; fs_pl_x[fs_owner]=80*16; fs_pl_y[fs_owner]=112*16;
    fs_ball_x=fs_pl_x[fs_owner]; fs_ball_y=fs_pl_y[fs_owner]; fs_ball_vx=fs_ball_vy=0;
    fs_held_ticks=0; fs_pickup_lock=0; fs_last_kicker=NOBODY;
    for(uint8_t i=0;i<4;i++) fs_cooldown[i]=fs_action[i]=fs_burst[i]=0;
}
// Normalize only on a kick, using the larger axis (no per-frame division).
static void kick(uint8_t i, int16_t tx, int16_t ty, uint8_t speed) {
    int16_t dx=tx-fs_ball_x, dy=ty-fs_ball_y, m=ab(dx);
    if(ab(dy)>m) m=ab(dy);
    if(!m) return;
    // Targets in court pixels keep intermediates within signed 16-bit range.
    dx >>= 4; dy >>= 4; m >>= 4; if(!m) m=1;
    fs_ball_vx=dx*speed/m; fs_ball_vy=dy*speed/m;
    fs_owner=NOBODY; fs_last_kicker=i; fs_pickup_lock=10; fs_held_ticks=0; fs_action[i]=10;
    sfx_play(SFX_TOUCH);
}
static void pass(uint8_t i) {
    uint8_t mate=i^1;
    kick(i,fs_pl_x[mate],fs_pl_y[mate],56); fs_pass_count++;
}
static void shoot(uint8_t i, int8_t aim) {
    int16_t x=80+aim*16;
    if(!aim) x=(rand()&1)?60:100;
    kick(i,x*16,team(i)?224*16:0,76); fs_shot_count++;
}
static void tackle(uint8_t i) {
    fs_action[i]=18; fs_cooldown[i]=60; fs_burst[i]=10;
    if(fs_owner!=NOBODY && team(fs_owner)!=team(i) && distance(i,fs_ball_x,fs_ball_y)<24*16) {
        fs_owner=i; fs_held_ticks=0; fs_tackle_count++; sfx_play(SFX_TOUCH);
    }
}
static void step_player(uint8_t i, int16_t tx, int16_t ty) {
    int16_t speed=fs_burst[i]?40:20;
    if(fs_owner==i) speed=18;
    if(fs_action[i]>10 && !fs_burst[i]) speed=8;
    int16_t dx=tx-fs_pl_x[i],dy=ty-fs_pl_y[i];
    if(ab(dx)>8 && ab(dy)>8) speed-=speed>>2;
    fs_pl_x[i]=clamp(fs_pl_x[i]+clamp(dx,-speed,speed),14*16,146*16);
    fs_pl_y[i]=clamp(fs_pl_y[i]+clamp(dy,-speed,speed),8*16,216*16);
}
static void ai(uint8_t i) {
    uint8_t own=fs_owner!=NOBODY && team(fs_owner)==team(i), mate=i^1;
    int16_t tx=fs_ball_x,ty=fs_ball_y;
    int16_t goal=team(i)?224*16:0;
    int16_t pressure=30000;
    for(uint8_t j=0;j<4;j++) if(team(j)!=team(i)) {
        int16_t d=distance(j,fs_ball_x,fs_ball_y); if(d<pressure) pressure=d;
    }
    fs_role[i]=0;
    if(fs_owner==i) {
        fs_role[i]=1; tx=80*16; ty=goal;
        if(fs_held_ticks>16 && ab(fs_pl_y[i]-goal)<52*16) { shoot(i,0); return; }
        if(fs_held_ticks>28 && (pressure<30*16 || fs_held_ticks>110) && distance(mate,fs_ball_x,fs_ball_y)>32*16) { pass(i); return; }
    } else if(own) {
        fs_role[i]=2;
        tx=fs_pl_x[fs_owner]<80*16?116*16:44*16;
        ty=clamp(fs_pl_y[fs_owner]+(team(i)?-32*16:32*16),28*16,196*16);
        if(!fs_spectator && i==1 && (joy_pressed&J_B)) { pass(fs_owner); }
    } else if(distance(mate,fs_ball_x,fs_ball_y)<distance(i,fs_ball_x,fs_ball_y)) {
        fs_role[i]=3; tx=clamp(fs_ball_x,52*16,108*16);
        ty=team(i)?32*16:192*16;
    } else if(fs_owner!=NOBODY && !fs_cooldown[i] && distance(i,fs_ball_x,fs_ball_y)<21*16) tackle(i);
    step_player(i,tx,ty);
}
static void ball_update(void) {
    if(fs_owner!=NOBODY) {
        fs_ball_x=fs_pl_x[fs_owner]; fs_ball_y=clamp(fs_pl_y[fs_owner]+(team(fs_owner)?6*16:-6*16),0,224*16);
        if(fs_held_ticks<255) fs_held_ticks++;
        return;
    }
    fs_ball_x+=fs_ball_vx; fs_ball_y+=fs_ball_vy;
    if(fs_ball_x<10*16 || fs_ball_x>150*16) { fs_ball_x=clamp(fs_ball_x,10*16,150*16); fs_ball_vx=-fs_ball_vx; }
    if(fs_ball_y<0 || fs_ball_y>224*16) {
        if(fs_ball_x>=56*16 && fs_ball_x<=104*16) {
            uint8_t s=fs_ball_y<0?0:1; score[s]++; fs_goal_timer=90;
            fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vx=fs_ball_vy=0;
            sfx_play(SFX_WHISTLE); hud(); return;
        }
        fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vy=-fs_ball_vy;
    }
    if(!(fs_sim_ticks&7)) { if(fs_ball_vx) fs_ball_vx+=fs_ball_vx>0?-1:1; if(fs_ball_vy) fs_ball_vy+=fs_ball_vy>0?-1:1; }
    if(fs_pickup_lock) fs_pickup_lock--;
    // Rotate pickup priority to avoid a permanent advantage to blue.
    for(uint8_t k=0;k<4;k++) {
        uint8_t i=(k+(fs_sim_ticks&3))&3;
        if(fs_pickup_lock && fs_last_kicker==i) continue;
        if(fs_action[i]>10) continue;
        if(distance(i,fs_ball_x,fs_ball_y)<((ab(fs_ball_vx)+ab(fs_ball_vy)>60)?4*16:12*16)) { fs_owner=i; fs_held_ticks=0; break; }
    }
}
static void draw(void) {
    int16_t by=fs_ball_y>>4,bx=fs_ball_x>>4;
    fs_cam_y=(uint8_t)clamp(fs_projection_y[by]-68,0,32); scroll_y=fs_cam_y;
    // Assign nearer players lower OAM indexes so overlapping bodies sort by depth.
    uint8_t order[4]={0,1,2,3};
    for(uint8_t i=0;i<4;i++) for(uint8_t j=i+1;j<4;j++) if(fs_pl_y[order[j]]>fs_pl_y[order[i]]) { uint8_t t=order[i]; order[i]=order[j]; order[j]=t; }
    for(uint8_t slot=0;slot<4;slot++) {
        uint8_t i=order[slot],n=1+slot*4;
        int16_t y=fs_pl_y[i]>>4,x=sx(fs_pl_x[i]>>4,y), sy=fs_projection_y[y]-fs_cam_y;
        uint8_t tile=(team(i)?80:0)+(fs_action[i]?32:((fs_sim_ticks>>3)&1)?8:16);
        uint8_t pal=S_BANK|(team(i)?SPR_PAL_CPU:SPR_PAL_YOU);
        for(uint8_t k=0;k<4;k++) { set_sprite_tile(n+k,tile+k*2); set_sprite_prop(n+k,pal); }
        place(n,x,sy-7); place(n+1,x,sy+9); place(n+2,x+8,sy-7); place(n+3,x+8,sy+9);
    }
    set_sprite_tile(0,(fs_sim_ticks&8)?2:0); set_sprite_prop(0,SPR_PAL_BALL);
    place(0,sx(bx,by)+4,fs_projection_y[by]-fs_cam_y+12);
    set_sprite_tile(17,6); set_sprite_prop(17,SPR_PAL_UI);
    place(17,sx(fs_pl_x[0]>>4,fs_pl_y[0]>>4)+4,fs_projection_y[fs_pl_y[0]>>4]-fs_cam_y-18);
}
void futsal_enter(void) BANKED {
    uint8_t buf[FUTSAL_PITCH_MAP_W*FUTSAL_PITCH_MAP_H];
    palettes_play();
    banked_bkg_data(SCENE_TILES,FUTSAL_PITCH_TILE_COUNT,futsal_pitch_tiles,BANK(futsal_pitch));
    banked_sprite_data(0,SPRITES_TILE_COUNT,sprites_tiles,BANK(sprites));
    banked_sprite_data_vbk1(0,PLAYERS_TILE_COUNT,players_tiles,BANK(players));
    banked_memcpy(buf,futsal_pitch_map,sizeof(buf),BANK(futsal_pitch)); set_bkg_tiles(0,0,FUTSAL_PITCH_MAP_W,FUTSAL_PITCH_MAP_H,buf);
    banked_memcpy(buf,futsal_pitch_attr,sizeof(buf),BANK(futsal_pitch)); VBK_REG=1; set_bkg_tiles(0,0,FUTSAL_PITCH_MAP_W,FUTSAL_PITCH_MAP_H,buf); VBK_REG=0;
    for(int16_t y=0;y<=224;y++) {
        fs_projection_y[y]=32+((int32_t)92*y*448+(int32_t)72*y*y)/114688L;
        fs_projection_s[y]=111+17*y/224;
    }
    initrand(game_seed); score[0]=score[1]=sets[0]=sets[1]=0;
    match_sets=1; winner=NOBODY; fs_paused=fs_goal_timer=0;
    fs_sim_ticks=fs_pass_count=fs_shot_count=fs_tackle_count=0;
    move_win(7,HUD_WY); SHOW_WIN; gfx_sprite_split(HUD_WY);
    kickoff(0); hud(); draw();
}
void futsal_update(void) BANKED {
    if(joy_pressed&J_START) { fs_paused=!fs_paused; hud(); }
    if(fs_paused) return;
    if(joy_pressed&J_SELECT) { fs_spectator=!fs_spectator; hud(); }
    if(fs_goal_timer) {
        if(!--fs_goal_timer) {
            if(score[0]>=5 || score[1]>=5) { winner=score[0]>=5?0:1; state_change(STATE_GAME_OVER); return; }
            kickoff(fs_ball_y==0?1:0); hud();
        }
        draw(); return;
    }
    fs_sim_ticks++;
    for(uint8_t i=0;i<4;i++) {
        if(fs_cooldown[i]) fs_cooldown[i]--; if(fs_action[i]) fs_action[i]--; if(fs_burst[i]) fs_burst[i]--;
        if(i || fs_spectator) ai(i);
        else {
            int8_t dx=((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0),dy=((joy&J_DOWN)!=0)-((joy&J_UP)!=0);
            if(fs_owner==0) { if(joy_pressed&J_A) shoot(0,dx); else if(joy_pressed&J_B) pass(0); }
            else {
                if((joy_pressed&J_A) && !fs_cooldown[0]) tackle(0);
                if((joy_pressed&J_B) && !fs_cooldown[0] && (fs_owner==NOBODY || team(fs_owner)!=0)) { fs_burst[0]=16; fs_cooldown[0]=60; }
            }
            step_player(0,fs_pl_x[0]+dx*64,fs_pl_y[0]+dy*64);
        }
    }
    ball_update(); draw();
}
