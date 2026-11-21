// Placeholder art, until PocketFutsal has real artwork. Sprites are ASCII:
// '.' or '0' is color 0 (see-through on sprites), '1'-'3' colors 1-3 (which actual
// colors is up to the palette: src/palettes.c). The pitch is drawn in code, a
// pixel at a time, then cut into unique 8x8 tiles and a map (tools/assets.mjs
// writes build/gen/pitch.c: its tiles, map and attributes).

/** 8 strings -> 16 bytes of 2bpp. */
export function asciiTile(rows, name = 'tile') {
  if (rows.length !== 8 || rows.some((r) => r.length !== 8)) throw new Error(`${name}: a tile is 8 rows of 8 characters`);
  const out = [];
  for (const row of rows) {
    let lo = 0, hi = 0;
    for (let x = 0; x < 8; x++) {
      const c = row[x] === '.' ? 0 : Number(row[x]);
      if (!(c >= 0 && c <= 3)) throw new Error(`${name}: bad color '${row[x]}'`);
      lo |= (c & 1) << (7 - x);
      hi |= (c >> 1) << (7 - x);
    }
    out.push(lo, hi);
  }
  return out;
}

// ---------------------------------------------------------------------------
// The pitch: one screen, 20 x 17 tiles (160 x 136 px) above the HUD row.
// Must match src/game.h: the field is x 8..151, y 8..127 inside the boards; the
// goal mouths are y 48..87 in the end boards (x 0..7 and 152..159).
export const PITCH_W = 20, PITCH_H = 17;
const W = PITCH_W * 8, H = PITCH_H * 8;
const FIELD_X0 = 8, FIELD_X1 = 151, FIELD_Y0 = 8, FIELD_Y1 = 127;
const MOUTH_Y0 = 48, MOUTH_Y1 = 87;
const CX = 80, CY = 68;
// Background palettes (src/gfx.h PLAY_PAL_*): field, boards, nets.
const PAL_FIELD = 0, PAL_BOARD = 1, PAL_NET = 2;

const inMouth = (y) => y >= MOUTH_Y0 && y <= MOUTH_Y1;

/** A tile's palette: the nets fill the end boards' mouths; the rest of the edge is boards. */
function tilePal(tx, ty) {
  const edgeX = tx === 0 || tx === PITCH_W - 1;
  if (edgeX && inMouth(ty * 8) && inMouth(ty * 8 + 7)) return PAL_NET;
  if (edgeX || ty === 0 || ty === PITCH_H - 1) return PAL_BOARD;
  return PAL_FIELD;
}

const ring = (x, y, cx, cy, r) => {
  const d = Math.hypot(x + 0.5 - cx, y + 0.5 - cy);
  return d > r - 0.6 && d < r + 0.6;
};

/** One pixel's color (0-3) in its tile's palette. */
function pixel(x, y) {
  const pal = tilePal(x >> 3, y >> 3);
  if (pal === PAL_NET) {
    // Field palette colors: 0 the dark inside of the goal, 1 the net, 3 the posts.
    if (y === MOUTH_Y0 || y === MOUTH_Y1) return 3;
    const lx = x < CX ? x : W - 1 - x;  // mirrored: the back of the net is the outer edge
    if (lx === 0) return 1;
    return (lx + y) % 4 === 0 || (lx - y + 400) % 4 === 0 ? 1 : 0;
  }
  if (pal === PAL_BOARD) {
    // The boards, seen from a little above: 1 face, 2 dark top and shadow, 3 a white
    // stripe along the field side. Distance from the field edge, 0 nearest.
    let d;
    if (y < FIELD_Y0) d = FIELD_Y0 - 1 - y;
    else if (y > FIELD_Y1) d = y - FIELD_Y1 - 1;
    else if (x < FIELD_X0) d = FIELD_X0 - 1 - x;
    else d = x - FIELD_X1 - 1;
    // Corners and the ends beside the mouths: whichever edge is nearer.
    if ((y < FIELD_Y0 || y > FIELD_Y1) && (x < FIELD_X0 || x > FIELD_X1)) {
      const dx = x < FIELD_X0 ? FIELD_X0 - 1 - x : x - FIELD_X1 - 1;
      d = Math.min(d, dx);
    }
    // The goalposts: white where the mouth meets the board.
    if ((x < FIELD_X0 || x > FIELD_X1) && (y === MOUTH_Y0 - 1 || y === MOUTH_Y1 + 1)) return 3;
    if (d === 0) return 2;
    if (d === 1) return 3;
    if (d >= 6) return 2;
    // A row of little rivets on the face.
    const along = y < FIELD_Y0 || y > FIELD_Y1 ? x : y;
    return d === 4 && along % 8 === 3 ? 2 : 1;
  }
  // The field: grass in 16 px stripes (0 light, 1 dark), lines in 3, 2 for the
  // spots' shading.
  const line =
    x === FIELD_X0 || x === FIELD_X1 || y === FIELD_Y0 || y === FIELD_Y1 ||  // touchlines and goal lines
    x === CX ||                                                              // halfway line
    ring(x, y, CX + 0.5, CY, 16) ||                                          // centre circle
    (ring(x, y, FIELD_X0, CY, 28) && x > FIELD_X0) ||                        // the penalty areas: arcs
    (ring(x, y, FIELD_X1 + 1, CY, 28) && x < FIELD_X1);
  if (line) return 3;
  const spot = (sx, sy) => Math.abs(x + 0.5 - sx) <= 1 && Math.abs(y + 0.5 - sy) <= 1;
  if (spot(CX + 0.5, CY) || spot(FIELD_X0 + 40, CY) || spot(FIELD_X1 + 1 - 40, CY)) return 3;
  return ((x - FIELD_X0) >> 4) & 1;
}

/** The pitch as unique tiles, a 20x17 map of tile indices and one of palettes. */
function buildPitch() {
  const tiles = [], keys = new Map(), map = [], attrs = [];
  for (let ty = 0; ty < PITCH_H; ty++) {
    for (let tx = 0; tx < PITCH_W; tx++) {
      const rows = [];
      for (let y = 0; y < 8; y++) {
        let r = '';
        for (let x = 0; x < 8; x++) r += pixel(tx * 8 + x, ty * 8 + y);
        rows.push(r);
      }
      const pal = tilePal(tx, ty);
      const key = rows.join('');  // the palette is an attribute: same pixels, same tile
      if (!keys.has(key)) {
        keys.set(key, tiles.length);
        tiles.push(rows);
      }
      map.push(keys.get(key));
      attrs.push(pal);
    }
  }
  return { tiles, map, attrs, w: PITCH_W, h: PITCH_H };
}
export const PITCH = buildPitch();

// The HUD's charge meter (on the window, UI palette: 2 the bar, 3 its rim), after
// the pitch's tiles: empty, half, full. src/play.c: T_METER.
const meterEmpty = ['33333333', '........', '........', '........', '........', '........', '........', '33333333'];
const meterHalf = ['33333333', '2222....', '2222....', '2222....', '2222....', '2222....', '2222....', '33333333'];
const meterFull = ['33333333', '22222222', '22222222', '22222222', '22222222', '22222222', '22222222', '33333333'];
export const HUD_TILES = [meterEmpty, meterHalf, meterFull];

// ---------------------------------------------------------------------------
// Sprites, in 8x16 mode: each 16x16 picture is two 8x16 columns (left, then
// right), each its top and bottom tile. Facing left is a flip in X (src/play.c
// swaps the columns). Colors: 1 skin, 2 jersey, 3 outline, hair, shorts, boots.
const top = [
  '......3333......',
  '.....333333.....',
  '.....3331113....',
  '.....3311113....',
  '......311113....',
  '.......3333.....',
];
const body = [
  '.....33222233...',
  '....3122222213..',
  '....3122222213..',
  '.....32222223...',
  '.....33333333...',
  '.....33333333...',
];
const player = (legs) => [...top, ...body, ...legs];
const stand = player(['......31..13....', '......31..13....', '.....333..333...', '................']);
const runA = player(['.....31....13...', '....31......13..', '...333......333.', '................']);
const runB = player(['......3113......', '.......31.......', '......3333......', '................']);
const kick = player(['......31...3113.', '......31....333.', '.....333........', '................']);
// The keeper, arms out wide to save.
const save = [
  ...top,
  '.1..33222233..1.',
  '.13122222222131.',
  '..312222222213..',
  '.....32222223...',
  '.....33333333...',
  '.....33333333...',
  '......31..13....',
  '......31..13....',
  '.....333..333...',
  '................',
];

/** A 16x16 picture -> its two 8x16 columns, as four 8x8 tiles (left top, left bottom, right top, right bottom). */
function columns(pic, name) {
  if (pic.length !== 16 || pic.some((r) => r.length !== 16)) throw new Error(`${name}: a sprite is 16 rows of 16 characters`);
  const col = (x0) => pic.map((r) => r.slice(x0, x0 + 8));
  const l = col(0), r = col(8);
  return [l.slice(0, 8), l.slice(8), r.slice(0, 8), r.slice(8)];
}

// The ball (8x8, the top of an 8x16 pair): 1 white, 2 its patches, 3 outline.
// Two frames, so it rolls.
const blank = Array(8).fill('........');
const ballA = ['..3333..', '.311213.', '31122113', '31211113', '31111213', '31121113', '.312111.', '..3333..'];
const ballB = ['..3333..', '.312113.', '31111213', '32111123', '31211113', '31112213', '.311113.', '..3333..'];
// Its shadow on the grass (3), and the marker over your player (1, outlined 3).
const shadow = ['........', '........', '........', '........', '..3333..', '.333333.', '..3333..', '........'];
const marker = ['.333333.', '.311113.', '..3113..', '...33...', '........', '........', '........', '........'];

// Order matters: src/play.c names them (S_STAND ... S_MARKER), 2 tiles per 8x16
// column, 4 per 16x16 frame.
export const SPRITE_TILES = [
  ...columns(stand, 'stand'),  // S_STAND
  ...columns(runA, 'runA'),    // S_RUN_A
  ...columns(runB, 'runB'),    // S_RUN_B
  ...columns(kick, 'kick'),    // S_KICK
  ...columns(save, 'save'),    // S_SAVE
  ballA, blank,                // S_BALL
  ballB, blank,                // S_BALL + 2
  shadow, blank,               // S_SHADOW
  marker, blank,               // S_MARKER
];
