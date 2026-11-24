#ifndef FUTSAL_GOALS_H
#define FUTSAL_GOALS_H
#include <gb/gb.h>
#define FS_GOAL_LEFT 60
#define FS_GOAL_RIGHT 100
#define FS_GOAL_HEIGHT 14
extern uint8_t fs_frame_timer,fs_frame_goal;
extern int8_t fs_goal_dx[2],fs_goal_dy[2];
extern uint16_t fs_post_hits,fs_bar_hits;
void futsal_goals_load(void) BANKED;
void futsal_goals_reset(void) BANKED;
void futsal_goals_draw(uint8_t cam) BANKED;
uint8_t futsal_goals_contact(int16_t old_x,int16_t old_y,int16_t old_z) BANKED;
void futsal_goals_step(void) BANKED;
#endif
