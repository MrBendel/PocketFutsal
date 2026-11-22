// BALL MACHINE (src/mg_machine.c): training-court background and sprite tiles.
//
// BG:  20 x 17 tiles (160 x 136 px, scroll_y = 0, CAM = 16): a Mario Tennis GBC
//      inspired ball-machine training court using the match's exact perspective
//      (projX / projY / projS).
//        ty 0..1  (y   0..15): stadium barrier wall & gold railing (pal 0)
//        ty 2..7  (y  16..63): metallic steel-blue far deck with riveted plates,
//                              vent grates, lit indicator lamps (pal 1/2/3) and
//                              the crisp net across halfway (y 54..63)
//        ty 8..16 (y  64..135): emerald green player half with 2px white lines,
//                              service T-markings and net drop-shadow (pal 4)
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode: top, then bottom), loaded
//      into sprite VRAM bank 0 from MG_SPR_TILES (src/minigame.h):
//        0  chunky ball-launcher turret aiming straight (24 x 16, 3 columns)
//        6  turret turned to your left (flipped: your right)
//        12 gold bullseye target ring (24 x 12, 3 columns)
//        18 muzzle flash starburst (8 x 8)
import { BLANK, canvas, toBackground } from './common.mjs';

// Match perspective (must match src/game.h and proj_init() in src/mg_machine.c).
const COURT_X0 = 8, COURT_X1 = 152, CX = 80;
const FAR = 0, NEAR = 224, HALF = 112;
const NET_H = 10;
const COURT_SY = 32;
const PROJ_K0 = 92, PROJ_K1 = 164;
const PROJ_S0 = 111;
const CAM = 16;

const projY = (fy) => COURT_SY + Math.floor((PROJ_K0 * fy * 448 + (PROJ_K1 - PROJ_K0) * fy * fy) / (448 * 256));
const projS = (fy) => PROJ_S0 + Math.floor(((128 - PROJ_S0) * fy) / NEAR);
const projX = (fx, fy) => CX + Math.floor(((fx - CX) * projS(fy)) / 128);
const screenY = (fy) => projY(fy) - CAM;

// Background palettes (set in src/mg_machine.c mg_machine_enter):
//   0: wall  (0 gold panel, 1 crimson pillar, 2 deep plum shadow, 3 cream rail)
//   1: metal (0 sky highlight, 1 steel blue, 2 deep navy seam/mesh, 3 white line)
//   2: led-r (same 0..2 as metal, 3 bright magenta-coral indicator lamp)
//   3: led-g (same 0..2 as metal, 3 bright amber-gold indicator lamp)
//   4: grass (0 bright emerald, 1 mowed emerald stripe, 2 forest apron, 3 white line)
const PAL_WALL = 0;
const PAL_METAL = 1;
const PAL_LED_R = 2;
const PAL_LED_G = 3;
const PAL_GRASS = 4;

function buildCourtBg() {
  const cv = canvas(20, 17, 1);
  const { px, W, H, plot, rect, hline, vline } = cv;
  // Track which far-half tiles carry a colored indicator lamp (no white lines).
  const tilePal = new Map();

  // --- 1. Top stadium railing wall (y = 0..15, ty = 0..1) ---
  // Gold slatted fence on top (y = 0..8), crimson base rail (y = 9..13), shadow (y = 14..15).
  rect(0, 0, W, 9, 0);
  rect(0, 9, W, 5, 1);
  rect(0, 14, W, 2, 2);
  hline(0, W - 1, 0, 3);
  hline(0, W - 1, 8, 2);
  hline(0, W - 1, 9, 3);
  hline(0, W - 1, 13, 2);
  // Repeating fence slats every 8px (vertical posts + specular).
  for (let x = 0; x < W; x += 8) {
    vline(x, 1, 7, 2);
    vline(x + 1, 1, 7, 3);
    plot(x + 4, 11, 2);
    plot(x + 5, 11, 3);
  }
  // Heavier crimson arena columns every 32px.
  for (const px0 of [0, 32, 64, 96, 128, 152]) {
    rect(px0, 0, 8, 14, 1);
    vline(px0, 0, 13, 2);
    vline(px0 + 7, 0, 13, 2);
    vline(px0 + 1, 1, 12, 3);
    hline(px0 + 1, px0 + 6, 0, 3);
    hline(px0 + 1, px0 + 6, 8, 2);
    hline(px0 + 1, px0 + 6, 13, 2);
  }

  // --- 2. Surface fill: far metallic deck (y = 16..63) & near emerald turf (y = 64..135) ---
  const depthAt = new Map();
  for (let q = 0; q <= NEAR * 8; q++) depthAt.set(screenY(q / 8), q / 8);

  // Far half outer walkway: light steel (0) with dark bevel near edges.
  rect(0, 16, W, 48, 0);
  // Near half outer apron: deep forest green (2).
  rect(0, 64, W, H - 64, 2);

  for (const [y, fy] of depthAt) {
    if (y < 16 || y >= H) continue;
    for (let x = 0; x < W; x++) {
      const fx = CX + ((x + 0.5 - CX) * 128) / projS(fy);
      if (y < 64) {
        // Metallic steel-blue deck inside the sidelines.
        if (fx >= COURT_X0 && fx <= COURT_X1) {
          // Subtle top-lit metallic gradient per plate band.
          const band = fy < 38 ? fy : fy < 76 ? fy - 38 : fy - 76;
          px[y][x] = band < 5 ? 0 : 1;
        } else {
          // Outer steel deck: light plate with subtle riveted edge.
          px[y][x] = (y & 7) === 0 ? 1 : 0;
        }
      } else {
        // Emerald green player half with clean mowed stripes.
        if (fx >= COURT_X0 && fx <= COURT_X1) {
          px[y][x] = (Math.floor(fy / 28) & 1) ? 1 : 0;
        }
      }
    }
  }

  // --- 3. Metallic far-deck architectural details (seams, rivets, grates, indicator lights) ---
  // Horizontal steel plate seams at y = 32 and y = 48 (exact tile boundaries!).
  for (const sy of [32, 48]) {
    const fy = sy === 32 ? 38 : 76;
    const x0 = projX(COURT_X0 + 1, fy), x1 = projX(COURT_X1 - 1, fy);
    hline(x0, x1, sy - 1, 2);
    hline(x0, x1, sy, 0);
  }
  // Vertical steel plate center seam (x = 80, y = 17..53).
  vline(79, 17, 53, 2);
  vline(80, 17, 53, 0);
  // Corner rivets along the plate seams and outer walkway.
  for (const ry of [19, 30, 34, 46, 50]) {
    for (const rx of [6, 10, 76, 83, 149, 153]) {
      plot(rx, ry, 2);
      plot(rx, ry - 1, 0);
    }
  }
  // Recessed mechanical vent grates on the middle plates (y = 37, 41).
  for (const vx of [44, 56, 98, 110]) {
    hline(vx, vx + 6, 37, 2);
    hline(vx, vx + 6, 38, 0);
    hline(vx, vx + 6, 41, 2);
    hline(vx, vx + 6, 42, 0);
  }
  // Glowing indicator lamps recessed into the steel deck (avoiding white lines!).
  // Placed cleanly inside dedicated 8x8 tiles so PAL_LED_R / PAL_LED_G only color lamps.
  const lamps = [
    // Upper row (ty = 3, y = 24..31): coral-red & gold lights flanking the machine lane.
    { tx: 4, ty: 3, pal: PAL_LED_R },
    { tx: 6, ty: 3, pal: PAL_LED_G },
    { tx: 13, ty: 3, pal: PAL_LED_G },
    { tx: 15, ty: 3, pal: PAL_LED_R },
    // Lower row (ty = 5, y = 40..47): target-lane status beacons.
    { tx: 4, ty: 5, pal: PAL_LED_G },
    { tx: 15, ty: 5, pal: PAL_LED_G },
  ];
  for (const { tx, ty, pal } of lamps) {
    const lx = tx * 8 + 2, ly = ty * 8 + 2;
    rect(lx - 1, ly - 1, 6, 5, 2);  // dark recessed bezel
    rect(lx, ly, 4, 3, 3);          // glowing LED core (color 3)
    plot(lx, ly, 0);                // specular glint
    tilePal.set(`${tx},${ty}`, pal);
  }

  // --- 4. Net drop-shadow on the green turf (drawn before lines so lines stay unbroken) ---
  const hy = screenY(HALF);  // 64
  const hx0 = projX(COURT_X0 - 6, HALF);
  const hx1 = projX(COURT_X1 + 5, HALF);
  hline(hx0, hx1, hy, 2);
  hline(hx0 + 1, hx1 - 1, hy + 1, 2);

  // --- 5. Court lines (crisp white, color 3) ---
  const line = (fx, fy) => {
    const x = projX(fx, fy), y = screenY(fy);
    plot(x, y, 3);
    if (fy >= HALF) {
      plot(x + 1, y, 3);
      plot(x, y + 1, 3);
      plot(x + 1, y + 1, 3);
    }
  };
  for (let t = 0; t <= 1; t += 1 / 2048) {
    const fy = FAR + t * (NEAR - FAR);
    const fx = COURT_X0 + t * (COURT_X1 - COURT_X0);
    line(COURT_X0, fy);
    line(COURT_X1 - 1, fy);
    line(fx, FAR);
    line(fx, NEAR - 1);
  }
  // Near half service T-markings (crisp spatial landmarks like Mario Tennis).
  for (let t = 0; t <= 1; t += 1 / 512) {
    const fx = COURT_X0 + t * (COURT_X1 - COURT_X0);
    line(fx, 174);
  }
  for (let t = 0; t <= 1; t += 1 / 256) {
    line(CX, HALF + t * (174 - HALF));
  }

  // --- 6. The Net across halfway (y = 54..63, inside ty = 6..7) ---
  // Net tape and fine checkerboard mesh.
  for (let y = hy - NET_H; y < hy; y++) {
    for (let x = hx0; x <= hx1; x++) {
      if (y === hy - NET_H || y === hy - NET_H + 1) plot(x, y, 3);       // bright white top tape
      else if (y === hy - NET_H + 2) plot(x, y, 2);                      // crisp dark tape shadow
      else if (y === hy - 1) plot(x, y, 2);                              // bottom cable
      else plot(x, y, ((x + y) & 1) === 0 ? 2 : (x & 3) === 0 ? 0 : 1); // crisp fine mesh
    }
  }
  // Metallic side posts with highlight and dark outline.
  for (const postX of [hx0, hx1]) {
    for (let y = hy - NET_H - 1; y < hy; y++) {
      plot(postX - 1, y, 2);
      plot(postX, y, 3);
      plot(postX + 1, y, 2);
    }
    hline(postX - 1, postX + 1, hy - NET_H - 2, 2);
    plot(postX, hy - NET_H - 1, 3);
  }

  return toBackground(cv, (tx, ty) => {
    if (ty < 2) return PAL_WALL;
    if (ty >= 8) return PAL_GRASS;
    return tilePal.get(`${tx},${ty}`) ?? PAL_METAL;
  });
}

export const BG = buildCourtBg();

/** A canvas -> its 8x16 columns as tile pairs (top, bottom), '.' for color 0. */
function columns(cv) {
  const out = [];
  const tile = (tx, ty) => Array.from({ length: 8 }, (_, y) =>
    cv.px[ty * 8 + y].slice(tx * 8, tx * 8 + 8).map((c) => (c ? String(c) : '.')).join(''));
  for (let tx = 0; tx < cv.wt; tx++) out.push(tile(tx, 0), cv.ht > 1 ? tile(tx, 1) : BLANK);
  return out;
}

/**
 * Chunky mechanical ball-launcher turret (24 x 16): twin top exhaust/feed struts,
 * armored rounded dome, heavy side pods, prominent circular cannon barrel shifted
 * by `ofs` (0 straight, -4 aimed left; hardware S_FLIPX mirrors it right), and
 * twin treads underneath.
 * Palette 3 (PAL_MACHINE): 1 bright metallic cream, 2 bold orange-red, 3 ink.
 */
function machine(ofs) {
  const cv = canvas(3, 2, 0);
  const { plot, rect, hline, vline } = cv;
  // Twin top stabilizer antennae / ball-feed rails.
  rect(2, 0, 3, 5, 3); vline(3, 1, 4, 1);
  rect(19, 0, 3, 5, 3); vline(20, 1, 4, 1);
  // Central rounded hopper dome packed with balls.
  hline(7, 16, 0, 3);
  rect(6, 1, 12, 4, 3);
  rect(7, 1, 10, 3, 1);
  // Subtle ball seam hints in the hopper.
  plot(9, 2, 2); plot(12, 1, 2); plot(14, 2, 2);
  // Heavy armored body with shoulder pods.
  rect(1, 5, 22, 9, 3);
  rect(2, 6, 20, 7, 2);
  // Metallic cream armor plates on shoulders & top bevel.
  rect(2, 6, 4, 6, 1);
  rect(18, 6, 4, 6, 1);
  hline(6, 17, 6, 1);
  // Shoulder rivets & status lamp.
  plot(3, 7, 2); plot(3, 10, 3);
  plot(20, 7, 2); plot(20, 10, 3);
  vline(6, 6, 12, 3);
  vline(17, 6, 12, 3);
  // Prominent circular launcher cannon barrel (shifted by ofs).
  const cx = 11.5 + ofs, cy = 9.5;
  for (let y = 5; y <= 14; y++) {
    for (let x = 1; x < 23; x++) {
      const d = Math.hypot(x - cx, (y - cy) * 1.08);
      if (d <= 2.2) plot(x, y, 3);           // deep dark cannon bore
      else if (d <= 3.4) plot(x, y, 1);      // bright cream-metal barrel lip
      else if (d <= 4.5) plot(x, y, 2);      // bold orange-red cannon ring
      else if (d <= 5.2 && y >= 5 && y <= 13) plot(x, y, 3); // crisp outer outline
    }
  }
  // Barrel glint inside the upper-left bore rim.
  plot(Math.round(cx - 2), 8, 1);
  // Twin rugged treads / wheels underneath.
  for (const wx of [3, 16]) {
    rect(wx, 14, 5, 2, 3);
    hline(wx + 1, wx + 3, 14, 1);
  }
  return columns(cv);
}

/**
 * Gold bullseye target ring (24 x 12): perspective oval with dark rim,
 * shimmering gold/amber band, and a crisp center target pip.
 */
function ring() {
  const cv = canvas(3, 2, 0);
  const rx = 11.5, ry = 5.5, cx = 11.5, cy = 5.5;
  for (let y = 0; y < 12; y++) {
    for (let x = 0; x < 24; x++) {
      const d = Math.hypot((x - cx) / rx, (y - cy) / ry);
      if (d > 1.04) continue;
      if (d >= 0.88) cv.plot(x, y, 3);
      else if (d >= 0.60) cv.plot(x, y, (x + y) % 2 ? 1 : 2);
      else if (d >= 0.50) cv.plot(x, y, 3);
      else if (d <= 0.22) cv.plot(x, y, 1);
    }
  }
  return columns(cv);
}

/** Muzzle flash starburst (palette SPR_PAL_POWER: 1 yellow, 2 orange, 3 red). */
const FLASH = [
  '3..2..3.',
  '.3.1.3..',
  '..212...',
  '2111112.',
  '..212...',
  '.3.1.3..',
  '3..2..3.',
  '........',
];

export const SPR = [
  ...machine(0),   // T_MACHINE
  ...machine(-4),  // T_MACHINE_TURN
  ...ring(),       // T_RING
  FLASH, BLANK,    // T_FLASH
];
