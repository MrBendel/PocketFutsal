// KEEPY-UPPY (src/mg_juggle.c): foot kicks, knees, headers, timing grades,
// called bonuses, milestones, stat scaling, dropping to finish, frame budget,
// and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, px, set16 } from './helpers.mjs';
import { startMinigame, goLive, toResults, MG, MG_PH, MG_RUN, STAT } from './mg_helpers.mjs';

const TOUCH = { NONE: 0, FOOT: 1, KNEE: 2, HEAD: 3 };
const GRADE = { NONE: 0, GOOD: 1, PERFECT: 2, MISS: 3, NICE: 4 };
const TOUCH_Z = [0, 5 * 16, 10 * 16, 26 * 16];

/** Start KEEPY-UPPY and advance into live play right after the opening kick-up. */
function liveJuggle(seed = 4242) {
  const e = startMinigame(MG.JUGGLE, seed);
  goLive(e);
  e.idle(2);
  return e;
}

/** Place the ball cleanly for a deterministic touch test (x, z in px; vx, vz in 1/16 px/frame). */
function placeBall(e, x, z, vx = 0, vz = 0) {
  set16(e, 'mg_juggle_bx', x * 16);
  set16(e, 'mg_juggle_bz', z * 16);
  set16(e, 'mg_juggle_vx', vx);
  set16(e, 'mg_juggle_vz', vz);
  e.write8(e.symbols.addr('mg_juggle_cool'), 0);
  e.write8(e.symbols.addr('mg_juggle_pending'), 0);
  e.write8(e.symbols.addr('mg_juggle_wind_t'), 0);
  for (let i = 0; i < 4; i++) e.write8(e.symbols.addr('mg_juggle_below') + i, 0);
}

/** Read the C string in mg_detail. */
function detail(e) {
  const base = e.symbols.addr('mg_detail');
  let s = '';
  for (let i = 0; i < 20; i++) {
    const c = e.read8(base + i);
    if (!c) break;
    s += String.fromCharCode(c);
  }
  return s;
}

test('opening kick-up launches the ball; D-pad moves the player within bounds', () => {
  const e = liveJuggle();
  assert.equal(e.v('mg_juggle_started'), 1);
  assert.ok(e.v('mg_juggle_vz', 'i16') > 0, 'opening kick-up sends the ball up');
  assert.ok(e.v('mg_juggle_bz', 'i16') > 0, 'ball is airborne');

  const x0 = e.v('mg_juggle_px', 'i16');
  e.hold('LEFT', 12);
  assert.ok(e.v('mg_juggle_px', 'i16') < x0, 'LEFT moves player left');
  assert.equal(e.v('mg_juggle_moving'), 1);

  const x1 = e.v('mg_juggle_px', 'i16');
  e.hold('RIGHT', 20);
  assert.ok(e.v('mg_juggle_px', 'i16') > x1, 'RIGHT moves player right');

  // Clamps at right edge (X_MAX = 148 * 16) while ball is airborne.
  placeBall(e, 80, 90, 0, 80);
  e.hold('RIGHT', 80);
  assert.equal(e.v('mg_juggle_px', 'i16'), 148 * 16, 'clamped at right edge');
});

test('foot juggle (A): PERFECT and GOOD timing grades, timing ring cue, and MISS when early or out of reach', () => {
  // PERFECT: ball falling onto foot height within pw frames.
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  // Wait until the ball is falling into the PERFECT cue window.
  placeBall(e, 80, 14, 0, -24);
  e.until((em) => em.v('mg_juggle_cue') === GRADE.PERFECT, 40, 'PERFECT cue');
  e.tap('A');
  e.until((em) => em.v('mg_juggle_touches') === 1, 10, 'foot contact');
  assert.equal(e.v('mg_juggle_points', 'u16'), 1);
  assert.equal(e.v('mg_juggle_grade'), GRADE.PERFECT);
  assert.equal(e.v('mg_juggle_perfects'), 1);
  assert.ok(e.v('mg_juggle_vz', 'i16') > 0, 'ball popped back up');

  // GOOD: press in the GOOD window (earlier than PERFECT).
  placeBall(e, 80, 20, 0, -24);
  e.until((em) => em.v('mg_juggle_cue') === GRADE.GOOD, 40, 'GOOD cue');
  e.tap('A');
  e.until((em) => em.v('mg_juggle_touches') === 2, 15, 'GOOD foot contact');
  assert.equal(e.v('mg_juggle_points', 'u16'), 2);
  assert.equal(e.v('mg_juggle_grade'), GRADE.GOOD);
  assert.notEqual(e.v('mg_juggle_vx', 'i16'), 0, 'GOOD touch adds horizontal drift');

  // MISS when way too high.
  placeBall(e, 80, 70, 0, 0);
  e.tap('A');
  assert.equal(e.v('mg_juggle_grade'), GRADE.MISS);
  assert.equal(e.v('mg_juggle_touches'), 2, 'whiff does not score');

  // MISS when out of reach horizontally.
  placeBall(e, 120, 10, 0, -24);
  e.until((em) => em.v('mg_juggle_cue') === GRADE.PERFECT, 40, 'PERFECT cue out of reach');
  e.tap('A');
  e.idle(4);
  assert.equal(e.v('mg_juggle_grade'), GRADE.MISS);
  assert.equal(e.v('mg_juggle_touches'), 2, 'out of reach does not score');
});

test('knee (UP+A) kills drift on PERFECT; header (B) scores 2 and called touches award +2 bonus', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);

  // Knee with UP+A right above knee height (10 px) with strong drift.
  placeBall(e, 80, 11, 20, -24);
  e.hold('UP+A', 1).idle(3);
  assert.equal(e.v('mg_juggle_touches'), 1);
  assert.equal(e.v('mg_juggle_points', 'u16'), 1);
  assert.equal(e.v('mg_juggle_swing'), TOUCH.KNEE);
  assert.equal(e.v('mg_juggle_grade'), GRADE.PERFECT);
  assert.ok(Math.abs(e.v('mg_juggle_vx', 'i16')) <= 1, 'PERFECT knee kills horizontal drift');

  // Header with B right above head height (26 px) scores 2.
  placeBall(e, 80, 27, 0, -24);
  e.tap('B').idle(3);
  assert.equal(e.v('mg_juggle_touches'), 2);
  assert.equal(e.v('mg_juggle_points', 'u16'), 3, 'header worth 2');
  assert.equal(e.v('mg_juggle_headers'), 1);
  assert.equal(e.v('mg_juggle_swing'), TOUCH.HEAD);
  assert.equal(e.v('mg_juggle_bonus'), 2);

  // Called HEAD! awards +2 bonus on top of header's 2 = +4 total.
  e.write8(e.symbols.addr('mg_juggle_call'), TOUCH.HEAD);
  placeBall(e, 80, 27, 0, -24);
  e.tap('B').idle(3);
  assert.equal(e.v('mg_juggle_points', 'u16'), 7, 'called header worth 4');
  assert.equal(e.v('mg_juggle_headers'), 2);
  assert.equal(e.v('mg_juggle_bonus'), 4);
  assert.equal(e.v('mg_juggle_call'), 0, 'call cleared after touch');
});

test('milestone at 10 points advances tier, triggers NICE!, sparkles, and fire ball', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  set16(e, 'mg_juggle_points', 9);
  assert.equal(e.v('mg_juggle_tier'), 0);
  placeBall(e, 80, 6, 0, -24);
  e.tap('A').idle(3);
  assert.equal(e.v('mg_juggle_points', 'u16'), 10);
  assert.equal(e.v('mg_juggle_tier'), 1, 'advanced to tier 1');
  assert.equal(e.v('mg_juggle_next', 'u16'), 25, 'next milestone is 25');
  assert.equal(e.v('mg_juggle_grade'), GRADE.NICE);
  assert.ok(e.v('mg_juggle_star_t') > 0, 'sparkles active');
  assert.ok(e.v('mg_juggle_fire_t') > 0, 'ball on fire');
});

test('high-tier fast-falling ball can still be kicked cleanly without false drop', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  e.write8(e.symbols.addr('mg_juggle_tier'), 5);
  e.write8(e.symbols.addr('mg_juggle_g'), 4);
  // Ball at z = 6 px (96 sub), falling at vz = -110 (would cross z=0 in 1 frame).
  placeBall(e, 80, 6, 0, -110);
  e.tap('A').idle(2);
  assert.equal(e.v('mg_juggle_dropped'), 0, 'saved instead of dropping');
  assert.equal(e.v('mg_juggle_touches'), 1);
  assert.ok(e.v('mg_juggle_vz', 'i16') > 0);
});

test('dropping the ball ends the run, fills detail, and reaches MG_PH.RESULT with XP', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  // Score a PERFECT header (2 pts) and a PERFECT kick (1 pt) = 3 pts.
  placeBall(e, 80, 27, 0, -24);
  e.tap('B').idle(3);
  placeBall(e, 80, 6, 0, -24);
  e.tap('A').idle(3);
  assert.equal(e.v('mg_juggle_points', 'u16'), 3);

  // Now let the ball drop to the grass.
  e.until((em) => em.v('mg_juggle_dropped') === 1, 200, 'ball dropped');
  assert.equal(e.v('mg_run'), MG_RUN.OVER);
  assert.equal(e.v('mg_score', 'u16'), 3);
  assert.equal(detail(e), 'PERFECT 2 HEAD 1');

  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.equal(e.v('mg_score', 'u16'), 3);
  assert.ok(e.v('mg_xp', 'u16') > 0, 'XP awarded');
});

test('stat scaling: higher CONTROL widens timing windows; higher REFLEX speeds up feet', () => {
  const low = startMinigame(MG.JUGGLE);
  const lowPw = low.v('mg_juggle_pw');
  const lowGw = low.v('mg_juggle_gw');
  const lowSpd = low.v('mg_juggle_speed');

  const high = startMinigame(MG.JUGGLE);
  // Boost CONTROL and REFLEX, then restart into a fresh run.
  high.write8(high.symbols.addr('stat_lv') + STAT.CONTROL, 16);
  high.write8(high.symbols.addr('stat_lv') + STAT.REFLEX, 16);
  goLive(high);
  // Drop immediately to finish, then press A on results to play again with the boosted stats.
  high.until((em) => em.v('mg_juggle_dropped') === 1, 200, 'drop');
  toResults(high);
  high.tap('A');
  high.until((em) => em.v('mg_phase') === MG_PH.PLAY && !em.v('in_transition'), 200, 'replay');

  assert.ok(high.v('mg_juggle_pw') > lowPw, `pw: ${high.v('mg_juggle_pw')} > ${lowPw}`);
  assert.ok(high.v('mg_juggle_gw') > lowGw, `gw: ${high.v('mg_juggle_gw')} > ${lowGw}`);
  assert.ok(high.v('mg_juggle_speed') > lowSpd, `speed: ${high.v('mg_juggle_speed')} > ${lowSpd}`);
});

test('an active juggle run stays well inside the frame budget', () => {
  const e = liveJuggle();
  // Enable wind, sparkles, fire, call, and grade word simultaneously for worst-case draw load.
  e.write8(e.symbols.addr('mg_juggle_wind'), 1);
  e.write8(e.symbols.addr('mg_juggle_wind_t'), 150);
  e.write8(e.symbols.addr('mg_juggle_call'), TOUCH.HEAD);

  let worst = 0;
  for (let f = 0; f < 240; f++) {
    const pxPos = px(e.v('mg_juggle_px', 'i16'));
    const bxPos = px(e.v('mg_juggle_bx', 'i16'));
    const cue = e.v('mg_juggle_cue');
    let btn = '';
    if (bxPos < pxPos - 2) btn = 'LEFT';
    else if (bxPos > pxPos + 2) btn = 'RIGHT';
    if (cue === GRADE.PERFECT && e.v('mg_juggle_cool') === 0) btn = btn ? `${btn}+A` : 'A';
    e.hold(btn, 1);
    if (e.v('mg_juggle_dropped')) break;
    worst = Math.max(worst, e.lastFrameCycles);
  }
  assert.ok(e.v('mg_juggle_touches') >= 2, `bot juggled ${e.v('mg_juggle_touches')} times`);
  assert.ok(worst < FRAME_LIMIT / 2, `worst frame ${worst} cycles (limit ${FRAME_LIMIT})`);
});

test('screen: juggle play (ring cue lit)', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  placeBall(e, 80, 14, 0, -24);
  e.until((em) => em.v('mg_juggle_cue') === GRADE.PERFECT, 40, 'PERFECT cue');
  matchGolden(e, 'juggle-play');
});

test('screen: juggle PERFECT header with +2 bonus', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  placeBall(e, 80, 27, 0, -24);
  e.tap('B').idle(20);
  matchGolden(e, 'juggle-perfect');
});

test('screen: juggle milestone celebration (NICE!, fire ball, sparkles, wind)', () => {
  const e = liveJuggle();
  set16(e, 'mg_juggle_px', 80 * 16);
  set16(e, 'mg_juggle_points', 9);
  placeBall(e, 80, 6, 0, -24);
  e.write8(e.symbols.addr('mg_juggle_wind'), 1);
  e.write8(e.symbols.addr('mg_juggle_wind_t'), 120);
  e.tap('A').idle(8);
  matchGolden(e, 'juggle-milestone');
});
