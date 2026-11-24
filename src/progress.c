#pragma bank 255
// Progression (progress.h): stat levels and XP, mini game bests, and the save in
// battery RAM. Cold code: runs when a mini game ends and in menus.
#include <gb/gb.h>
#include <string.h>
#include "progress.h"

uint8_t stat_lv[STAT_COUNT];
uint16_t stat_xp[STAT_COUNT];
uint16_t mg_best[MG_COUNT];
uint16_t mg_plays[MG_COUNT];
uint8_t progress_loaded;
uint8_t progress_from_save;

// The save, at the start of SRAM bank 0: a tag, a version, the data, a checksum.
// A new version (or a bad checksum) starts fresh: bump SAVE_VERSION when the
// layout changes.
#define SAVE_VERSION 3
typedef struct {
    char tag[4];  // "PFUT"
    uint8_t version;
    uint8_t lv[STAT_COUNT];
    uint16_t xp[STAT_COUNT];
    uint16_t best[MG_COUNT];
    uint16_t plays[MG_COUNT];
    uint8_t sum;  // the bytes before it, added up, plus SUM_SEED
} Save;
#define SUM_SEED 0x5A
#define SRAM_SAVE ((Save *)0xA000)

uint16_t stat_xp_next(uint8_t lv) BANKED {
    // 30, 45, 60 ... 315 XP: a level or two a session early on, slower later.
    return 15 + 15 * (uint16_t)lv;
}

uint8_t player_level(void) BANKED {
    uint8_t lv = 1;
    for (uint8_t s = 0; s < STAT_COUNT; s++) lv += stat_lv[s] - 1;
    return lv;
}

uint8_t progress_award(uint8_t stat, uint16_t xp) BANKED {
    uint8_t gained = 0;
    if (stat >= STAT_COUNT) return 0;
    while (xp && stat_lv[stat] < STAT_LV_MAX) {
        uint16_t need = stat_xp_next(stat_lv[stat]) - stat_xp[stat];
        if (xp < need) {
            stat_xp[stat] += xp;
            return gained;
        }
        xp -= need;
        stat_xp[stat] = 0;
        stat_lv[stat]++;
        gained++;
    }
    return gained;
}

static uint8_t checksum(const Save *s) {
    const uint8_t *p = (const uint8_t *)s;
    uint8_t sum = SUM_SEED;
    for (uint8_t i = 0; i < sizeof(Save) - 1; i++) sum += p[i];
    return sum;
}

static void fresh(void) {
    for (uint8_t s = 0; s < STAT_COUNT; s++) stat_lv[s] = 1, stat_xp[s] = 0;
    for (uint8_t g = 0; g < MG_COUNT; g++) mg_best[g] = mg_plays[g] = 0;
}

void progress_load(void) BANKED {
    if (progress_loaded) return;
    progress_loaded = 1;
    progress_from_save = 0;
    Save s;
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy(&s, SRAM_SAVE, sizeof(Save));
    DISABLE_RAM;
    fresh();
    if (s.tag[0] != 'P' || s.tag[1] != 'F' || s.tag[2] != 'U' || s.tag[3] != 'T') return;
    if (s.version != SAVE_VERSION || s.sum != checksum(&s)) return;
    for (uint8_t i = 0; i < STAT_COUNT; i++) {
        uint8_t lv = s.lv[i];
        stat_lv[i] = lv < 1 ? 1 : lv > STAT_LV_MAX ? STAT_LV_MAX : lv;
        stat_xp[i] = s.xp[i];
    }
    for (uint8_t g = 0; g < MG_COUNT; g++) mg_best[g] = s.best[g], mg_plays[g] = s.plays[g];
    progress_from_save = 1;
}

void progress_save(void) BANKED {
    Save s;
    s.tag[0] = 'P', s.tag[1] = 'F', s.tag[2] = 'U', s.tag[3] = 'T';
    s.version = SAVE_VERSION;
    for (uint8_t i = 0; i < STAT_COUNT; i++) s.lv[i] = stat_lv[i], s.xp[i] = stat_xp[i];
    for (uint8_t g = 0; g < MG_COUNT; g++) s.best[g] = mg_best[g], s.plays[g] = mg_plays[g];
    s.sum = checksum(&s);
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy(SRAM_SAVE, &s, sizeof(Save));
    DISABLE_RAM;
}

void progress_reset(void) BANKED {
    fresh();
    progress_loaded = 1;
    progress_save();
}
