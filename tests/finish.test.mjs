// What ends rallies (play.c): a slam is a finisher, met clean only set with a full
// meter; rally heat builds each touch.
import { test } from 'node:test';
import { assert, startGame, served, YOU, CPU, PHASE_RALLY, SHOT_POWER, CHARGE_MAX, at, set16, placeBall, placePlayer } from './helpers.mjs';

/** Your slam (on fire, a finisher) coming at the CPU, standing readied with `charge`. */
function slamAtCpu(charge) {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  placePlayer(emu, YOU, 80, 190);
  placePlayer(emu, CPU, 80, 40);
  placeBall(emu, 80, 64);  // close: the CPU's meter fills by itself as it waits
  set16(emu, 'ball_vy', -40);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.write8(emu.symbols.addr('ball_finisher'), 1);
  emu.write8(emu.symbols.addr('pl_shot') + CPU, SHOT_POWER);
  emu.write8(emu.symbols.addr('pl_charge') + CPU, charge);
  emu.until((e) => at(e, 'pl_shots', CPU) > 0 || at(e, 'pl_move', CPU) === 6 || e.v('goal_timer'), 60, 'the CPU meeting it');
  return emu;
}

test('a slam is a finisher: short of a full meter, met standing, it pops up; set with a full meter, clean', () => {
  const short = slamAtCpu(CHARGE_MAX - 8);
  assert.equal(short.v('ball_pop'), 1, 'popped up: another slam for you');
  const full = slamAtCpu(CHARGE_MAX);
  assert.equal(at(full, 'pl_shots', CPU), 1, 'met');
  assert.equal(full.v('ball_pop'), 0, 'clean');
  assert.ok(full.v('ball_vy', 'i16') > 0, 'returned');
});

/** Your drive, two presses, at a ball coming; rally_n touches so far this point. */
function driveAfter(touches) {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  emu.write8(emu.symbols.addr('rally_n'), touches);
  emu.write8(emu.symbols.addr('rally_heat'), Math.min(touches >> 1, 12));
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 80, 124);
  set16(emu, 'ball_vy', 36);
  emu.tap('A').tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 60, 'the strike');
  return emu;
}

test('rally heat: deep in a rally the same strike goes faster, a ball on fire takes more to meet; a new point, none', () => {
  const fresh = driveAfter(0), long = driveAfter(30);
  const pace = (e) => Math.abs(e.v('ball_vx', 'i16')) + Math.abs(e.v('ball_vy', 'i16'));
  assert.ok(pace(long) > pace(fresh), `faster: ${pace(fresh)} then ${pace(long)}`);
  assert.equal(long.v('rally_heat'), 12, 'at most');
  assert.equal(long.v('rally_meet'), 6);
  set16(long, 'ball_z', 0);
  long.until((e) => e.v('goal_timer'), 300, 'the point');
  long.until((e) => !e.v('goal_timer'), 200, 'the next');
  assert.equal(long.v('rally_n'), 0, 'reset');
  assert.equal(long.v('rally_heat'), 0);
});
