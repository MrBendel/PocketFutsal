// Cone Slalom (src/mg_slalom.c): vertical-scrolling dribble gates, heavy touches,
// cone bumps, stat scaling, frame budget, and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, set16, set16At, at, px } from './helpers.mjs';
import { startMinigame, goLive, toResults, MG, MG_PH, STAT } from './mg_helpers.mjs';

const put8 = (e, name, v) => e.write8(e.symbols.addr(name), v);
const put8At = (e, name, i, v) => e.write8(e.symbols.addr(name) + i, v);

/** Start Cone Slalom and advance past READY / GO! into live play. */
function liveSlalom(seed = 4242) {
  return goLive(startMinigame(MG.SLALOM, seed));
}

/** Place gate i cleanly at (gx, gy_px) with half-width gw, active. */
function placeGate(e, i, gx, gyPx, gw = 20) {
  put8At(e, 'mg_slalom_gx', i, gx);
  put8At(e, 'mg_slalom_gw', i, gw);
  set16At(e, 'mg_slalom_gy', i, gyPx * 16);
  put8At(e, 'mg_slalom_gst', i, 0);
  put8At(e, 'mg_slalom_ghit', i, 0);
}

test('steering, pace (UP/DOWN/B), and hardware scrolling work in Cone Slalom', () => {
  const e = liveSlalom();
  const x0 = e.v('mg_slalom_px', 'i16');
  const sy0 = e.v('scroll_y');
  e.hold('RIGHT', 12);
  assert.ok(e.v('mg_slalom_px', 'i16') > x0, 'RIGHT steers player right');
  assert.notEqual(e.v('scroll_y'), sy0, 'pitch scrolls vertically');
  e.hold('UP', 10);
  const fastSpd = e.v('mg_slalom_spd');
  e.hold('DOWN', 10);
  const slowSpd = e.v('mg_slalom_spd');
  assert.ok(fastSpd > slowSpd, `UP (${fastSpd}) faster than DOWN (${slowSpd})`);
  e.hold('B', 6);
  assert.ok(e.v('mg_slalom_spd') < fastSpd, 'B close-control slows pace');
});

test('dribbling through gates scores +1, then +2 on a 3+ streak; missing breaks streak', () => {
  const e = liveSlalom();
  // Put player & ball centered at x = 80, clear all 3 gates off-screen first.
  set16(e, 'mg_slalom_px', 80 * 16);
  set16(e, 'mg_slalom_bx', 80 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  put8(e, 'mg_slalom_loose', 0);
  placeGate(e, 0, 80, 78, 20);
  placeGate(e, 1, 80, 26, 20);
  placeGate(e, 2, 80, -30, 20);

  // Gate 0 passes over ball -> +1, streak 1.
  e.idle(12);
  assert.equal(e.v('mg_slalom_gates'), 1);
  assert.equal(e.v('mg_slalom_streak'), 1);
  assert.equal(e.v('mg_slalom_points', 'u16'), 1);

  // Place gate 1 and gate 2 so streak hits 3 -> +2 bonus!
  placeGate(e, 1, 80, 80, 20);
  e.idle(10);
  assert.equal(e.v('mg_slalom_streak'), 2);
  assert.equal(e.v('mg_slalom_points', 'u16'), 2);

  placeGate(e, 2, 80, 80, 20);
  e.idle(10);
  assert.equal(e.v('mg_slalom_streak'), 3);
  assert.equal(e.v('mg_slalom_points', 'u16'), 4, '3rd consecutive gate awards +2');

  // Now a gate far off to the left scrolls past -> missed, streak resets to 0.
  placeGate(e, 0, 42, 81, 16);
  e.idle(10);
  assert.equal(e.v('mg_slalom_misses'), 1);
  assert.equal(e.v('mg_slalom_streak'), 0);
});

test('heavy touch knocks ball loose while sprinting; B traps it and A quick-cuts', () => {
  const e = liveSlalom();
  // Keep gates out of the way.
  placeGate(e, 0, 80, -40, 20);
  placeGate(e, 1, 80, -90, 20);
  placeGate(e, 2, 80, -140, 20);
  set16(e, 'mg_slalom_px', 60 * 16);
  set16(e, 'mg_slalom_bx', 60 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  put8(e, 'mg_slalom_loose', 0);

  // Sprint + cut right for 15 frames triggers a heavy touch -> ball goes loose.
  e.hold('UP+RIGHT', 15);
  assert.equal(e.v('mg_slalom_loose'), 1, 'heavy touch knocks ball loose');

  // Place ball near player and hold B to cushion/trap it back.
  const pxCur = e.v('mg_slalom_px', 'i16');
  set16(e, 'mg_slalom_bx', pxCur + 6 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  e.hold('B', 4);
  assert.equal(e.v('mg_slalom_loose'), 0, 'B traps loose ball back to feet');

  // Tap A with LEFT: quick cut moves ball left and starts cooldown.
  const bxBefore = e.v('mg_slalom_bx', 'i16');
  e.hold('LEFT+A', 1).idle(2);
  assert.ok(e.v('mg_slalom_cut_cd') > 0, 'A sets quick-cut cooldown');
  assert.ok(e.v('mg_slalom_bx', 'i16') < bxBefore, 'LEFT+A cuts ball left');
});

test('hitting a cone knocks ball loose and breaks streak; out-of-bounds resets ball', () => {
  const e = liveSlalom();
  set16(e, 'mg_slalom_px', 60 * 16);
  set16(e, 'mg_slalom_bx', 60 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  put8(e, 'mg_slalom_streak', 4);
  // Gate centered at 80 with half-width 20 -> left cone is at x = 60, right in front of ball!
  placeGate(e, 0, 80, 80, 20);
  placeGate(e, 1, 80, -40, 20);
  placeGate(e, 2, 80, -90, 20);
  e.idle(6);
  assert.ok(e.v('mg_slalom_hits') >= 1, 'cone hit recorded');
  assert.equal(e.v('mg_slalom_streak'), 0, 'cone hit breaks streak');
  assert.equal(e.v('mg_slalom_loose'), 1, 'cone hit knocks ball loose');

  // Force loose ball past left sideline -> out-of-bounds resets it to feet.
  set16(e, 'mg_slalom_bx', 16 * 16);
  set16(e, 'mg_slalom_bvx', -16);
  e.idle(2);
  assert.ok(e.v('mg_slalom_outs') >= 1, 'out of bounds recorded');
  assert.equal(e.v('mg_slalom_loose'), 0, 'ball reset to feet after out');
});

test('stat scaling: REFLEX increases steer speed and CONTROL increases dribble control', () => {
  const e1 = liveSlalom();
  const spd1 = e1.v('mg_slalom_steer_spd');
  const ctrl1 = e1.v('mg_slalom_ctrl');

  const e2 = startMinigame(MG.SLALOM);
  // Raise REFLEX and CONTROL before entering live play (actually read in enter, so restart).
  put8(e2, 'stat_lv', 1);
  e2.write8(e2.symbols.addr('stat_lv') + STAT.REFLEX, 18);
  e2.write8(e2.symbols.addr('stat_lv') + STAT.CONTROL, 18);
  e2.write8(e2.symbols.addr('state_reenter'), 1);
  e2.idle(35);
  goLive(e2);
  assert.ok(e2.v('mg_slalom_steer_spd') > spd1, `REFLEX 18 steer (${e2.v('mg_slalom_steer_spd')}) > lv1 (${spd1})`);
  assert.ok(e2.v('mg_slalom_ctrl') > ctrl1, `CONTROL 18 ctrl (${e2.v('mg_slalom_ctrl')}) > lv1 (${ctrl1})`);
});

test('bot weaves through gates, stays inside frame budget, and finishes to MG_PH.RESULT', () => {
  const e = liveSlalom(777);
  // Shorten clock to 8 seconds (480 frames) so bot clears several gates and finishes fast.
  set16(e, 'mg_slalom_clock', 480);
  let worst = 0;

  for (let f = 0; f < 500 && e.v('mg_run') === 1; f++) {
    // Pick lowest active gate above the ball and steer toward its center.
    let targetX = 80;
    let bestY = -32000;
    for (let i = 0; i < 3; i++) {
      if (at(e, 'mg_slalom_gst', i) === 0) {
        const gy = at(e, 'mg_slalom_gy', i, 'i16');
        if (gy > bestY && gy < 104 * 16) {
          bestY = gy;
          targetX = at(e, 'mg_slalom_gx', i);
        }
      }
    }
    const pxCur = px(e.v('mg_slalom_px', 'i16'));
    const btn = pxCur < targetX - 3 ? 'RIGHT' : pxCur > targetX + 3 ? 'LEFT' : [];
    e.hold(btn, 1);
    if (e.lastFrameCycles > worst) worst = e.lastFrameCycles;
  }

  assert.ok(worst < FRAME_LIMIT, `worst frame ${worst} >= budget ${FRAME_LIMIT}`);
  assert.ok(e.v('mg_slalom_gates') >= 3, `bot cleared ${e.v('mg_slalom_gates')} gates`);
  assert.ok(e.v('mg_slalom_points', 'u16') >= 3, `bot scored ${e.v('mg_slalom_points', 'u16')}`);

  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.ok(e.v('mg_xp', 'u16') >= 9, 'XP awarded for Cone Slalom');
});

test('screen: slalom live, gate clear (+2 streak), and cone hit', () => {
  const e = liveSlalom(1234);
  set16(e, 'mg_slalom_px', 80 * 16);
  set16(e, 'mg_slalom_bx', 80 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  put8(e, 'mg_slalom_loose', 0);
  placeGate(e, 0, 80, 56, 20);
  placeGate(e, 1, 56, 8, 20);
  placeGate(e, 2, 106, -42, 20);
  e.idle(4);
  matchGolden(e, 'slalom-live');

  // Streak clear (+2 popup & fire ball).
  put8(e, 'mg_slalom_streak', 3);
  placeGate(e, 0, 80, 82, 20);
  e.idle(6);
  matchGolden(e, 'slalom-clear');

  // Cone bump (tilted cone + HIT! popup).
  set16(e, 'mg_slalom_px', 60 * 16);
  set16(e, 'mg_slalom_bx', 60 * 16);
  set16(e, 'mg_slalom_by', 84 * 16);
  put8(e, 'mg_slalom_loose', 0);
  placeGate(e, 0, 80, 81, 20);
  e.idle(4);
  matchGolden(e, 'slalom-hit');
});
