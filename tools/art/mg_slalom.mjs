// CONE SLALOM (src/mg_slalom.c): seamless 20x4 vertical-scrolling training pitch
// and 8x16 sprites (striped traffic cones, gate target chevron, popups).
//
// BG palettes (set in src/mg_slalom.c):
//   0  turf: 0 bright emerald, 1 darker mowed stripe, 2 deep apron/shadow, 3 crisp white lines
//   1  side boards: 0 dark navy frame, 1 crimson panel, 2 gold rail/rivets, 3 bright cream trim
// SPR palettes (reusing SPR_PAL_TRAIL_POWER / SPR_PAL_TRAIL_CURVE in src/mg_slalom.c):
//   SPR_PAL_TRAIL_POWER (3): cone palette — 1 crisp white stripe, 2 vivid orange, 3 dark outline
//   SPR_PAL_TRAIL_CURVE (4): gate & popup palette — 1 bright white, 2 gold/lime glow, 3 dark outline
import { BLANK, canvas, toBackground } from './common.mjs';

function buildBg() {
  // 20 x 4 tiles (160 x 32 px): tiles seamlessly vertically for hardware scroll_y.
  const cv = canvas(20, 4, 0);

  // Mowed emerald grass bands: top 16 px light (0), bottom 16 px darker (1).
  cv.rect(16, 0, 128, 16, 0);
  cv.rect(16, 16, 128, 16, 1);

  // Dark forest apron outside the sidelines + subtle shadow along the boards.
  cv.rect(16, 0, 6, 32, 2);
  cv.rect(138, 0, 6, 32, 2);

  // Crisp 2 px white sidelines at x = 22..23 and x = 136..137.
  cv.rect(22, 0, 2, 32, 3);
  cv.rect(136, 0, 2, 32, 3);

  // Subtle dashed center track guide (8 px dash, 8 px gap).
  cv.rect(79, 4, 2, 8, 3);
  cv.rect(79, 20, 2, 8, 3);

  // Subtle grass-tuft accents (color 2) on the mowed bands.
  for (const [tx, ty] of [[38, 6], [116, 11], [54, 22], [102, 26]]) {
    cv.plot(tx, ty, 2);
    cv.plot(tx - 1, ty + 1, 2);
    cv.plot(tx + 1, ty + 1, 2);
  }

  // Stadium side perimeter boards (left cols 0..1, right cols 18..19, pal 1).
  for (const side of [0, 1]) {
    const x0 = side === 0 ? 0 : 144;
    // Outer dark wall base (0)
    cv.rect(x0, 0, 16, 32, 0);
    // Crimson stadium board face (1)
    cv.rect(x0 + 2, 1, 12, 30, 1);
    // Horizontal panel seams every 16 px
    cv.hline(x0, x0 + 15, 0, 0);
    cv.hline(x0, x0 + 15, 16, 0);
    // Gold vertical rail & rivets (2)
    const railX = side === 0 ? x0 + 13 : x0 + 2;
    cv.vline(railX, 0, 31, 2);
    cv.vline(side === 0 ? x0 + 14 : x0 + 1, 0, 31, 3);
    // Inner bright trim & panel rivets
    for (const ry of [4, 12, 20, 28]) {
      cv.plot(x0 + 4, ry, 2);
      cv.plot(x0 + 10, ry, 2);
    }
    // Decorative gold stripe block in each 16 px panel
    cv.rect(x0 + 5, 6, 5, 4, 2);
    cv.rect(x0 + 6, 7, 3, 2, 3);
    cv.rect(x0 + 5, 22, 5, 4, 2);
    cv.rect(x0 + 6, 23, 3, 2, 3);
  }

  return toBackground(cv, (tx) => (tx < 2 || tx >= 18 ? 1 : 0));
}

export const BG = buildBg();

// 8x16 striped traffic cone: top 8x8 + bottom 8x8 (1 white, 2 orange, 3 outline).
const coneTop = [
  '........',
  '...33...',
  '..3223..',
  '..3223..',
  '..3113..',
  '.311113.',
  '.322223.',
  '.322223.',
];
const coneBot = [
  '.311113.',
  '31111113',
  '32222223',
  '32222223',
  '33222233',
  '32222223',
  '33333333',
  '........',
];

// Tilted / knocked cone (when bumped).
const coneHitTop = [
  '........',
  '........',
  '....33..',
  '...3223.',
  '..31123.',
  '.32113..',
  '32223...',
  '3333....',
];
const coneHitBot = [
  '.33333..',
  '3222223.',
  '3333333.',
  '........',
  '........',
  '........',
  '........',
  '........',
];

// Gate target chevron (centered between the next gate's cones): bright gold/white.
const gateChevron = [
  '.33..33.',
  '31233213',
  '32122123',
  '.321123.',
  '..3113..',
  '...33...',
  '........',
  '........',
];

// Popups: "+1", "+2", "HI"/"T!", "OU"/"T!" (outlined 8x8 in top half of 8x16).
const plus1 = [
  '....33..',
  '.3.3113.',
  '313.313.',
  '313.313.',
  '.3..313.',
  '...31113',
  '...33333',
  '........',
];
const plus2 = [
  '...3333.',
  '.3.32123',
  '31333213',
  '313.3123',
  '.3.31233',
  '...31113',
  '...33333',
  '........',
];
const hitL = [
  '33.33.33',
  '31313313',
  '31113313',
  '31313313',
  '31313313',
  '33.33.33',
  '........',
  '........',
];
const hitR = [
  '33333.33',
  '33133313',
  '.313.313',
  '.313..33',
  '.313.313',
  '.333..33',
  '........',
  '........',
];

// Order must match T_* constants in src/mg_slalom.c (2 tiles per 8x16 sprite).
export const SPR = [
  coneTop, coneBot,       // +0  T_CONE
  coneHitTop, coneHitBot, // +2  T_CONE_HIT
  gateChevron, BLANK,     // +4  T_CHEVRON
  plus1, BLANK,           // +6  T_PLUS1
  plus2, BLANK,           // +8  T_PLUS2
  hitL, BLANK,            // +10 T_HIT_L
  hitR, BLANK,            // +12 T_HIT_R
];
