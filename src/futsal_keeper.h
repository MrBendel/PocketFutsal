// The 2v2 goalkeepers (futsal_keeper.c): one on each goal line, AI for both teams.
#ifndef FUTSAL_KEEPER_H
#define FUTSAL_KEEPER_H
#include <gb/gb.h>
#define FS_KP_TILE 168   // bank-0 sprite tiles: past the court's BG tiles (80..160), before the side atlas (214)
#define FS_KP_OAM 30     // eight OBJ slots (the goals use none): only one goal shows at a time
extern int16_t fs_kp_x[2];        // court x, SUB: [0] the top goal's (red), [1] the bottom one's (blue)
extern uint8_t fs_kp_dive[2],fs_kp_save[2];
extern int16_t fs_kp_to[2];
extern uint16_t fs_saves;
void futsal_keeper_load(void) BANKED;
void futsal_keeper_reset(void) BANKED;
// A shot struck at goal g (0 top, 1 bottom), aimed at tx (SUB), t ticks out, at speed:
// the keeper dives, and decides now whether it gets there.
void futsal_keeper_shot(uint8_t g,int16_t tx,uint8_t t,uint8_t speed) BANKED;
void futsal_keeper_step(void) BANKED;
// The ball's moved from old_y: if it crossed a keeper set to save it, parried (1).
uint8_t futsal_keeper_check(int16_t old_y) BANKED;
void futsal_keeper_draw(uint8_t cam) BANKED;
#endif
