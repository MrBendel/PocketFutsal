#pragma bank 255
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"
#include "game.h"
#include "futsal_stadium.h"
#include "futsal_stands.h"
#include "futsal_score.h"
#if FUTSAL_STANDS_TILE_COUNT > 32
#error Stadium overlaps the end of signed BG tile memory
#endif
uint8_t fs_board_score[2];
void futsal_stadium_load(void) BANKED {
    static const uint16_t palette[4]={RGB8(88,160,64),RGB8(24,32,48),RGB8(104,128,152),RGB8(248,224,144)};
    memcpy(&pal_base_bg[28],palette,sizeof(palette));
    VBK_REG=1;
    banked_bkg_data(96,FUTSAL_STANDS_TILE_COUNT,futsal_stands_tiles,BANK(futsal_stands));
    VBK_REG=0;
    fs_board_score[0]=fs_board_score[1]=0;
}
void futsal_stadium_score(void) BANKED {
    // The first twelve court tiles are distinct, bank-zero score cells. Only
    // queue changed fields; normal rally frames perform no scoreboard work.
    for(uint8_t i=0;i<2;i++)if(score[i]!=fs_board_score[i]){
        uint8_t n=score[i]>5?5:score[i];
        gfx_vram_copy((uint8_t *)(0x9500+(uint16_t)i*96),futsal_score_tiles+((uint16_t)n*12+i*6)*16,96,BANK(futsal_score));
        fs_board_score[i]=score[i];
    }
}
