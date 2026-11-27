// Powering up near the slam zone runs you to its middle (play.c, ZONE_PULL_*).
import { test } from 'node:test';
import { assert, startGame, served, YOU, CPU, PHASE_RALLY, MOVE_BICYCLE, at, set16, placeBall, placePlayer } from './helpers.mjs';

/** A CPU lob coming down to you; you `off` px across and `offY` in depth from where to meet it. */
function lob(off, offY) {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 60, 50);
  set16(emu, 'ball_vy', 26);
  set16(emu, 'ball_vz', 28);
  set16(emu, 'ball_vx', 4);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  emu.write8(emu.symbols.addr('ball_landings'), 0);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  emu.idle(4);
  const mx = at(emu, 'meet_x', YOU, 'i16'), my = at(emu, 'meet_y', YOU, 'i16');
  placePlayer(emu, YOU, mx + off, my + offY);
  emu.idle(2);
  return { emu, mx, my };
}

test('powering up near the slam zone runs you to its middle, and you slam it', () => {
  const { emu, mx, my } = lob(34, 20);  // off to the side, out of the leap's reach
  for (let k = 0; k < 4; k++) emu.tap('A');  // a power-up: half a meter
  const off = (e) => Math.abs(Math.floor(at(e, 'pl_x', YOU, 'i16') / 16) - mx) + Math.abs(Math.floor(at(e, 'pl_y', YOU, 'i16') / 16) - my);
  emu.until((e) => off(e) <= 4 || at(e, 'pl_shots', YOU) > 0, 40, 'run to the middle');
  const dx = Math.abs(Math.floor(at(emu, 'pl_x', YOU, 'i16') / 16) - mx), dy = Math.abs(Math.floor(at(emu, 'pl_y', YOU, 'i16') / 16) - my);
  assert.ok(dx <= 4 && dy <= 4, `in the middle: ${dx}, ${dy} px off`);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 150, 'the slam');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_BICYCLE, 'a slam out of the air');
  assert.equal(emu.v('ball_power'), 1, 'on fire');
});

test('too far from the zone, powering up does not run you there', () => {
  const { emu } = lob(70, 0);
  const x0 = at(emu, 'pl_x', YOU, 'i16');
  for (let k = 0; k < 4; k++) emu.tap('A');
  emu.idle(12);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x0, 'planted where you stood');
});
