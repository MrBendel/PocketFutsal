#ifndef FUTSAL_FX_H
#define FUTSAL_FX_H
#include <gb/gb.h>
extern uint8_t fs_fx_timer,fs_fx_frame;
extern int16_t fs_fx_x,fs_fx_y;
void futsal_fx_load(void) BANKED;
void futsal_fx_start(int16_t x,int16_t y,int16_t z,uint8_t aerial) BANKED;
void futsal_fx_draw(uint8_t cam) BANKED;
#endif
