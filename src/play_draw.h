// The match's drawing: play_draw.c.
#ifndef PLAY_DRAW_H
#define PLAY_DRAW_H
#include <gb/gb.h>
void play_draw(void) BANKED;       // a frame (play.c then draws the aim marker)
void play_draw_load(void) BANKED;  // sprite tiles and the streamed art, as the match loads
extern uint8_t ball_oam, slide_shown[2], zone_on, zone_lit, draw_lie;
extern int8_t draw_lift;
// play.c's, drawn from here
extern uint8_t ball_flight, flight_frame, meet_flight[2];
extern int16_t ball_hx, ball_hwy;
#endif
