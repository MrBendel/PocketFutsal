#pragma bank 255
// The court's perspective, worked out once as a match starts (32-bit, cold: out of
// play.c's bank). The same formula as tools/tile-art.mjs.
#include <gb/gb.h>
#include "game.h"
#include "proj.h"

static int32_t floordiv(int32_t a, int32_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

void proj_init(void) BANKED {
    for (int16_t i = 0; i < 256; i++) {
        int32_t fy = i - PROJ_OFS;
        int32_t row = COURT_SY + floordiv((int32_t)PROJ_K0 * 448 * fy + (int32_t)(PROJ_K1 - PROJ_K0) * fy * fy, 114688L);  /* 448 * 256 */
        proj_y[i] = (uint8_t)row;
        proj_s[i] = (uint8_t)(PROJ_S0 + floordiv((int32_t)(128 - PROJ_S0) * fy, COURT_NEAR));
    }
}

