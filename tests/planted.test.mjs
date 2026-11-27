// Readying a strike plants you only once the ball's coming (play.c, you_planted).
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

test('queued while your own shot is going over, you still run; a press once it is coming plants you', () => {
  const emu = afterYourKick();
  emu.hold('A', 2);
  assert.ok(at(emu, 'pl_shot', YOU), 'queued');
  let x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 20);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') - x0 > 16 * 20, 'ran right with the strike queued');
  assert.ok(at(emu, 'pl_shot', YOU), 'still queued');
  emu.idle(20);  // stopped
  emu.write8(emu.symbols.addr('last_touch'), CPU);  // the CPU's return
  placeBall(emu, 80, 60);
  set16(emu, 'ball_z', 16 * 10);
  set16(emu, 'ball_vy', 20);
  emu.hold('A', 2);
  x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x0, 'pressed with the ball coming: planted, the d-pad only aims');
});
