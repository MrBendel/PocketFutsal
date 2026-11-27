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
#include "futsal_draw.h"
#if SCENE_TILES + FUTSAL_PITCH_TILE_COUNT > 214
#error Futsal court overlaps side sprite atlas
#endif
#if 214 + SIDE_ROSTER_TILE_COUNT > 242
#error Futsal side sprite atlas overlaps contact effects
#endif



uint8_t fs_pace,fs_pace_phase,fs_pace_mask;
uint8_t fs_input_pending,fs_input_pad,fs_pressed,fs_pressed_pad;
uint8_t fs_spectator = 1, fs_paused, fs_owner, fs_goal_timer, fs_cam_y, fs_cam_target;
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
// Mario Strikers' control (2026-11-27, playtest: "it's kind of impossible to play right
// now"): you play whichever blue has the ball, or the one a pass is going to; on
// defence, the nearer blue as you lose it, and B switches. The ball stays at the
// carrier's feet: only a tackle, a pass or a shot moves it. A tackle that gets there
// puts the carrier down and pops the ball loose.
uint8_t fs_ctrl;          // the blue you play (0 or 1)
uint8_t fs_down[4];       // ticks a tackled player is still on the floor
uint8_t fs_defending;     // 2 the reds have it, 1 it's loose and not ours, 0 not: for the switch as it happens
uint16_t fs_switches;
#define HUMAN(i) (!fs_spectator&&(i)==fs_ctrl)
#define FS_FOOT_X (6*16)  // the ball at the carrier's feet: this far ahead across ...
#define FS_FOOT_Y (5*16)  // ... and in depth
#define FS_DOWN 36        // ticks on the floor, tackled
#define FS_TACKLE_RANGE (18*16)  // an AI presser slides in from this near the ball
#define FS_CARRY_SPEED 19  // SUB a tick with the ball (24 without): a defender gets back, as in Strikers (22 before)
#define FS_TACKLE_COOL 40  // ticks before a slide again (65 before: a miss left the attack a second)
// Strikers' shooting: hold A to charge (slowed, open to a tackle), release to shoot.
// The charge sets its pace (64 SUB a tick at none to 128 full, on fire) and, under
// half, doubles its spread. Each completed pass in a row adds FS_CHAIN_BONUS to the
// next shot (FS_CHAIN_MAX of them). A shot short of full can be blocked: a player in
// its path within 9 px takes it (5 px for a full one). A and B together, with the
// ball: a dodge, a burst no tackle can stop for FS_DODGE ticks.
#define FS_GUARD_RANGE (80*16)  // the carrier this near our goal line: the cover guards the goal
#define FS_CHAIN_MAX 2
#define FS_CHAIN_BONUS 8
#define FS_DODGE 14
#define FS_DODGE_COOL 60
uint8_t fs_shot_charge;   // yours, held: 1..FS_CHARGE_MAX
uint8_t fs_shot_power;    // the next shot's charge (set before shoot())
uint8_t fs_shot_speed;    // its pace
uint8_t fs_chain,fs_chain_team;  // completed passes in a row, and whose
uint8_t fs_dodge[4];
uint16_t fs_dodges,fs_blocks;
uint8_t fs_ai_act;  // the AI carrier's next strike: 1 a shot, 2 a pass, 3 a lob (struck the tick after)
int8_t fs_ai_aim;
uint8_t fs_ai_who;
uint8_t fs_redraw_only;  // after a FRAME! restart: a frame that only draws (the goal back in place)
int8_t fs_cut_dir;
uint8_t fs_cut_window,fs_cut_count,fs_cut_cooldown,fs_stumble_player,fs_stumble_timer;
uint16_t fs_ankle_count;
uint8_t fs_ball_oam, fs_hud_pending,fs_goal_refresh;
uint8_t fs_power_trail, fs_trail_n, fs_trail_i;
uint8_t fs_trail_x[16], fs_trail_y[16];
uint8_t fs_projection_y[225], fs_projection_s[225];
// Reserve OAM 0/1 for the landing cue: a crowded line must not drop half its
// outline. Depth-sorted actors/ball use 2..18, marker 19, trails 20..22,
// player shadows 23..26, ball shadow 27, contact effects 28/29.

static int16_t ab(int16_t n) { return n < 0 ? -n : n; }
static int16_t clamp(int16_t n, int16_t lo, int16_t hi) { return n < lo ? lo : n > hi ? hi : n; }
#define team(i) ((uint8_t)((i) >> 1))
// A quarter, toward zero, as v/4 is, by shifts: a signed division by a constant
// still calls the division routine (a landing ball's frame ran past the budget).
#define QUARTER(v) ((v)<0?-((-(v))>>2):(v)>>2)
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
static void hud(void) {
    char line[] = "BLUE 0 - 0 RED  WATCH";
    line[5] += score[0]; line[9] += score[1];
    if (!fs_spectator) { line[15]='P'; line[16]='L'; line[17]='A'; line[18]='Y'; line[19]=' '; }
    win_text_queued(0,0,line);
    if (fs_paused) win_text_queued(7,0,"PAUSED");
    else if (fs_goal_timer) win_text_queued(6,0,"GOAL!");
    else if (fs_frame_timer) win_text_queued(6,0,"FRAME!");
}
static void kickoff(uint8_t side) {
    futsal_goals_reset();fs_goal_refresh=1;fs_ai_act=0;
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
    fs_ctrl=0;fs_defending=side;
    fs_shot_charge=fs_shot_power=fs_chain=0;fs_chain_team=side;
    fs_cut_dir=0;fs_cut_window=fs_cut_count=fs_cut_cooldown=fs_stumble_timer=0;
    fs_stumble_player=NOBODY;
    for(uint8_t i=0;i<4;i++) {
        fs_tackling[i]=fs_cooldown[i]=fs_action[i]=fs_burst[i]=fs_moving[i]=fs_flip[i]=0;
        fs_view[i]=team(i)?0:1;
        fs_side[i]=0;
        fs_aerial[i]=fs_air_hit[i]=fs_air_kind[i]=fs_down[i]=fs_dodge[i]=0;
        fs_vx[i]=fs_vy[i]=fs_slide_vx[i]=fs_slide_vy[i]=0;
        fs_dribble_x[i]=0; fs_dribble_y[i]=team(i)?1:-1;
        fs_think[i]=((i<<1)+fs_kickoff_phase)&7;
        fs_ai_tx[i]=fs_pl_x[i];fs_ai_ty[i]=fs_pl_y[i];
    }
}
// Futnet's launch model: a precise vector at a chosen court speed, then an arc
// calculated from travel time. No direction table and no fixed-duration flight.
// 4096 / a shot's speed (64, 66 .. 128): its flight, sum*16/speed, without a division.
// 256 / t (t 1..40): a volley's start height over its flight, without a division.
static const uint8_t air_rec[41]={0,255,128,85,64,51,43,37,32,28,26,23,21,20,18,17,16,15,14,13,13,12,12,11,11,10,10,9,9,9,9,8,8,8,8,7,7,7,7,7,6};
static const uint8_t shot_inv[33]={64,62,60,59,57,55,54,53,51,50,49,48,47,46,45,44,43,42,41,40,39,39,38,37,37,36,35,35,34,34,33,33,32};
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
    speed=fs_is_shot?fs_shot_speed:64;
    // At full pace (one division, a shift's numerator), then scaled to this one's:
    // an 8-bit multiply, not a 16-bit one and a second division.
    fs_ball_vx=((uint16_t)ab(dx)<<7)/(uint16_t)sum;
    if(speed!=128)fs_ball_vx=(int16_t)(((uint16_t)(uint8_t)fs_ball_vx*speed)>>7);
    if(dx<0)fs_ball_vx=-fs_ball_vx;
    fs_ball_vy=(dx||dy)?speed-ab(fs_ball_vx):0;if(dy<0)fs_ball_vy=-fs_ball_vy;
    // Speeds are powers of two: distance / speed is a cheap shift. As in
    // Futnet, long deliveries get more air time and short exchanges stay low.
    t=fs_is_shot?(int16_t)(((uint16_t)sum*shot_inv[(speed-64)>>1]+128)>>8):(sum+2)>>2;
    if(t<4)t=4; // Court bounds keep even the longest quick pass below 96.
    fs_ball_vz=(3*(t+1))>>3;
    if(fs_air_shot)fs_ball_vz-=t<=40?(int16_t)(((uint16_t)fs_ball_z*air_rec[t])>>8):fs_ball_z/t;else fs_ball_z=0;
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
    // (Yours: a quarter of the lead. You take the receiver over as it goes, and the
    // run you'd be led on was the AI's: stop, and a full lead sails past you.)
    else if(HUMAN(i)){tx+=fs_vx[mate]*8;ty+=fs_vy[mate]*8;}
    else {tx+=fs_vx[mate]*32;ty+=fs_vy[mate]*32;}
    kick(i,clamp(tx,14*16,146*16),clamp(ty,16*16,208*16));
    // React to the delivery immediately rather than following a stale support run.
    fs_ai_tx[mate]=fs_target_x;fs_ai_ty[mate]=fs_target_y;
    if(HUMAN(i)){fs_ctrl=mate;fs_switches++;}  // you play the receiver as it goes
    fs_pass_count++;
    if(lob)fs_lob_count++;
}
static void shoot(uint8_t i, int8_t aim) {
    static int16_t x;
    static uint8_t roll,power;
    power=fs_shot_power+fs_chain*FS_CHAIN_BONUS;if(power>FS_CHARGE_MAX)power=FS_CHARGE_MAX;
    fs_shot_speed=64+(power<<1);fs_shot_power=fs_chain=0;
    fs_shot_range=team(i)?224-(fs_ball_y>>4):(fs_ball_y>>4);
    x=80+aim*14;if(!aim)x=(rand()&1)?70:90;
    roll=rand();
    if(fs_shot_range>144){
        // Full-court efforts are wild clearances, not goal-seeking lasers.
        x=(roll&1)?128+(roll&7):32-(roll&7);fs_shot_error=x-80;
    }else{
        if(fs_shot_range>96){fs_shot_error=(roll&15)-(roll>>4);if(fs_shot_range>112)fs_shot_error+=fs_shot_error>>1;}
        else if(fs_shot_range>48)fs_shot_error=(roll&7)-((roll>>3)&7);
        else fs_shot_error=(roll&3)-((roll>>2)&3);
        // Futsal has no keepers: what stops a shot is its aim. A snatched one sprays
        // twice as wide, a part charge half as much again; only a full one is true.
        if(power<FS_CHARGE_MAX/2)fs_shot_error<<=1;
        else if(power<FS_CHARGE_MAX)fs_shot_error+=fs_shot_error>>1;
        x+=fs_shot_error;
    }
    fs_receiver=NOBODY;fs_is_shot=1;fs_lob=0;
    kick(i,x*16,team(i)?224*16:0);fs_power_trail=power>=FS_CHARGE_MAX?48:0;fs_shot_count++;
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
    if(fs_air_hit[i]==2){
        // Met last tick: struck now, on a tick of its own (with the jump's other
        // work it ran past the frame budget). Unless someone's taken it meanwhile.
        fs_air_hit[i]=1;
        if(fs_owner!=NOBODY||fs_kicked)return;
        fs_air_shot=1;fs_thought=1;
        if(fs_receiver==i)fs_completed_passes++;
        // Met in the air: full near goal (64 px); from farther, a part charge (it sprays).
        fs_shot_power=(team(i)?224*16-fs_ball_y:fs_ball_y)<64*16?FS_CHARGE_MAX:FS_CHARGE_MAX/2;
        shoot(i,HUMAN(i)?((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0):0);
        fs_aerial_count++;
        return;
    }
    if(t<=20&&t>=10&&!fs_air_hit[i]&&!fs_kicked&&fs_owner==NOBODY&&fs_ball_vz<0&&fs_ball_z>=8*16&&fs_ball_z<=44*16&&distance(i,fs_ball_x,fs_ball_y)<20*16)
        fs_air_hit[i]=2;
}
static void switch_to(uint8_t j){
    if(fs_tackling[fs_ctrl]||fs_aerial[fs_ctrl]||j==fs_ctrl)return;
    fs_ctrl=j;fs_switches++;fs_pass_charge=fs_pending_kick=fs_shot_charge=0;
    fs_cut_dir=0;fs_cut_count=fs_cut_window=0;
}
// B: with the ball, a pass (tap) or a lob (hold); without it, play the other blue.
static void pass_input(int8_t dx,int8_t dy){
    static uint8_t c;
    c=fs_ctrl;
    if(fs_pressed&J_B){
        if(fs_owner==c){
            fs_pass_charge=1;
            fs_pass_dx=((fs_pressed_pad&J_RIGHT)!=0)-((fs_pressed_pad&J_LEFT)!=0);
            fs_pass_dy=((fs_pressed_pad&J_DOWN)!=0)-((fs_pressed_pad&J_UP)!=0);fs_pending_kick=0;
        }else if(fs_owner!=NOBODY||fs_receiver!=c)switch_to(c^1);
    }else if(fs_pass_charge&&(joy&J_B)){
        if(fs_pass_charge<30)fs_pass_charge++;
        if(dx||dy){fs_pass_dx=dx;fs_pass_dy=dy;}
    }
    if(fs_pass_charge&&fs_owner!=c)fs_pass_charge=0;
    if(fs_pass_charge&&!(joy&J_B)){
        uint8_t lob=fs_pass_charge>=12;
        fs_pending_kick=lob?3:1;fs_pending_timer=20;
        fs_pending_dx=fs_pass_dx;fs_pending_dy=fs_pass_dy;
        fs_pass_charge=0;
    }
}
static void dodge(uint8_t i){
    fs_dodge[i]=FS_DODGE;fs_burst[i]=10;fs_cooldown[i]=FS_DODGE_COOL;fs_dodges++;sfx_play(SFX_TICK);
}
// The AI aims for the far post: across the goal from where it shoots.
static int8_t away(void){
    return fs_ball_x<80*16?1:fs_ball_x>80*16?-1:((rand()&1)?1:-1);
}
static void tackle(uint8_t i) {
    fs_tackling[i]=18;fs_action[i]=18;fs_cooldown[i]=FS_TACKLE_COOL;fs_burst[i]=10;
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
    static uint8_t slide,burst,ax,ay,mx,my,v;
    x=fs_pl_x[i];y=fs_pl_y[i];dx=tx-x;dy=ty-y;
    vx=fs_vx[i];vy=fs_vy[i];want_x=want_y=0;
    slide=fs_tackling[i];burst=fs_burst[i];
    speed=fs_owner==i?FS_CARRY_SPEED:24;if(burst)speed=44;
    if(HUMAN(i)&&(fs_pass_charge||fs_shot_charge))speed=12;
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
    if(slide>4&&slide<=14&&fs_owner!=NOBODY&&((fs_owner^i)&2)&&fs_ball_z<=6*16&&!fs_dodge[fs_owner]){
        // Contact is at the leading foot during the lunge, not on button press.
        // The ball at the leading foot, or the carrier's body in the slide's path.
        dx=fs_ball_x-x-vx*4;dy=fs_ball_y-y-vy*4;
        if(!(dx>=-7*16&&dx<=7*16&&dy>=-7*16&&dy<=7*16)){
            v=fs_owner;dx=fs_pl_x[v]-x-vx*2;dy=fs_pl_y[v]-y-vy*2;
        }
        if(dx>=-7*16&&dx<=7*16&&dy>=-7*16&&dy<=7*16){
            // Got there: the carrier goes down, the ball pops loose ahead of the slide.
            v=fs_owner;fs_down[v]=fs_action[v]=FS_DOWN;fs_vx[v]=fs_vy[v]=0;fs_tackling[v]=fs_aerial[v]=0;
            fs_owner=NOBODY;fs_held_ticks=0;fs_receiver=NOBODY;fs_last_kicker=v;fs_pickup_lock=FS_DOWN;
            fs_ball_vx=fs_slide_vx[i]>>1;fs_ball_vy=fs_slide_vy[i]>>1;fs_ball_z=0;fs_ball_vz=10;fs_ball_fall=0;
            fs_tackle_count++;sfx_play(SFX_TOUCH);
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
    static uint8_t c;
    c=fs_ctrl;
    if(fs_owner!=c||fs_cut_cooldown||fs_ball_z>3*16||fs_tackling[c]||(joy&(J_A|J_B))){
        fs_cut_dir=0;fs_cut_count=fs_cut_window=0;return;
    }
    if(!dx)return;
    vx=fs_vx[c];
    if(!fs_cut_dir){if((dx<0&&vx<=-8)||(dx>0&&vx>=8))fs_cut_dir=dx;return;}
    if(dx==fs_cut_dir)return;
    // A second button flick before the body has changed direction is ignored.
    if((fs_cut_dir<0&&vx>-8)||(fs_cut_dir>0&&vx<8))return;
    fs_cut_dir=dx;fs_cut_window=28;
    if(++fs_cut_count<2)return;
    fs_cut_count=0;
    defender=fs_presser[1];
    if(fs_stumble_timer||fs_tackling[defender]||fs_aerial[defender]||distance(c,fs_ball_x,fs_ball_y)>20*16||distance(defender,fs_pl_x[c],fs_pl_y[c])>28*16)return;
    vx=fs_vx[defender];
    if((dx<0&&vx<6)||(dx>0&&vx>-6))return;
    fs_stumble_player=defender;fs_stumble_timer=22;
    fs_action[defender]=34;fs_burst[defender]=0;
    fs_cut_cooldown=90;fs_ankle_count++;
    sfx_play(SFX_PARRY);
}
static void ai(uint8_t i){
    if(fs_stumble_timer&&i==fs_stumble_player){step_player(i,fs_pl_x[i],fs_pl_y[i]);return;}
    // The carrier's shot or pass, decided last tick: struck now, on a tick of its own
    // (a decision and a launch together ran past the frame budget). No one else thinks.
    if(fs_ai_act&&fs_ai_who==i){
        if(fs_owner!=i){fs_ai_act=0;goto think;}  // lost it meanwhile
        if(fs_ai_act==1)shoot(i,fs_ai_aim);else pass(i,0,0,fs_ai_act==3);
        fs_ai_act=0;fs_thought=1;return;
    }
think:

    static uint8_t mate,own,side,mark,presser,defender;
    static int16_t goal,tx,ty,pressure,mate_space,d,forward;
    // An AI carrier sees a slide coming (in its crouch, near) and dodges it, half the time.
    if(fs_owner==i&&!fs_cooldown[i]){
        mate=(i&2)^2;
        if((fs_tackling[mate]>14&&distance(mate,fs_ball_x,fs_ball_y)<FS_TACKLE_RANGE+6*16)||
           (fs_tackling[mate+1]>14&&distance(mate+1,fs_ball_x,fs_ball_y)<FS_TACKLE_RANGE+6*16)){
            fs_cooldown[i]=8;  // (a look either way, not every tick)
            if(rand()&1)dodge(i);
        }
    }
    // The presser goes in as soon as the carrier's in range, between its decisions
    // too (they come every 8 ticks or so: a carrier runs 11 px between them).
    if(fs_owner!=NOBODY&&team(fs_owner)!=team(i)&&fs_presser[team(i)]==i&&!fs_cooldown[i]&&!fs_tackling[i]
       &&fs_ball_z<=6*16&&distance(i,fs_ball_x,fs_ball_y)<FS_TACKLE_RANGE){tackle(i);step_player(i,fs_ball_x,fs_ball_y);return;}
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
        else if(fs_held_ticks>10&&ab(fs_pl_y[i]-goal)<48*16&&pressure>14*16){fs_shot_power=12+(rand()&15);fs_ai_act=1;fs_ai_who=i;fs_ai_aim=away();}
        else if(fs_held_ticks>12&&mate_space>18*16&&forward>-32*16&&distance(mate,fs_ball_x,fs_ball_y)>24*16&&(pressure<48*16||fs_held_ticks>100)){fs_ai_act=2+((fs_pass_count&3)==3);fs_ai_who=i;}
        else if(fs_held_ticks>18&&ab(fs_pl_y[i]-goal)<64*16){fs_shot_power=8+(rand()&15);fs_ai_act=1;fs_ai_who=i;fs_ai_aim=away();}
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
        if(!side&&!fs_spectator)presser=fs_presser[0]=fs_ctrl;  // you press; your partner covers
        else if(!fs_tackling[presser]&&distance(presser^1,fs_ball_x,fs_ball_y)+24*16<distance(presser,fs_ball_x,fs_ball_y))
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
            // A carrier near our goal: the last defender guards its mouth, between the
            // ball and the goal's middle, there to block a shot (futsal has no keepers).
            d=side?fs_ball_y:224*16-fs_ball_y;
            // Out a quarter of the way to the ball, on its line to the goal's middle:
            // narrowing the angle, the way a futsal defender does (no keeper behind).
            if(fs_owner!=NOBODY&&d<FS_GUARD_RANGE){
                tx=80*16+((fs_ball_x-80*16)>>2);
                ty=side?12*16+(fs_ball_y>>2):212*16-((224*16-fs_ball_y)>>2);
            }
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
        // At the carrier's feet, the way they face, rolling with them (its velocity
        // theirs, so it's drawn rolling; set back so the step below lands it there).
        fs_ball_vx=fs_vx[i];fs_ball_vy=fs_vy[i];fs_ball_z=fs_ball_vz=0;
        fs_ball_x=fs_pl_x[i]+fs_dribble_x[i]*FS_FOOT_X-fs_ball_vx;
        fs_ball_y=fs_pl_y[i]+fs_dribble_y[i]*FS_FOOT_Y-fs_ball_vy;
    }
    old_x=fs_ball_x;old_y=fs_ball_y;old_z=fs_ball_z;
    fs_ball_x+=fs_ball_vx; fs_ball_y+=fs_ball_vy;
    if(fs_flight_left&&!--fs_flight_left&&!fs_is_shot){
        // A delivery sheds pace as it lands instead of rolling through a
        // missed receiver and carrying all the way into the goal.
        fs_ball_vx=QUARTER(fs_ball_vx);fs_ball_vy=QUARTER(fs_ball_vy);
    }
    if(fs_ball_z>0||fs_ball_vz){
        if(fs_lob)fs_ball_vz-=(++fs_ball_fall&1)?1:2;
        else if(++fs_ball_fall&3)fs_ball_vz--;
        fs_ball_z+=fs_ball_vz;
        if(fs_ball_z<=0){
            fs_ball_z=0;fs_ball_vz=(-fs_ball_vz*3)>>2;
            fs_lob=0;
            if(fs_ball_vz<5)fs_ball_vz=0;
            fs_ball_vx-=QUARTER(fs_ball_vx);fs_ball_vy-=QUARTER(fs_ball_vy);
        }
    }
    if(fs_ball_x<10*16 || fs_ball_x>150*16) { fs_ball_x=clamp(fs_ball_x,10*16,150*16); fs_ball_vx=-(fs_ball_vx-QUARTER(fs_ball_vx)); }
    if(fs_ball_y<0 || fs_ball_y>224*16) {
        // Well wide of the posts (30 px from the middle: a post's 23, and a tick's
        // drift): the wall, without the goal's crossing sweep.
        contact=ab(old_x-80*16)<30*16||ab(fs_ball_x-80*16)<30*16?futsal_goals_contact(old_x,old_y,old_z):0;
        if(contact==2){fs_power_trail=0;fs_hud_pending=1;return;}
        if(contact==1) {
            uint8_t s=fs_ball_y<0?0:1; score[s]++; fs_goal_timer=90;
            fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vx=fs_ball_vy=0;
            sfx_play(SFX_WHISTLE); fs_hud_pending=1; return;
        }
        fs_ball_y=clamp(fs_ball_y,0,224*16); fs_ball_vy=-(fs_ball_vy-QUARTER(fs_ball_vy));
    }
    if(!dribbling&&!fs_ball_z&&(!fs_is_shot||!(fs_sim_ticks&3))) { if(fs_ball_vx) fs_ball_vx+=fs_ball_vx>0?-1:1; if(fs_ball_vy) fs_ball_vy+=fs_ball_vy>0?-1:1; }
    if(fs_pickup_lock) fs_pickup_lock--;
    if(fs_ball_z>8*16||fs_kicked)return;
    if(fs_owner!=NOBODY)return;  // at someone's feet: only a tackle takes it
    // Rotate pickup priority to avoid a permanent advantage to blue.
    // A shot still flying: blocked within 9 px, 5 for a full one; slowed, collected.
    reach=fs_is_shot&&ab(fs_ball_vx)+ab(fs_ball_vy)>48?(fs_shot_speed>=112?6*16:11*16):14*16;
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
            if(fs_receiver==i){fs_completed_passes++;if(fs_chain<FS_CHAIN_MAX)fs_chain++;}
            if(fs_is_shot&&team(i)!=team(fs_last_kicker))fs_blocks++;
            fs_owner=i;fs_held_ticks=0;fs_receiver=NOBODY;fs_touch_timer=0;fs_lob=0;
            // A trap kills the delivery's speed without moving its position.
            fs_ball_vx=fs_ball_vy=0;fs_ball_vz=0;fs_ball_z=0;break;
        }
    }
}
void futsal_enter(void) BANKED {
    uint8_t buf[FUTSAL_PITCH_MAP_W*8];  // eight rows at a time
    palettes_futsal();
    gfx_ball_init(116);
    futsal_fx_load();
    futsal_goals_load();
    banked_sprite_data(124,FUTSAL_ZONE_TILE_COUNT,futsal_zone_tiles,BANK(futsal_zone));
    set_sprite_tile(0,124);set_sprite_tile(1,126);
    for(uint8_t k=0;k<3;k++){set_sprite_tile(20+k,8+(k<<1));set_sprite_prop(20+k,4);move_sprite(20+k,0,0);}
    set_sprite_tile(27,4);set_sprite_prop(27,SPR_PAL_UI);
    set_sprite_tile(19,6);set_sprite_prop(19,SPR_PAL_UI);
    for(uint8_t i=0;i<4;i++){set_sprite_tile(23+i,254);set_sprite_prop(23+i,SPR_PAL_UI);}
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
    for(uint8_t r=0;r<FUTSAL_PITCH_MAP_H;r+=8){
        banked_memcpy(buf,futsal_pitch_map+r*FUTSAL_PITCH_MAP_W,sizeof(buf),BANK(futsal_pitch)); set_bkg_tiles(0,r,FUTSAL_PITCH_MAP_W,8,buf);
        banked_memcpy(buf,futsal_pitch_attr+r*FUTSAL_PITCH_MAP_W,sizeof(buf),BANK(futsal_pitch)); VBK_REG=1; set_bkg_tiles(0,r,FUTSAL_PITCH_MAP_W,8,buf); VBK_REG=0;
    }
    // A fixed three-quarter view (tools/futsal-court-art.mjs): depth at 6/7 from map
    // row 32, no vanishing point; across, as it is (scale 128: futsal_fx.c reads it).
    for(int16_t y=0;y<=224;y++) {
        fs_projection_y[y]=32+(uint8_t)((y*6)/7);
        fs_projection_s[y]=128;
    }
    fs_cam_y=fs_cam_target=0;
    initrand(game_seed); score[0]=score[1]=sets[0]=sets[1]=0;
    match_sets=1; winner=NOBODY; fs_paused=fs_goal_timer=fs_hud_pending=0;
    fs_sim_ticks=fs_pass_count=fs_shot_count=fs_tackle_count=fs_completed_passes=fs_dribble_touches=fs_ankle_count=0;
    fs_lob_count=fs_aerial_count=0;
    if(fs_pace>2)fs_pace=0;
    fs_pace_mask=fs_pace?7:3;
    fs_pace_phase=fs_input_pending=0;
    move_win(7,HUD_WY); SHOW_WIN; gfx_sprite_split(HUD_WY);
    kickoff(0); hud(); futsal_draw();
}
void futsal_update(void) BANKED {
    static uint8_t k,i,human;
    static int8_t dx,dy;
    if(joy_pressed&J_START) { fs_paused=!fs_paused; fs_hud_pending=fs_input_pending=0; hud(); return; }
    if(fs_paused) return;
    // Text VRAM writes get their own event frame instead of sharing a busy
    // physics/AI/actor frame. Match time and player momentum stay frozen here.
    if(fs_hud_pending) { fs_hud_pending=0; hud(); return; }
    if(fs_redraw_only) { fs_redraw_only=0; futsal_draw(); return; }  // the goal's reset drawn alone
    if(joy_pressed&J_SELECT) { fs_spectator=!fs_spectator;fs_pass_charge=fs_pending_kick=fs_return_call=fs_cut_count=fs_cut_window=fs_input_pending=0;fs_cut_dir=0;hud();return; }
    if(fs_frame_timer){
        futsal_goals_step();
        futsal_goals_draw(fs_cam_y);
        if(!fs_frame_timer){
            // The restart on a frame of its own; the goal's reset and the redraw follow.
            kickoff(fs_frame_goal^1);fs_hud_pending=fs_redraw_only=1;return;
        }
        futsal_draw();return;
    }
    if(fs_goal_timer) {
        if(!--fs_goal_timer) {
            if(score[0]>=5 || score[1]>=5) { winner=score[0]>=5?0:1; state_change(STATE_GAME_OVER); return; }
            kickoff(fs_ball_y==0?1:0); fs_hud_pending=1;
            return;  // drawn next frame: a restart and a full redraw together ran past the budget
        }
        futsal_draw(); return;
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
    // The ring pulses when a jump now meets it: the jump's strike comes 8..18 ticks on,
    // when a lob is within reach (its last ~10 ticks) and still up (to ~4 ticks out).
    fs_zone_ready=fs_lob&&fs_ball_vz<0&&fs_flight_left<=22&&fs_ball_z>=10*16;
    if(fs_pending_timer)fs_pending_timer--;else fs_pending_kick=0;
    if(!fs_spectator){
        // You play the blue with the ball (or the one it's passed to); as the reds win
        // it (or it runs loose from you), the nearer blue.
        // (fs_defending: the reds had it, or it was loose and not coming to a blue,
        // last tick; the switch comes as that starts, or as the reds pick it up.)
        if(fs_owner!=fs_ctrl)fs_shot_charge=0;
        if(fs_owner<2){fs_ctrl=fs_owner;fs_defending=0;}
        else if(fs_owner==NOBODY&&fs_receiver<2)fs_defending=0;
        else{
            if((!fs_defending||(fs_owner!=NOBODY&&fs_defending!=2))&&!fs_aerial[fs_ctrl]
               &&distance(fs_ctrl^1,fs_ball_x,fs_ball_y)<distance(fs_ctrl,fs_ball_x,fs_ball_y))switch_to(fs_ctrl^1);
            fs_defending=fs_owner!=NOBODY?2:1;
        }
    }
    // Who you play this tick, fixed for it: a switch mid-tick (a pass, B) plays
    // from the next, or the new one would take the same press again.
    human=fs_spectator?NOBODY:fs_ctrl;
    // A run of passes is the side's that has it: the other side winning it ends it.
    if(fs_owner!=NOBODY&&team(fs_owner)!=fs_chain_team){fs_chain=0;fs_chain_team=team(fs_owner);}
    // Alternate team order: the last team to update must not always win a
    // contested touch now that the dribbled ball has its own position.
    for(k=0;k<4;k++) {
        i=k^(fs_sim_ticks&2);
        fs_moving[i]=0;
        if(fs_tackling[i]) fs_tackling[i]--;
        if(fs_cooldown[i]) fs_cooldown[i]--; if(fs_action[i]) fs_action[i]--; if(fs_burst[i]) fs_burst[i]--; if(fs_dodge[i]) fs_dodge[i]--;
        if(fs_aerial[i]){aerial_update(i);continue;}
        if(fs_down[i]){fs_down[i]--;step_player(i,fs_pl_x[i],fs_pl_y[i]);continue;}
        if(i!=human) ai(i);
        else {
            dx=((joy&J_RIGHT)!=0)-((joy&J_LEFT)!=0);dy=((joy&J_DOWN)!=0)-((joy&J_UP)!=0);
            // A quick tap on a paced-out frame is delivered on the next
            // simulation tick, with the aim that accompanied the press.
            fs_pressed=joy_pressed|fs_input_pending;
            fs_pressed_pad=fs_input_pending?fs_input_pad:joy;
            fs_input_pending=0;
            crossover(dx);
            pass_input(dx,dy);
            if(fs_owner==i) {
                if((fs_pressed&(J_A|J_B))&&(joy&(J_A|J_B))==(J_A|J_B)){
                    // Both: a dodge, the pass or the shot called off.
                    fs_pass_charge=fs_shot_charge=fs_pending_kick=0;
                    if(!fs_cooldown[i])dodge(i);
                }else if(fs_pressed&J_A){
                    fs_shot_charge=1;fs_pending_kick=0;fs_pass_charge=0;
                    fs_pending_dx=((fs_pressed_pad&J_RIGHT)!=0)-((fs_pressed_pad&J_LEFT)!=0);
                }
                if(fs_shot_charge){
                    if(joy&J_A){
                        if(fs_shot_charge<FS_CHARGE_MAX)fs_shot_charge++;
                        if(dx)fs_pending_dx=dx;
                    }else{
                        fs_shot_power=fs_shot_charge;fs_shot_charge=0;
                        fs_pending_kick=2;fs_pending_timer=20;
                    }
                }
                if(fs_pending_kick&&distance(i,fs_ball_x,fs_ball_y)<=12*16){
                    if(fs_pending_kick==2)shoot(i,fs_pending_dx);else pass(i,fs_pending_dx,fs_pending_dy,fs_pending_kick==3);
                    fs_pending_kick=0;
                }
            }
            else {
                fs_pending_kick=0;
                if(fs_pressed&J_A){if(fs_lob)aerial_try(i);else if(!fs_cooldown[i])tackle(i);}
            }
            if(!fs_kicked&&!fs_aerial[i])step_player(i,fs_pl_x[i]+dx*64,fs_pl_y[i]+dy*64);
        }
    }
    ball_update();gfx_ball_step(fs_ball_vx,fs_ball_vy);if(!fs_hud_pending)futsal_draw();
}
