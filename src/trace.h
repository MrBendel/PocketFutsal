// The ball's flight traced on, to where each side meets it (trace.c); and the
// heights it needs, shared with play.c.
#ifndef TRACE_H
#define TRACE_H
#include <gb/gb.h>
#define REACH_Z 36        // px: a header reaches this high (a jump); a bicycle kick 40
#define BOUNCE_MIN 15     // a first bounce gets up at least this: about 9 px, 40 frames
#define LOB_BOUNCE 20     // a lob bounces up at least this: about 16 px, 54 frames, on back at three quarters of its pace
#define SLAM_Z 26         // px: a lob or a pop-up is met this high coming down (a header)
// In the court (court px; the lines are in).
#define IN_COURT(px, py) ((px) >= COURT_X0 && (px) <= COURT_X1 && (py) >= COURT_FAR && (py) <= COURT_NEAR)
void predict(uint8_t i) BANKED;
#endif
