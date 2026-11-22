// SHOOTOUT (src/mg_shootout.c): its background and its own sprite tiles.
//
// BG:  20 x 18 tiles, the camera behind the penalty spot facing a futsal goal.
//      Top to bottom: the kick scoreboard (rows 0-1, palette 4), the crowd (rows
//      2-8, palette 2), the boards (rows 9-10, palette 1), and the court (rows
//      11-17, palette 0) with its goal line, the 6 m line and the spot. The goal
//      (palette 3) stands in front of the crowd and boards: its posts sit on tile
//      edges so every tile keeps to one palette. src/mg_shootout.c sets the colors
//      and places its sprites to match the measurements below (GOAL_*, SPOT_*).
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode: top, then bottom), loaded into
//      sprite VRAM bank 0 from MG_SPR_TILES. Order matters: src/mg_shootout.c T_*.
import { canvas, toBackground } from './common.mjs';

// --- measurements (src/mg_shootout.c must match) ---
const GOAL_L = 32, GOAL_R = 127;   // the posts' outer edges (4 px wide)
const GOAL_TOP = 32, GOAL_LINE = 88;  // the crossbar's top, the goal line
const SPOT_X = 80, SPOT_Y = 118;
const SLOTS_YOU = [24, 34, 44, 54, 64], SLOTS_CPU = [88, 98, 108, 118, 128], SLOT_Y = 4;

const cv = canvas(20, 18, 0);
const { plot, rect, hline, vline, stamp } = cv;

// --- the scoreboard (palette 4: 0 panel, 1 text, 2 slot ring, 3 gold) ---
rect(0, 0, 160, 16, 0);
hline(0, 159, 0, 2);
hline(0, 159, 14, 3);
hline(0, 159, 15, 2);
const FONT = {
  Y: ['3...3', '3...3', '.3.3.', '..3..', '..3..', '..3..', '..3..'],
  O: ['.333.', '3...3', '3...3', '3...3', '3...3', '3...3', '.333.'],
  U: ['3...3', '3...3', '3...3', '3...3', '3...3', '3...3', '.333.'],
  C: ['.3333', '3....', '3....', '3....', '3....', '3....', '.3333'],
  P: ['3333.', '3...3', '3...3', '3333.', '3....', '3....', '3....'],
};
const word = (w, x, y, c) => [...w].forEach((ch, i) =>
  stamp(FONT[ch].map((r) => r.replaceAll('3', String(c))), x + i * 6, y));
word('YOU', 3, 4, 1);
word('CPU', 141, 4, 1);
const ring = ['..2222..', '.2....2.', '2......2', '2......2', '2......2', '2......2', '.2....2.', '..2222..'];
for (const x of [...SLOTS_YOU, ...SLOTS_CPU]) stamp(ring, x, SLOT_Y);
// A little gold ball between the two teams' kicks.
stamp(['.333.', '33.33', '3.3.3', '33.33', '.333.'], 77, 5);

// --- the crowd (palette 2: 0 stands, 1 red shirts, 2 faces, 3 gold shirts) ---
// One fan a tile, four kinds, so the crowd costs only a few tiles.
const FANS = [
  ['........', '...22...', '..2222..', '...22...', '.111111.', '11111111', '11111111', '........'],
  ['........', '........', '...22...', '..2222..', '..3223..', '.333333.', '33333333', '........'],
  ['........', '..22....', '.2222...', '..22....', '111111..', '1111111.', '11111111', '........'],
  ['........', '....22..', '...2222.', '....22..', '..333333', '.3333333', '33333333', '........'],
];
let seed = 7;
const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) >> 16;
for (let ty = 2; ty <= 8; ty++) {
  for (let tx = 0; tx < 20; tx++) {
    rect(tx * 8, ty * 8, 8, 8, 0);
    if (rnd() % 7 !== 0) stamp(FANS[rnd() & 3], tx * 8, ty * 8);
  }
}

// --- the boards (palette 1: 0 navy floor, 1 blue face, 2 dark, 3 white) ---
rect(0, 72, 160, 16, 1);
hline(0, 159, 72, 2);
hline(0, 159, 73, 3);
hline(0, 159, 74, 2);
rect(0, 84, 160, 4, 2);
// Framed stadium ad panels with crisp borders and chevron/block graphics.
for (const [x0, w] of [[2, 28], [130, 28]]) {
  rect(x0, 76, w, 7, 2);
  rect(x0 + 1, 77, w - 2, 5, 1);
  for (let x = x0 + 3; x < x0 + w - 3; x += 4) rect(x, 78, 2, 3, 3);
}

// --- the court (palette 0: 0 court, 1 court dark, 2 navy, 3 line) ---
const bands = [[88, 97, 0], [98, 109, 1], [110, 123, 0], [124, 143, 1]];
for (const [y0, y1, c] of bands) rect(0, y0, 160, y1 - y0 + 1, c);
hline(0, 159, GOAL_LINE, 3);
hline(0, 159, GOAL_LINE + 1, 3);
// The 6 m line: straight through the spot, its ends curving up to the goal line.
let prev = null;
for (let x = 0; x < 160; x++) {
  const d = Math.max(0, Math.abs(x - SPOT_X) - 28);
  const y = Math.round(SPOT_Y - (d * d) / 100);
  const from = prev === null ? y : Math.min(prev, y), to = prev === null ? y : Math.max(prev, y);
  for (let yy = from; yy <= to; yy++) plot(x, yy, 3);
  plot(x, y + 1, 3);
  prev = y;
}
// The spot (the ball sits on it) and a dark patch where the goal shades the floor.
rect(SPOT_X - 2, SPOT_Y - 1, 5, 4, 3);
for (let x = GOAL_L + 4; x <= GOAL_R - 4; x++) if ((x & 1) === 0) plot(x, GOAL_LINE + 2, 1);

// --- the goal (palette 3: 0 the net's dark inside, 1 mesh, 2 white posts, 3 grey) ---
rect(GOAL_L, GOAL_TOP, GOAL_R - GOAL_L + 1, GOAL_LINE - GOAL_TOP, 0);
// The back of the net, smaller (further away), and the side nets' edges to it.
const BL = 46, BR = 113, BT = 42, BB = 82;
for (let y = GOAL_TOP + 4; y < GOAL_LINE; y++) {
  for (let x = GOAL_L + 4; x <= GOAL_R - 4; x++) {
    const inBack = x >= BL && x <= BR && y >= BT && y <= BB;
    const p = inBack ? 5 : 6;  // the back net's mesh is finer: it's further away
    if ((x + y) % p === 0 || (x - y + 600) % p === 0) plot(x, y, 1);
  }
}
hline(BL, BR, BT, 3); hline(BL, BR, BB, 3); vline(BL, BT, BB, 3); vline(BR, BT, BB, 3);
const line = (x0, y0, x1, y1, c) => {
  const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0));
  for (let i = 0; i <= n; i++) plot(x0 + ((x1 - x0) * i) / n, y0 + ((y1 - y0) * i) / n, c);
};
line(GOAL_L + 4, GOAL_TOP + 4, BL, BT, 3);
line(GOAL_R - 4, GOAL_TOP + 4, BR, BT, 3);
line(GOAL_L + 4, GOAL_LINE - 1, BL, BB, 3);
line(GOAL_R - 4, GOAL_LINE - 1, BR, BB, 3);
// The frame: crisp 1px dark outer outline, bright white posts/crossbar, grey inner shade.
rect(GOAL_L, GOAL_TOP, GOAL_R - GOAL_L + 1, 4, 2);
rect(GOAL_L, GOAL_TOP, 4, GOAL_LINE - GOAL_TOP, 2);
rect(GOAL_R - 3, GOAL_TOP, 4, GOAL_LINE - GOAL_TOP, 2);
hline(GOAL_L, GOAL_R, GOAL_TOP, 0);
vline(GOAL_L, GOAL_TOP, GOAL_LINE - 1, 0);
vline(GOAL_R, GOAL_TOP, GOAL_LINE - 1, 0);
hline(GOAL_L + 3, GOAL_R - 3, GOAL_TOP + 3, 3);
vline(GOAL_L + 3, GOAL_TOP + 3, GOAL_LINE - 1, 3);
vline(GOAL_R - 3, GOAL_TOP + 3, GOAL_LINE - 1, 3);

function palOf(tx, ty) {
  if (ty <= 1) return 4;
  const inGoal = tx >= GOAL_L / 8 && tx <= GOAL_R >> 3 && ty >= GOAL_TOP / 8 && ty < GOAL_LINE / 8;
  if (inGoal) return 3;
  if (ty <= 8) return 2;
  if (ty <= 10) return 1;
  return 0;
}

export const BG = toBackground(cv, palOf);

// --- sprites ---
const blank = Array(8).fill('........');
// A kick on the scoreboard (sprite palette 3: 1 green, 2 red, 3 ink).
const markGoal = ['..3333..', '.311113.', '31111113', '31111113', '31111113', '31111113', '.311113.', '..3333..'];
const markMiss = ['33....33', '323..323', '.323323.', '..3223..', '..3223..', '.323323.', '323..323', '33....33'];

// The power meter (sprite palette 4: 1 green, 2 red, 3 ink), 8 x 64: the top 8
// too strong (red), the bottom 18 too weak (hatched), between them a good kick.
const meter = Array.from({ length: 64 }, (_, y) => {
  const p = 63 - y;
  if (y === 0 || y === 63) return '.333333.';
  const fill = p > 55 ? '2' : p >= 18 ? '1' : (y & 1 ? '3' : '.');
  return '3' + fill.repeat(6) + '3';
});
const meterTiles = [];
for (let s = 0; s < 4; s++) {
  meterTiles.push(meter.slice(s * 16, s * 16 + 8), meter.slice(s * 16 + 8, s * 16 + 16));
}
// The meter's cursor, pointing at it from the left (palette 7: 1 gold, 3 dark), its tip on row 3.
const cursor = ['33......', '3133....', '311133..', '31111133', '311133..', '3133....', '33......', '........'];
// The sweet spot's edges, pointing at the meter from the right (tip on row 2).
const tick = ['..333...', '.3113...', '31113...', '.3113...', '..333...', '........', '........', '........'];

// Banner letters, 8 x 16 (palette: 1 highlight, 2 body, 3 outline), from 6 x 10 shapes.
const SHAPES = {
  G: ['.####.', '##..##', '##....', '##....', '##.###', '##..##', '##..##', '##..##', '##..##', '.####.'],
  O: ['.####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.'],
  A: ['..##..', '.####.', '##..##', '##..##', '##..##', '######', '##..##', '##..##', '##..##', '##..##'],
  L: ['##....', '##....', '##....', '##....', '##....', '##....', '##....', '##....', '######', '######'],
  '!': ['..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '......', '..##..', '..##..'],
  S: ['.####.', '##..##', '##....', '###...', '.####.', '...###', '....##', '....##', '##..##', '.####.'],
  V: ['##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '.####.', '.####.', '..##..'],
  E: ['######', '######', '##....', '##....', '#####.', '#####.', '##....', '##....', '######', '######'],
  D: ['#####.', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '##..##', '#####.'],
  R: ['#####.', '##..##', '##..##', '##..##', '#####.', '####..', '##.##.', '##..##', '##..##', '##..##'],
  W: ['##..##', '##..##', '##..##', '##..##', '##..##', '######', '######', '######', '###.##', '##..##'],
  I: ['######', '######', '..##..', '..##..', '..##..', '..##..', '..##..', '..##..', '######', '######'],
};
export const LETTERS = 'GOAL!SVEDRWI';  // src/mg_shootout.c letter_tile() follows this order
function letterTiles(shape) {
  const g = Array.from({ length: 16 }, () => Array(8).fill('.'));
  shape.forEach((r, y) => [...r].forEach((ch, x) => { if (ch === '#') g[y + 3][x + 1] = '2'; }));
  // Highlight the top of each stroke.
  for (let y = 15; y >= 1; y--) for (let x = 0; x < 8; x++) if (g[y][x] === '2' && g[y - 1][x] === '.') g[y][x] = '1';
  // Outline.
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 8; x++) {
      if (g[y][x] !== '.') continue;
      for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [-1, 1], [1, -1], [-1, -1]]) {
        const v = g[y + dy]?.[x + dx];
        if (v === '1' || v === '2') { g[y][x] = '3'; break; }
      }
    }
  }
  const rows = g.map((r) => r.join(''));
  return [rows.slice(0, 8), rows.slice(8, 16)];
}

export const SPR = [
  markGoal, blank,   // T_MARK_GOAL 0
  markMiss, blank,   // T_MARK_MISS 2
  ...meterTiles,     // T_METER 4 .. 11
  cursor, blank,     // T_CURSOR 12
  tick, blank,       // T_TICK 14
  ...[...LETTERS].flatMap((ch) => letterTiles(SHAPES[ch])),  // T_LETTERS 16 ..
];
