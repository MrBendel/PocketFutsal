// WALL RALLY (src/mg_rally.c): street-futsal / squash rebound wall court!
//
// BG:  20 x 18 screen tiles.
//      Palettes (set in src/mg_rally.c):
//        0  two-tone street-futsal court floor (slate-teal surface + crisp white lines)
//        1  warm red-brick side piers & upper wall
//        2  painted plaster rebound face with white target frame & angle zone marks
//        3  overhead night floodlight rafters
//        4  crimson squash TIN / fault strip along the bottom of the wall (z < 8)
// SPR: impact starburst + "PT!", "TIN", "OUT" callout popups (8x16 pairs).
import { BLANK, canvas, toBackground } from './common.mjs';

export const WALL_FOOT = 56;
export const gy = (d) => WALL_FOOT + 0.5 * d + 0.00202 * d * d;
export const sc = (d) => 0.75 + 0.25 * d / 104;
export const projX = (x, d) => 80 + (x - 80) * sc(d);

const cv = canvas(20, 18, 0);
const { plot, rect, hline } = cv;

// Row 0 (y = 0..7): Overhead floodlight truss (pal 3: 0 dark steel, 1 lamp glow, 2 midnight, 3 bright bulb).
rect(0, 0, 160, 8, 2);
hline(0, 159, 0, 0);
hline(0, 159, 7, 0);
for (let x = 4; x < 160; x += 24) {
  rect(x, 1, 12, 5, 0);
  rect(x + 2, 2, 8, 3, 3);
  hline(x + 1, x + 10, 5, 1);
}

// Rows 1..5 (y = 8..47): Brick outer piers (cols 0..1 and 18..19, pal 1) + painted rebound face (cols 2..17, pal 2).
// Brick pattern helper (0 mortar/light, 1 mid brick, 2 deep shadow, 3 highlight).
function brickRect(x0, y0, w, h) {
  rect(x0, y0, w, h, 1);
  for (let y = y0; y < y0 + h; y += 4) {
    hline(x0, x0 + w - 1, y, 2);
    const shift = ((y >> 2) & 1) ? 4 : 0;
    for (let x = x0 + shift; x < x0 + w; x += 8) {
      for (let dy = 1; dy < 4 && y + dy < y0 + h; dy++) plot(x, y + dy, 2);
    }
    if (y + 1 < y0 + h) hline(x0, x0 + w - 1, y + 1, 0);
  }
}
brickRect(0, 8, 16, 40);
brickRect(144, 8, 16, 40);
// Top brick cornice across row 1 (y = 8..15).
brickRect(16, 8, 128, 8);

// Painted rebound target face (cols 2..17, rows 2..5 => x = 16..143, y = 16..47, pal 2:
// 0 concrete light, 1 concrete mid, 2 dark seam, 3 crisp painted white).
rect(16, 16, 128, 32, 0);
// Subtle concrete block seams
for (let y = 24; y < 48; y += 8) hline(16, 143, y, 1);
for (let x = 32; x < 144; x += 16) {
  for (let y = 16; y < 48; y++) if ((y & 1) === 0) plot(x, y, 1);
}
// Bold white outer rebound target rectangle
hline(16, 143, 16, 3);
hline(16, 143, 17, 3);
hline(16, 143, 46, 3);
for (let y = 16; y < 48; y++) {
  plot(16, y, 3); plot(17, y, 3);
  plot(142, y, 3); plot(143, y, 3);
}
// Dashed vertical 1/3 zone guides (left angle / center / right angle) + center bullseye cross
for (let y = 20; y < 45; y += 4) {
  plot(56, y, 3); plot(56, y + 1, 3);
  plot(103, y, 3); plot(103, y + 1, 3);
}
hline(76, 83, 31, 3);
hline(76, 83, 32, 3);
for (let y = 28; y <= 35; y++) { plot(79, y, 3); plot(80, y, 3); }

// Row 6 (y = 48..55): Crimson Squash TIN / Fault Strip (pal 4: 0 red, 1 dark red, 2 black border, 3 white foul tape).
rect(0, 48, 160, 8, 0);
hline(0, 159, 48, 3);  // crisp white top tin tape!
hline(0, 159, 49, 3);
hline(0, 159, 50, 2);
for (let x = 6; x < 156; x += 12) {
  rect(x, 52, 6, 2, 1);
}
hline(0, 159, 55, 2);

// Rows 7..17 (y = 56..143): Two-tone squash-futsal court in perspective (pal 0:
// 0 light court teal, 1 deep court teal, 2 dark out-of-bounds apron, 3 crisp white lines).
for (let y = WALL_FOOT; y < 144; y++) {
  let d = 0;
  while (d < 126 && gy(d + 1) <= y) d++;
  const xl = Math.round(projX(0, d));
  const xr = Math.round(projX(160, d));
  const band = (Math.floor(d / 16) & 1) ? 1 : 0;
  for (let x = 0; x < 160; x++) {
    if (x < xl - 1 || x > xr + 1) plot(x, y, 2);
    else if (x <= xl || x >= xr) plot(x, y, 3);
    else plot(x, y, band);
  }
}
// Short line across at depth 48 + center half-court T-line from depth 48 to 108 (squash boxes!).
const shortY = Math.round(gy(48));
const shortL = Math.round(projX(0, 48));
const shortR = Math.round(projX(160, 48));
rect(shortL, shortY, shortR - shortL + 1, 2, 3);
for (let y = shortY; y < 136; y++) {
  plot(79, y, 3);
  plot(80, y, 3);
}
// Left & right service box ticks at depth 76.
const boxY = Math.round(gy(76));
hline(Math.round(projX(0, 76)), Math.round(projX(28, 76)), boxY, 3);
hline(Math.round(projX(132, 76)), Math.round(projX(160, 76)), boxY, 3);

export const BG = toBackground(cv, (tx, ty) => {
  if (ty === 0) return 3;
  if (ty === 1) return 1;
  if (ty <= 5) return tx <= 1 || tx >= 18 ? 1 : 2;
  if (ty === 6) return 4;
  return 0;
});

// --- sprites (8x16 pairs from MG_SPR_TILES) ---
const STAR = [
  '...33...', '..3113..', '.313313.', '31311313', '31311313', '.313313.', '..3113..', '...33...',
];
const CH_P = ['.33333..', '.311113.', '.313313.', '.311113.', '.31333..', '.313....', '.333....', '........'];
const CH_T = ['.333333.', '.3111113', '...313..', '...313..', '...313..', '...313..', '...333..', '........'];
const CH_BANG = ['..333...', '..313...', '..313...', '..313...', '..333...', '..313...', '..333...', '........'];
const CH_I = ['..3333..', '..3113..', '...313..', '...313..', '...313..', '..3113..', '..3333..', '........'];
const CH_N = ['.33..33.', '.313.313', '.3113313', '.3111113', '.3131113', '.313.313', '.333..33', '........'];
const CH_O = ['..3333..', '.311113.', '.313313.', '.313313.', '.313313.', '.311113.', '..3333..', '........'];
const CH_U = ['.33..33.', '.313.313', '.313.313', '.313.313', '.313.313', '.3111113', '..3333..', '........'];

export const SPR = [
  STAR, BLANK,     // 0: wall impact starburst
  CH_P, BLANK,     // 2: P
  CH_T, BLANK,     // 4: T
  CH_BANG, BLANK,  // 6: !
  CH_I, BLANK,     // 8: I
  CH_N, BLANK,     // 10: N
  CH_O, BLANK,     // 12: O
  CH_U, BLANK,     // 14: U
];
