// WALL RALLY (src/mg_rally.c): 1v1 Wall Squash vs CPU (first to 5 pts) + Solo
// endless rebound streak, tin faults, double-bounce points, stat scaling, frame
// budget, and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, px, set16 } from './helpers.mjs';
import {
  openGame, phaseSettled, startMinigame, goLive, toResults, MG, MG_PH, MG_RUN, STAT, OAM_MG_FREE,
} from './mg_helpers.mjs';

const BALL_WAIT = 0, BALL_TO_WALL = 1, BALL_FROM_WALL = 2, BALL_DEAD = 3;
const POP_PT = 1, POP_TIN = 2, POP_OUT = 3;

function readDetail(e) {
  const base = e.symbols.addr('mg_detail');
  let s = '';
  for (let i = 0; i < 20; i++) {
    const ch = e.read8(base + i);
    if (!ch) break;
    s += String.fromCharCode(ch);
  }
  return s;
}

test('VS CPU squash: alternating wall rebounds between YOU and CPU, bounce reticle, and golden', () => {
  const e = startMinigame(MG.RALLY, 4242, { vs: true });
  assert.equal(e.v('mg_vs'), 1);
  goLive(e);
  e.idle(6);
  matchGolden(e, 'rally-vs-live');

  // Opening feed comes off the wall to YOU (turn === 0) with floor bounce marker visible.
  assert.equal(e.v('mg_rally_ball'), BALL_FROM_WALL);
  assert.equal(e.v('mg_rally_turn'), 0);
  const markY = e.read8(0xFE00 + OAM_MG_FREE * 4);
  assert.ok(markY > 0 && markY < 160, `rebound landing reticle visible on YOUR turn (OAM y=${markY})`);

  // Place ball right in front of YOU and strike it to the wall with A!
  const yx = e.v('mg_rally_px', 'i16');
  const yd = e.v('mg_rally_pd', 'i16');
  set16(e, 'mg_rally_bx', yx);
  set16(e, 'mg_rally_bd', yd - 32);
  set16(e, 'mg_rally_bz', 6 * 16);
  e.tap('A');
  assert.equal(e.v('mg_rally_ball'), BALL_TO_WALL, 'YOU struck ball toward the wall');
  assert.equal(e.v('mg_rally_last'), 0);

  // Wait for wall rebound -> turn switches to CPU (1), and CPU returns it back to wall!
  e.until((em) => em.v('mg_rally_ball') === BALL_FROM_WALL, 60, 'ball rebounds off wall');
  assert.equal(e.v('mg_rally_turn'), 1, 'turn alternates to CPU after wall rebound');
  assert.equal(e.v('mg_rally_streak'), 1);

  e.until((em) => em.v('mg_rally_ball') === BALL_TO_WALL && em.v('mg_rally_last') === 1, 90, 'CPU strikes rebound');
  e.until((em) => em.v('mg_rally_ball') === BALL_FROM_WALL && em.v('mg_rally_turn') === 0, 60, 'CPU shot rebounds back to YOU');
  assert.equal(e.v('mg_rally_streak'), 2);
});

test('VS CPU faults & win: tin fault (< 8 px) and double bounce award points; 5 points wins', () => {
  const e = startMinigame(MG.RALLY, 1337, { vs: true });
  goLive(e);
  e.idle(4);

  // 1) CPU clips the red tin (z = 4 < TIN_Z = 8) -> YOUR point!
  set16(e, 'mg_rally_bx', 80 * 16);
  set16(e, 'mg_rally_bd', 1 * 16);
  set16(e, 'mg_rally_bz', 4 * 16);
  set16(e, 'mg_rally_vx', 0);
  set16(e, 'mg_rally_vd', -32);
  set16(e, 'mg_rally_vz', 2);
  e.write8(e.symbols.addr('mg_rally_ball'), BALL_TO_WALL);
  e.write8(e.symbols.addr('mg_rally_last'), 1);
  e.idle(2);
  assert.equal(e.v('mg_rally_ball'), BALL_DEAD);
  assert.equal(e.v('mg_rally_pop_kind'), POP_TIN);
  assert.equal(e.v('mg_rally_you_pts'), 1, 'CPU tin fault awards YOU a point');

  // 2) Wait for next serve, then force a double bounce on CPU's turn at 4-2 -> 5-2 match win!
  e.until((em) => em.v('mg_rally_ball') !== BALL_DEAD, 60, 'next serve');
  e.write8(e.symbols.addr('mg_rally_you_pts'), 4);
  e.write8(e.symbols.addr('mg_rally_cpu_pts'), 2);
  set16(e, 'mg_rally_cx', 140 * 16);
  set16(e, 'mg_rally_bx', 20 * 16);
  set16(e, 'mg_rally_bd', 76 * 16);
  set16(e, 'mg_rally_bz', 2 * 16);
  set16(e, 'mg_rally_vx', 0);
  set16(e, 'mg_rally_vd', 8);
  set16(e, 'mg_rally_vz', -8);
  e.write8(e.symbols.addr('mg_rally_ball'), BALL_FROM_WALL);
  e.write8(e.symbols.addr('mg_rally_turn'), 1);
  e.write8(e.symbols.addr('mg_rally_bounces'), 1);
  e.until((em) => em.v('mg_run') === MG_RUN.OVER, 30, '2nd bounce finishes match');
  assert.equal(e.v('mg_rally_you_pts'), 5);
  assert.equal(e.v('mg_won'), 1);
  assert.equal(readDetail(e), 'YOU 5 - 2 CPU');
  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.ok(e.v('mg_xp', 'u16') > 20, 'awarded XP + win bonus');
});

test('SOLO mode: every rebound comes back to YOU, power adds +2, and 3 misses end the run', () => {
  const e = startMinigame(MG.RALLY, 2026, { vs: false });
  assert.equal(e.v('mg_vs'), 0);
  goLive(e);
  e.idle(4);
  matchGolden(e, 'rally-solo-live');

  // Normal return -> +1 point, turn stays 0 (YOU).
  const yx = e.v('mg_rally_px', 'i16');
  const yd = e.v('mg_rally_pd', 'i16');
  set16(e, 'mg_rally_bx', yx);
  set16(e, 'mg_rally_bd', yd - 32);
  set16(e, 'mg_rally_bz', 6 * 16);
  e.tap('A');
  e.until((em) => em.v('mg_rally_ball') === BALL_FROM_WALL, 60, 'solo rebound');
  assert.equal(e.v('mg_rally_turn'), 0, 'in SOLO mode every rebound returns to YOU');
  assert.equal(e.v('mg_rally_points', 'u16'), 1);

  // Full-charge power shot -> +2 points (+3 total).
  set16(e, 'mg_rally_bx', yx);
  set16(e, 'mg_rally_bd', yd - 32);
  set16(e, 'mg_rally_bz', 6 * 16);
  e.write8(e.symbols.addr('mg_rally_charge'), 48);
  e.tap('A');
  e.until((em) => em.v('mg_rally_ball') === BALL_FROM_WALL, 60, 'power solo rebound');
  assert.equal(e.v('mg_rally_points', 'u16'), 3, 'power shot scores +2');

  // 3rd miss ends the solo run and records BEST RALLY detail.
  e.write8(e.symbols.addr('mg_rally_misses'), 2);
  set16(e, 'mg_rally_px', 20 * 16);
  set16(e, 'mg_rally_bx', 140 * 16);
  set16(e, 'mg_rally_bd', 90 * 16);
  set16(e, 'mg_rally_bz', 1 * 16);
  set16(e, 'mg_rally_vz', -8);
  e.write8(e.symbols.addr('mg_rally_bounces'), 1);
  e.until((em) => em.v('mg_run') === MG_RUN.OVER, 30, '3rd solo miss');
  assert.equal(e.v('mg_rally_misses'), 3);
  assert.equal(readDetail(e), 'BEST RALLY 2');
});

test('REFLEX increases speed and reach; POWER accelerates charge; busy rally fits frame budget', () => {
  const eLow = startMinigame(MG.RALLY, 505, { vs: true });
  goLive(eLow);
  const spd1 = eLow.v('mg_rally_speed');
  const rch1 = eLow.v('mg_rally_reach');
  const crg1 = eLow.v('mg_rally_charge_rate');

  const eHigh = openGame(MG.RALLY);
  eHigh.write8(eHigh.symbols.addr('stat_lv') + STAT.REFLEX, 14);
  eHigh.write8(eHigh.symbols.addr('stat_lv') + STAT.POWER, 15);
  eHigh.writeBytes(eHigh.symbols.addr('test_seed'), [505 & 255, 505 >> 8]);
  eHigh.idle(25).tap('A');
  phaseSettled(eHigh, MG_PH.PLAY, 200, 'high stats');
  goLive(eHigh);
  assert.ok(eHigh.v('mg_rally_speed') > spd1, 'REFLEX raises speed');
  assert.ok(eHigh.v('mg_rally_reach') > rch1, 'REFLEX raises reach');
  assert.ok(eHigh.v('mg_rally_charge_rate') > crg1, 'POWER raises charge rate');

  // Frame budget check across an active 1v1 rally with charging and trails.
  let peak = 0;
  for (let f = 0; f < 300; f++) {
    const px0 = px(eHigh.v('mg_rally_px', 'i16'));
    const tx0 = eHigh.v('mg_rally_land_x', 'i16');
    const keys = [];
    if (px0 < tx0 - 3) keys.push('RIGHT');
    else if (px0 > tx0 + 3) keys.push('LEFT');
    if (f % 16 < 8) keys.push((f & 16) ? 'B' : 'A');
    eHigh.hold(keys, 1);
    if (eHigh.lastFrameCycles > peak) peak = eHigh.lastFrameCycles;
  }
  assert.ok(peak < FRAME_LIMIT, `peak ${peak} cycles exceeded budget ${FRAME_LIMIT}`);
});
