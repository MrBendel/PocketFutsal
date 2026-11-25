#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include "gfx.h"
#include "game.h"
#include "futsal_results.h"
#include "futsal_reactions.h"
#include "futsal_goal.h"

uint8_t fs_result_tick,fs_result_frame[4],fs_result_outcome[4];

void futsal_results_update(void) BANKED {
    static const uint8_t win_cycle[8]={0,1,2,2,1,0,0,0};
    static const uint8_t win_lift[8]={0,2,5,6,3,0,0,0};
    static const uint8_t lose_cycle[8]={0,0,1,1,2,2,1,2};
    for(uint8_t i=0;i<4;i++){
        uint8_t phase=((fs_result_tick+i*5)>>3)&7;
        uint8_t won=fs_result_outcome[i]==0;
        uint8_t f=won?win_cycle[phase]:lose_cycle[phase],x=(i&1)?104:64;
        uint8_t ground=i<2?86:120;
        fs_result_frame[i]=f;
        gfx_actor(i*4,i*24+f*8,0,x,ground-(won?win_lift[phase]:0),S_BANK|i,0);
        // The shadow stays on the floor throughout the winner's jump.
        move_sprite(16+i,x+4,ground+12);
    }
    fs_result_tick++;
}
void futsal_results_enter(void) BANKED {
    fs_result_tick=0;
    banked_sprite_data(0,2,futsal_goal_tiles+64,BANK(futsal_goal));
    for(uint8_t i=0;i<4;i++){
        uint8_t outcome=(i>>1)==winner?0:1;
        fs_result_outcome[i]=outcome;
        // Results own sprite bank one: all three poses are resident, so the
        // animation never streams tile data or touches the gameplay atlas.
        banked_sprite_data_vbk1(i*24,24,futsal_reactions_tiles+((uint16_t)i*48+outcome*24)*16,BANK(futsal_reactions));
        set_sprite_tile(16+i,0);set_sprite_prop(16+i,SPR_PAL_UI);
    }
    // Two team rows keep every head, body and shadow below the scanline cap.
    futsal_results_update();gfx_sprites(1);
}
