// Cheap visual phases; match timers and collision timing remain in play.c.
#include "game.h"
#include "player_anim.h"
uint8_t player_move_pose(uint8_t move, uint8_t timer, uint8_t front, uint8_t *lie, int8_t *lift) {
    uint8_t view=front?80:0, extra=front?8:0;
    *lie=0; *lift=0;
    if(move==MOVE_BICYCLE) {
        if(timer>20) { *lift=4; return 168+extra; }
        if(timer>12) { *lift=12; return 56+view; }
        if(timer>8) { *lift=8; return 168+extra; }
        if(timer>4) { *lift=2; return 184+extra; }
        return 24+view;
    }
    if(move==MOVE_HEADER) {
        if(timer>14) return 24+view;
        if(timer>8) { *lift=6; return 48+view; }
        if(timer>4) { *lift=3; return 48+view; }
        return 184+extra;
    }
    if(timer<=4) return 24+view;
    if(timer<=8) return 200+extra;
    if(move==MOVE_SLIDE && timer>60) return 24+view;
    *lie=1;
    return move==MOVE_DOWN ? 216 : 160;
}
