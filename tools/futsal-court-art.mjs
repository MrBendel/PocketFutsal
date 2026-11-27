// The 2v2 futsal court, seen from a fixed three-quarter view (2026-11-27, from the
// playtest: "a taller field and keep the perspective fixed so we're not doing this
// foreshortened angle back"). Play is in the same flat court units as before (x 8..152
// across, depth 0 at the top goal line to 224 at the bottom one); the screen shows x
// as it is and depth at a fixed 6/7, with no vanishing point. The map is 20 x 32 tiles
// (160 x 256 px), the most a Game Boy background holds without streaming rows; the
// camera scrolls it up and down 120 px. src/futsal.c (fs_projection_y) matches COURT_TOP
// and courtY; tools/goal-background.mjs places the goals from them.
export const COURT_W = 20, COURT_H = 32;
export const COURT_TOP = 32;  // the top goal line's map row, px: room above for its goal
// Both goal lines on tile rows (32, 224), so no tile mixes the court and the grass
// under a goal (the goals' palettes have one or the other).
/** Depth -> map row (px). */
export const courtY = (fy) => COURT_TOP + Math.floor((fy * 6) / 7);
export const COURT_X0 = 8, COURT_X1 = 152;
// Palettes (src/palettes.c palettes_futsal): 0 the court (0 floor, 1 its bands, 2 the
// green round it, 3 lines), 3 the grass (0 light, 1 dark, 2 the same green, 3 cream).
export const COURT_PALETTES = [
  [[48, 144, 184], [40, 132, 172], [88, 160, 64], [248, 240, 208]],
  [[88, 160, 64], [40, 88, 56], [24, 48, 56], [208, 224, 184]],
  [[88, 160, 64], [208, 152, 72], [96, 64, 40], [248, 240, 208]],
  [[136, 192, 72], [40, 104, 56], [88, 160, 64], [248, 240, 208]],
];
const W = COURT_W * 8, H = COURT_H * 8;
const FLOOR = 0, BAND = 1, GREEN = 2, LINE = 3;

function draw() {
  const px = Array.from({ length: H }, () => new Array(W).fill(GREEN));
  const inCourt = Array.from({ length: H }, () => new Array(W).fill(false));
  const plot = (x, y, c) => { if (x >= 0 && x < W && y >= 0 && y < H) px[y][x] = c; };
  // The floor, in bands across (every 28 units of depth).
  for (let fy = 0; fy < 224; fy++) {
    const y0 = courtY(fy), y1 = courtY(fy + 1);
    for (let y = y0; y < Math.max(y1, y0 + 1); y++) for (let x = COURT_X0; x < COURT_X1; x++) {
      px[y][x] = Math.floor(fy / 28) & 1 ? BAND : FLOOR;
      inCourt[y][x] = true;
    }
  }
  // Lines, 2 px: a point of the court.
  const dot = (fx, fy) => {
    const x = Math.round(fx), y = courtY(Math.round(fy));
    plot(x, y, LINE); plot(x + 1, y, LINE); plot(x, y + 1, LINE); plot(x + 1, y + 1, LINE);
  };
  const seg = (x0, y0, x1, y1) => {
    const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0)) * 2 + 1;
    for (let k = 0; k <= n; k++) dot(x0 + ((x1 - x0) * k) / n, y0 + ((y1 - y0) * k) / n);
  };
  seg(COURT_X0, 0, COURT_X1 - 2, 0);
  seg(COURT_X0, 222, COURT_X1 - 2, 222);
  seg(COURT_X0, 0, COURT_X0, 222);
  seg(COURT_X1 - 2, 0, COURT_X1 - 2, 222);
  seg(COURT_X0, 111, COURT_X1 - 2, 111);
  const arc = (cx, cy, r, a0, a1) => { for (let a = a0; a <= a1; a += 0.02) dot(cx + Math.cos(a) * r, cy + Math.sin(a) * r); };
  arc(79, 111, 21, 0, Math.PI * 2);
  dot(79, 111);
  // The penalty areas: a quarter circle round each post's foot, 32 out, joined.
  for (const [end, s] of [[0, 1], [222, -1]]) {
    const top = end + s * 32;
    seg(60, top, 99, top);
    for (let a = 0; a <= Math.PI / 2; a += 0.01) {
      dot(60 - Math.sin(a) * 32, end + s * Math.cos(a) * 32);
      dot(99 + Math.sin(a) * 32, end + s * Math.cos(a) * 32);
    }
    dot(79, end + s * 34);   // the penalty spot, 6 m
    dot(79, end + s * 56);   // the second, 10 m
  }
  // The grass round it: shrubs along the sides and ends, darker tufts.
  const bush = (cx, cy) => {
    for (let dy = -3; dy <= 3; dy++) for (let dx = -3; dx <= 3; dx++) {
      if (dx * dx + dy * dy > 11 || inCourt[cy + dy]?.[cx + dx]) continue;
      plot(cx + dx, cy + dy, Math.abs(dx) + Math.abs(dy) > 3 ? 1 : 0);
    }
  };
  for (let y = 44; y < 240; y += 32) { bush(3, y); bush(156, y); }
  return { px, inCourt };
}

/** Unique tiles, the map (tile indices from 0) and each cell's palette. */
function build() {
  const { px, inCourt } = draw();
  const tiles = [], keys = new Map(), map = [], attrs = [];
  for (let ty = 0; ty < COURT_H; ty++) for (let tx = 0; tx < COURT_W; tx++) {
    let court = false;
    for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) court ||= inCourt[ty * 8 + y][tx * 8 + x] || px[ty * 8 + y][tx * 8 + x] === LINE;
    // Grass-only cells take the grass palette: their 0/1 are its light and dark greens.
    const rows = [];
    for (let y = 0; y < 8; y++) rows.push(px[ty * 8 + y].slice(tx * 8, tx * 8 + 8).join(''));
    const pal = court ? 0 : 3;
    const key = pal + ':' + rows.join('');
    if (!keys.has(key)) { keys.set(key, tiles.length); tiles.push(rows); }
    map.push(keys.get(key));
    attrs.push(pal);
  }
  return { tiles, map, attrs, w: COURT_W, h: COURT_H };
}
export const FUTSAL_COURT = build();
// Its tiles go in from SCENE_TILES (80), VRAM bank 0.
export const courtTile = (n) => 80 + n;
export const FUTSAL_COURT_MAP = { ...FUTSAL_COURT, map: FUTSAL_COURT.map.map(courtTile) };
