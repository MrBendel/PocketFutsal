// BALL MACHINE (src/mg_machine.c): firing, returns, gold ring bonus, misses,
// pace ramp, RPG stat scaling (REFLEX + CONTROL), full scripted run to results,
// frame budget, and golden screens.
import { test } from 'node:test';
import {
  assert, matchGolden, FRAME_LIMIT, CENTER_X, HALFWAY, px, set16,
} from './helpers.mjs';
import {
  startMinigame, goLive, openGame, phaseSettled, toResults, MG, MG_PH, MG_RUN, STAT,
} from './mg_helpers.mjs';

// Must match src/mg_machine.c.
const BS_NONE = 0, BS_IN = 1, BS_OUT = 2, BS_DEAD = 3;
const MS_WAIT = 0, MS_TELL = 1, MS_FLIGHT = 2;
const R_NONE = 0, R_IN = 1, R_TARGET = 2, R_NET = 3, R_OUT = 4, R_MISS = 5;

/** Start BALL MACHINE live on a deterministic seed. */
function startLive(seed = 4242) {
  return goLive(startMinigame(MG.MACHINE, seed));
}

/** Place the ball in court px with velocity in SUB/frame. */
function placeBall(emu, x, y, z, vx, vy, vz, state = BS_IN) {
  set16(emu, 'mg_machine_bx', x * 16);
  set16(emu, 'mg_machine_by', y * 16);
  set16(emu, 'mg_machine_bz', z * 16);
  set16(emu, 'mg_machine_bvx', vx);
  set16(emu, 'mg_machine_bvy', vy);
  set16(emu, 'mg_machine_bvz', vz);
  emu.write8(emu.symbols.addr('mg_machine_ball'), state);
  emu.write8(emu.symbols.addr('mg_machine_bounces'), 0);
  emu.write8(emu.symbols.addr('mg_machine_fall'), 0);
}

/** Place the player in court px, standing still. */
function placePlayer(emu, x, y) {
  set16(emu, 'mg_machine_px', x * 16);
  set16(emu, 'mg_machine_py', y * 16);
  emu.write8(emu.symbols.addr('mg_machine_pvx'), 0);
  emu.write8(emu.symbols.addr('mg_machine_pvy'), 0);
  emu.write8(emu.symbols.addr('mg_machine_swing'), 0);
  emu.write8(emu.symbols.addr('mg_machine_move_t'), 0);
}

test('machine telegraphs and fires a ball over the net into your half', () => {
  const e = startLive(4242);
  // Opening live screen with training court, HUD, and machine waiting.
  matchGolden(e, 'mg-machine-start');
  e.until((u) => u.v('mg_machine_state') === MS_TELL, 80, 'machine tell');
  e.until((u) => u.v('mg_machine_state') === MS_FLIGHT, 80, 'machine fire');
  assert.equal(e.v('mg_machine_ball'), BS_IN);
  assert.ok(e.v('mg_machine_bvy', 'i16') > 0, 'fired ball travels toward the player');
  assert.ok(e.v('mg_machine_recoil') > 0, 'machine recoils on firing');
  // Step 2 frames so muzzle flash + flying ball are clearly visible.
  e.idle(2);
  matchGolden(e, 'mg-machine-firing');
});

test('striking an incoming ball scores +1 in court and +3 in the gold ring', () => {
  const e = startLive(1337);
  // Park the machine in flight so it does not fire a second ball mid-setup.
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);

  // 1) Clean strike aimed at center (80, 54) while ring is off to the left (32, 20) -> R_IN (+1).
  set16(e, 'mg_machine_tx', 32);
  set16(e, 'mg_machine_ty', 20);
  placePlayer(e, CENTER_X, 180);
  placeBall(e, CENTER_X, 176, 3, 0, 12, 0, BS_IN);
  e.tap('A');
  assert.equal(e.v('mg_machine_ball'), BS_OUT, 'A strikes the ball back');
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 140, 'return lands');
  assert.equal(e.v('mg_machine_result'), R_IN);
  assert.equal(e.v('mg_machine_points', 'u16'), 1);
  assert.equal(e.v('mg_machine_returns', 'u16'), 1);
  assert.equal(e.v('mg_machine_targets', 'u16'), 0);

  // 2) Clean strike aimed straight into the gold ring at (80, 52) -> R_TARGET (+3).
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
  e.write8(e.symbols.addr('mg_machine_result'), R_NONE);
  set16(e, 'mg_machine_tx', CENTER_X);
  set16(e, 'mg_machine_ty', 52);
  placePlayer(e, CENTER_X, 180);
  placeBall(e, CENTER_X, 176, 3, 0, 12, 0, BS_IN);
  e.tap('A');
  assert.equal(e.v('mg_machine_ball'), BS_OUT);
  // Capture mid-return action shot (player in strike pose, ball soaring over net toward ring).
  e.idle(12);
  matchGolden(e, 'mg-machine-return');
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 140, 'target return lands');
  assert.equal(e.v('mg_machine_result'), R_TARGET);
  assert.equal(e.v('mg_machine_points', 'u16'), 4);
  assert.equal(e.v('mg_machine_returns', 'u16'), 2);
  assert.equal(e.v('mg_machine_targets', 'u16'), 1);
});

test('misses (second bounce, out, net) increment misses and 3 misses end the game', () => {
  const e = startLive(2025);
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
  placePlayer(e, 20, 210);  // out of the way

  // 1) Second bounce -> R_MISS.
  placeBall(e, CENTER_X, 170, 6, 0, 0, -4, BS_IN);
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 120, 'second bounce miss');
  assert.equal(e.v('mg_machine_result'), R_MISS);
  assert.equal(e.v('mg_machine_misses'), 1);

  // 2) Return landing past the far baseline -> R_OUT.
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
  e.write8(e.symbols.addr('mg_machine_result'), R_NONE);
  placeBall(e, CENTER_X, 10, 4, 0, -24, -4, BS_OUT);
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 60, 'out of bounds');
  assert.equal(e.v('mg_machine_result'), R_OUT);
  assert.equal(e.v('mg_machine_misses'), 2);

  // 3) Low return crossing the net -> R_NET -> third miss triggers game end.
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
  e.write8(e.symbols.addr('mg_machine_result'), R_NONE);
  placeBall(e, CENTER_X, HALFWAY + 2, 4, 0, -32, 0, BS_OUT);
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 30, 'netted return');
  assert.equal(e.v('mg_machine_result'), R_NET);
  assert.equal(e.v('mg_machine_misses'), 3);
  assert.ok(e.v('mg_machine_end') > 0, 'end countdown started on third miss');

  // After countdown + over banner, shell transitions to MG_PH.RESULT.
  toResults(e, 300);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
});

test('pace level ramps after every 4 returns', () => {
  const e = startLive(777);
  assert.equal(e.v('mg_machine_level'), 0);
  // Put returns at 3 so the next return triggers level 1.
  set16(e, 'mg_machine_returns', 3);
  e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
  set16(e, 'mg_machine_tx', 32);
  set16(e, 'mg_machine_ty', 20);
  placePlayer(e, CENTER_X, 180);
  placeBall(e, CENTER_X, 176, 3, 0, 12, 0, BS_IN);
  e.tap('A');
  e.until((u) => u.v('mg_machine_result') !== R_NONE, 140, '4th return lands');
  assert.equal(e.v('mg_machine_returns', 'u16'), 4);
  assert.equal(e.v('mg_machine_level'), 1);
});

test('REFLEX increases run speed and reach; CONTROL reduces return scatter', () => {
  const enterWithStats = (reflexLv, controlLv) => {
    const e = openGame(MG.MACHINE);
    e.write8(e.symbols.addr('stat_lv') + STAT.REFLEX, reflexLv);
    e.write8(e.symbols.addr('stat_lv') + STAT.CONTROL, controlLv);
    e.writeBytes(e.symbols.addr('test_seed'), [0x39, 0x05]);
    e.idle(25).tap('A');
    phaseSettled(e, MG_PH.PLAY, 200, 'machine play');
    return goLive(e);
  };

  const low = enterWithStats(1, 1);
  const high = enterWithStats(10, 15);
  assert.ok(high.v('mg_machine_speed') > low.v('mg_machine_speed'), 'REFLEX increases run speed');
  assert.ok(high.v('mg_machine_reach') > low.v('mg_machine_reach'), 'REFLEX increases reach');

  // Same slightly off-center contact: measure average landing error over 6 shots.
  const avgErr = (e) => {
    let err = 0;
    for (let i = 0; i < 6; i++) {
      e.write8(e.symbols.addr('mg_machine_state'), MS_FLIGHT);
      e.write8(e.symbols.addr('mg_machine_result'), R_NONE);
      placePlayer(e, CENTER_X, 180);
      placeBall(e, CENTER_X + 8, 176, 3, 0, 12, 0, BS_IN);
      e.tap('A');
      const lx = e.v('mg_machine_land_x', 'i16');
      const ly = e.v('mg_machine_land_y', 'i16');
      err += Math.abs(lx - CENTER_X) + Math.abs(ly - 52);
      e.idle(16);
    }
    return err;
  };
  assert.ok(avgErr(high) < avgErr(low), 'higher CONTROL tightens return scatter');
});

test('scripted bot plays a full game, scores points, and reaches MG_PH.RESULT', () => {
  const e = startLive(4242);
  let targetReturns = 5;
  for (let f = 0; f < 3000; f++) {
    if (e.v('mg_run') === MG_RUN.OVER) break;
    const returns = e.v('mg_machine_returns', 'u16');
    const ball = e.v('mg_machine_ball');
    const buttons = [];

    if (returns < targetReturns) {
      const px0 = px(e.v('mg_machine_px', 'i16'));
      const py0 = px(e.v('mg_machine_py', 'i16'));
      const bx = px(e.v('mg_machine_bx', 'i16'));
      const by = px(e.v('mg_machine_by', 'i16'));
      const bz = px(e.v('mg_machine_bz', 'i16'));
      const tx = ball === BS_IN ? e.v('mg_machine_shot_x', 'i16') : CENTER_X;
      const ty = ball === BS_IN ? Math.min(220, e.v('mg_machine_shot_y', 'i16') + 4) : 184;

      const inReach = ball === BS_IN && by >= HALFWAY
        && Math.abs(bx - px0) <= e.v('mg_machine_reach') - 2
        && (py0 - by) <= 10 && (py0 - by) >= -4 && bz <= 32;

      if (inReach) {
        // Aim toward the gold ring on strike.
        const rx = e.v('mg_machine_tx', 'i16');
        const ry = e.v('mg_machine_ty', 'i16');
        if (rx < 60) buttons.push('LEFT');
        else if (rx > 100) buttons.push('RIGHT');
        if (ry < 36) buttons.push('UP');
        else if (ry > 72) buttons.push('DOWN');
        buttons.push('A');
      } else {
        if (tx < px0 - 3) buttons.push('LEFT');
        else if (tx > px0 + 3) buttons.push('RIGHT');
        if (ty < py0 - 3) buttons.push('UP');
        else if (ty > py0 + 3) buttons.push('DOWN');
      }
    }
    // After scoring targetReturns, stand still so 3 misses finish the game naturally.
    if (buttons.length) e.hold(buttons, 1);
    else e.idle(1);
  }

  assert.equal(e.v('mg_run'), MG_RUN.OVER, 'game ended via mg_finish after 3 misses');
  assert.ok(e.v('mg_machine_returns', 'u16') >= targetReturns, 'bot returned at least 5 balls');
  assert.ok(e.v('mg_score', 'u16') >= targetReturns, 'score reflects returns');
  toResults(e, 200);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.ok(e.v('mg_xp', 'u16') > 0, 'earned XP on results screen');
});

test('a busy ball machine rally stays inside the frame budget', () => {
  const e = startLive(9001);
  let peak = 0;
  for (let f = 0; f < 360; f++) {
    const dir = (f & 32) ? 'RIGHT' : 'LEFT';
    const act = (f % 20 === 0) ? [dir, 'A'] : [dir];
    e.hold(act, 1);
    if (e.lastFrameCycles > peak) peak = e.lastFrameCycles;
  }
  assert.ok(peak < FRAME_LIMIT, `peak ${peak} cycles exceeded budget ${FRAME_LIMIT}`);
});
