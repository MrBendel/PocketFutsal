// KEEPY-UPPY (src/mg_juggle.c): its background and its own sprite tiles.
//
// BG:  a training ground seen side-on, 20 x 18 tiles (the window's HUD covers the
//      bottom row): sky with clouds and two floodlights, a line of trees, the
//      advertising boards, then the grass the player juggles on (feet at y 124,
//      FLOOR_Y in src/mg_juggle.c). Palettes (set in src/mg_juggle.c enter):
//        0 sky:    0 pale sky, 1 cloud white, 2 deep sky, 3 dark (floodlights)
//        1 trees:  0 pale sky, 1 light green, 2 mid green, 3 dark (the same dark)
//        2 boards: 0 navy, 1 blue, 2 white, 3 red
//        3 grass:  0 light grass, 1 dark grass, 2 chalk, 3 shadow
// SPR: 8x16 pairs (top, then bottom) from MG_SPR_TILES, in the order of the T_*
//      offsets in src/mg_juggle.c (checked at the bottom of this file).
import { canvas, toBackground } from './common.mjs';

// ---------------------------------------------------------------------------
// The background.
const SKY_ROWS = 5;     // rows 0-4: palette 0
const TREE_ROWS = 11;   // rows 5-10: palette 1
const BOARD_ROWS = 13;  // rows 11-12: palette 2, then grass
const cv = canvas(20, 18, 0);
const { plot, rect, hline } = cv;

// Sky: deep blue at the top, dithered into pale sky by y 30.
for (let y = 0; y < 40; y++) {
  for (let x = 0; x < 160; x++) {
    let c = 0;
    if (y < 10) c = 2;
    else if (y < 16) c = (x + y) % 2 === 0 ? 2 : 0;
    else if (y < 24) c = (x + 2 * (y % 2)) % 4 === 0 ? 2 : 0;
    else if (y < 30) c = y % 2 === 0 && (x + 4 * ((y >> 1) % 2)) % 8 === 0 ? 2 : 0;
    plot(x, y, c);
  }
}
// Clouds: white puffs with flat bottoms (inside rows 0-4).
function cloud(cx, cy, puffs) {
  for (const [dx, dy, r] of puffs) {
    for (let y = -r; y <= r; y++) {
      for (let x = -r - 1; x <= r + 1; x++) {
        if ((x * x) / ((r + 1) * (r + 1)) + (y * y) / (r * r) <= 1 && dy + y <= 3) plot(cx + dx + x, cy + dy + y, 1);
      }
    }
  }
}
cloud(44, 14, [[0, 0, 4], [6, -3, 5], [13, 0, 4], [-6, 1, 3]]);
cloud(104, 26, [[0, 0, 3], [5, -2, 4], [11, 0, 3]]);
cloud(126, 9, [[0, 0, 3], [5, -1, 3]]);

// Floodlights: a lamp head of 3 x 2 lamps on a pole, one each side.
function floodlight(x0) {
  rect(x0 - 7, 6, 15, 10, 3);
  for (let ly = 0; ly < 2; ly++) for (let lx = 0; lx < 3; lx++) rect(x0 - 6 + lx * 5, 7 + ly * 4, 3, 3, 1);
  rect(x0 - 1, 16, 2, 60, 3);
  hline(x0 - 3, x0 + 2, 26, 3);  // a crossbar
}
floodlight(14);
floodlight(146);

// Trees: a bumpy canopy from y 40 (staying inside palette 1 rows 5-10),
// highlights up top, darker underneath.
const crowns = [];
for (let i = 0, x = -6; x < 170; i++) {
  const r = 9 + ((i * 7) % 5);
  crowns.push([x, 52 + ((i * 5) % 7) - 2, r]);
  x += 11 + ((i * 3) % 6);
}
for (let y = SKY_ROWS * 8; y < 88; y++) {
  for (let x = 0; x < 160; x++) {
    let inside = y >= 60;
    let hi = false;
    for (const [cx, cy, r] of crowns) {
      if ((x - cx) ** 2 + (y - cy) ** 2 <= r * r) {
        inside = true;
        // the upper left of each crown catches the light
        if ((x - cx + 3) ** 2 + (y - cy + 4) ** 2 <= (r - 4) * (r - 4) && y < cy) hi = true;
      }
    }
    if (!inside) continue;
    let c = 2;
    if (hi) c = ((x & 7) * 3 + (y & 7) * 5) % 7 === 0 ? 2 : 1;
    else if (y > 76) c = y > 82 || (x + y) % 2 === 0 ? 3 : 2;
    else if (((x & 7) * 5 + (y & 7) * 3) % 11 === 0) c = 3;  // leaf speckle
    else if (((x & 7) * 7 + (y & 7) * 2) % 13 === 0) c = 1;
    plot(x, y, c);
  }
}

// The advertising boards (y 88-103): a navy rim, panels of blue and red with
// white letters.
const FONT = {
  P: ['1110', '1001', '1001', '1110', '1000', '1000', '1000'],
  O: ['0110', '1001', '1001', '1001', '1001', '1001', '0110'],
  C: ['0111', '1000', '1000', '1000', '1000', '1000', '0111'],
  K: ['1001', '1010', '1100', '1100', '1010', '1001', '1001'],
  E: ['1111', '1000', '1000', '1110', '1000', '1000', '1111'],
  T: ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
  F: ['1111', '1000', '1000', '1110', '1000', '1000', '1000'],
  U: ['1001', '1001', '1001', '1001', '1001', '1001', '0110'],
  S: ['0111', '1000', '1000', '0110', '0001', '0001', '1110'],
  A: ['0110', '1001', '1001', '1111', '1001', '1001', '1001'],
  L: ['1000', '1000', '1000', '1000', '1000', '1000', '1111'],
  R: ['1110', '1001', '1001', '1110', '1010', '1001', '1001'],
  I: ['111', '010', '010', '010', '010', '010', '111'],
  N: ['1001', '1101', '1101', '1011', '1011', '1001', '1001'],
  G: ['0111', '1000', '1000', '1011', '1001', '1001', '0111'],
  '!': ['1', '1', '1', '1', '1', '0', '1'],
};
const textWidth = (s) => [...s].reduce((w, ch) => w + FONT[ch][0].length + 1, -1);
function boardText(s, cx, y, c) {
  let x = Math.round(cx - textWidth(s) / 2);
  for (const ch of s) {
    const g = FONT[ch];
    g.forEach((row, dy) => [...row].forEach((b, dx) => { if (b === '1') plot(x + dx, y + dy, c); }));
    x += g[0].length + 1;
  }
}
rect(0, 88, 160, 16, 0);
const panels = [['POCKET', 1], ['FUTSAL', 3], ['TRAIN', 1], ['GOAL!', 3]];
panels.forEach(([word, face], i) => {
  const x0 = i * 40;
  rect(x0 + 1, 90, 38, 12, face);
  hline(x0 + 1, x0 + 38, 90, 2);  // a white top edge
  boardText(word, x0 + 20, 93, 2);
});

// Grass: the boards' shadow, then mown bands that widen toward you, a chalk
// line far off, and a few tufts.
const bands = [[104, 106, 3], [106, 111, 1], [111, 117, 0], [117, 124, 1], [124, 132, 0], [132, 144, 1]];
for (const [y0, y1, c] of bands) rect(0, y0, 160, y1 - y0, c);
hline(0, 159, 109, 2);
for (let i = 0; i < 26; i++) {
  const x = ((i * 5) % 20) * 8 + 2 + (i % 3), y = 114 + ((i * 7 + (i >> 2)) % 3) * 8 + (i % 2) * 2;
  plot(x, y, 3), plot(x + 2, y, 3), plot(x + 1, y - 1, 3);
}

const palOf = (tx, ty) => (ty < SKY_ROWS ? 0 : ty < TREE_ROWS ? 1 : ty < BOARD_ROWS ? 2 : 3);
export const BG = toBackground(cv, palOf);

// ---------------------------------------------------------------------------
// Sprites. A picture (rows of '.0123', any multiple of 8 wide, 16 high) -> its
// 8x16 columns, each a top and a bottom tile.
function columns(pic) {
  const tiles = [];
  for (let x = 0; x < pic[0].length; x += 8) {
    tiles.push(pic.slice(0, 8).map((r) => r.slice(x, x + 8)));
    tiles.push(pic.slice(8, 16).map((r) => r.slice(x, x + 8)));
  }
  return tiles;
}
const blankPic = (w) => Array.from({ length: 16 }, () => Array(w).fill('.'));
const join = (pic) => pic.map((r) => r.join(''));

// The timing ring on the floor under the ball: six sizes, smallest first, each
// 16 wide (two columns). Dark: a shadow (color 3). Lit: bright filled core
// (color 1: gold under marks = PERFECT, white under cool = GOOD) with a crisp
// 1 px color 3 rim outside/around it so even the smallest size glows brightly.
const RING_SIZES = [[8, 4], [10, 4], [12, 4], [14, 5], [15, 5], [16, 6]];
function ring(w, h, lit) {
  const p = blankPic(16);
  const a = w / 2, b = h / 2;
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const dx = x + 0.5 - 8, dy = y + 0.5 - 8;
      if ((dx * dx) / (a * a) + (dy * dy) / (b * b) <= 1) p[y][x] = lit ? '1' : '3';
    }
  }
  if (!lit) return join(p);
  // A crisp 1 px rim around the core, leaving the central rows bright color 1.
  const q = p.map((r) => [...r]);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    if (p[y][x] !== '1') continue;
    const edge = [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([u, v]) => (p[y + v]?.[x + u] ?? '.') === '.');
    if (edge) q[y][x] = '3';
  }
  return join(q);
}

// A small 3x5 font for the words above the player, drawn with a 1 px outline.
const SMALL = {
  P: ['111', '101', '111', '100', '100'], E: ['111', '100', '110', '100', '111'],
  R: ['110', '101', '110', '101', '101'], F: ['111', '100', '110', '100', '100'],
  C: ['011', '100', '100', '100', '011'], T: ['111', '010', '010', '010', '010'],
  G: ['011', '100', '101', '101', '011'], O: ['010', '101', '101', '101', '010'],
  D: ['110', '101', '101', '101', '110'], M: ['101', '111', '111', '101', '101'],
  I: ['111', '010', '010', '010', '111'], S: ['011', '100', '010', '001', '110'],
  H: ['101', '101', '111', '101', '101'], A: ['010', '101', '111', '101', '101'],
  K: ['101', '110', '100', '110', '101'], N: ['101', '111', '111', '111', '101'],
  '!': ['1', '1', '1', '0', '1'], '+': ['000', '010', '111', '010', '000'],
  '2': ['110', '001', '010', '100', '111'], '3': ['110', '001', '010', '001', '110'],
  '4': ['101', '101', '111', '001', '001'],
};
/** A word -> 8x16 columns: letters in `fill` on rows 5-9, outlined in 3. */
function word(s, fill = '1') {
  const glyphs = [...s].map((ch) => SMALL[ch]);
  const tw = glyphs.reduce((w, g) => w + g[0].length + 1, -1);
  const w = Math.ceil((tw + 2) / 8) * 8;
  const p = blankPic(w);
  let x = Math.floor((w - tw) / 2);
  for (const g of glyphs) {
    g.forEach((row, dy) => [...row].forEach((b, dx) => {
      if (b !== '1') return;
      for (let v = -1; v <= 1; v++) for (let u = -1; u <= 1; u++) {
        const yy = 5 + dy + v, xx = x + dx + u;
        if (p[yy][xx] === '.') p[yy][xx] = '3';
      }
    }));
    g.forEach((row, dy) => [...row].forEach((b, dx) => { if (b === '1') p[5 + dy][x + dx] = fill; }));
    x += g[0].length + 1;
  }
  return columns(join(p));
}

const pad16 = (rows) => [...rows, ...Array(16 - rows.length).fill('........')];
// A sparkle for the milestones: two frames, a plus and a cross (centred on row 6).
const starA = pad16(['........', '........', '........', '...3....', '..313...', '.31113..', '3111113.', '.31113..', '..313...', '...3....']);
const starB = pad16(['........', '........', '........', '3.....3.', '.3...3..', '..313...', '..111...', '..313...', '.3...3..', '3.....3.']);
// A gust of wind: crisp white streaks with a cool blue underside.
const streak = pad16(['........', '........', '........', '........', '........', '1111111.', '.33333..', '........', '..111111', '...3333.']);

const parts = [];
const add = (name, tiles) => parts.push([name, tiles]);
RING_SIZES.forEach(([w, h], k) => add(`ring_dark_${k}`, columns(ring(w, h, false))));
RING_SIZES.forEach(([w, h], k) => add(`ring_lit_${k}`, columns(ring(w, h, true))));
add('perfect', word('PERFECT'));
add('good', word('GOOD'));
add('miss', word('MISS', '2'));
add('head', word('HEAD!'));
add('knee', word('KNEE!'));
add('plus2', word('+2'));
add('plus3', word('+3'));
add('plus4', word('+4'));
add('star', [...columns(starA), ...columns(starB)]);
add('streak', columns(streak));
add('nice', word('NICE!'));

// The offsets src/mg_juggle.c uses (T_*): keep the two in step.
const EXPECT = {
  ring_dark_0: 0, ring_lit_0: 24, perfect: 48, good: 56, miss: 62, head: 68, knee: 74,
  plus2: 80, plus3: 84, plus4: 88, star: 92, streak: 96, nice: 98,
};
let at = 0;
for (const [name, tiles] of parts) {
  if (name in EXPECT && EXPECT[name] !== at) throw new Error(`mg_juggle sprite ${name} at ${at}, src/mg_juggle.c expects ${EXPECT[name]}`);
  at += tiles.length;
}
export const SPR = parts.flatMap(([, tiles]) => tiles);
if (SPR.length > 112) throw new Error(`mg_juggle: ${SPR.length} sprite tiles`);
