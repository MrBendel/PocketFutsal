// Shared helpers for the mini games' art (tools/art/mg_*.mjs). Backgrounds are
// drawn a pixel at a time in color indices 0-3 (the palette is a tile attribute,
// chosen per tile by palOf), then cut into unique 8x8 tiles and a map, exactly
// like the court in tools/tile-art.mjs. tools/assets.mjs turns the result into
// build/gen/<name>.c: <name>_tiles, <name>_map, <name>_attr, <NAME>_MAP_W/H.

/** A blank canvas, wt x ht tiles, filled with color `fill`, and drawing helpers. */
export function canvas(wt, ht, fill = 0) {
  const W = wt * 8, H = ht * 8;
  const px = Array.from({ length: H }, () => new Array(W).fill(fill));
  const plot = (x, y, c) => {
    x = Math.round(x); y = Math.round(y);
    if (x >= 0 && x < W && y >= 0 && y < H) px[y][x] = c;
  };
  const rect = (x0, y0, w, h, c) => {
    for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) plot(x, y, c);
  };
  const hline = (x0, x1, y, c) => { for (let x = x0; x <= x1; x++) plot(x, y, c); };
  const vline = (x, y0, y1, c) => { for (let y = y0; y <= y1; y++) plot(x, y, c); };
  /** Stamp ASCII rows ('.' skips, '0'-'3' colors) with the top-left at (x, y). */
  const stamp = (rows, x, y) => rows.forEach((r, dy) => [...r].forEach((ch, dx) => {
    if (ch !== '.') plot(x + dx, y + dy, Number(ch));
  }));
  return { px, W, H, wt, ht, plot, rect, hline, vline, stamp };
}

/**
 * A canvas -> { tiles, map, attrs, w, h } for an asset entry with `map:`.
 * palOf(tx, ty) gives each tile's background palette (0-5 are the scene's).
 * Identical pixels share a tile even under different palettes.
 */
export function toBackground(cv, palOf = () => 0) {
  const tiles = [], keys = new Map(), map = [], attrs = [];
  for (let ty = 0; ty < cv.ht; ty++) {
    for (let tx = 0; tx < cv.wt; tx++) {
      const rows = [];
      for (let y = 0; y < 8; y++) rows.push(cv.px[ty * 8 + y].slice(tx * 8, tx * 8 + 8).join(''));
      const key = rows.join('');
      if (!keys.has(key)) {
        keys.set(key, tiles.length);
        tiles.push(rows);
      }
      map.push(keys.get(key));
      attrs.push(palOf(tx, ty));
    }
  }
  return { tiles, map, attrs, w: cv.wt, h: cv.ht };
}

export const BLANK = Array(8).fill('........');

/** A placeholder background: one plain tile, wt x ht (the screen: 20 x 18). */
export function plainBackground(wt = 20, ht = 18, fill = 0) {
  return toBackground(canvas(wt, ht, fill));
}
