// CONE SLALOM (src/mg_slalom.c): TimberMan-style two-lane dribble sprint!
// Seamless 20x4 vertical-scrolling training pitch (two 16 px grass bands matching
// STEP_PX = 16 so cones stay locked to the turf as you step!) and 8x16 sprites
// (striped cones, flying hit cone, checkered finish banner, +2S! penalty popup, turf dust).
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

  // Dashed center lane divider (splits Left dribble lane x=52 & Right dribble lane x=108).
  cv.rect(79, 3, 2, 10, 3);
  cv.rect(79, 19, 2, 10, 3);

  // Subtle grass-tuft accents (color 2) on the mowed bands.
  for (const [tx, ty] of [[36, 6], [120, 11], [66, 22], [94, 26]]) {
    cv.plot(tx, ty, 2);
    cv.plot(tx - 1, ty + 1, 2);
    cv.plot(tx + 1, ty + 1, 2);
  }

  // Stadium side perimeter boards (left cols 0..1, right cols 18..19, pal 1).
  for (const side of [0, 1]) {
    const x0 = side === 0 ? 0 : 144;
    cv.rect(x0, 0, 16, 32, 0);
    cv.rect(x0 + 2, 1, 12, 30, 1);
    cv.hline(x0, x0 + 15, 0, 0);
    cv.hline(x0, x0 + 15, 16, 0);
    const railX = side === 0 ? x0 + 13 : x0 + 2;
    cv.vline(railX, 0, 31, 2);
    cv.vline(side === 0 ? x0 + 14 : x0 + 1, 0, 31, 3);
    for (const ry of [4, 12, 20, 28]) {
      cv.plot(x0 + 4, ry, 2);
      cv.plot(x0 + 10, ry, 2);
    }
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

// Checkered finish line banner strip (8x8 top tile).
const finishChecker = [
  '33333333',
  '11331133',
  '11331133',
  '33113311',
  '33113311',
  '11331133',
  '33333333',
  '........',
];

// Penalty popups: "+2", "+1", "S!" (outlined 8x8 in top half of 8x16).
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
const secBang = [
  '.3333.33',
  '31223313',
  '3123.313',
  '.3123313',
  '..312333',
  '32213313',
  '.333..33',
  '........',
];

// Turf dust puff kicked up behind boots on each dribble tap.
const turfDust = [
  '........',
  '..3..3..',
  '.313313.',
  '31211213',
  '.332233.',
  '........',
  '........',
  '........',
];

export const SPR = [
  coneTop, coneBot,       // +0: T_CONE
  coneHitTop, coneHitBot, // +2: T_CONE_HIT
  finishChecker, BLANK,   // +4: T_FINISH
  plus2, BLANK,           // +6: T_PLUS2
  plus1, BLANK,           // +8: T_PLUS1
  secBang, BLANK,         // +10: T_SBANG ("S!")
  turfDust, BLANK,        // +12: T_DUST
];
