// The court's perspective tables (proj.c): by depth from -16 to 239 (index depth +
// PROJ_OFS), the map row (proj_y) and the width scale /128 (proj_s), in play.c.
#ifndef PROJ_H
#define PROJ_H
#include <gb/gb.h>
#define PROJ_OFS 16
extern uint8_t proj_y[256], proj_s[256];
void proj_init(void) BANKED;
#endif
