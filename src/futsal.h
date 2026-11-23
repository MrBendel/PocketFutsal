// Four players: slots 0/1 blue, 2/3 red. Positions in 1/16 court units.
#ifndef FUTSAL_H
#define FUTSAL_H
#include <stdint.h>
#include "game.h"
extern uint8_t fs_spectator, fs_paused, fs_owner, fs_goal_timer, fs_cam_y;
extern int16_t fs_pl_x[4], fs_pl_y[4], fs_ball_x, fs_ball_y, fs_ball_vx, fs_ball_vy;
extern int8_t fs_vx[4], fs_vy[4];
extern int16_t fs_ball_z, fs_ball_vz, fs_target_x, fs_target_y;
extern uint8_t fs_receiver, fs_flight_left;
extern uint16_t fs_completed_passes;
extern uint8_t fs_touch_timer;
extern uint16_t fs_dribble_touches;
extern uint8_t fs_pass_charge,fs_lob,fs_aerial[4],fs_zone_ready;
extern uint16_t fs_lob_count,fs_aerial_count;
extern uint16_t fs_sim_ticks, fs_pass_count, fs_shot_count, fs_tackle_count;
#endif
