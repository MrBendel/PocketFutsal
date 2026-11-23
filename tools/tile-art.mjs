// Placeholder art, until PocketFutsal has real artwork. Sprites are ASCII:
// '.' or '0' is color 0 (see-through on sprites), '1'-'3' colors 1-3 (which actual
// colors is up to the palette: src/palettes.c). The court is drawn in code, a
// pixel at a time, then cut into unique 8x8 tiles and a map (tools/assets.mjs
// writes build/gen/pitch.c: its tiles, map and attributes).
//
// Proportions follow Mario Tennis (GBC), measured from its sheets (docs/design).

import {BALL_FRAMES} from './ball-art.mjs';

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
// The court: futnet singles (9 m x 12.8 m: each half a little wider than deep).
// Play happens in flat court coordinates (src/game.h): x 8..152 across, depth y 0
// (the far baseline, the CPU's) to 224 (the near one, yours), the net across
// halfway. It's drawn in perspective, like Mario Tennis: the far half squashed, the
// far end narrower. These must match src/game.h and proj_init() in src/play.c.
const COURT_X0 = 8, COURT_X1 = 152, CX = 80;
const FAR = 0, NEAR = 224, HALF = 112;
const NET_H = 10;        // px: the net's height (src/game.h)
const COURT_SY = 32;     // the far baseline's map row, in px
const PROJ_K0 = 92, PROJ_K1 = 164;  // rows per depth unit, /256, at the far and near lines
const PROJ_S0 = 111;     // the width scale at the far line, /128 (128 at the near one)

/** Depth -> map row (px). The integral of a row height growing from K0 to K1. */
export const projY = (fy) => COURT_SY + Math.floor((PROJ_K0 * fy * 448 + (PROJ_K1 - PROJ_K0) * fy * fy) / (448 * 256));
/** Depth -> width scale, /128. */
export const projS = (fy) => PROJ_S0 + Math.floor(((128 - PROJ_S0) * fy) / NEAR);
/** A court point -> screen x. */
export const projX = (fx, fy) => CX + Math.floor(((fx - CX) * projS(fy)) / 128);

// The map: 20 x 21 tiles (160 x 168 px); the camera scrolls it up and down 32 px.
export const PITCH_W = 20, PITCH_H = 21;
const W = PITCH_W * 8, H = PITCH_H * 8;
// Background palettes (src/gfx.h PLAY_PAL_*): the court (everything but the
// boards), the boards along the top and bottom.
const PAL_FIELD = 0, PAL_BOARD = 1;
// Court palette colors: 0 the court, 1 its darker bands, 2 the floor round it
// (and the net's mesh), 3 lines, the net's tape and posts.
const C_COURT = 0, C_BAND = 1, C_FLOOR = 2, C_LINE = 3;

const BOARD_TOP = 8;      // map rows above this are the far boards
const BOARD_BOTTOM = 160; // and from this down, the near ones

function drawCourt(futsal = false) {
  const px = Array.from({ length: H }, () => new Array(W).fill(C_FLOOR));
  const plot = (x, y, c) => {
    if (x >= 0 && x < W && y >= 0 && y < H) px[y][x] = c;
  };
  // The depth at each row (the last depth projecting onto it), for the surface.
  const depthAt = new Map();
  for (let q = 0; q <= NEAR * 8; q++) depthAt.set(projY(q / 8), q / 8);
  for (const [y, fy] of depthAt) {
    for (let x = 0; x < W; x++) {
      const fx = CX + ((x + 0.5 - CX) * 128) / projS(fy);
      if (fx >= COURT_X0 && fx <= COURT_X1) px[y][x] = Math.floor(fy / 28) & 1 ? C_BAND : C_COURT;
    }
  }
  // Lines: 2 px in the near half, 1 px in the far (as Mario Tennis draws them).
  const line = (fx, fy) => {
    const x = projX(fx, fy), y = projY(fy);
    plot(x, y, C_LINE);
    if (fy > HALF) {
      plot(x + 1, y, C_LINE);
      plot(x, y + 1, C_LINE);
      plot(x + 1, y + 1, C_LINE);
    }
  };
  // The lines: the sidelines, the baselines, and the centre line under the net.
  // A ball landing on a line is in.
  for (let t = 0; t <= 1; t += 1 / 2048) {
    const fy = FAR + t * (NEAR - FAR), fx = COURT_X0 + t * (COURT_X1 - COURT_X0);
    line(COURT_X0, fy);
    line(COURT_X1 - 1, fy);
    line(fx, FAR);
    line(fx, NEAR - 1);
    line(fx, HALF);
  }
  if (futsal) {
    // Open futsal court: goal mouths and penalty areas at both ends.
    for (const end of [FAR, NEAR]) {
      const inward = end === FAR ? 1 : -1;
      for (let t = 0; t <= 1; t += 1 / 256) {
        line(56 + t * 48, end - inward * 12);
        line(56, end - inward * t * 12);
        line(104, end - inward * t * 12);
        line(40 + t * 80, end + inward * 32);
        line(40, end + inward * t * 32);
        line(120, end + inward * t * 32);
      }
  }
  } else {
    // The net across halfway: NET_H px high, a dark mesh under a white tape, a post
    // at each end a little outside the sidelines.
    const hy = projY(HALF), hx0 = projX(COURT_X0 - 6, HALF), hx1 = projX(COURT_X1 + 5, HALF);
    for (let y = hy - NET_H; y < hy; y++) {
      for (let x = hx0; x <= hx1; x++) {
        if (y <= hy - NET_H + 1) plot(x, y, C_LINE);
        else if ((x + y) % 2 === 0) plot(x, y, C_FLOOR);
      }
  }
    for (let y = hy - NET_H - 2; y <= hy + 1; y++) plot(hx0, y, C_LINE), plot(hx1, y, C_LINE);
  }
  // The boards, top and bottom (their own palette): 1 face, 2 dark top and shadow,
  // 3 a white stripe on the court side, rivets.
  for (let y = 0; y < H; y++) {
    if (y >= BOARD_TOP && y < BOARD_BOTTOM) continue;
    const d = y < BOARD_TOP ? BOARD_TOP - 1 - y : y - BOARD_BOTTOM;  // 0 nearest the court
    for (let x = 0; x < W; x++) px[y][x] = d === 0 ? 2 : d === 1 ? 3 : d >= 6 ? 2 : d === 4 && x % 8 === 3 ? 2 : 1;
  }
  if (futsal) {
    // Club Court: a quiet blue playing surface with detailed green surrounds.
    for (let y = 0; y < 32; y++) for (let x = 0; x < W; x++) plot(x, y, 0);
    for (let y = 3; y <= 16; y++) for (let x = 0; x < W; x++) {
      plot(x, y, y === 3 || y === 16 || x % 24 === 0 ? 1 : 2);
      if (y > 4 && y < 15 && ((x + y) % 6 === 0 || (x - y + 180) % 6 === 0)) plot(x, y, 3);
    }
    for (let x = 0; x < W; x += 24) {
      for (let y = 1; y < 19; y++) plot(x, y, 1), plot(x + 1, y, 3);
    }
    // Small shrubs around the apron; side planting stays outside the touchlines.
    const bush = (cx, cy, top = false) => {
      for (let dy = -3; dy <= 3; dy++) for (let dx = -3; dx <= 3; dx++) {
        if (dx * dx + dy * dy > 11) continue;
        plot(cx + dx, cy + dy, top ? (Math.abs(dx) + Math.abs(dy) > 3 ? 2 : 1) : (Math.abs(dx) + Math.abs(dy) > 3 ? 1 : 0));
      }
      plot(cx - 1, cy - 2, 0);
    };
    for (const x of [6, 22, 38, 122]) bush(x, 25, true);
    for (const y of [45, 69, 101, 129]) bush(3, y), bush(156, y);
    for (let x = 0; x < W; x += 8) for (let y = 38; y < H; y += 16) {
      if (x < 5 || x > 154 || y > 154) plot(x, y, 1);
    }
    // Bench occupies whole palette-aligned tiles in the top-right apron.
    for (let y = 16; y < 32; y++) for (let x = 128; x < 152; x++) plot(x, y, 0);
    for (let y = 20; y <= 26; y++) for (let x = 130; x <= 149; x++) plot(x, y, y % 3 === 0 ? 2 : 1);
    for (const x of [132, 147]) for (let y = 26; y <= 29; y++) plot(x, y, 2);
    // Center circle is projected from the same flat coordinates as the ball.
    for (let a = 0; a < Math.PI * 2; a += 0.025) line(80 + Math.cos(a) * 21, HALF + Math.sin(a) * 21);
    line(80, HALF);
    // Goal mouths match collision coordinates x=56..104. Dark net interiors
    // and a cream frame read clearly against the bright court and grass.
    for (const end of [FAR, NEAR]) {
      const y = projY(end), left = projX(56, end), right = projX(104, end);
      const back = y + (end === FAR ? -9 : 9);
      for (let gy = Math.min(y, back); gy <= Math.max(y, back); gy++) for (let gx = left; gx <= right; gx++) {
        const rim = gx === left || gx === right || gy === back;
        plot(gx, gy, rim || ((gx + gy) % 5 === 0 && gy !== y) ? 3 : 1);
      }
      for (let gx = left; gx <= right; gx++) plot(gx, y, 3);
    }
  }
  return px;
}

/** The court as unique tiles, a 20x21 map of tile indices and one of palettes. */
function buildPitch(futsal = false) {
  const px = drawCourt(futsal);
  const tiles = [], keys = new Map(), map = [], attrs = [];
  for (let ty = 0; ty < PITCH_H; ty++) {
    for (let tx = 0; tx < PITCH_W; tx++) {
      const rows = [];
      for (let y = 0; y < 8; y++) rows.push(px[ty * 8 + y].slice(tx * 8, tx * 8 + 8).join(''));
      let pal = ty * 8 < BOARD_TOP || ty * 8 >= BOARD_BOTTOM ? PAL_BOARD : PAL_FIELD;
      if (futsal) {
        const sy = ty * 8, fy = Math.min(NEAR, Math.max(0, (sy - COURT_SY) * 2));
        pal = sy < 32 ? 1 : sy >= projY(NEAR) || tx * 8 + 7 < projX(COURT_X0, fy) || tx * 8 > projX(COURT_X1, fy) ? 3 : 0;
        if (ty >= 2 && ty < 4 && tx >= 16 && tx < 19) pal = 2;
      }
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
// the court's tiles: empty, half, full. src/play.c: T_METER.
const meterEmpty = ['33333333', '........', '........', '........', '........', '........', '........', '33333333'];
const meterHalf = ['33333333', '2222....', '2222....', '2222....', '2222....', '2222....', '2222....', '33333333'];
const meterFull = ['33333333', '22222222', '22222222', '22222222', '22222222', '22222222', '22222222', '33333333'];
export const HUD_TILES = [meterEmpty, meterHalf, meterFull];

// ---------------------------------------------------------------------------
// The players: 16 x 24, Mario Tennis's size (its players are 14-16 x 23). In 8x16
// sprite mode a frame is 4 sprites: two columns, each a 16-row top and an 8-row
// bottom (the bottom sprite's second tile blank). Two views: from behind (you, at
// the bottom of the court) and from the front (the CPU). Colors: 1 skin, 2 jersey
// and socks, 3 outline, hair, shorts, boots. Drawn kicking to the right; src/play.c
// flips them.
const HEAD_FRONT = [
  '.....333333.....',
  '....33333333....',
  '...3333333333...',
  '...3311111133...',
  '...3131111313...',
  '...3111111113...',
  '....31133113....',
  '.....311113.....',
  '......3113......',
];
const HEAD_BACK = [
  '.....333333.....',
  '....33333333....',
  '...3333333333...',
  '...3333333333...',
  '..133333333331..',
  '...3333333333...',
  '....33333333....',
  '.....311113.....',
  '......3113......',
];
const TORSO = {
  down: ['...3322222233...', '..312222222213..', '..312222222213..', '..313222222313..', '...3222222223...'],
  run: ['...3322222233...', '..312222222213..', '.3122222222223..', '.313222222222313', '...3222222223...'],
  wide: ['3333322222233333', '1113222222223111', '...3222222223...', '...3222222223...', '...3222222223...'],
};
const LEGS = {
  stand: [
    '....33333333....', '....33333333....', '....333..333....', '....313..313....', '....313..313....',
    '....313..313....', '....323..323....', '....323..323....', '...3333..3333...', '................',
  ],
  runA: [
    '....33333333....', '....33333333....', '...333....333...', '...313....313...', '..313......313..',
    '..313......313..', '..323......323..', '..323......323..', '.3333......3333.', '................',
  ],
  runB: [
    '....33333333....', '....33333333....', '.....333333.....', '.....313313.....', '.....313313.....',
    '.....3133333....', '.....323.333....', '.....323........', '....3333........', '................',
  ],
  kick: [
    '....33333333....', '....3333333333..', '....333...33333.', '....313.....3133', '....313......323',
    '....313.......33', '....323.........', '....323.........', '...3333.........', '................',
  ],
  volley: [
    '....333333333333', '....333333332233', '....333.....3333', '....313.........', '....313.........',
    '....313.........', '....323.........', '....323.........', '...3333.........', '................',
  ],
  // Knees bent (8 rows: the frame is 2 rows shorter).
  crouch: [
    '...3333333333...', '...3333..3333...', '..313......313..', '..313......313..', '..323......323..',
    '..323......323..', '.3333......3333.', '................',
  ],
};
const BLANK2 = ['................', '................'];

/** Each view's frames, in src/play.c's order (F_STAND ...). */
function poses(head) {
  return {
    stand: [...head, ...TORSO.down, ...LEGS.stand],
    runA: [...head, ...TORSO.run, ...LEGS.runA],
    runB: [...head, ...TORSO.run, ...LEGS.runB],
    ready: [...BLANK2, ...head, ...TORSO.wide, ...LEGS.crouch],
    kick: [...head, ...TORSO.wide, ...LEGS.kick],
    volley: [...head, ...TORSO.wide, ...LEGS.volley],
    header: [...head, ...TORSO.wide, ...LEGS.crouch, ...BLANK2],  // in the air, knees up
    bicycle: null,  // filled in below: the kick upside down
    win: [...head, ...TORSO.wide, ...LEGS.stand],
    lose: [...BLANK2, ...head, ...TORSO.down, ...LEGS.crouch],
  };
}
const turn180 = (pic) => pic.map((r) => [...r].reverse().join('')).reverse();
/** 16 wide x 24 high -> 24 x 16, turned a quarter clockwise (the head to the right). */
const turn90 = (pic) => Array.from({ length: 16 }, (_, y) => Array.from({ length: 24 }, (_, x) => pic[23 - x][y]).join(''));

const blank = Array(8).fill('........');
const cut = (pic, x, y) => pic.slice(y, y + 8).map((r) => r.slice(x, x + 8));

/** A standing frame -> 8 tiles: left column (top pair, then bottom and a blank), then the right. */
function upright(pic, name) {
  if (pic.length !== 24 || pic.some((r) => r.length !== 16)) throw new Error(`${name}: a frame is 24 rows of 16`);
  return [cut(pic, 0, 0), cut(pic, 0, 8), cut(pic, 0, 16), blank, cut(pic, 8, 0), cut(pic, 8, 8), cut(pic, 8, 16), blank];
}
/** A lying frame (24 x 16) -> 8 tiles: three 8x16 columns, then two blanks. */
function lying(pic, name) {
  if (pic.length !== 16 || pic.some((r) => r.length !== 24)) throw new Error(`${name}: a lying frame is 16 rows of 24`);
  return [cut(pic, 0, 0), cut(pic, 0, 8), cut(pic, 8, 0), cut(pic, 8, 8), cut(pic, 16, 0), cut(pic, 16, 8), blank, blank];
}

const FRAME_ORDER = ['stand', 'runA', 'runB', 'ready', 'kick', 'volley', 'header', 'bicycle', 'win', 'lose'];
function viewTiles(head) {
  const p = poses(head);
  p.bicycle = turn180(p.kick);
  return FRAME_ORDER.flatMap((n) => upright(p[n], n));
}
// Loaded into VRAM bank 1 from tile 0: the back view's 10 frames, the front
// view's 10, then the dive (8 tiles a frame). src/play.c: F_*, VIEW_FRONT, F_DIVE.
export const PLAYER_TILES = [
  ...viewTiles(HEAD_BACK),
  ...viewTiles(HEAD_FRONT),
  ...lying(turn90(poses(HEAD_FRONT).stand), 'dive'),
];

// The ball (8x8, the top of an 8x16 pair): 1 white, 2 its patches, 3 outline.
// The first two of six spin frames. Four extra frames load into mode-specific
// free slots; the shadow stays on the court. All use VRAM bank 0.
const [ballA,ballB] = BALL_FRAMES;
const shadow = ['........', '........', '........', '........', '..3333..', '.333333.', '..3333..', '........'];

// The aim marker (1 bright, 3 outline): where your queued strike will land.
const aimMark = ['........', '..1111..', '.13..31.', '.1.33.1.', '.1.33.1.', '.13..31.', '..1111..', '........'];

// A trail's dots (1 core, 2 middle, 3 edge), largest (nearest the ball) first.
const trail0 = ['..2222..', '.211112.', '21111112', '21111112', '21111112', '21111112', '.211112.', '..2222..'];
const trail1 = ['........', '..2222..', '.212212.', '.221122.', '.221122.', '.212212.', '..2222..', '........'];
const trail2 = ['........', '........', '..3333..', '..3223..', '..3223..', '..3333..', '........', '........'];
const trail3 = ['........', '........', '........', '...33...', '...33...', '........', '........', '........'];

// Order matters: src/play.c names them (S_BALL, S_SHADOW, S_AIM, S_TRAIL), 2 tiles per 8x16 sprite.
export const SPRITE_TILES = [
  ballA, blank,   // S_BALL
  ballB, blank,   // S_BALL + 2
  shadow, blank,  // S_SHADOW
  aimMark, blank, // S_AIM
  trail0, blank,  // S_TRAIL
  trail1, blank,
  trail2, blank,
  trail3, blank,
  // 16x8 ground ellipse: two 8x16 hardware sprites, tiles 16 and 18.
  ['........','........','.....333','..333333','.3333333','..333333','.....333','........'], blank,
  ['........','........','333.....','333333..','3333333.','333333..','333.....','........'], blank,
];

export const FUTSAL_PITCH = buildPitch(true);
// Two 8x16 pairs form a gold landing ellipse. The blank lower halves keep it
// on the ground; futsal loads these into its final free slots below tile 128.
const zoneLeft=['......11','...11133','.1133...','113.....','113.....','.1133...','...11133','......11'];
export const FUTSAL_ZONE=[zoneLeft,blank,zoneLeft.map(r=>[...r].reverse().join('')),blank];
