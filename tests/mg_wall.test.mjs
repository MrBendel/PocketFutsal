// TARGET WALL (src/mg_wall.c): deterministic rules, streaks, stat scaling,
// scripted run to MG_PH.RESULT, frame budget, and golden screens.
import { test } from 'node:test';
import { assert, set16, matchGolden, FRAME_LIMIT } from './helpers.mjs';
import { startMinigame, goLive, toResults, phaseSettled, MG, MG_PH, MG_RUN, STAT } from './mg_helpers.mjs';

const SUB = 16;
const BALL_HELD = 0, BALL_OUT = 1, BALL_BACK = 2, BALL_DEAD = 3;
const NO_PANEL = 0xff;
const CHARGE_FULL = 64;
const COL_X = [16, 80, 144];
const ROW_Z = [43, 21];

/** Launch the ball straight into panel `p` (0..5) so it hits the wall on the next frame. */
function hitPanel(e, p, { fire = 0 } = {}) {
  const row = p >= 3 ? 1 : 0;
  const col = p >= 3 ? p - 3 : p;
  set16(e, 'mg_wall_bx', COL_X[col] * SUB);
  set16(e, 'mg_wall_bd', 1 * SUB);
  set16(e, 'mg_wall_bz', ROW_Z[row] * SUB);
  set16(e, 'mg_wall_vx', 0);
  set16(e, 'mg_wall_vd', -32);
  set16(e, 'mg_wall_vz', 2);  // cancels GRAV=2 on the step
  e.write8(e.symbols.addr('mg_wall_ball'), BALL_OUT);
  e.write8(e.symbols.addr('mg_wall_fire'), fire);
  e.idle(1);
}

test('hitting the lit panel scores, increments streak, flashes, and lights a new panel', () => {
  const e = startMinigame(MG.WALL, 4242);
  goLive(e);
  e.idle(12);
  matchGolden(e, 'wall-live');

  const lit = e.v('mg_wall_lit');
  assert.equal(lit, 4, 'opens with bottom-center panel lit');
  assert.equal(e.v('mg_wall_points', 'u16'), 0);
  assert.equal(e.v('mg_wall_mult'), 1);

  hitPanel(e, lit);
  assert.equal(e.v('mg_wall_hit_panel'), lit);
  assert.equal(e.v('mg_wall_points', 'u16'), 1, 'scores 1x on first hit');
  assert.equal(e.v('mg_wall_hits'), 1);
  assert.equal(e.v('mg_wall_shots'), 1);
  assert.equal(e.v('mg_wall_mult'), 2, 'streak multiplier rises to 2');
  assert.equal(e.v('mg_wall_lit'), NO_PANEL, 'lit panel clears during flash');
  assert.ok(e.v('mg_wall_flash') > 0, 'hit panel flashes');
  assert.ok(e.v('mg_wall_pop') > 0 && e.v('mg_wall_pop_n') === 1, '+1 popup appears');
  assert.equal(e.v('mg_wall_ball'), BALL_BACK, 'ball rebounds off the wall');

  // Step a few frames into the flash so the green panel + rising +1 popup are visible.
  e.idle(4);
  matchGolden(e, 'wall-hit');

  // After the flash ends, a different panel lights up.
  e.until((em) => em.v('mg_wall_flash') === 0, 40, 'flash ends');
  const next = e.v('mg_wall_lit');
  assert.ok(next < 6 && next !== lit, `new lit panel (${next}) differs from old (${lit})`);
});

test('streaks cap at 4x, power shots add +1, and hitting an unlit panel resets the streak', () => {
  const e = startMinigame(MG.WALL, 1234);
  goLive(e);
  e.idle(2);

  // Hit 4 lit panels in a row: 1 + 2 + 3 + 4 = 10 points, mult caps at 4.
  for (let i = 0; i < 4; i++) {
    const lit = e.v('mg_wall_lit');
    hitPanel(e, lit);
    e.until((em) => em.v('mg_wall_flash') === 0, 40, 'flash ends');
  }
  assert.equal(e.v('mg_wall_points', 'u16'), 10);
  assert.equal(e.v('mg_wall_hits'), 4);
  assert.equal(e.v('mg_wall_mult'), 4, 'multiplier caps at 4');

  // A power shot on the lit panel scores mult + 1 = 5.
  const lit = e.v('mg_wall_lit');
  hitPanel(e, lit, { fire: 1 });
  assert.equal(e.v('mg_wall_points', 'u16'), 15, 'power shot adds +1 bonus');
  assert.equal(e.v('mg_wall_pop_n'), 5);
  e.until((em) => em.v('mg_wall_flash') === 0, 40, 'flash ends');

  // Hitting an unlit panel scores nothing and resets mult to 1.
  const curLit = e.v('mg_wall_lit');
  const unlit = (curLit + 1) % 6;
  hitPanel(e, unlit);
  assert.equal(e.v('mg_wall_hit_panel'), unlit);
  assert.equal(e.v('mg_wall_points', 'u16'), 15, 'unlit panel scores 0');
  assert.equal(e.v('mg_wall_hits'), 5, 'hits unchanged');
  assert.equal(e.v('mg_wall_shots'), 6, 'shots still counted');
  assert.equal(e.v('mg_wall_mult'), 1, 'multiplier resets to 1');
});

test('double bounce counts as a miss, resets streak, and feeds a new ball', () => {
  const e = startMinigame(MG.WALL, 4242);
  goLive(e);
  e.write8(e.symbols.addr('mg_wall_mult'), 3);

  // Wait for the fed ball to bounce twice without being struck.
  e.until((em) => em.v('mg_wall_ball') === BALL_DEAD, 200, 'ball misses');
  assert.equal(e.v('mg_wall_mult'), 1, 'miss resets streak');
  assert.ok(e.v('mg_wall_miss') > 0, 'MISS banner appears');
  assert.ok(e.v('mg_wall_wait') > 0, 'wait timer set before next feed');

  // After the wait timer expires, a fresh ball is fed.
  e.until((em) => em.v('mg_wall_ball') === BALL_BACK, 80, 'next ball fed');
  assert.equal(e.v('mg_wall_bounces'), 0);
});

test('charging shows the aim reticle and strikes when the ball enters reach', () => {
  const e = startMinigame(MG.WALL, 4242);
  goLive(e);
  e.idle(2);

  // Place ball approaching from the wall, hold UP+LEFT+A to charge and aim top-left.
  set16(e, 'mg_wall_px', 80 * SUB);
  set16(e, 'mg_wall_pd', 88 * SUB);
  set16(e, 'mg_wall_bx', 80 * SUB);
  set16(e, 'mg_wall_bd', 20 * SUB);
  set16(e, 'mg_wall_bz', 16 * SUB);
  set16(e, 'mg_wall_vx', 0);
  set16(e, 'mg_wall_vd', 20);
  set16(e, 'mg_wall_vz', 14);
  e.write8(e.symbols.addr('mg_wall_ball'), BALL_BACK);

  e.hold('A', 35);
  assert.equal(e.v('mg_wall_charging'), 1, 'holding A charges');
  assert.equal(e.v('mg_wall_charge'), CHARGE_FULL, 'reaches full charge');
  matchGolden(e, 'wall-charge');

  // Keep holding UP+LEFT+A frame-by-frame until strike triggers.
  for (let f = 0; f < 60 && e.v('mg_wall_ball') !== BALL_OUT; f++) e.hold('UP+LEFT+A', 1);
  assert.equal(e.v('mg_wall_ball'), BALL_OUT, 'ball was struck toward the wall');
  assert.equal(e.v('mg_wall_aim_col'), 0, 'aimed left column');
  assert.equal(e.v('mg_wall_aim_row'), 0, 'aimed top row');
  assert.equal(e.v('mg_wall_fire'), 1, 'full charge produces a power shot');
});

test('late in the clock an idle lit panel moves on, but never while a shot is in flight', () => {
  const e = startMinigame(MG.WALL, 4242);
  goLive(e);
  e.idle(2);

  const orig = e.v('mg_wall_lit');
  // Set clock to 20s (limit = 120 + 80 = 200 frames) and lit_age just under limit.
  e.write8(e.symbols.addr('mg_wall_time'), 20);
  set16(e, 'mg_wall_lit_age', 199);
  // With a shot in flight (BALL_OUT), the panel must NOT move.
  e.write8(e.symbols.addr('mg_wall_ball'), BALL_OUT);
  set16(e, 'mg_wall_bd', 60 * SUB);
  set16(e, 'mg_wall_vd', -16);
  e.idle(5);
  assert.equal(e.v('mg_wall_lit'), orig, 'lit panel stays put while shot is in flight');

  // Once the ball is not BALL_OUT, it moves to a new panel on the next frame.
  e.write8(e.symbols.addr('mg_wall_ball'), BALL_BACK);
  e.idle(1);
  const moved = e.v('mg_wall_lit');
  assert.ok(moved < 6 && moved !== orig, `lit panel moved from ${orig} to ${moved}`);
});

test('AIM narrows strike spread and POWER speeds up charging and power shots', () => {
  // Low stats (level 1)
  const low = startMinigame(MG.WALL, 4242);
  goLive(low);
  const errLow = low.v('mg_wall_err');
  const rateLow = low.v('mg_wall_charge_rate');
  assert.equal(errLow, 29, 'AIM lv 1 -> spread 29');
  assert.equal(rateLow, 2, 'POWER lv 1 -> charge rate 2');

  // High stats (AIM = 20, POWER = 20): finish once, set stats, restart into PLAY.
  const high = startMinigame(MG.WALL, 4242);
  goLive(high);
  high.write8(high.symbols.addr('mg_wall_time'), 1);
  high.write8(high.symbols.addr('mg_wall_tick'), 59);
  toResults(high);
  high.write8(high.symbols.addr('stat_lv') + STAT.AIM, 20);
  high.write8(high.symbols.addr('stat_lv') + STAT.POWER, 20);
  high.tap('A');
  phaseSettled(high, MG_PH.PLAY, 200, 'restart');
  goLive(high);

  const errHigh = high.v('mg_wall_err');
  const rateHigh = high.v('mg_wall_charge_rate');
  assert.equal(errHigh, 10, 'AIM lv 20 -> spread 10');
  assert.equal(rateHigh, 7, 'POWER lv 20 -> charge rate 7');
  assert.ok(errHigh < errLow, 'higher AIM narrows spread');
  assert.ok(rateHigh > rateLow, 'higher POWER increases charge rate');
});

test('a scripted run plays rallies, finishes on the clock, and reaches MG_PH.RESULT', () => {
  const e = startMinigame(MG.WALL, 4242);
  // Give the bot solid AIM so its aimed strikes reliably score.
  goLive(e);
  e.write8(e.symbols.addr('mg_wall_err'), 6);

  let maxCycles = 0;
  // Play ~600 frames (~10 seconds of real rallies), steering to the ball and aiming at mg_wall_lit.
  for (let f = 0; f < 600 && e.v('mg_run') === MG_RUN.LIVE; f++) {
    const px = e.v('mg_wall_px', 'i16') >> 4;
    const pd = e.v('mg_wall_pd', 'i16') >> 4;
    const bx = e.v('mg_wall_bx', 'i16') >> 4;
    const bd = e.v('mg_wall_bd', 'i16') >> 4;
    const ball = e.v('mg_wall_ball');
    const lit = e.v('mg_wall_lit');

    const btns = [];
    const nearBall = ball === BALL_BACK && Math.abs(bx - px) <= 10 && bd >= pd - 12 && bd <= pd + 6;
    if (nearBall && lit < 6) {
      // Aim at the lit panel and strike!
      const col = lit % 3;
      const row = lit >= 3 ? 1 : 0;
      if (col === 0) btns.push('LEFT');
      else if (col === 2) btns.push('RIGHT');
      if (row === 0) btns.push('UP');
      btns.push('A');
    } else if (ball === BALL_BACK) {
      if (bx < px - 3) btns.push('LEFT');
      else if (bx > px + 3) btns.push('RIGHT');
      if (pd > 84) btns.push('UP');
      else if (pd < 76) btns.push('DOWN');
    } else {
      if (px > 84) btns.push('LEFT');
      else if (px < 76) btns.push('RIGHT');
    }

    if (btns.length) e.hold(btns, 1);
    else e.idle(1);
    if (e.lastFrameCycles > maxCycles) maxCycles = e.lastFrameCycles;
  }

  assert.ok(e.v('mg_wall_shots') >= 3, `bot struck at least 3 shots (got ${e.v('mg_wall_shots')})`);
  assert.ok(e.v('mg_wall_hits') >= 2, `bot hit at least 2 lit panels (got ${e.v('mg_wall_hits')})`);
  assert.ok(e.v('mg_wall_points', 'u16') >= 2, `bot scored points (got ${e.v('mg_wall_points', 'u16')})`);
  assert.ok(maxCycles < FRAME_LIMIT, `busy rally stayed inside frame budget (${maxCycles} < ${FRAME_LIMIT})`);

  // Fast-forward to the final second and let the clock naturally finish the game.
  e.write8(e.symbols.addr('mg_wall_time'), 1);
  e.write8(e.symbols.addr('mg_wall_tick'), 50);
  e.until((em) => em.v('mg_run') === MG_RUN.OVER, 30, 'clock hits 0');
  assert.equal(e.v('mg_score', 'u16'), e.v('mg_wall_points', 'u16'), 'mg_finish receives final points');

  // Verify results screen and mg_detail string ("TARGETS X OF Y").
  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  const detailAddr = e.symbols.addr('mg_detail');
  let detail = '';
  for (let i = 0; i < 20; i++) {
    const ch = e.read8(detailAddr + i);
    if (!ch) break;
    detail += String.fromCharCode(ch);
  }
  assert.match(detail, /^TARGETS \d+ OF \d+$/, `unexpected mg_detail: "${detail}"`);
});
