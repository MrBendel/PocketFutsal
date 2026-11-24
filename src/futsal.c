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
#include "side_roster.h"
#include "futsal_zone.h"
#include "futsal_fx.h"
#include "futsal_goals.h"
#if SCENE_TILES + FUTSAL_PITCH_TILE_COUNT > 214
#error Futsal court overlaps side sprite atlas
#endif
#if 214 + SIDE_ROSTER_TILE_COUNT > 242
#error Futsal side sprite atlas overlaps contact effects
#endif



uint8_t fs_pace,fs_pace_phase,fs_pace_mask;
uint8_t fs_input_pending,fs_input_pad,fs_pressed,fs_pressed_pad;
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
uint8_t fs_shot_range;
int8_t fs_shot_error;
uint8_t fs_return_call;
uint8_t fs_ball_fall,fs_kicked;
uint8_t fs_touch_timer,fs_pending_kick,fs_pending_timer;
uint8_t fs_pass_charge,fs_pass_source,fs_return_lob,fs_lob,fs_air_shot;
int8_t fs_pass_dx,fs_pass_dy;
uint8_t fs_aerial[4],fs_air_hit[4],fs_air_kind[4],fs_zone_ready;
uint16_t fs_lob_count,fs_aerial_count;
int8_t fs_pending_dx,fs_pending_dy;
uint16_t fs_dribble_touches;
uint8_t fs_cooldown[4], fs_action[4], fs_burst[4], fs_held_ticks, fs_pickup_lock, fs_last_kicker;
uint8_t fs_tackling[4];
uint8_t fs_moving[4], fs_view[4], fs_flip[4], fs_pose[4];
uint8_t fs_side[4];
uint8_t fs_role[4]; // 0 chase, 1 carry, 2 support, 3 cover
uint8_t fs_kickoff_phase,fs_thought;
uint8_t fs_presser[2]; // One challenger per team; partner protects the outlet.
int8_t fs_cut_dir;
uint8_t fs_cut_window,fs_cut_count,fs_cut_cooldown,fs_stumble_player,fs_stumble_timer;
uint16_t fs_ankle_count;
uint8_t fs_ball_oam, fs_hud_pending,fs_goal_refresh;
uint8_t fs_power_trail, fs_trail_n, fs_trail_i;
uint8_t fs_trail_x[16], fs_trail_y[16];
uint8_t fs_projection_y[225], fs_projection_s[225];

static int16_t ab(int16_t n) { return n < 0 ? -n : n; }
static int16_t clamp(int16_t n, int16_t lo, int16_t hi) { return n < lo ? lo : n > hi ? hi : n; }
#define team(i) ((uint8_t)((i) >> 1))
// SDCC ABI: slot in A, x in DE, y above the return address on the stack;
// return Manhattan distance in BC and remove the stacked y argument.
static uint16_t distance(uint8_t i,int16_t x,int16_t y) __naked {
    i;x;y;
    __asm
        add a,a
        ld c,a
        ld b,#0
        ld hl,#_fs_pl_x
        add hl,bc
        ld a,(hl+)
        sub e
        ld e,a
        ld a,(hl)
        sbc d
        ld d,a
        bit 7,d
        jr z,02000$
        xor a
        sub e
        ld e,a
        sbc a,a
        sub d
        ld d,a
02000$:
        push de
        ld hl,#_fs_pl_y
        add hl,bc
        ld a,(hl+)
        ld b,(hl)
        ld c,a
        ldhl sp,#4
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld a,c
        sub e
        ld c,a
        ld a,b
        sbc d
        ld b,a
        bit 7,b
        jr z,02001$
        xor a
        sub c
        ld c,a
        sbc a,a
        sub b
        ld b,a
02001$:
        pop de
        ld h,b
        ld l,c
        add hl,de
        ld b,h
        ld c,l
        pop hl
        pop af
        jp (hl)
    __endasm;
}
// SDCC passes x in A, y in E and returns a byte in A. The court scale
// is 111..128: multiplying its 0..17 complement needs at most five bits.
// Keep the original signed rounding exactly (covered across all 18 scales).
static uint8_t sx(uint8_t x,uint8_t y) __naked {
    x;y;
    __asm
        ld c,a
        ld d,#0
        ld hl,#_fs_projection_s
        add hl,de
        ld a,#128
        sub (hl)
        push af
        ld a,c
        sub #80
        jr nc,01000$
        cpl
        inc a
01000$:
        ld e,a
        ld d,#0
        pop af
        ld hl,#0
        or a
        jr z,01004$
01001$:
        srl a
        jr nc,01002$
        add hl,de
01002$:
        sla e
        rl d
        or a
        jr nz,01001$
        ld a,c
        cp #80
        jr c,01003$
        ld de,#127
        add hl,de
        add hl,hl
        ld a,c
        sub h
        ret
01003$:
        add hl,hl
        ld a,c
        add h
        ret
01004$:
        ld a,c
        ret
    __endasm;
}
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
    else if (fs_frame_timer) win_text(6,0,"FRAME!");
}
static void kickoff(uint8_t side) {
    futsal_goals_reset();fs_goal_refresh=1;
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
    fs_ball_fall=0;
    fs_touch_timer=fs_pending_kick=fs_pending_timer=0;
    fs_pass_charge=fs_return_lob=fs_lob=fs_air_shot=fs_zone_ready=0;
    // A fixed slot-based schedule replayed the same successful red press at
    // every restart. Stagger perception evenly, with a seeded phase per kickoff.
    fs_kickoff_phase=rand()&7;
    fs_presser[0]=0;fs_presser[1]=2;
    fs_cut_dir=0;fs_cut_window=fs_cut_count=fs_cut_cooldown=fs_stumble_timer=0;
    fs_stumble_player=NOBODY;
    for(uint8_t i=0;i<4;i++) {
        fs_tackling[i]=fs_cooldown[i]=fs_action[i]=fs_burst[i]=fs_moving[i]=fs_flip[i]=0;
        fs_view[i]=team(i)?0:1;
        fs_side[i]=0;
        fs_aerial[i]=fs_air_hit[i]=fs_air_kind[i]=0;
        fs_vx[i]=fs_vy[i]=fs_slide_vx[i]=fs_slide_vy[i]=0;
        fs_dribble_x[i]=0; fs_dribble_y[i]=team(i)?1:-1;
        fs_think[i]=((i<<1)+fs_kickoff_phase)&7;
        fs_ai_tx[i]=fs_pl_x[i];fs_ai_ty[i]=fs_pl_y[i];
    }
}
// Futnet's launch model: a precise vector at a chosen court speed, then an arc
// calculated from travel time. No direction table and no fixed-duration flight.
static void kick(uint8_t i, int16_t tx, int16_t ty) {
    static int16_t dx,dy,sum,t;
    static uint8_t speed;
    // Passes clamp their destinations before launch; shot targets are already
    // on the court. Avoid three software clamp calls on busy shot frames.
    dx=tx-fs_ball_x;dy=ty-fs_ball_y;
    if(fs_lob){
        fs_ball_vx=dx<0?-((-dx)>>6):dx>>6;
        fs_ball_vy=dy<0?-((-dy)>>6):dy>>6;
        t=64;fs_ball_z=0;fs_ball_vz=48;
    }else{
    dx=dx<0?-((-dx)>>4):dx>>4;dy=dy<0?-((-dy)>>4):dy>>4;
    sum=ab(dx)+ab(dy);if(!sum)sum=1;
    speed=fs_is_shot?128:64;
    fs_ball_vx=(fs_is_shot?((uint16_t)ab(dx)<<7):((uint16_t)ab(dx)<<6))/(uint16_t)sum;
    if(dx<0)fs_ball_vx=-fs_ball_vx;
    fs_ball_vy=(dx||dy)?speed-ab(fs_ball_vx):0;if(dy<0)fs_ball_vy=-fs_ball_vy;
    // Speeds are powers of two: distance / speed is a cheap shift. As in
    // Futnet, long deliveries get more air time and short exchanges stay low.
    t=fs_is_shot?(sum+4)>>3:(sum+2)>>2;
    if(t<4)t=4; // Court bounds keep even the longest quick pass below 96.
    fs_ball_vz=(3*(t+1))>>3;
    if(fs_air_shot)fs_ball_vz-=fs_ball_z/t;else fs_ball_z=0;
    }
    fs_target_x=tx;fs_target_y=ty;fs_flight_left=t;fs_ball_fall=0;
    fs_air_shot=0;
    fs_kicked=1;
    if(dx)fs_flip[i]=dx<0;if(dy)fs_view[i]=dy<0;
    fs_power_trail=fs_trail_n=0;
    shadow_OAM[18].y=0;shadow_OAM[19].y=0;shadow_OAM[20].y=0;
    fs_owner=NOBODY; fs_last_kicker=i; fs_pickup_lock=10; fs_held_ticks=0; fs_action[i]=10;
    fs_touch_timer=0;
    sfx_play(SFX_TOUCH);
}
static void pass(uint8_t i, int8_t dx, int8_t dy,uint8_t lob) {
    static uint8_t mate;
    static int16_t tx,ty;
    mate=i^1;
    fs_receiver=mate;fs_is_shot=0;fs_lob=lob;
    // A direction sends the receiver into space; neutral leads their current run.
    tx=fs_pl_x[mate];ty=fs_pl_y[mate];
    if(dx||dy){tx+=dx*24*16;ty+=dy*24*16;}
    else {tx+=fs_vx[mate]*32;ty+=fs_vy[mate]*32;}
    // A called lob gives the fixed human player a forward finishing zone.
    if(lob&&i==1&&!fs_spectator)ty-=16*16;
    kick(i,clamp(tx,14*16,146*16),clamp(ty,16*16,208*16));
    // React to the delivery immediately rather than following a stale support run.
    fs_ai_tx[mate]=fs_target_x;fs_ai_ty[mate]=fs_target_y;
    if(i==1)fs_return_call=fs_return_lob=0;
    fs_pass_count++;
    if(lob)fs_lob_count++;
}
static void shoot(uint8_t i, int8_t aim) {
    static int16_t x;
    static uint8_t roll;
    fs_shot_range=team(i)?224-(fs_ball_y>>4):(fs_ball_y>>4);
    x=80+aim*12;if(!aim)x=(rand()&1)?70:90;
    roll=rand();
    if(fs_shot_range>144){
        // Full-court efforts are wild clearances, not goal-seeking lasers.
        x=(roll&1)?128+(roll&7):32-(roll&7);fs_shot_error=x-80;
    }else{
        if(fs_shot_range>96)fs_shot_error=(roll&15)-(roll>>4);
        else if(fs_shot_range>48)fs_shot_error=(roll&7)-((roll>>3)&7);
        else fs_shot_error=(roll&1)-((roll>>1)&1);
        x+=fs_shot_error;
    }
    fs_receiver=NOBODY;fs_is_shot=1;fs_lob=0;
    kick(i,x*16,team(i)?224*16:0);fs_power_trail=48;fs_shot_count++;
    futsal_fx_start(fs_ball_x,fs_ball_y,fs_ball_z,fs_aerial[i]!=0);
}
static uint8_t aerial_try(uint8_t i){
    if(!fs_zone_ready||fs_owner!=NOBODY||fs_aerial[i]||fs_tackling[i]||distance(i,fs_target_x,fs_target_y)>20*16)return 0;
    fs_aerial[i]=fs_action[i]=28;fs_cooldown[i]=44;fs_air_hit[i]=0;
    fs_air_kind[i]=team(i)?fs_ball_y<fs_pl_y[i]-4*16:fs_ball_y>fs_pl_y[i]+4*16;
    fs_vx[i]=fs_vy[i]=0;fs_burst[i]=0;
    return 1;
}
static void aerial_update(uint8_t i){
    uint8_t t=--fs_aerial[i];
    if(t<=20&&t>=10&&!fs_air_hit[i]&&!fs_kicked&&fs_owner==NOBODY&&fs_ball_vz<0&&fs_ball_z>=8*16&&fs_ball_z<=44*16&&distance(i,fs_ball_x,fs_ball_y)<20*16){
        fs_air_hit[i]=1;fs_air_shot=1;
        if(fs_receiver==i)fs_completed_passes++;
        shoot(i,(!fs_spectator&&!i)?((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0):0);
        fs_aerial_count++;
    }
}
static void pass_input(int8_t dx,int8_t dy){
    if(fs_pressed&J_B){
        if(fs_owner==0||fs_owner==1||(fs_owner==NOBODY&&fs_receiver==1)){
            fs_pass_charge=1;fs_pass_source=fs_owner==0?0:1;
            fs_pass_dx=((fs_pressed_pad&J_RIGHT)!=0)-((fs_pressed_pad&J_LEFT)!=0);
            fs_pass_dy=((fs_pressed_pad&J_DOWN)!=0)-((fs_pressed_pad&J_UP)!=0);fs_pending_kick=0;
        }else if(!fs_cooldown[0]){fs_burst[0]=16;fs_cooldown[0]=60;}
    }else if(fs_pass_charge&&(joy&J_B)){
        if(fs_pass_charge<30)fs_pass_charge++;
        if(dx||dy){fs_pass_dx=dx;fs_pass_dy=dy;}
    }
    if(fs_pass_charge&&fs_pass_source==0&&fs_owner!=0)fs_pass_charge=0;
    if(fs_pass_charge&&fs_owner!=NOBODY&&team(fs_owner))fs_pass_charge=0;
    if(fs_pass_charge&&!(joy&J_B)){
        uint8_t lob=fs_pass_charge>=12;
        if(!fs_pass_source){
            fs_pending_kick=lob?3:1;fs_pending_timer=20;
            fs_pending_dx=fs_pass_dx;fs_pending_dy=fs_pass_dy;
        }else{fs_return_call=lob?80:48;fs_return_lob=lob;}
        fs_pass_charge=0;
    }
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
    static int8_t vx,vy,want_x,want_y,speed,side_delta;
    static uint8_t slide,burst,ax,ay,mx,my;
    x=fs_pl_x[i];y=fs_pl_y[i];dx=tx-x;dy=ty-y;
    vx=fs_vx[i];vy=fs_vy[i];want_x=want_y=0;
    slide=fs_tackling[i];burst=fs_burst[i];
    speed=fs_owner==i?22:24;if(burst)speed=44;
    if(!fs_spectator&&!i&&fs_pass_charge&&fs_pass_source==0)speed=12;
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
    mx=vx<0?-vx:vx;my=vy<0?-vy:vy;
    // A little hysteresis avoids flickering views on nearly diagonal runs.
    side_delta=mx-my;
    if(side_delta>3)fs_side[i]=1;else if(side_delta<-3)fs_side[i]=0;
    if(vx<-3)fs_flip[i]=1;else if(vx>3)fs_flip[i]=0;
    if(vy<-3)fs_view[i]=1;else if(vy>3)fs_view[i]=0;
    if(vx||vy){fs_dribble_x[i]=vx<0?-1:vx>0?1:0;fs_dribble_y[i]=vy<0?-1:vy>0?1:0;}
}
// Two genuine lateral reversals within half a second sell a crossover.
// Only the nearby presser, already leaning the wrong way, can be wrong-footed.
static void crossover(int8_t dx){
    static uint8_t defender;
    static int8_t vx;
    if(fs_owner!=0||fs_cut_cooldown||fs_ball_z>3*16||fs_tackling[0]||(joy&(J_A|J_B))){
        fs_cut_dir=0;fs_cut_count=fs_cut_window=0;return;
    }
    if(!dx)return;
    vx=fs_vx[0];
    if(!fs_cut_dir){if((dx<0&&vx<=-8)||(dx>0&&vx>=8))fs_cut_dir=dx;return;}
    if(dx==fs_cut_dir)return;
    // A second button flick before the body has changed direction is ignored.
    if((fs_cut_dir<0&&vx>-8)||(fs_cut_dir>0&&vx<8))return;
    fs_cut_dir=dx;fs_cut_window=28;
    if(++fs_cut_count<2)return;
    fs_cut_count=0;
    defender=fs_presser[1];
    if(fs_stumble_timer||fs_tackling[defender]||fs_aerial[defender]||distance(0,fs_ball_x,fs_ball_y)>20*16||distance(defender,fs_pl_x[0],fs_pl_y[0])>28*16)return;
    vx=fs_vx[defender];
    if((dx<0&&vx<6)||(dx>0&&vx>-6))return;
    fs_stumble_player=defender;fs_stumble_timer=22;
    fs_action[defender]=34;fs_burst[defender]=0;
    fs_cut_cooldown=90;fs_ankle_count++;
    sfx_play(SFX_PARRY);
}
static void ai(uint8_t i){
    if(fs_stumble_timer&&i==fs_stumble_player){step_player(i,fs_pl_x[i],fs_pl_y[i]);return;}

    static uint8_t mate,own,side,mark,presser,defender;
    static int16_t goal,tx,ty,pressure,mate_space,d,forward;
    if(!fs_spectator&&i==1&&fs_owner==i&&fs_return_call){
        if(distance(i,fs_ball_x,fs_ball_y)<=12*16)pass(i,0,0,fs_return_lob);
        else step_player(i,fs_ball_x,fs_ball_y);
        return;
    }
    if(!fs_spectator&&i==1&&fs_owner==i&&fs_pass_charge&&fs_pass_source==1){step_player(i,fs_ball_x,fs_ball_y);return;}
    if(fs_think[i]||fs_kicked||fs_thought){if(fs_think[i])fs_think[i]--;step_player(i,fs_ai_tx[i],fs_ai_ty[i]);return;}
    fs_thought=1;
    fs_think[i]=7;
    // Cached movement frames do not need to rebuild the tactical scratch.
    mate=i^1;side=team(i);own=fs_owner!=NOBODY&&team(fs_owner)==side;
    goal=side?224*16:0;tx=fs_ball_x;ty=fs_ball_y;
    if(fs_lob&&fs_receiver==i&&aerial_try(i))return;
    fs_role[i]=0;
    if(fs_owner==i){
        fs_role[i]=1;
        pressure=mate_space=30000;defender=0;
        for(uint8_t j=0;j<4;j++)if(team(j)!=team(i)){
            d=distance(j,fs_ball_x,fs_ball_y);
            if(d<pressure){pressure=d;defender=j;}
            d=distance(j,fs_pl_x[mate],fs_pl_y[mate]);if(d<mate_space)mate_space=d;
        }
        tx=(fs_pl_x[i]<80*16?58:102)*16;ty=goal;
        // Attack a side of the press instead of driving into its centre.
        if(pressure<64*16&&ab(fs_pl_x[defender]-fs_pl_x[i])<40*16)tx=fs_pl_x[defender]<80*16?120*16:40*16;
        forward=side?fs_pl_y[mate]-fs_pl_y[i]:fs_pl_y[i]-fs_pl_y[mate];
        if(distance(i,fs_ball_x,fs_ball_y)>12*16){tx=fs_ball_x;ty=fs_ball_y;}
        else if(fs_held_ticks>10&&ab(fs_pl_y[i]-goal)<48*16&&pressure>14*16)shoot(i,0);
        else if(fs_held_ticks>12&&mate_space>18*16&&forward>-32*16&&distance(mate,fs_ball_x,fs_ball_y)>24*16&&(pressure<48*16||fs_held_ticks>100))pass(i,0,0,(fs_pass_count&3)==3);
        else if(fs_held_ticks>18&&ab(fs_pl_y[i]-goal)<64*16)shoot(i,0);
    }else if(own){
        fs_role[i]=2;
        // Keep the far lane through the middle of the court instead of
        // crossing behind the carrier every time they cross the centre line.
        tx=fs_pl_x[i]<80*16?32*16:128*16;
        if(fs_pl_x[fs_owner]<64*16)tx=128*16;
        else if(fs_pl_x[fs_owner]>96*16)tx=32*16;
        ty=clamp(fs_pl_y[fs_owner]+(side?24*16:-24*16),24*16,200*16);
    }else if(fs_owner==NOBODY&&fs_receiver==i){
        fs_role[i]=2;tx=fs_flight_left?fs_target_x:fs_ball_x;ty=fs_flight_left?fs_target_y:fs_ball_y;
    }else if(fs_owner==NOBODY&&fs_receiver!=NOBODY&&team(fs_receiver)==team(i)){
        // The passer continues a forward run while their delivery is airborne.
        fs_role[i]=2;tx=fs_target_x<80*16?120*16:40*16;
        ty=clamp(fs_target_y+(team(i)?32*16:-32*16),24*16,200*16);
    }else{
        // A press belongs to one player. Only hand it over when the partner
        // is clearly closer; tiny distance changes must not pull both in.
        presser=fs_presser[side];
        if(!fs_tackling[presser]&&distance(presser^1,fs_ball_x,fs_ball_y)+24*16<distance(presser,fs_ball_x,fs_ball_y))
            presser=fs_presser[side]=presser^1;
        if(i!=presser){
            fs_role[i]=3;
            // Cover the other attacker from the goal side, not the ball.
            // On a loose ball protect a separate lane behind the challenger.
            mark=fs_owner!=NOBODY?fs_owner^1:fs_receiver;
            if(mark!=NOBODY){
                tx=fs_pl_x[mark];
                if(tx<28*16)tx=28*16;else if(tx>132*16)tx=132*16;
                ty=fs_pl_y[mark]+(side?-24*16:24*16);
            }else{
                tx=fs_ball_x<80*16?104*16:56*16;
                ty=fs_ball_y+(side?-40*16:40*16);
            }
            // Bound once after selecting cover; avoid software clamp calls
            // on busy four-player movement frames.
            if(ty<24*16)ty=24*16;else if(ty>200*16)ty=200*16;
            // If attackers bunch up, retain width rather than joining them.
            if(ab(tx-fs_ball_x)<28*16&&ab(ty-fs_ball_y)<32*16)
                tx=fs_ball_x<80*16?128*16:32*16;
        }else{
            tx=fs_ball_x+fs_ball_vx*6;ty=fs_ball_y+fs_ball_vy*6;
            if(tx<16*16)tx=16*16;else if(tx>144*16)tx=144*16;
            if(ty<12*16)ty=12*16;else if(ty>212*16)ty=212*16;
            if(fs_owner!=NOBODY&&!fs_cooldown[i]&&fs_ball_z<=6*16&&distance(i,fs_ball_x,fs_ball_y)<16*16)tackle(i);
        }
    }
    fs_ai_tx[i]=tx;fs_ai_ty[i]=ty;
    // A one-frame foot plant on release also keeps launch division separate
    // from this player's acceleration/tackle work on the Game Boy CPU.
    if(!fs_kicked)step_player(i,tx,ty);
}
static void ball_update(void) {
    static uint8_t i,k,dribbling,reach;
    static int16_t gap,dx,old_x,old_y,old_z;
    static uint8_t contact;
    static int8_t vx,vy;
    dribbling=fs_owner!=NOBODY;
    if(fs_owner!=NOBODY) {
        fs_power_trail=0;
        i=fs_owner;fs_flight_left=0;fs_is_shot=0;
        if(fs_held_ticks<255) fs_held_ticks++;
        if(fs_touch_timer)fs_touch_timer--;
        gap=distance(i,fs_ball_x,fs_ball_y);
        if(gap>24*16){
            // Control is an association, not a tether. A hard cut can leave
            // the rolling ball behind, where either team can collect it.
            fs_owner=NOBODY;fs_last_kicker=i;fs_pickup_lock=4;
            fs_receiver=NOBODY;fs_pending_kick=0;
        }else if(!fs_touch_timer&&gap<9*16&&fs_ball_z<=3*16){
            vx=fs_vx[i];vy=fs_vy[i];
            if(vx||vy){
                fs_ball_vx=vx+(vx<0?-16:vx>0?16:0);
                fs_ball_vy=vy+(vy<0?-16:vy>0?16:0);
                fs_ball_z=0;fs_ball_vz=6;fs_ball_fall=0;
                fs_touch_timer=10;fs_dribble_touches++;
            }
        }
        // The ball rolls between discrete touches. It keeps its old direction
        // through a turn and slows independently when the player stops.
        if(fs_ball_vx>2)fs_ball_vx-=2;else if(fs_ball_vx<-2)fs_ball_vx+=2;else fs_ball_vx=0;
        if(fs_ball_vy>2)fs_ball_vy-=2;else if(fs_ball_vy<-2)fs_ball_vy+=2;else fs_ball_vy=0;
    }
    old_x=fs_ball_x;old_y=fs_ball_y;old_z=fs_ball_z;
    fs_ball_x+=fs_ball_vx; fs_ball_y+=fs_ball_vy;
    if(fs_flight_left&&!--fs_flight_left&&!fs_is_shot){
        // A delivery sheds pace as it lands instead of rolling through a
        // missed receiver and carrying all the way into the goal.
        fs_ball_vx/=4;fs_ball_vy/=4;
    }
    if(fs_ball_z>0||fs_ball_vz){
        if(fs_lob)fs_ball_vz-=(++fs_ball_fall&1)?1:2;
        else if(++fs_ball_fall&3)fs_ball_vz--;
        fs_ball_z+=fs_ball_vz;
        if(fs_ball_z<=0){
            fs_ball_z=0;fs_ball_vz=(-fs_ball_vz*3)>>2;
            fs_lob=0;
            if(fs_ball_vz<5)fs_ball_vz=0;
            fs_ball_vx-=fs_ball_vx/4;fs_ball_vy-=fs_ball_vy/4;
        }
    }
    if(fs_ball_x<10*16 || fs_ball_x>150*16) { fs_ball_x=clamp(fs_ball_x,10*16,150*16); fs_ball_vx=-(fs_ball_vx-fs_ball_vx/4); }
    if(fs_ball_y<0 || fs_ball_y>224*16) {
        contact=futsal_goals_contact(old_x,old_y,old_z);
        if(contact==2){fs_power_trail=0;fs_hud_pending=1;return;}
        if(contact==1) {
            uint8_t s=fs_ball_y<0?0:1; score[s]++; fs_goal_timer=90;
            fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vx=fs_ball_vy=0;
            sfx_play(SFX_WHISTLE); fs_hud_pending=1; return;
        }
        fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vy=-(fs_ball_vy-fs_ball_vy/4);
    }
    if(!dribbling&&!fs_ball_z&&(!fs_is_shot||!(fs_sim_ticks&3))) { if(fs_ball_vx) fs_ball_vx+=fs_ball_vx>0?-1:1; if(fs_ball_vy) fs_ball_vy+=fs_ball_vy>0?-1:1; }
    if(fs_pickup_lock) fs_pickup_lock--;
    if(fs_ball_z>8*16||fs_kicked)return;
    if(fs_owner!=NOBODY){
        // Check the two opposing feet on alternating frames. Teammates do not
        // steal a controlled dribble; opponents can intercept its exposed ball.
        i=((fs_owner&2)^2)+(fs_sim_ticks&1);
        if(fs_action[i]<=10&&distance(i,fs_ball_x,fs_ball_y)<6*16){
            fs_owner=i;fs_held_ticks=0;fs_touch_timer=6;fs_pending_kick=0;
        }
        return;
    }
    // Rotate pickup priority to avoid a permanent advantage to blue.
    reach=fs_is_shot&&ab(fs_ball_vx)+ab(fs_ball_vy)>48?5*16:14*16;
    for(k=0;k<4;k++) {
        i=(k+(fs_sim_ticks&3))&3;
        if(fs_pickup_lock && fs_last_kicker==i) continue;
        if(fs_action[i]>10||fs_aerial[i]) continue;
        // Reject the far axis first and compute shot reach once, not four
        // times. This is the same Manhattan contact test without stack calls.
        gap=fs_pl_y[i]-fs_ball_y;if(gap<0)gap=-gap;
        if((uint16_t)gap>=reach)continue;
        dx=fs_pl_x[i]-fs_ball_x;if(dx<0)dx=-dx;
        if((uint16_t)(gap+dx)<reach) {
            if(fs_receiver==i)fs_completed_passes++;
            fs_owner=i;fs_held_ticks=0;fs_receiver=NOBODY;fs_touch_timer=0;fs_lob=0;
            // A trap kills the delivery's speed without moving its position.
            fs_ball_vx=fs_ball_vy=0;fs_ball_vz=0;fs_ball_z=0;break;
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
        move_sprite(21+i,x+4,sy+12);
        if(fs_aerial[i]){
            uint8_t t=fs_aerial[i];
            tile=(i<<6)+(fs_view[i]<<5)+24;pal=S_BANK|i;
            if(t>24||t<5){tile=28+i*24+(fs_view[i]<<3);pal=i;}
            else if(t<=20&&t>=10&&fs_air_kind[i]){gfx_bicycle(n,tile,x,sy-12,pal,fs_flip[i]);continue;}
            else sy-=t>=10?10:5;
            gfx_actor(n,tile,0,x,sy,pal,fs_flip[i]);continue;
        }
        if(fs_stumble_timer&&i==fs_stumble_player){
            // Reuse this character's connected crouch/head, then stand up.
            if(fs_stumble_timer>8)gfx_actor(n,28+i*24+(fs_view[i]<<3),0,x,sy,i,fs_flip[i]);
            else gfx_side(n,i,0,x,(uint8_t)sy,fs_flip[i]);
            continue;
        }
        if(!i&&fs_pass_charge&&fs_pass_source==0&&fs_pass_charge>4){
            pal=fs_pass_charge>=12&&(fs_sim_ticks&4)?SPR_PAL_POWER:0;
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
            if(fs_trail_n>age) place(18+k,fs_trail_x[entry],fs_trail_y[entry]-fs_cam_y);
            else move_sprite(18+k,0,0);
        }
    }
    move_sprite(25,ball_sx,(uint8_t)(ball_wy-fs_cam_y+1));
    place(17,marker_x,marker_y);
    if(fs_lob&&fs_owner==NOBODY&&!fs_goal_timer){
        uint8_t zy=(uint16_t)fs_target_y>>4,zx=sx((uint16_t)fs_target_x>>4,zy);
        uint8_t pal=fs_zone_ready&&(fs_sim_ticks&4)?SPR_PAL_POWER:SPR_PAL_UI;
        set_sprite_prop(26,pal);set_sprite_prop(27,pal);
        move_sprite(26,zx,fs_projection_y[zy]-fs_cam_y+12);
        move_sprite(27,zx+8,fs_projection_y[zy]-fs_cam_y+12);
    }else{move_sprite(26,0,0);move_sprite(27,0,0);}
    if(fs_fx_timer||fs_fx_frame!=255)futsal_fx_draw(fs_cam_y);
    if(fs_goal_refresh){
        fs_goal_refresh=0;futsal_goals_draw(fs_cam_y);
    }
}
void futsal_enter(void) BANKED {
    uint8_t buf[FUTSAL_PITCH_MAP_W*FUTSAL_PITCH_MAP_H];
    palettes_futsal();
    gfx_ball_init(116);
    futsal_fx_load();
    futsal_goals_load();
    banked_sprite_data(124,FUTSAL_ZONE_TILE_COUNT,futsal_zone_tiles,BANK(futsal_zone));
    set_sprite_tile(26,124);set_sprite_tile(27,126);
    for(uint8_t k=0;k<3;k++){set_sprite_tile(18+k,8+(k<<1));set_sprite_prop(18+k,4);move_sprite(18+k,0,0);}
    set_sprite_tile(25,4);set_sprite_prop(25,SPR_PAL_UI);
    set_sprite_tile(17,6);set_sprite_prop(17,SPR_PAL_UI);
    for(uint8_t i=0;i<4;i++){set_sprite_tile(21+i,254);set_sprite_prop(21+i,SPR_PAL_UI);}
    banked_bkg_data(SCENE_TILES,FUTSAL_PITCH_TILE_COUNT,futsal_pitch_tiles,BANK(futsal_pitch));
    banked_sprite_data(0,SPRITES_TILE_COUNT,sprites_tiles,BANK(sprites));
    banked_sprite_data(20,ACTION_SPIKY_TILE_COUNT,action_spiky_tiles,BANK(action_spiky));
    banked_sprite_data(44,ACTION_CAP_TILE_COUNT,action_cap_tiles,BANK(action_cap));
    banked_sprite_data(68,ACTION_CROP_TILE_COUNT,action_crop_tiles,BANK(action_crop));
    banked_sprite_data(92,ACTION_BANDANA_TILE_COUNT,action_bandana_tiles,BANK(action_bandana));
    banked_sprite_data(214,SIDE_ROSTER_TILE_COUNT,side_roster_tiles,BANK(side_roster));
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
    fs_sim_ticks=fs_pass_count=fs_shot_count=fs_tackle_count=fs_completed_passes=fs_dribble_touches=fs_ankle_count=0;
    fs_lob_count=fs_aerial_count=0;
    if(fs_pace>2)fs_pace=0;
    fs_pace_mask=fs_pace?7:3;
    fs_pace_phase=fs_input_pending=0;
    move_win(7,HUD_WY); SHOW_WIN; gfx_sprite_split(HUD_WY);
    kickoff(0); hud(); draw();
}
void futsal_update(void) BANKED {
    static uint8_t k,i;
    static int8_t dx,dy;
    if(joy_pressed&J_START) { fs_paused=!fs_paused; fs_hud_pending=fs_input_pending=0; hud(); return; }
    if(fs_paused) return;
    // Text VRAM writes get their own event frame instead of sharing a busy
    // physics/AI/actor frame. Match time and player momentum stay frozen here.
    if(fs_hud_pending) { fs_hud_pending=0; hud(); return; }
    if(joy_pressed&J_SELECT) { fs_spectator=!fs_spectator;fs_pass_charge=fs_pending_kick=fs_return_call=fs_cut_count=fs_cut_window=fs_input_pending=0;fs_cut_dir=0;hud();return; }
    if(fs_frame_timer){
        futsal_goals_step();
        futsal_goals_draw(fs_cam_y);
        if(!fs_frame_timer){
            kickoff(fs_frame_goal^1);futsal_goals_draw(fs_cam_y);fs_goal_refresh=0;fs_hud_pending=1;
        }
        draw();return;
    }
    if(fs_goal_timer) {
        if(!--fs_goal_timer) {
            if(score[0]>=5 || score[1]>=5) { winner=score[0]>=5?0:1; state_change(STATE_GAME_OVER); return; }
            kickoff(fs_ball_y==0?1:0); fs_hud_pending=1;
        }
        draw(); return;
    }
    // Pace the whole simulation, preserving movement, arcs and animation
    // relationships. Presentation and immediate pause controls still run at 60Hz.
    if(fs_pace<2){
        if(joy_pressed&(J_A|J_B)){
            fs_input_pending|=joy_pressed&(J_A|J_B);fs_input_pad=joy;
        }
        fs_pace_phase=(fs_pace_phase+1)&7;
        if(!(fs_pace_phase&fs_pace_mask))return;
    }
    fs_sim_ticks++;
    fs_kicked=fs_thought=0;
    if(fs_stumble_timer)fs_stumble_timer--;
    if(fs_cut_cooldown)fs_cut_cooldown--;
    if(fs_cut_window)fs_cut_window--;else fs_cut_count=0;
    fs_zone_ready=fs_lob&&fs_ball_vz<0&&fs_flight_left<=28&&fs_ball_z>=10*16;
    if(fs_pending_timer)fs_pending_timer--;else fs_pending_kick=0;
    if(fs_return_call)fs_return_call--;
    if(fs_owner!=NOBODY&&team(fs_owner))fs_return_call=0;
    // Alternate team order: the last team to update must not always win a
    // contested touch now that the dribbled ball has its own position.
    for(k=0;k<4;k++) {
        i=k^(fs_sim_ticks&2);
        fs_moving[i]=0;
        if(fs_tackling[i]) fs_tackling[i]--;
        if(fs_cooldown[i]) fs_cooldown[i]--; if(fs_action[i]) fs_action[i]--; if(fs_burst[i]) fs_burst[i]--;
        if(fs_aerial[i]){aerial_update(i);continue;}
        if(i || fs_spectator) ai(i);
        else {
            dx=((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0);dy=((joy&J_DOWN)!=0)-((joy&J_UP)!=0);
            // A quick tap on a paced-out frame is delivered on the next
            // simulation tick, with the aim that accompanied the press.
            fs_pressed=joy_pressed|fs_input_pending;
            fs_pressed_pad=fs_input_pending?fs_input_pad:joy;
            fs_input_pending=0;
            crossover(dx);
            pass_input(dx,dy);
            if(fs_owner==0) {
                if(fs_pressed&J_A){
                    fs_pending_kick=2;fs_pending_timer=20;fs_pass_charge=0;
                    fs_pending_dx=((fs_pressed_pad&J_RIGHT)!=0)-((fs_pressed_pad&J_LEFT)!=0);
                    fs_pending_dy=((fs_pressed_pad&J_DOWN)!=0)-((fs_pressed_pad&J_UP)!=0);
                }
                if(fs_pending_kick&&distance(0,fs_ball_x,fs_ball_y)<=12*16){
                    if(fs_pending_kick==2)shoot(0,fs_pending_dx);else pass(0,fs_pending_dx,fs_pending_dy,fs_pending_kick==3);
                    fs_pending_kick=0;
                }
            }
            else {
                fs_pending_kick=0;
                if(fs_pressed&J_A){if(fs_lob)aerial_try(0);else if(!fs_cooldown[0])tackle(0);}
            }
            if(!fs_kicked&&!fs_aerial[0])step_player(0,fs_pl_x[0]+dx*64,fs_pl_y[0]+dy*64);
        }
    }
    ball_update();gfx_ball_step(fs_ball_vx,fs_ball_vy);if(!fs_hud_pending)draw();
}
