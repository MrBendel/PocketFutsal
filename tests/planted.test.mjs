// Readying a strike plants you only once the ball's coming, and only powering up (flashing,
// half a meter) locks you: a tap is a kick, and you run on to meet it (play.c, you_planted).
import { test } from 'node:test';
import { assert, startGame, served, YOU, CPU, PHASE_RALLY, at, set16, placeBall, placePlayer } from './helpers.mjs';

/** Mid-rally, your shot just struck and heading over the net to the CPU. */
function afterYourKick() {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  placePlayer(emu, CPU, 80, 30);
  placePlayer(emu, YOU, 40, 180);
  placeBall(emu, 60, 150);
  set16(emu, 'ball_z', 16 * 10);
  set16(emu, 'ball_vy', -20);
  return emu;
}

test('queued while your own shot is going over, you still run; powered up once it is coming, you are locked', () => {
  const emu = afterYourKick();
  emu.tap('A').tap('A');  // two presses: a power-up (one is a kick, at the air)
  assert.ok(at(emu, 'pl_shot', YOU), 'queued');
  let x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 20);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') - x0 > 16 * 20, 'ran right with the strike queued');
  assert.ok(at(emu, 'pl_shot', YOU), 'still queued');
  emu.idle(20);  // stopped
  emu.write8(emu.symbols.addr('last_touch'), CPU);  // the CPU's return
  placeBall(emu, 80, 100);  // over the net, high, coming slowly
  set16(emu, 'ball_z', 16 * 40);
  set16(emu, 'ball_vy', 12);
  emu.hold('A', 2);
  x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 8);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') > x0, 'one more tap, the ball coming: a kick, you still run');
  emu.idle(6);
  for (let k = 0; k < 2; k++) { emu.hold('A', 2); emu.idle(2); }
  assert.ok(at(emu, 'pl_charge', YOU) >= 16, 'half a meter: flashing');
  emu.until((e) => at(e, 'pl_vx', YOU) === 0, 40, 'the coast over');
  x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x0, 'powering up: locked, the d-pad only aims');
});

/** Mid-rally, the CPU's ball rolling at you from (80, y) at 2.25 px a frame; you at (80, 190). */
function coming(y) {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 80, y);
  set16(emu, 'ball_vy', 36);
  return emu;
}

test('a press is a kick, now: the ball there in time is struck; too early, a whiff', () => {
  const near = coming(172);  // a few frames off
  near.tap('A');
  assert.ok(near.v('you_swing'), 'swinging (the kick pose)');
  near.until((e) => at(e, 'pl_shots', YOU) > 0, 12, 'the kick meeting it');
  assert.ok(near.v('ball_vy', 'i16') < 0, 'returned');

  const early = coming(124);  // about 30 frames off: past the kick
  early.tap('A');
  assert.ok(early.v('you_swing'), 'swinging');
  early.idle(14);
  assert.equal(early.v('you_swing'), 0, 'the kick over');
  assert.equal(at(early, 'pl_shot', YOU), 0, 'a whiff: the strike gone');
  assert.equal(at(early, 'pl_shots', YOU), 0, 'nothing struck');
});

test('a second press in the kick makes it a power-up: queued till the ball comes, then struck', () => {
  const emu = coming(124);
  emu.tap('A').tap('A');
  assert.equal(emu.v('you_swing'), 0, 'the kick called off');
  emu.idle(14);
  assert.ok(at(emu, 'pl_shot', YOU), 'still queued past the kick');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'returned');
});
