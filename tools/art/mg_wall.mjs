// TARGET WALL (src/mg_wall.c): its background and its own sprite tiles.
//
// BG:  20 x 18 tiles (the window's HUD covers the bottom row). A Mario Tennis
//      GBC-inspired training wall: a striped stadium rail across the top, a
//      riveted steel surround framing six metallic target panels (3 across, 2
//      high, each 6 x 2 tiles), a crisp rebound barrier, and a vibrant green
//      court in perspective with white lines and alternating grass bands.
//      Palettes (set in src/mg_wall.c):
//        0  green court (floor)
//        1  unlit target panel (riveted steel blue)
//        2  lit target panel (glowing gold/amber)
//        3  hit target panel (electric green/white flash)
//        4  stadium top rail (crimson/gold accent)
//        5  steel wall frame & rebound barrier
//      The game lights a panel by rewriting its 12 tiles' attributes
//      (palette 1 -> 2 -> 3), so every panel is drawn alike and only ever holds
//      panel pixels.
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode: top, then bottom), loaded into
//      sprite VRAM bank 0 after the shared ball/shadow tiles (MG_SPR_TILES in
//      src/minigame.h). Colors 1-3 (0 is see-through).
import { BLANK, canvas, toBackground } from './common.mjs';

// --- the perspective (src/mg_wall.c's tables gy_tab / s_tab are these, sampled
// every 2 px of depth). Depth d: 0 at the wall's foot, ~104 at the bottom of the
// playfield. World x: 0..160 across at the near end; the wall is 3/4 as wide.
export const WALL_FOOT = 56;  // screen line of the wall's foot
export const gy = (d) => WALL_FOOT + 0.5 * d + 0.00202 * d * d;
export const sc = (d) => 0.75 + 0.25 * d / 104;
export const projX = (x, d) => 80 + (x - 80) * sc(d);

// --- the background ---
const cv = canvas(20, 18, 0);
const { plot, rect, hline } = cv;

// Row 0 (y = 0..7): Stadium top rail (palette 4: 0 crimson, 1 gold, 2 navy, 3 white).
// Crisp alternating gold & crimson stadium bays with dark posts and a bright top trim.
rect(0, 0, 160, 8, 2);
hline(0, 159, 0, 3);
hline(0, 159, 7, 2);
for (let x = 0; x < 160; x += 8) {
  const bay = ((x >> 3) & 1) ? 0 : 1;
  rect(x + 1, 1, 6, 5, bay);
  hline(x + 1, x + 6, 1, 3);       // top highlight
  plot(x + 1, 5, 2);               // subtle lower corner shadow
  plot(x + 6, 5, 2);
  hline(x + 1, x + 6, 6, 0);       // crimson lower trim strip
}

// Row 1 (y = 8..15): Riveted steel header beam (palette 5: 0 slate, 1 deep steel, 2 navy, 3 silver).
rect(0, 8, 160, 8, 0);
hline(0, 159, 8, 3);
hline(0, 159, 9, 0);
hline(0, 159, 13, 1);
hline(0, 159, 14, 1);
hline(0, 159, 15, 2);
for (let x = 4; x < 160; x += 16) {
  plot(x, 10, 3); plot(x + 1, 11, 2);
  plot(x + 8, 10, 3); plot(x + 9, 11, 2);
}

// Side pillars (col 0 and col 19, rows 2..5, y = 16..47): riveted steel struts (palette 5).
function pillar(x0) {
  rect(x0, 16, 8, 32, 0);
  for (let y = 16; y < 48; y++) {
    plot(x0, y, 2);
    plot(x0 + 1, y, 3);
    plot(x0 + 6, y, 1);
    plot(x0 + 7, y, 2);
  }
  for (let y = 16; y < 48; y += 16) {
    hline(x0, x0 + 7, y, 2);
    hline(x0 + 1, x0 + 6, y + 1, 3);
    plot(x0 + 3, y + 7, 3);
    plot(x0 + 4, y + 8, 2);
  }
}
pillar(0);
pillar(152);

// The six target panels: 48 x 16 each (rows 2..5, cols 1..18).
// Palettes 1 (unlit), 2 (lit), 3 (hit):
//   0 = panel face, 1 = mid-dark/rings, 2 = deep navy outline, 3 = bright highlight/core.
export const PANEL_X0 = 8, PANEL_Y0 = 16, PANEL_W = 48, PANEL_H = 16;
function panel(px, py) {
  rect(px, py, PANEL_W, PANEL_H, 0);
  // Crisp outer frame
  hline(px, px + PANEL_W - 1, py, 2);
  hline(px, px + PANEL_W - 1, py + PANEL_H - 1, 2);
  for (let y = py; y < py + PANEL_H; y++) {
    plot(px, y, 2);
    plot(px + PANEL_W - 1, y, 2);
  }
  // Inner bevel: bright top-left, shaded bottom-right
  hline(px + 1, px + PANEL_W - 2, py + 1, 3);
  for (let y = py + 1; y < py + PANEL_H - 1; y++) plot(px + 1, y, 3);
  hline(px + 2, px + PANEL_W - 2, py + PANEL_H - 2, 1);
  for (let y = py + 2; y < py + PANEL_H - 1; y++) plot(px + PANEL_W - 2, y, 1);

  // Four corner rivets (bright top-left glint, dark bottom-right shadow)
  for (const [rx, ry] of [
    [px + 3, py + 3],
    [px + PANEL_W - 5, py + 3],
    [px + 3, py + PANEL_H - 5],
    [px + PANEL_W - 5, py + PANEL_H - 5],
  ]) {
    plot(rx, ry, 3);
    plot(rx + 1, ry + 1, 2);
  }

  // Side accent dashes (flanking the target bullseye)
  hline(px + 4, px + 7, py + 7, 1);
  hline(px + 4, px + 7, py + 8, 2);
  hline(px + PANEL_W - 8, px + PANEL_W - 5, py + 7, 1);
  hline(px + PANEL_W - 8, px + PANEL_W - 5, py + 8, 2);

  // Target bullseye (crisp ellipse in the center)
  const cx = px + 23.5, cy = py + 7.5;
  const ell = (rx, ry, c) => {
    for (let y = py + 2; y < py + PANEL_H - 2; y++) {
      for (let x = px + 8; x < px + PANEL_W - 8; x++) {
        const u = (x - cx) / rx, v = (y - cy) / ry;
        if (u * u + v * v <= 1) plot(x, y, c);
      }
    }
  };
  ell(13.5, 5.8, 2);  // crisp dark outer ring for maximum pop
  ell(12.2, 5.0, 1);  // outer colored ring
  ell(8.6, 3.6, 0);   // inner face gap
  ell(5.6, 2.5, 1);   // inner colored ring
  ell(2.8, 1.4, 3);   // bright glowing bullseye center
}
for (let r = 0; r < 2; r++) {
  for (let c = 0; c < 3; c++) panel(PANEL_X0 + c * PANEL_W, PANEL_Y0 + r * PANEL_H);
}

// Row 6 (y = 48..55): Rebound kickboard / athletic barrier (palette 5).
// White top tape, crisp net-like dark/steel grid pattern, dark bottom shadow.
rect(0, 48, 160, 8, 1);
hline(0, 159, 48, 2);
hline(0, 159, 49, 3);
hline(0, 159, 50, 3);
for (let y = 51; y <= 54; y++) {
  for (let x = 0; x < 160; x++) {
    plot(x, y, ((x & 3) === 0 || (y & 1) === 0) ? 2 : 0);
  }
}
hline(0, 159, 55, 2);

// Rows 7..17 (y = 56..143): Vibrant green court in perspective (palette 0).
// 0 = bright court green, 1 = deeper court green band, 2 = dark border/shadow, 3 = chalk white.
function depthAt(y) {
  let d = 0;
  while (gy(d + 0.25) <= y) d += 0.25;
  return d;
}

// Alternating perspective grass/court bands + darker green outside the sidelines.
const bandDepths = [0, 14, 30, 48, 68, 90, 120];
for (let y = 56; y < 144; y++) {
  const d = depthAt(y);
  const xl = Math.round(projX(0, d));
  const xr = Math.round(projX(160, d));
  let b = 0;
  while (b + 1 < bandDepths.length && d >= bandDepths[b + 1]) b++;
  const grass = b & 1;
  for (let x = 0; x < 160; x++) {
    if (x < xl || x > xr) plot(x, y, 2);
    else plot(x, y, grass);
  }
}
// Wall-foot shadow right under the kickboard.
hline(0, 159, 56, 2);
hline(0, 159, 57, 2);

// Perspective sidelines (world x 0 and 160), crisp white.
function sideline(wx) {
  for (let y = 58; y < 144; y++) {
    const d = depthAt(y);
    const x = Math.round(projX(wx, d));
    plot(x, y, 3);
    if (sc(d) > 0.88) plot(x + (wx < 80 ? 1 : -1), y, 3);
  }
}
sideline(0);
sideline(160);

// Center service line (world x = 80) from the wall foot to the strike line (depth 56).
const strikeY = Math.round(gy(56));
for (let y = 58; y <= strikeY; y++) {
  plot(79, y, 3);
  plot(80, y, 3);
}

// Strike line across (world depth 56): stay behind it.
{
  const xa = Math.round(projX(0, 56));
  const xb = Math.round(projX(160, 56));
  hline(xa, xb, strikeY, 3);
  hline(xa, xb, strikeY + 1, 3);
}

// Subtle center tick on the near side (at depth 96) for player alignment reference.
{
  const y = Math.round(gy(96));
  hline(77, 82, y, 3);
}

// Palettes by tile:
//   row 0        -> 4 (stadium rail)
//   row 1, row 6 -> 5 (steel header & kickboard)
//   rows 2..5    -> cols 1..18: 1 (unlit panel), cols 0 & 19: 5 (steel pillars)
//   rows 7..17   -> 0 (green court)
const palOf = (tx, ty) => {
  if (ty === 0) return 4;
  if (ty === 1 || ty === 6) return 5;
  if (ty >= 2 && ty <= 5) return tx >= 1 && tx <= 18 ? 1 : 5;
  return 0;
};
export const BG = toBackground(cv, palOf);

// --- sprites ---
// Glyphs 5 wide x 7 tall ('#' filled), drawn into the top tile of a pair with a
// dark outline (color 3) round a fill (color `fill`); the bottom tile is blank.
const GLYPHS = {
  '+': ['.....', '..#..', '..#..', '#####', '..#..', '..#..', '.....'],
  '0': ['.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'],
  '1': ['..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'],
  '2': ['.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'],
  '3': ['####.', '....#', '....#', '.###.', '....#', '....#', '####.'],
  '4': ['...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'],
  '5': ['#####', '#....', '####.', '....#', '....#', '#...#', '.###.'],
  '6': ['.###.', '#....', '#....', '####.', '#...#', '#...#', '.###.'],
  '7': ['#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'],
  '8': ['.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'],
  '9': ['.###.', '#...#', '#...#', '.####', '....#', '....#', '.###.'],
  'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
  'I': ['.###.', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'],
  'S': ['.####', '#....', '#....', '.###.', '....#', '....#', '####.'],
};
function glyph(ch, fill) {
  const g = GLYPHS[ch];
  const px = Array.from({ length: 8 }, () => new Array(8).fill('.'));
  // glyph at (1, 0)..(5, 6); outline round it
  for (let y = 0; y < 7; y++) for (let x = 0; x < 5; x++) {
    if (g[y][x] !== '#') continue;
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      const X = x + 1 + dx, Y = y + dy;
      if (X >= 0 && X < 8 && Y >= 0 && Y < 8 && px[Y][X] === '.') px[Y][X] = '3';
    }
  }
  for (let y = 0; y < 7; y++) for (let x = 0; x < 5; x++) if (g[y][x] === '#') px[y][x + 1] = String(fill);
  return px.map((r) => r.join(''));
}

// Sprite tile numbers (relative to MG_SPR_TILES), src/mg_wall.c's WT_*:
//   0  '+'            (pairs: top glyph, bottom blank)
//   2  '0' .. 20 '9'  (digit n at 2 + 2n)
//   22 'M', 24 'I', 26 'S'  (the MISS banner, in red)
export const SPR = [
  glyph('+', 1), BLANK,
  ...'0123456789'.split('').flatMap((ch) => [glyph(ch, 1), BLANK]),
  ...'MIS'.split('').flatMap((ch) => [glyph(ch, 2), BLANK]),
];
