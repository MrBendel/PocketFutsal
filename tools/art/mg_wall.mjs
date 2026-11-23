// TARGET WALL (src/mg_wall.c): futsal goal wall with sliding shutter panels
// that open up into netted goal holes!
//
// BG:  20 x 20 tiles (rows 0..17 are the 160x144 screen; rows 18..19 cols 0..5
//      hold the 6x2 OPEN GOAL HOLE template tiles, copied into RAM by mg_wall.c
//      so set_panel() swaps a shutter panel into a real netted goal opening!).
//      Palettes (set in src/mg_wall.c):
//        0  green futsal court in perspective
//        1  closed steel shutter panel (riveted steel blue)
//        2  OPEN goal hole (dark net interior, crisp twine mesh, glowing gold rim)
//        3  GOAL! hit flash (electric green/white net flash)
//        4  stadium top rail (crimson/gold bays)
//        5  white futsal goalposts & crossbar with dark outlines
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode), loaded from MG_SPR_TILES.
import { BLANK, canvas, toBackground } from './common.mjs';

export const WALL_FOOT = 56;
export const gy = (d) => WALL_FOOT + 0.5 * d + 0.00202 * d * d;
export const sc = (d) => 0.75 + 0.25 * d / 104;
export const projX = (x, d) => 80 + (x - 80) * sc(d);

// 20 x 20 canvas: rows 0..17 = screen, rows 18..19 cols 0..5 = open-hole template.
const cv = canvas(20, 20, 0);
const { plot, rect, hline } = cv;

// Row 0 (y = 0..7): Stadium top rail (pal 4: 0 crimson, 1 gold, 2 navy, 3 white).
rect(0, 0, 160, 8, 2);
hline(0, 159, 0, 3);
hline(0, 159, 7, 2);
for (let x = 0; x < 160; x += 8) {
  const bay = ((x >> 3) & 1) ? 0 : 1;
  rect(x + 1, 1, 6, 5, bay);
  hline(x + 1, x + 6, 1, 3);
  plot(x + 1, 5, 2);
  plot(x + 6, 5, 2);
  hline(x + 1, x + 6, 6, 0);
}

// Row 1 (y = 8..15): White futsal crossbar (pal 5: 0 light grey, 1 mid steel, 2 dark outline, 3 crisp white).
rect(0, 8, 160, 8, 3);
hline(0, 159, 8, 2);
hline(0, 159, 9, 3);
hline(0, 159, 10, 3);
hline(0, 159, 11, 3);
hline(0, 159, 12, 0);
hline(0, 159, 13, 1);
hline(0, 159, 14, 1);
hline(0, 159, 15, 2);
// Corner goalpost caps at x = 0..7 and x = 152..159.
for (const x0 of [0, 152]) {
  for (let y = 8; y < 16; y++) { plot(x0, y, 2); plot(x0 + 7, y, 2); }
}

// Side goalposts (col 0 and col 19, rows 2..6, y = 16..55): crisp white futsal posts (pal 5).
function goalpost(x0) {
  rect(x0, 16, 8, 40, 3);
  for (let y = 16; y < 56; y++) {
    plot(x0, y, 2);
    plot(x0 + 1, y, 3);
    plot(x0 + 2, y, 3);
    plot(x0 + 3, y, 3);
    plot(x0 + 4, y, 0);
    plot(x0 + 5, y, 1);
    plot(x0 + 6, y, 1);
    plot(x0 + 7, y, 2);
  }
  hline(x0, x0 + 7, 55, 2);
}
goalpost(0);
goalpost(152);

// Six CLOSED steel shutter panels (48 x 16 each, rows 2..5, cols 1..18, pal 1).
export const PANEL_X0 = 8, PANEL_Y0 = 16, PANEL_W = 48, PANEL_H = 16;
function closedPanel(px, py) {
  rect(px, py, PANEL_W, PANEL_H, 0);
  // Outer dark frame
  hline(px, px + PANEL_W - 1, py, 2);
  hline(px, px + PANEL_W - 1, py + PANEL_H - 1, 2);
  for (let y = py; y < py + PANEL_H; y++) { plot(px, y, 2); plot(px + PANEL_W - 1, y, 2); }
  // Inner metallic bevel
  hline(px + 1, px + PANEL_W - 2, py + 1, 3);
  for (let y = py + 1; y < py + PANEL_H - 1; y++) plot(px + 1, y, 3);
  hline(px + 2, px + PANEL_W - 2, py + PANEL_H - 2, 1);
  for (let y = py + 2; y < py + PANEL_H - 1; y++) plot(px + PANEL_W - 2, y, 1);
  // Horizontal shutter slats (reads unmistakably as a CLOSED metal door!)
  for (const sy of [py + 5, py + 10]) {
    hline(px + 3, px + PANEL_W - 4, sy, 2);
    hline(px + 3, px + PANEL_W - 4, sy + 1, 1);
  }
  // Center lock emblem + corner rivets
  rect(px + 21, py + 6, 6, 4, 2);
  rect(px + 22, py + 7, 4, 2, 1);
  for (const [rx, ry] of [
    [px + 3, py + 3], [px + PANEL_W - 5, py + 3],
    [px + 3, py + PANEL_H - 5], [px + PANEL_W - 5, py + PANEL_H - 5],
  ]) {
    plot(rx, ry, 3); plot(rx + 1, ry + 1, 2);
  }
}
for (let r = 0; r < 2; r++) {
  for (let c = 0; c < 3; c++) closedPanel(PANEL_X0 + c * PANEL_W, PANEL_Y0 + r * PANEL_H);
}

// Row 6 (y = 48..55, cols 1..18): Goal-line lower kickboard / base strip (pal 5).
rect(8, 48, 144, 8, 1);
hline(8, 151, 48, 2);
hline(8, 151, 49, 3);
hline(8, 151, 50, 0);
for (let y = 51; y <= 54; y++) {
  for (let x = 8; x < 152; x++) plot(x, y, ((x & 3) === 0 || (y & 1) === 0) ? 2 : 0);
}
hline(8, 151, 55, 2);

// Rows 7..17 (y = 56..143): Vibrant emerald futsal pitch in perspective (pal 0).
function depthAt(y) {
  let d = 0;
  while (gy(d + 0.25) <= y) d += 0.25;
  return d;
}
const bandDepths = [0, 14, 30, 48, 68, 90, 120];
for (let y = 56; y < 144; y++) {
  const d = depthAt(y);
  let band = 0;
  while (band + 1 < bandDepths.length && d >= bandDepths[band + 1]) band++;
  const x0 = Math.round(projX(0, d)), x1 = Math.round(projX(160, d));
  for (let x = 0; x < 160; x++) {
    if (x < x0 || x > x1) plot(x, y, 2);
    else plot(x, y, band & 1);
  }
}
// Goal-line shadow at the foot of the wall.
hline(Math.round(projX(0, 0)), Math.round(projX(160, 0)), 56, 2);
hline(Math.round(projX(0, 0)), Math.round(projX(160, 0)), 57, 2);
// White sidelines, goal line, penalty box, and strike spot.
for (let y = 56; y < 144; y++) {
  const d = depthAt(y);
  const x0 = Math.round(projX(0, d)), x1 = Math.round(projX(160, d));
  const w = d >= 44 ? 2 : 1;
  for (let k = 0; k < w; k++) { plot(x0 + k, y, 3); plot(x1 - k, y, 3); }
}
const strikeY = Math.round(gy(56));
rect(Math.round(projX(0, 56)), strikeY, Math.round(projX(160, 56)) - Math.round(projX(0, 56)) + 1, 2, 3);
rect(79, 57, 2, strikeY - 57, 3);
rect(77, Math.round(gy(92)), 6, 1, 3);

// --- Rows 18..19, cols 0..5 (y = 144..159, x = 0..47): OPEN GOAL HOLE template! ---
// Palette 2 (open): 0 deep dark goal interior, 1 goal-net twine, 2 black shadow/border, 3 bright gold rim.
// Palette 3 (GOAL flash): 0 electric green, 1 bright mint net, 2 deep green border, 3 white flash.
(function openHoleTemplate(px, py) {
  // Deep dark goal interior (0)
  rect(px, py, PANEL_W, PANEL_H, 0);
  // Crisp diamond goal-net mesh inside the opening (1)
  for (let y = py + 2; y < py + PANEL_H - 2; y++) {
    for (let x = px + 2; x < px + PANEL_W - 2; x++) {
      if (((x + y) & 3) === 0 || ((x - y) & 3) === 0) plot(x, y, 1);
    }
  }
  // Inner top/left cast shadow inside the open cavity (2)
  hline(px + 2, px + PANEL_W - 3, py + 2, 2);
  for (let y = py + 2; y < py + PANEL_H - 2; y++) plot(px + 2, y, 2);
  // Glowing gold/white outer opening rim (3) with 1px dark outer edge (2)
  hline(px, px + PANEL_W - 1, py, 2);
  hline(px, px + PANEL_W - 1, py + PANEL_H - 1, 2);
  for (let y = py; y < py + PANEL_H; y++) { plot(px, y, 2); plot(px + PANEL_W - 1, y, 2); }
  hline(px + 1, px + PANEL_W - 2, py + 1, 3);
  hline(px + 1, px + PANEL_W - 2, py + PANEL_H - 2, 3);
  for (let y = py + 1; y < py + PANEL_H - 1; y++) {
    plot(px + 1, y, 3);
    plot(px + PANEL_W - 2, y, 3);
  }
})(0, 144);

export const BG = toBackground(cv, (tx, ty) => {
  if (ty === 0) return 4;
  if (ty === 1) return 5;
  if (ty <= 5) return tx === 0 || tx === 19 ? 5 : 1;
  if (ty === 6) return 5;
  if (ty >= 18) return 2;
  return 0;
});

// --- sprites (unchanged popups: +1..+9, MISS) ---
const PLUS = [
  '........', '...33...', '..3113..', '.311113.', '.311113.', '..3113..', '...33...', '........',
];
const DIGITS = {
  0: ['..3333..', '.311113.', '.313313.', '.313313.', '.313313.', '.311113.', '..3333..', '........'],
  1: ['...33...', '..3113..', '...313..', '...313..', '...313..', '..31113.', '..33333.', '........'],
  2: ['..3333..', '.311113.', '....313.', '..3313..', '.3133...', '.311113.', '.333333.', '........'],
  3: ['..3333..', '.311113.', '....313.', '..31113.', '....313.', '.311113.', '..3333..', '........'],
  4: ['.33..33.', '.313.313', '.3111113', '.3333113', '....313.', '....313.', '....333.', '........'],
  5: ['.333333.', '.3111113', '.313333.', '.311113.', '....313.', '.311113.', '..3333..', '........'],
  6: ['..3333..', '.311113.', '.31333..', '.311113.', '.313313.', '.311113.', '..3333..', '........'],
  7: ['.333333.', '.3111113', '....313.', '...313..', '..313...', '..313...', '..333...', '........'],
  8: ['..3333..', '.311113.', '.313313.', '..3113..', '.313313.', '.311113.', '..3333..', '........'],
  9: ['..3333..', '.311113.', '.313313.', '..31113.', '....313.', '.311113.', '..3333..', '........'],
};
const MISS_M = ['.33..33.', '31133113', '31111113', '31311313', '31333313', '313..313', '333..333', '........'];
const MISS_I = ['..3333..', '..3113..', '...313..', '...313..', '...313..', '..3113..', '..3333..', '........'];
const MISS_S = ['..3333..', '.311113.', '.31333..', '..3113..', '...3313.', '.311113.', '..3333..', '........'];

export const SPR = [
  PLUS, BLANK,
  ...Array.from({ length: 10 }, (_, d) => [DIGITS[d], BLANK]).flat(),
  MISS_M, BLANK,
  MISS_I, BLANK,
  MISS_S, BLANK,
];
