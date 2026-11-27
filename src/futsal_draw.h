// The 2v2 match's drawing (futsal_draw.c), out of futsal.c's bank: the camera, the
// players by depth, the ball, its trail and shadow, your marker, the lob ring, the keeper.
#ifndef FUTSAL_DRAW_H
#define FUTSAL_DRAW_H
#include <gb/gb.h>
#define FS_CAM_MAX 112  // map px: the most the camera scrolls (256-row map, 136 rows showing; the grass past the bottom goal mostly hidden)
#define FS_CAM_LEAD 16  // px ahead of the carrier
#define FS_CHARGE_MAX 32  // a full shot's charge (futsal.c)
void futsal_draw(void) BANKED;
#endif
