// BALL MACHINE (src/mg_machine.c): soccer passing pitch & pop-up defender dummies.
//
// BG:  20 x 17 tiles (160 x 136 px, scroll_y = 0, CAM = 16): an open futsal
//      training pitch in perspective (no tennis net!), with:
//        ty 0..1  (y   0..15): stadium barrier wall & gold railing (pal 0)
//        ty 2     (y  16..23): metallic steel-blue launcher apron for the ball
//                              cannon, with riveted plates & status lamps (pal 1/2/3)
//        ty 3..16 (y  24..135): open emerald futsal pitch with mowed stripes,
//                              2px white sidelines, halfway line & center circle (pal 4)
// SPR: 8x8 sprite tiles in pairs (8x16 sprite mode: top, then bottom), loaded
//      into sprite VRAM bank 0 from MG_SPR_TILES (src/minigame.h):
//        0   ball-launcher turret aiming straight (24 x 16, 3 columns)
//        6   turret turned to your left (flipped: your right)
//        12  pop-up defender dummy standing upright (16 x 16, 2 columns)
//        16  pop-up defender dummy knocked over flat (16 x 16, 2 columns)
//        20  muzzle flash starburst (8 x 8)
import { BLANK, canvas, toBackground } from './common.mjs';

// Match perspective (must match src/game.h and proj_init()).
const COURT_X0 = 8, COURT_X1 = 152, CX = 80;
const FAR = 0, NEAR = 224, HALF = 112;
const COURT_SY = 32;
const PROJ_K0 = 92, PROJ_K1 = 164;
const PROJ_S0 = 111;
const CAM = 16;

const projY = (fy) => COURT_SY + Math.floor((PROJ_K0 * fy * 448 + (PROJ_K1 - PROJ_K0) * fy * fy) / (448 * 256));
const projS = (fy) => PROJ_S0 + Math.floor(((128 - PROJ_S0) * fy) / NEAR);
const projX = (fx, fy) => CX + Math.floor(((fx - CX) * projS(fy)) / 128);
const screenY = (fy) => projY(fy) - CAM;

// Background palettes (set in src/mg_machine.c):
//   0: wall  (0 gold panel, 1 crimson pillar, 2 deep plum shadow, 3 cream rail)
//   1: metal (0 sky highlight, 1 steel blue, 2 deep navy seam, 3 white)
//   2: led-r (same 0..2 as metal, 3 bright coral-red lamp)
//   3: led-g (same 0..2 as metal, 3 bright amber-gold lamp)
//   4: grass (0 bright emerald, 1 mowed emerald stripe, 2 forest apron, 3 white line)
const PAL_WALL = 0;
const PAL_METAL = 1;
const PAL_LED_R = 2;
const PAL_LED_G = 3;
const PAL_GRASS = 4;

function buildCourtBg() {
  const cv = canvas(20, 17, 1);
  const { px, W, H, plot, rect, hline, vline } = cv;
  const tilePal = new Map();

  // --- 1. Top stadium railing wall (y = 0..15, ty = 0..1) ---
  rect(0, 0, W, 9, 0);
  rect(0, 9, W, 5, 1);
  rect(0, 14, W, 2, 2);
  hline(0, W - 1, 0, 3);
  hline(0, W - 1, 8, 2);
  hline(0, W - 1, 9, 3);
  hline(0, W - 1, 13, 2);
  for (let x = 0; x < W; x += 8) {
    vline(x, 1, 7, 2);
    vline(x + 1, 1, 7, 3);
    plot(x + 4, 11, 2);
    plot(x + 5, 11, 3);
  }
  for (const px0 of [0, 32, 64, 96, 128, 152]) {
    rect(px0, 0, 8, 14, 1);
    vline(px0, 0, 13, 2);
    vline(px0 + 7, 0, 13, 2);
    vline(px0 + 1, 1, 12, 3);
    hline(px0 + 1, px0 + 6, 0, 3);
    hline(px0 + 1, px0 + 6, 8, 2);
    hline(px0 + 1, px0 + 6, 13, 2);
  }

  // --- 2. Steel launcher apron (y = 16..23, ty = 2) ---
  rect(0, 16, W, 8, 1);
  hline(0, W - 1, 16, 0);
  hline(0, W - 1, 23, 2);
  for (let x = 0; x < W; x += 16) {
    vline(x, 16, 22, 2);
    plot(x + 2, 18, 0);
    plot(x + 13, 18, 0);
  }
  for (const { tx, pal } of [
    { tx: 3, pal: PAL_LED_R },
    { tx: 5, pal: PAL_LED_G },
    { tx: 14, pal: PAL_LED_G },
    { tx: 16, pal: PAL_LED_R },
  ]) {
    const lx = tx * 8 + 2, ly = 18;
    rect(lx - 1, ly - 1, 6, 5, 2);
    rect(lx, ly, 4, 3, 3);
    plot(lx, ly, 0);
    tilePal.set(`${tx},2`, pal);
  }

  // --- 3. Open emerald futsal pitch (y = 24..135, ty = 3..16) ---
  const depthAt = new Map();
  for (let q = 0; q <= NEAR * 8; q++) depthAt.set(screenY(q / 8), q / 8);

  rect(0, 24, W, H - 24, 2);
  for (const [y, fy] of depthAt) {
    if (y < 24 || y >= H) continue;
    for (let x = 0; x < W; x++) {
      const fx = CX + ((x + 0.5 - CX) * 128) / projS(fy);
      if (fx >= COURT_X0 && fx <= COURT_X1) {
        px[y][x] = (Math.floor(fy / 28) & 1) ? 1 : 0;
      }
    }
  }
  for (let y = 24; y < Math.min(H, screenY(0) + 1); y++) {
    const x0 = projX(COURT_X0, 0), x1 = projX(COURT_X1, 0);
    for (let x = x0; x <= x1; x++) px[y][x] = 0;
  }

  // --- 4. Crisp white futsal pitch lines (no tennis net!) ---
  const line = (fx, fy, thick = false) => {
    const x = projX(fx, fy), y = screenY(fy);
    if (y < 24 || y >= H) return;
    plot(x, y, 3);
    if (thick || fy > HALF) {
      plot(x + 1, y, 3);
      plot(x, y + 1, 3);
      plot(x + 1, y + 1, 3);
    }
  };
  for (let t = 0; t <= 1; t += 1 / 1024) {
    const fy = FAR + t * (NEAR - FAR);
    const fx = COURT_X0 + t * (COURT_X1 - COURT_X0);
    line(COURT_X0, fy);
    line(COURT_X1 - 1, fy);
    line(fx, HALF, true);
    line(fx, NEAR - 1, true);
    line(fx, 18);
  }
  // Center circle & spot on the halfway line.
  for (let a = 0; a < 360; a += 3) {
    const rad = (a * Math.PI) / 180;
    const fx = CX + Math.cos(rad) * 24;
    const fy = HALF + Math.sin(rad) * 24;
    line(fx, fy);
  }
  for (let dy = -1; dy <= 1; dy++) {
    for (let dx = -1; dx <= 1; dx++) plot(projX(CX, HALF) + dx, screenY(HALF) + dy, 3);
  }
  // Player-side box markings.
  for (let t = 0; t <= 1; t += 1 / 256) {
    line(36 + t * 88, 176);
    line(36, 176 + t * 48);
    line(124, 176 + t * 48);
  }

  return toBackground(cv, (tx, ty) => {
    if (ty <= 1) return PAL_WALL;
    if (ty === 2) return tilePal.get(`${tx},${ty}`) ?? PAL_METAL;
    return PAL_GRASS;
  });
}

export const BG = buildCourtBg();

// --- SPRITES ---
const cut = (pic, x, y) => pic.slice(y, y + 8).map((r) => r.slice(x, x + 8));
function cols(pic, w, name) {
  if (pic.length !== 16 || pic.some((r) => r.length !== w)) throw new Error(`${name}: expected ${w}x16`);
  const out = [];
  for (let x = 0; x < w; x += 8) out.push(cut(pic, x, 0), cut(pic, x, 8));
  return out;
}

const MACHINE = [
  '..33....33333333....33..',
  '.3123..3311111133..3213.',
  '.3123.311122221113.3213.',
  '.3123311222222221133213.',
  '.3223311111111111133223.',
  '332233322333333223332233',
  '311232223223322322232113',
  '313232232333333232232313',
  '3112322323....3232232113',
  '3112322323....3232232113',
  '322232232333333232232223',
  '332232223223322322232233',
  '.3333322233333322233333.',
  '..32233322222222333223..',
  '..3333..33333333..3333..',
  '........................',
];

const MACHINE_TURN = [
  '..33....33333333....33..',
  '.3123..3311111133..3213.',
  '.3123.311122221113.3213.',
  '.3123311222222221133213.',
  '.3223311111111111133223.',
  '332233322233332223332233',
  '311232232233222222232113',
  '313232323333222222232313',
  '31123323...3222222232113',
  '31123323...3222222232113',
  '322232323333222222232223',
  '332232232233222222232233',
  '.3333322233332222233333.',
  '..32233322222222333223..',
  '..3333..33333333..3333..',
  '........................',
];

// Pop-up defender training dummy (16 x 16): upright cutout player on a hinged base!
const DUMMY_UP = [
  '.....333333.....',
  '....31111113....',
  '....31311313....',
  '....31111113....',
  '.....331133.....',
  '..333222222333..',
  '.32222111122223.',
  '.32232211223223.',
  '..33.322223.33..',
  '.....321123.....',
  '.....322223.....',
  '....32233223....',
  '....33333333....',
  '...3311111133...',
  '..333333333333..',
  '................',
];

// Knocked-over pop-up dummy lying flat on its spring base (16 x 16).
const DUMMY_DOWN = [
  '................',
  '................',
  '................',
  '................',
  '................',
  '................',
  '................',
  '.....333333.....',
  '...3322112233...',
  '..322211112223..',
  '.33222222222233.',
  '.31133333333113.',
  '..333333333333..',
  '...3311111133...',
  '..333333333333..',
  '................',
];

const FLASH = [
  '...11...',
  '.1.12.1.',
  '..1221..',
  '11211211',
  '..1221..',
  '.1.21.1.',
  '...11...',
  '........',
];

export const SPR = [
  ...cols(MACHINE, 24, 'machine'),           // +0..5   T_MACHINE
  ...cols(MACHINE_TURN, 24, 'machineTurn'),  // +6..11  T_MACHINE_TURN
  ...cols(DUMMY_UP, 16, 'dummyUp'),          // +12..15 T_DUMMY_UP
  ...cols(DUMMY_DOWN, 16, 'dummyDown'),      // +16..19 T_DUMMY_DOWN
  FLASH, BLANK,                              // +20..21 T_FLASH
];
