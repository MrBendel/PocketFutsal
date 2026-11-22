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
#include "futsal_spiky.h"
#include "futsal_cap.h"
#include "futsal_crop.h"
#include "futsal_bandana.h"
#include "action_spiky.h"
#include "action_cap.h"
#include "action_crop.h"
#include "action_bandana.h"



uint8_t fs_spectator = 1, fs_paused, fs_owner, fs_goal_timer, fs_cam_y;
int16_t fs_pl_x[4], fs_pl_y[4], fs_ball_x, fs_ball_y, fs_ball_vx, fs_ball_vy;
uint16_t fs_sim_ticks, fs_pass_count, fs_shot_count, fs_tackle_count;
int8_t fs_vx[4], fs_vy[4], fs_slide_vx[4], fs_slide_vy[4];
int8_t fs_dribble_x[4], fs_dribble_y[4];
uint8_t fs_think[4];
int16_t fs_ai_tx[4], fs_ai_ty[4];
int16_t fs_ball_z, fs_ball_vz, fs_target_x, fs_target_y;
uint8_t fs_receiver, fs_flight_left, fs_is_shot;
uint16_t fs_completed_passes;
uint8_t fs_return_call;
uint8_t fs_cooldown[4], fs_action[4], fs_burst[4], fs_held_ticks, fs_pickup_lock, fs_last_kicker;
uint8_t fs_tackling[4];
uint8_t fs_moving[4], fs_view[4], fs_flip[4], fs_pose[4];
uint8_t fs_role[4]; // 0 chase, 1 carry, 2 support, 3 cover
uint8_t fs_ball_oam, fs_hud_pending;
uint8_t fs_power_trail, fs_trail_n, fs_trail_i;
uint8_t fs_trail_x[16], fs_trail_y[16];
uint8_t fs_projection_y[225], fs_projection_s[225];

static int16_t ab(int16_t n) { return n < 0 ? -n : n; }
static int16_t clamp(int16_t n, int16_t lo, int16_t hi) { return n < lo ? lo : n > hi ? hi : n; }
static uint8_t team(uint8_t i) { return i >> 1; }
static int16_t distance(uint8_t i,int16_t x,int16_t y){
    int16_t dx=fs_pl_x[i]-x,dy=fs_pl_y[i]-y;
    if(dx<0)dx=-dx;if(dy<0)dy=-dy;
    return dx+dy;
}
static uint8_t sx(uint8_t x, uint8_t y) { return 80 + (((x-80)*(int16_t)fs_projection_s[y]) >> 7); }
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
    fs_power_trail=fs_trail_n=fs_trail_i=0;
    fs_receiver=NOBODY; fs_flight_left=fs_is_shot=0; fs_ball_z=16; fs_ball_vz=0;
    fs_owner=side*2; fs_pl_x[fs_owner]=80*16; fs_pl_y[fs_owner]=112*16;
    // Start with a usable lateral outlet, not a teammate stranded deep behind.
    fs_pl_x[fs_owner^1]=(side?44:116)*16;
    fs_pl_y[fs_owner^1]=(side?96:128)*16;
    fs_ball_x=fs_pl_x[fs_owner]; fs_ball_y=fs_pl_y[fs_owner]; fs_ball_vx=fs_ball_vy=0;
    fs_held_ticks=0; fs_pickup_lock=0; fs_last_kicker=NOBODY;
    fs_return_call=0;
    for(uint8_t i=0;i<4;i++) {
        fs_tackling[i]=fs_cooldown[i]=fs_action[i]=fs_burst[i]=fs_moving[i]=fs_flip[i]=0;
        fs_view[i]=team(i)?0:1;
        fs_vx[i]=fs_vy[i]=fs_slide_vx[i]=fs_slide_vy[i]=0;
        fs_dribble_x[i]=0; fs_dribble_y[i]=team(i)?1:-1;
        fs_think[i]=i<<1; fs_ai_tx[i]=fs_pl_x[i]; fs_ai_ty[i]=fs_pl_y[i];
    }
}
// Plan one ballistic flight at release; only additions and gravity per frame.
static void kick(uint8_t i, int16_t tx, int16_t ty) {
    tx=clamp(tx,14*16,146*16); ty=clamp(ty,0,224*16);
    int16_t dx=tx-fs_ball_x,dy=ty-fs_ball_y;
    // One readable half-second arc. A power-of-two duration makes launch
    // velocities exact shifts instead of expensive software division.
    fs_target_x=tx;fs_target_y=ty;fs_flight_left=32;
    fs_ball_vx=dx<0?-((-dx)>>5):dx>>5;
    fs_ball_vy=dy<0?-((-dy)>>5):dy>>5;
    fs_ball_z=16;fs_ball_vz=31;
    if(dx)fs_flip[i]=dx<0;if(dy)fs_view[i]=dy<0;
    fs_power_trail=fs_trail_n=0;
    shadow_OAM[18].y=0;shadow_OAM[19].y=0;shadow_OAM[20].y=0;shadow_OAM[21].y=0;
    fs_owner=NOBODY; fs_last_kicker=i; fs_pickup_lock=10; fs_held_ticks=0; fs_action[i]=10;
    sfx_play(SFX_TOUCH);
}
static void pass(uint8_t i, int8_t dx, int8_t dy) {
    static uint8_t mate;
    static int16_t tx,ty;
    mate=i^1;
    fs_receiver=mate; fs_is_shot=0;
    // A direction sends the receiver into space; neutral leads their current run.
    tx=fs_pl_x[mate];ty=fs_pl_y[mate];
    if(dx||dy){tx+=dx*24*16;ty+=dy*24*16;}
    else {tx+=fs_vx[mate]*32;ty+=fs_vy[mate]*32;}
    kick(i,tx,clamp(ty,16*16,208*16));
    // React to the delivery immediately rather than following a stale support run.
    fs_ai_tx[mate]=fs_target_x;fs_ai_ty[mate]=fs_target_y;
    if(i==1)fs_return_call=0;
    fs_pass_count++;
}
static void shoot(uint8_t i, int8_t aim) {
    int16_t x=80+aim*16;
    if(!aim)x=(rand()&1)?60:100;
    fs_receiver=NOBODY; fs_is_shot=1;
    kick(i,x*16,team(i)?224*16:0);fs_power_trail=48;fs_shot_count++;
}
static void tackle(uint8_t i) {
    fs_tackling[i]=18;fs_action[i]=18;fs_cooldown[i]=65;fs_burst[i]=10;
    int16_t dx=fs_ball_x-fs_pl_x[i],dy=fs_ball_y-fs_pl_y[i];
    int8_t vx=dx<-4*16?-36:dx>4*16?36:0,vy=dy<-4*16?-36:dy>4*16?36:0;
    if(vx&&vy){vx=vx<0?-27:27;vy=vy<0?-27:27;}
    if(!vx&&!vy){vx=fs_dribble_x[i]*36;vy=fs_dribble_y[i]*36;}
    fs_slide_vx[i]=vx;fs_slide_vy[i]=vy;
}
// Velocities fit a signed byte. Copy them once rather than repeatedly indexing
// word arrays; this hot path runs for all four players every frame.
#define EASE(v,t,a) do { if((v)<(t)){(v)+=(a);if((v)>(t))(v)=(t);}else if((v)>(t)){(v)-=(a);if((v)<(t))(v)=(t);} } while(0)
static void step_player(uint8_t i,int16_t tx,int16_t ty){
    // Serial scratch: static locals avoid hundreds of SDCC stack spills in
    // this four-times-per-frame path (no interrupts call the simulation).
    static int16_t x,y,dx,dy;
    static int8_t vx,vy,want_x,want_y,speed;
    static uint8_t slide,burst,ax,ay;
    x=fs_pl_x[i];y=fs_pl_y[i];dx=tx-x;dy=ty-y;
    vx=fs_vx[i];vy=fs_vy[i];want_x=want_y=0;
    slide=fs_tackling[i];burst=fs_burst[i];
    speed=fs_owner==i?22:24;if(burst)speed=44;
    if(slide>14||(slide&&slide<=4))speed=8;
    if((dx>12||dx<-12)&&(dy>12||dy<-12))speed-=speed>>2;
    if(dx>12)want_x=speed;else if(dx<-12)want_x=-speed;
    if(dy>12)want_y=speed;else if(dy<-12)want_y=-speed;
    if(slide>4&&slide<=14){vx=fs_slide_vx[i];vy=fs_slide_vy[i];}
    else{
        ax=burst?6:((vx<0&&want_x>0)||(vx>0&&want_x<0)?2:3);
        ay=burst?6:((vy<0&&want_y>0)||(vy>0&&want_y<0)?2:3);
        EASE(vx,want_x,ax);EASE(vy,want_y,ay);
    }
    x+=vx;y+=vy;
    if(x<14*16){x=14*16;vx=0;}else if(x>146*16){x=146*16;vx=0;}
    if(y<8*16){y=8*16;vy=0;}else if(y>216*16){y=216*16;vy=0;}
    fs_pl_x[i]=x;fs_pl_y[i]=y;fs_vx[i]=vx;fs_vy[i]=vy;
    if(slide>4&&slide<=14&&fs_owner!=NOBODY&&((fs_owner^i)&2)&&fs_ball_z<=6*16){
        // Contact is at the leading foot during the lunge, not on button press.
        dx=fs_ball_x-x-vx*4;dy=fs_ball_y-y-vy*4;
        if(dx>=-7*16&&dx<=7*16&&dy>=-7*16&&dy<=7*16){
            fs_owner=i;fs_held_ticks=0;fs_receiver=NOBODY;fs_tackle_count++;sfx_play(SFX_TOUCH);
        }
    }
    fs_moving[i]=vx||vy;
    if(vx<-3)fs_flip[i]=1;else if(vx>3)fs_flip[i]=0;
    if(vy<-3)fs_view[i]=1;else if(vy>3)fs_view[i]=0;
    if(vx||vy){fs_dribble_x[i]=vx<0?-1:vx>0?1:0;fs_dribble_y[i]=vy<0?-1:vy>0?1:0;}
}
static void ai(uint8_t i){
    uint8_t mate=i^1, own=fs_owner!=NOBODY&&team(fs_owner)==team(i);
    int16_t goal=team(i)?224*16:0,tx=fs_ball_x,ty=fs_ball_y;
    if(!fs_spectator&&i==1&&fs_owner==i&&((joy_pressed&J_B)||fs_return_call)){
        pass(i,0,0);step_player(i,fs_ai_tx[i],fs_ai_ty[i]);return;
    }
    if(fs_think[i]){fs_think[i]--;step_player(i,fs_ai_tx[i],fs_ai_ty[i]);return;}
    fs_think[i]=7;
    fs_role[i]=0;
    if(fs_owner==i){
        fs_role[i]=1;
        int16_t pressure=30000,mate_space=30000;uint8_t defender=0;
        for(uint8_t j=0;j<4;j++)if(team(j)!=team(i)){
            int16_t d=distance(j,fs_ball_x,fs_ball_y);
            if(d<pressure){pressure=d;defender=j;}
            d=distance(j,fs_pl_x[mate],fs_pl_y[mate]);if(d<mate_space)mate_space=d;
        }
        tx=(fs_pl_x[i]<80*16?58:102)*16;ty=goal;
        // Attack a side of the press instead of driving into its centre.
        if(pressure<42*16&&ab(fs_pl_x[defender]-fs_pl_x[i])<24*16)tx=fs_pl_x[defender]<80*16?120*16:40*16;
        int16_t forward=team(i)?fs_pl_y[mate]-fs_pl_y[i]:fs_pl_y[i]-fs_pl_y[mate];
        if(fs_held_ticks>10&&ab(fs_pl_y[i]-goal)<48*16&&pressure>14*16)shoot(i,0);
        else if(fs_held_ticks>12&&mate_space>18*16&&forward>-32*16&&distance(mate,fs_ball_x,fs_ball_y)>24*16&&(pressure<48*16||fs_held_ticks>100))pass(i,0,0);
        else if(fs_held_ticks>18&&ab(fs_pl_y[i]-goal)<64*16)shoot(i,0);
    }else if(own){
        fs_role[i]=2;
        tx=fs_pl_x[fs_owner]<80*16?120*16:40*16;
        ty=clamp(fs_pl_y[fs_owner]+(team(i)?44*16:-44*16),24*16,200*16);
    }else if(fs_owner==NOBODY&&fs_receiver==i){
        fs_role[i]=2;tx=fs_flight_left?fs_target_x:fs_ball_x;ty=fs_flight_left?fs_target_y:fs_ball_y;
    }else if(fs_owner==NOBODY&&fs_receiver!=NOBODY&&team(fs_receiver)==team(i)){
        // The passer continues a forward run while their delivery is airborne.
        fs_role[i]=2;tx=fs_target_x<80*16?120*16:40*16;
        ty=clamp(fs_target_y+(team(i)?32*16:-32*16),24*16,200*16);
    }else{
        // Eight-frame perception interval gives momentum and feints a purpose.
        if(distance(mate,fs_ball_x,fs_ball_y)<distance(i,fs_ball_x,fs_ball_y)){
            fs_role[i]=3;
            tx=clamp(fs_ball_x,40*16,120*16);
            ty=team(i)?clamp(fs_ball_y-40*16,28*16,104*16):clamp(fs_ball_y+40*16,120*16,196*16);
        }else{
            tx=clamp(fs_ball_x+fs_ball_vx*6,16*16,144*16);
            ty=clamp(fs_ball_y+fs_ball_vy*6,12*16,212*16);
            if(fs_owner!=NOBODY&&!fs_cooldown[i]&&fs_ball_z<=6*16&&distance(i,fs_ball_x,fs_ball_y)<20*16)tackle(i);
        }
    }
    fs_ai_tx[i]=tx;fs_ai_ty[i]=ty;
    step_player(i,tx,ty);
}
static void ball_update(void) {
    if(fs_owner!=NOBODY) {
        fs_power_trail=0;
        uint8_t i=fs_owner;int16_t offset=fs_dribble_x[i]&&fs_dribble_y[i]?5*16:7*16;
        fs_ball_x=clamp(fs_pl_x[i]+fs_dribble_x[i]*offset,10*16,150*16);
        fs_ball_y=clamp(fs_pl_y[i]+fs_dribble_y[i]*offset,0,224*16);
        fs_ball_z=16;fs_ball_vz=0;fs_flight_left=0;
        fs_ball_vx=fs_vx[i];fs_ball_vy=fs_vy[i];
        if(fs_held_ticks<255) fs_held_ticks++;
        return;
    }
    fs_ball_x+=fs_ball_vx; fs_ball_y+=fs_ball_vy;
    if(fs_flight_left)fs_flight_left--;
    if(fs_ball_z>0||fs_ball_vz){
        fs_ball_z+=fs_ball_vz;fs_ball_vz-=2;
        if(fs_ball_z<=0){fs_ball_z=0;fs_ball_vz=(-fs_ball_vz)>>1;if(fs_ball_vz<8)fs_ball_vz=0;}
    }
    if(fs_ball_x<10*16 || fs_ball_x>150*16) { fs_ball_x=clamp(fs_ball_x,10*16,150*16); fs_ball_vx=-fs_ball_vx; }
    if(fs_ball_y<0 || fs_ball_y>224*16) {
        if(fs_ball_x>=56*16 && fs_ball_x<=104*16 && fs_ball_z<=14*16) {
            uint8_t s=fs_ball_y<0?0:1; score[s]++; fs_goal_timer=90;
            fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vx=fs_ball_vy=0;
            sfx_play(SFX_WHISTLE); fs_hud_pending=1; return;
        }
        fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vy=-fs_ball_vy;
    }
    if(!fs_ball_z&&!(fs_sim_ticks&3)) { if(fs_ball_vx) fs_ball_vx+=fs_ball_vx>0?-1:1; if(fs_ball_vy) fs_ball_vy+=fs_ball_vy>0?-1:1; }
    if(fs_pickup_lock) fs_pickup_lock--;
    if(fs_ball_z>8*16)return;
    // Rotate pickup priority to avoid a permanent advantage to blue.
    for(uint8_t k=0;k<4;k++) {
        uint8_t i=(k+(fs_sim_ticks&3))&3;
        if(fs_pickup_lock && fs_last_kicker==i) continue;
        if(fs_action[i]>10) continue;
        if(distance(i,fs_ball_x,fs_ball_y)<(fs_is_shot&&ab(fs_ball_vx)+ab(fs_ball_vy)>48?5*16:14*16)) {
            if(fs_receiver==i)fs_completed_passes++;
            fs_owner=i;fs_held_ticks=0;fs_receiver=NOBODY;fs_ball_z=16;fs_ball_vz=0;
            fs_ball_vx=fs_vx[i];fs_ball_vy=fs_vy[i];break;
        }
    }
}
static void draw(void) {
    static uint8_t by,bx,ball_sx,ball_wy,marker_x;
    static int16_t marker_y;
    by=(uint16_t)fs_ball_y>>4;bx=(uint16_t)fs_ball_x>>4;
    uint8_t row=fs_projection_y[by];
    fs_cam_y=row<=68?0:row>=100?32:row-68;scroll_y=fs_cam_y;
    ball_sx=sx(bx,by)+4;ball_wy=fs_projection_y[by]+12;
    // Assign nearer players lower OAM indexes so overlapping bodies sort by depth.
    static uint8_t order[4]={0,1,2,3};
    // Five comparisons, fixed indices: no nested loop/index bookkeeping.
#define DEPTH_SWAP(a,b) do { if(fs_pl_y[order[a]]<fs_pl_y[order[b]]){uint8_t t=order[a];order[a]=order[b];order[b]=t;} } while(0)
    DEPTH_SWAP(0,1);DEPTH_SWAP(2,3);DEPTH_SWAP(0,2);DEPTH_SWAP(1,3);DEPTH_SWAP(1,2);
    fs_ball_oam=0;
    for(uint8_t i=0;i<4;i++) if(fs_pl_y[i]>fs_ball_y) fs_ball_oam+=4;
    for(uint8_t slot=0;slot<4;slot++) {
        static uint8_t i,n,tile,pal,pose,y,x;
        static int16_t sy;
        i=order[slot];n=slot<<2;if(n>=fs_ball_oam)n++;
        y=(uint16_t)fs_pl_y[i]>>4;x=sx((uint16_t)fs_pl_x[i]>>4,y);sy=fs_projection_y[y]-fs_cam_y;
        if(!i){marker_x=x+4;marker_y=sy-18;}
        gfx_player_shadow(22+(i<<1),x,sy);
        if(fs_tackling[i]>4 && fs_tackling[i]<=14) {
            uint8_t base=20+i*24, flip=!fs_flip[i];
            gfx_actor(n,base,1,x,sy,i,flip);
            continue;
        }
        pose=fs_action[i]?3:(fs_moving[i]?(((fs_sim_ticks>>3)&1)?1:2):0);
        fs_pose[i]=pose;
        tile=(i<<6)+(fs_view[i]<<5)+(pose<<3);
        pal=S_BANK|i|(fs_flip[i]?S_FLIPX:0);
        if(fs_tackling[i]) { tile=20+i*24+8+(fs_view[i]<<3); pal &= ~S_BANK; }
        gfx_actor(n,tile,0,x,sy,pal,fs_flip[i]);
    }
    set_sprite_tile(fs_ball_oam,(fs_sim_ticks&8)?2:0); set_sprite_prop(fs_ball_oam,fs_power_trail?SPR_PAL_POWER:SPR_PAL_BALL);
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
        for(uint8_t k=0;k<4;k++) {
            uint8_t age=(k+1)<<1, entry=(fs_trail_i-age)&15;
            if(fs_trail_n>age) place(18+k,fs_trail_x[entry],fs_trail_y[entry]-fs_cam_y);
            else move_sprite(18+k,0,0);
        }
    }
    move_sprite(30,ball_sx,(uint8_t)(ball_wy-fs_cam_y+1));
    place(17,marker_x,marker_y);
}
void futsal_enter(void) BANKED {
    uint8_t buf[FUTSAL_PITCH_MAP_W*FUTSAL_PITCH_MAP_H];
    palettes_futsal();
    for(uint8_t k=0;k<4;k++){set_sprite_tile(18+k,8+(k<<1));set_sprite_prop(18+k,4);move_sprite(18+k,0,0);}
    set_sprite_tile(30,4);set_sprite_prop(30,SPR_PAL_UI);
    set_sprite_tile(17,6);set_sprite_prop(17,SPR_PAL_UI);
    for(uint8_t i=0;i<4;i++) gfx_player_shadow_init(22+(i<<1));
    banked_bkg_data(SCENE_TILES,FUTSAL_PITCH_TILE_COUNT,futsal_pitch_tiles,BANK(futsal_pitch));
    banked_sprite_data(0,SPRITES_TILE_COUNT,sprites_tiles,BANK(sprites));
    banked_sprite_data(20,ACTION_SPIKY_TILE_COUNT,action_spiky_tiles,BANK(action_spiky));
    banked_sprite_data(44,ACTION_CAP_TILE_COUNT,action_cap_tiles,BANK(action_cap));
    banked_sprite_data(68,ACTION_CROP_TILE_COUNT,action_crop_tiles,BANK(action_crop));
    banked_sprite_data(92,ACTION_BANDANA_TILE_COUNT,action_bandana_tiles,BANK(action_bandana));
    banked_sprite_data_vbk1(0,FUTSAL_SPIKY_TILE_COUNT,futsal_spiky_tiles,BANK(futsal_spiky));
    banked_sprite_data_vbk1(64,FUTSAL_CAP_TILE_COUNT,futsal_cap_tiles,BANK(futsal_cap));
    banked_sprite_data_vbk1(128,FUTSAL_CROP_TILE_COUNT,futsal_crop_tiles,BANK(futsal_crop));
    banked_sprite_data_vbk1(192,FUTSAL_BANDANA_TILE_COUNT,futsal_bandana_tiles,BANK(futsal_bandana));
    banked_memcpy(buf,futsal_pitch_map,sizeof(buf),BANK(futsal_pitch)); set_bkg_tiles(0,0,FUTSAL_PITCH_MAP_W,FUTSAL_PITCH_MAP_H,buf);
    banked_memcpy(buf,futsal_pitch_attr,sizeof(buf),BANK(futsal_pitch)); VBK_REG=1; set_bkg_tiles(0,0,FUTSAL_PITCH_MAP_W,FUTSAL_PITCH_MAP_H,buf); VBK_REG=0;
    for(int16_t y=0;y<=224;y++) {
        fs_projection_y[y]=32+((int32_t)92*y*448+(int32_t)72*y*y)/114688L;
        fs_projection_s[y]=111+17*y/224;
    }
    initrand(game_seed); score[0]=score[1]=sets[0]=sets[1]=0;
    match_sets=1; winner=NOBODY; fs_paused=fs_goal_timer=fs_hud_pending=0;
    fs_sim_ticks=fs_pass_count=fs_shot_count=fs_tackle_count=fs_completed_passes=0;
    move_win(7,HUD_WY); SHOW_WIN; gfx_sprite_split(HUD_WY);
    kickoff(0); hud(); draw();
}
void futsal_update(void) BANKED {
    if(joy_pressed&J_START) { fs_paused=!fs_paused; fs_hud_pending=0; hud(); return; }
    if(fs_paused) return;
    // Text VRAM writes get their own event frame instead of sharing a busy
    // physics/AI/actor frame. Match time and player momentum stay frozen here.
    if(fs_hud_pending) { fs_hud_pending=0; hud(); return; }
    if(joy_pressed&J_SELECT) { fs_spectator=!fs_spectator; hud(); return; }
    if(fs_goal_timer) {
        if(!--fs_goal_timer) {
            if(score[0]>=5 || score[1]>=5) { winner=score[0]>=5?0:1; state_change(STATE_GAME_OVER); return; }
            kickoff(fs_ball_y==0?1:0); fs_hud_pending=1;
        }
        draw(); return;
    }
    fs_sim_ticks++;
    if(fs_return_call)fs_return_call--;
    if(fs_owner!=NOBODY&&team(fs_owner))fs_return_call=0;
    for(uint8_t i=0;i<4;i++) {
        fs_moving[i]=0;
        if(fs_tackling[i]) fs_tackling[i]--;
        if(fs_cooldown[i]) fs_cooldown[i]--; if(fs_action[i]) fs_action[i]--; if(fs_burst[i]) fs_burst[i]--;
        if(i || fs_spectator) ai(i);
        else {
            int8_t dx=((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0),dy=((joy&J_DOWN)!=0)-((joy&J_UP)!=0);
            if(fs_owner==0) { if(joy_pressed&J_A) shoot(0,dx); else if(joy_pressed&J_B) pass(0,dx,dy); }
            else {
                if((joy_pressed&J_A) && !fs_cooldown[0]) tackle(0);
                if((joy_pressed&J_B)&&fs_owner==NOBODY&&fs_receiver==1)fs_return_call=48;
                else if((joy_pressed&J_B) && !fs_cooldown[0] && (fs_owner==NOBODY || team(fs_owner)!=0)) { fs_burst[0]=16; fs_cooldown[0]=60; }
            }
            step_player(0,fs_pl_x[0]+dx*64,fs_pl_y[0]+dy*64);
        }
    }
    ball_update(); if(!fs_hud_pending)draw();
}
