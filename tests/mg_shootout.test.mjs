// SHOOTOUT (src/mg_shootout.c): aiming, power meter, shooting outcomes,
// goalkeeping dives/timing/tell, stat scaling, full shootout completion,
// sudden death, frame budget, and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, set16 } from './helpers.mjs';
import { openGame, phaseSettled, startMinigame, goLive, toResults, MG, MG_PH, MG_RUN, STAT } from './mg_helpers.mjs';

// Must match src/mg_shootout.c.
const P = { INTRO: 0, AIM: 1, POWER: 2, RUNUP: 3, CPURUN: 4, FLIGHT: 5, RESULT: 6, DONE: 7 };
const KICK = { YOU: 0, CPU: 1 };
const R = { NONE: 0, GOAL: 1, SAVED: 2, OVER: 3, WIDE: 4 };
const MARK = { NONE: 0, GOAL: 1, MISS: 2 };

function liveShootout(seed = 4242) {
  return goLive(startMinigame(MG.SHOOTOUT, seed));
}

function untilPhase(e, phase, frames = 200) {
  return e.until((x) => x.v('mg_shootout_phase') === phase, frames, `phase ${phase}`);
}

/** Read a null-terminated C string from WRAM. */
function readCString(e, sym, max = 24) {
  const base = e.symbols.addr(sym);
  let s = '';
  for (let i = 0; i < max; i++) {
    const ch = e.read8(base + i);
    if (!ch) break;
    s += String.fromCharCode(ch);
  }
  return s;
}

/** From P_AIM, aim at (col, row), press A, wait until power reaches targetPower, press A. */
function takeKick(e, col, row, targetPower) {
  untilPhase(e, P.AIM, 120);
  while (e.v('mg_shootout_col') > col) e.tap('LEFT');
  while (e.v('mg_shootout_col') < col) e.tap('RIGHT');
  if (row && !e.v('mg_shootout_row')) e.tap('UP');
  if (!row && e.v('mg_shootout_row')) e.tap('DOWN');
  e.tap('A');
  assert.equal(e.v('mg_shootout_phase'), P.POWER);
  e.until((x) => x.v('mg_shootout_power') >= targetPower, 120, `power >= ${targetPower}`);
  e.tap('A');
  assert.equal(e.v('mg_shootout_phase'), P.RUNUP);
}

test('shooting: d-pad aims all 6 spots, A sweeps power, B cancels back to aim', () => {
  const e = liveShootout();
  untilPhase(e, P.AIM);
  assert.equal(e.v('mg_shootout_kicker'), KICK.YOU);
  assert.deepEqual([e.v('mg_shootout_col'), e.v('mg_shootout_row')], [1, 0]);

  e.tap('LEFT').tap('LEFT');
  assert.equal(e.v('mg_shootout_col'), 0, 'clamps at left post');
  e.tap('UP').tap('UP');
  assert.equal(e.v('mg_shootout_row'), 1, 'clamps at top');
  e.tap('RIGHT').tap('RIGHT').tap('RIGHT');
  assert.equal(e.v('mg_shootout_col'), 2, 'clamps at right post');
  e.tap('DOWN');
  assert.equal(e.v('mg_shootout_row'), 0);

  // A starts power sweep; B returns to aim and resets power.
  e.tap('A');
  assert.equal(e.v('mg_shootout_phase'), P.POWER);
  e.idle(10);
  assert.ok(e.v('mg_shootout_power') > 10, 'meter is sweeping up');
  e.tap('B');
  assert.equal(e.v('mg_shootout_phase'), P.AIM);
  assert.equal(e.v('mg_shootout_power'), 0, 'power reset on cancel');
});

test('shooting: sweet-spot corner beats keeper, weak shot is read and saved, overpower goes over', () => {
  // 1) Sweet-spot corner shot -> GOAL even if keeper guesses right.
  let e = liveShootout(101);
  untilPhase(e, P.AIM);
  takeKick(e, 2, 1, 50);  // top right, sweet spot (48..55)
  untilPhase(e, P.FLIGHT, 40);
  assert.equal(e.v('mg_shootout_over'), 0);
  assert.ok(e.v('mg_shootout_flight') <= 16, `fast sweet-spot flight: ${e.v('mg_shootout_flight')}`);
  // Force keeper to guess the exact right column to prove sweet spot beats it in a corner.
  e.write8(e.symbols.addr('mg_shootout_kguess'), 2);
  untilPhase(e, P.RESULT, 60);
  assert.equal(e.v('mg_shootout_result'), R.GOAL);
  assert.equal(e.v('mg_shootout_you_goals'), 1);
  assert.equal(e.v('mg_shootout_you_kicks'), 1);
  assert.equal(e.read8(e.symbols.addr('mg_shootout_marks_you')), MARK.GOAL);
  assert.equal(e.read8(e.symbols.addr('mg_shootout_hist') + 2), 1, 'history recorded');

  // 2) Weak shot (< 18) -> CPU keeper reads it and saves.
  e = liveShootout(102);
  takeKick(e, 0, 0, 5);
  untilPhase(e, P.FLIGHT, 40);
  assert.equal(e.v('mg_shootout_flight'), 40, 'weak flight');
  assert.equal(e.v('mg_shootout_kguess'), 0, 'keeper read weak shot');
  untilPhase(e, P.RESULT, 60);
  assert.equal(e.v('mg_shootout_result'), R.SAVED);
  assert.equal(e.v('mg_shootout_you_goals'), 0);
  assert.equal(e.read8(e.symbols.addr('mg_shootout_marks_you')), MARK.MISS);

  // 3) Overpowered high shot (> 55) -> OVER!
  e = liveShootout(103);
  takeKick(e, 1, 1, 58);
  untilPhase(e, P.FLIGHT, 40);
  assert.equal(e.v('mg_shootout_over'), 1);
  untilPhase(e, P.RESULT, 40);
  assert.equal(e.v('mg_shootout_result'), R.OVER);
  assert.equal(e.v('mg_shootout_you_goals'), 0);

  // 4) Low aim with slight overpower (56..59) rises to high without going over.
  e = liveShootout(104);
  untilPhase(e, P.AIM);
  e.tap('A');
  e.until((x) => x.v('mg_shootout_power') >= 56 && x.v('mg_shootout_power') <= 59, 120, 'power 56..59');
  e.tap('A');
  untilPhase(e, P.FLIGHT, 40);
  assert.equal(e.v('mg_shootout_over'), 0, 'slight overpower from low stays under bar');
  assert.equal(e.v('mg_shootout_row'), 1, 'rose to high');
});

test('goalkeeping: timely dive saves, wrong side concedes, staying still saves center only', () => {
  // Helper: skip straight to CPU run-up with deterministic target.
  function setupCpuKick(col, row, miss = 0, seed = 202) {
    const e = liveShootout(seed);
    untilPhase(e, P.AIM);
    // Advance to CPU's turn by finishing kick 0 quickly.
    e.write8(e.symbols.addr('mg_shootout_you_kicks'), 1);
    e.write8(e.symbols.addr('mg_shootout_phase'), P.RESULT);
    e.write8(e.symbols.addr('mg_shootout_t'), 79);
    untilPhase(e, P.CPURUN, 80);
    assert.equal(e.v('mg_shootout_kicker'), KICK.CPU);
    e.write8(e.symbols.addr('mg_shootout_col'), col);
    e.write8(e.symbols.addr('mg_shootout_row'), row);
    e.write8(e.symbols.addr('mg_shootout_miss'), miss);
    return e;
  }

  // 1) Timely dive to the right corner & height -> SAVED!
  let e = setupCpuKick(0, 0);
  const run = e.v('mg_shootout_run');
  const tell = e.v('mg_shootout_tell');
  assert.ok(tell >= 18, `readable goalie tell window (${tell}f)`);
  e.until((x) => x.v('mg_shootout_clock', 'i16') >= run - tell + 2, 120, 'into tell window');
  const cueY = e.read8(0xFE00 + 14 * 4);
  assert.ok(cueY > 0 && cueY < 160, `goalie aim cue reticle visible during tell (OAM y=${cueY})`);
  e.until((x) => x.v('mg_shootout_clock', 'i16') >= run - 4, 120, 'just before kick');
  e.hold('LEFT+A', 1);
  assert.equal(e.v('mg_shootout_dived'), 1);
  assert.equal(e.v('mg_shootout_kguess'), 0);
  assert.equal(e.v('mg_shootout_switched'), 0, 'within tell window: CPU did not switch');
  untilPhase(e, P.RESULT, 60);
  assert.equal(e.v('mg_shootout_result'), R.SAVED);
  assert.equal(e.v('mg_shootout_saves'), 1);
  assert.equal(e.v('mg_shootout_cpu_goals'), 0);

  // 2) Wrong direction -> GOAL!
  e = setupCpuKick(2, 1);
  const run2 = e.v('mg_shootout_run');
  e.until((x) => x.v('mg_shootout_clock', 'i16') >= run2 - 3, 120, 'just before kick');
  e.hold('LEFT+A', 1);
  untilPhase(e, P.RESULT, 60);
  assert.equal(e.v('mg_shootout_result'), R.GOAL);
  assert.equal(e.v('mg_shootout_cpu_goals'), 1);

  // 3) No dive on center shot -> SAVED; no dive on corner shot -> GOAL.
  e = setupCpuKick(1, 0);
  untilPhase(e, P.RESULT, 120);
  assert.equal(e.v('mg_shootout_result'), R.SAVED, 'standing center stops center shot');

  e = setupCpuKick(2, 0);
  untilPhase(e, P.RESULT, 120);
  assert.equal(e.v('mg_shootout_result'), R.GOAL, 'standing center concedes corner shot');

  // 4) CPU off-target low corner -> WIDE!
  e = setupCpuKick(0, 0, 1);
  untilPhase(e, P.RESULT, 120);
  assert.equal(e.v('mg_shootout_result'), R.WIDE);
  assert.equal(e.v('mg_shootout_cpu_goals'), 0);
});

test('stat scaling: POWER widens sweet spot & speeds shots; REFLEX lengthens tell & speeds dives', () => {
  // Low stats (level 1).
  const e1 = liveShootout(303);
  const sweet1 = e1.v('mg_shootout_sweet_lo');
  untilPhase(e1, P.AIM);
  takeKick(e1, 2, 0, 50);
  untilPhase(e1, P.FLIGHT, 40);
  const flight1 = e1.v('mg_shootout_flight');

  // High POWER & REFLEX (level 12): set stats on the brief card before entering play.
  const e2 = openGame(MG.SHOOTOUT);
  e2.writeBytes(e2.symbols.addr('test_seed'), [303 & 255, 303 >> 8]);
  e2.write8(e2.symbols.addr('stat_lv') + STAT.POWER, 12);
  e2.write8(e2.symbols.addr('stat_lv') + STAT.REFLEX, 12);
  e2.idle(25).tap('A');
  phaseSettled(e2, MG_PH.PLAY, 200, 'play with high stats');
  goLive(e2);

  const sweet2 = e2.v('mg_shootout_sweet_lo');
  assert.ok(sweet2 < sweet1, `higher POWER widens sweet spot: ${sweet2} < ${sweet1}`);
  untilPhase(e2, P.AIM);
  takeKick(e2, 2, 0, 50);
  untilPhase(e2, P.FLIGHT, 40);
  const flight2 = e2.v('mg_shootout_flight');
  assert.ok(flight2 < flight1, `higher POWER speeds shot: ${flight2} < ${flight1}`);

  // Advance to CPU run-up and verify REFLEX gives earlier tell and faster dive.
  e2.write8(e2.symbols.addr('mg_shootout_you_kicks'), 1);
  e2.write8(e2.symbols.addr('mg_shootout_phase'), P.RESULT);
  e2.write8(e2.symbols.addr('mg_shootout_t'), 79);
  untilPhase(e2, P.CPURUN, 80);
  const tellHigh = e2.v('mg_shootout_tell');
  assert.ok(tellHigh >= 18, `REFLEX 12 gives early tell: ${tellHigh}`);
  e2.hold('LEFT+A', 1);
  const moveHigh = e2.v('mg_shootout_kmove');
  assert.ok(moveHigh <= 10, `REFLEX 12 gives quick dive: ${moveHigh}`);
});

test('full shootout: scripted win reaches MG_PH.RESULT with mg_won, mg_score, and mg_detail', () => {
  const e = liveShootout(777);
  // Play out kicks until early clinch (e.g. 3-0 after 3 rounds).
  for (let round = 0; round < 5 && e.v('mg_run') === MG_RUN.LIVE; round++) {
    // Your kick: sweet-spot corner!
    untilPhase(e, P.AIM, 160);
    takeKick(e, round & 1 ? 0 : 2, 1, 50);
    untilPhase(e, P.RESULT, 80);
    assert.equal(e.v('mg_shootout_result'), R.GOAL, `round ${round + 1} goal`);
    if (e.v('mg_run') !== MG_RUN.LIVE) break;

    // CPU kick: read its target and dive at the right moment to save it.
    untilPhase(e, P.CPURUN, 160);
    const col = e.v('mg_shootout_col');
    const row = e.v('mg_shootout_row');
    const run = e.v('mg_shootout_run');
    e.until((x) => x.v('mg_shootout_clock', 'i16') >= run - 3, 120, 'dive window');
    const keys = [];
    if (col === 0) keys.push('LEFT');
    else if (col === 2) keys.push('RIGHT');
    if (row) keys.push('UP');
    keys.push('A');
    e.hold(keys.join('+'), 1);
    untilPhase(e, P.RESULT, 80);
    // Wait for result to transition (either next kick or finish).
    e.until((x) => x.v('mg_shootout_phase') !== P.RESULT || x.v('mg_run') === MG_RUN.OVER, 100, 'next or over');
  }
  assert.equal(e.v('mg_run'), MG_RUN.OVER, 'shootout clinched early');
  const goals = e.v('mg_shootout_you_goals');
  const saves = e.v('mg_shootout_saves');
  const cpuGoals = e.v('mg_shootout_cpu_goals');
  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.equal(e.v('mg_won'), 1);
  assert.equal(e.v('mg_score', 'u16'), goals + saves);
  assert.equal(readCString(e, 'mg_detail'), `YOU ${goals} - ${cpuGoals} CPU`);
});

test('sudden death: tied 5-5 continues to round 6 and decides after the pair of kicks', () => {
  const e = liveShootout(888);
  untilPhase(e, P.AIM);
  // Set up tied 5-5 after regulation, about to start kick 6.
  e.write8(e.symbols.addr('mg_shootout_you_goals'), 5);
  e.write8(e.symbols.addr('mg_shootout_cpu_goals'), 5);
  e.write8(e.symbols.addr('mg_shootout_you_kicks'), 5);
  e.write8(e.symbols.addr('mg_shootout_cpu_kicks'), 5);
  // Score kick 6 (6-5, CPU still gets its reply!).
  takeKick(e, 2, 1, 50);
  untilPhase(e, P.RESULT, 80);
  assert.equal(e.v('mg_shootout_you_goals'), 6);
  untilPhase(e, P.CPURUN, 160);
  assert.equal(e.v('mg_run'), MG_RUN.LIVE, 'sudden death waits for CPU reply');
  // Save CPU kick 6 -> YOU WIN 6 - 5!
  e.write8(e.symbols.addr('mg_shootout_col'), 0);
  e.write8(e.symbols.addr('mg_shootout_row'), 0);
  e.write8(e.symbols.addr('mg_shootout_miss'), 0);
  const run = e.v('mg_shootout_run');
  e.until((x) => x.v('mg_shootout_clock', 'i16') >= run - 3, 120, 'dive window');
  e.hold('LEFT+A', 1);
  e.until((x) => x.v('mg_run') === MG_RUN.OVER, 160, 'sudden death win');
  assert.equal(e.v('mg_won'), 1);
  assert.equal(readCString(e, 'mg_detail'), 'YOU 6 - 5 CPU');
});

test('a busy shootout stays inside the frame budget', () => {
  const e = liveShootout(555);
  let worst = 0;
  for (let f = 0; f < 400; f++) {
    const ph = e.v('mg_shootout_phase');
    let pad = '';
    if (ph === P.AIM) pad = f & 1 ? 'RIGHT' : 'A';
    else if (ph === P.POWER && e.v('mg_shootout_power') >= 45) pad = 'A';
    else if (ph === P.CPURUN && e.v('mg_shootout_clock', 'i16') >= e.v('mg_shootout_run') - 4) pad = 'LEFT+A';
    e.hold(pad, 1);
    if (e.v('mg_phase') !== MG_PH.PLAY || e.v('in_transition')) continue;
    worst = Math.max(worst, e.lastFrameCycles);
  }
  assert.ok(worst < FRAME_LIMIT / 2, `worst frame ${worst} cycles (limit ${FRAME_LIMIT})`);
});

test('screen: shootout aiming and power meter', () => {
  const e = liveShootout(4242);
  untilPhase(e, P.AIM);
  // Score round 1 and save CPU round 1 so both green & red marks show on the scoreboard.
  takeKick(e, 2, 1, 50);
  untilPhase(e, P.CPURUN, 180);
  e.write8(e.symbols.addr('mg_shootout_col'), 0);
  e.write8(e.symbols.addr('mg_shootout_row'), 0);
  e.write8(e.symbols.addr('mg_shootout_miss'), 0);
  const run = e.v('mg_shootout_run');
  e.until((x) => x.v('mg_shootout_clock', 'i16') >= run - 3, 120, 'dive window');
  e.hold('LEFT+A', 1);
  untilPhase(e, P.AIM, 180);
  // Round 2: aim top-right and start power meter sweeping near the sweet spot.
  e.tap('RIGHT').tap('UP').tap('A');
  e.until((x) => x.v('mg_shootout_power') >= 44, 80, 'power near sweet spot');
  matchGolden(e, 'shootout-aim');
});

test('screen: shootout mid-flight with keeper diving', () => {
  const e = liveShootout(4242);
  untilPhase(e, P.AIM);
  takeKick(e, 2, 1, 50);
  untilPhase(e, P.FLIGHT, 40);
  e.write8(e.symbols.addr('mg_shootout_kguess'), 0);
  set16(e, 'mg_shootout_dive_at', e.v('mg_shootout_contact', 'i16') + 2);
  e.idle(9);
  matchGolden(e, 'shootout-dive');
});

test('screen: shootout GOAL! banner', () => {
  const e = liveShootout(4242);
  untilPhase(e, P.AIM);
  takeKick(e, 2, 1, 50);
  untilPhase(e, P.RESULT, 80);
  e.idle(12);
  matchGolden(e, 'shootout-goal');
});
