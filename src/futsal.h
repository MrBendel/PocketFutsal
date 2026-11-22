// Four players: slots 0/1 blue, 2/3 red. Positions in 1/16 court units.
#ifndef FUTSAL_H
#define FUTSAL_H
#include <stdint.h>
#include "game.h"
extern uint8_t fs_spectator, fs_paused, fs_owner, fs_goal_timer, fs_cam_y;
extern int16_t fs_pl_x[4], fs_pl_y[4], fs_ball_x, fs_ball_y, fs_ball_vx, fs_ball_vy;
extern uint16_t fs_sim_ticks, fs_pass_count, fs_shot_count, fs_tackle_count;
#endif
