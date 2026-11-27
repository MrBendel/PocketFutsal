// A trail on every strike struck flashing: a drive from half a meter (play.c, strike()).
import { test } from 'node:test';
import { assert, startGame, served, YOU, CPU, PHASE_RALLY, at, set16, placeBall, placePlayer } from './helpers.mjs';

function driveWith(presses) {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 80, 124);
  set16(emu, 'ball_vy', 36);
  for (let k = 0; k < presses; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 60, 'the strike');
  return { trail: emu.v('ball_trail'), len: emu.v('trail_len'), power: emu.v('ball_power') };
}

test('an A drive struck flashing (half a meter) leaves a red trail, shorter than on fire; less, none', () => {
  const half = driveWith(4);
  assert.equal(half.power, 0, 'not on fire');
  assert.equal(half.trail, 1, 'a red trail');
  assert.equal(half.len, 3, 'three dots');
  const two = driveWith(2);
  assert.equal(two.trail, 0, 'two presses (not flashing): none');
});
