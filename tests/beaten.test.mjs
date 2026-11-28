// A missed slide: beaten, that player doesn't play the ball again till the other side
// strikes it (play.c, pl_beaten).
import { test } from 'node:test';
import { assert, startGame, served, YOU, CPU, PHASE_RALLY, MOVE_SLIDE, SHOT_POWER, at, set16, placeBall, placePlayer } from './helpers.mjs';

test('slid for it and missed: up again, you still can not play that ball; the next point, you can', () => {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 150, 130);  // 70 px across: too far, a miss at full stretch
  set16(emu, 'ball_vy', 40);
  emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
  emu.write8(emu.symbols.addr('pl_charge') + YOU, 12);
  emu.until((e) => at(e, 'pl_move', YOU) === MOVE_SLIDE, 60, 'the slide');
  emu.until((e) => at(e, 'pl_beaten', YOU) === 1, 40, 'missed');
  assert.equal(at(emu, 'pl_shots', YOU), 0);
  emu.until((e) => at(e, 'pl_move_timer', YOU) === 0, 120, 'up again');
  // The same ball, back in reach and rolling at you: it goes by.
  placeBall(emu, Math.floor(at(emu, 'pl_x', YOU, 'i16') / 16), Math.floor(at(emu, 'pl_y', YOU, 'i16') / 16) - 20);
  set16(emu, 'ball_vy', 30);
  emu.tap('A').tap('A');
  emu.idle(30);
  assert.equal(at(emu, 'pl_shots', YOU), 0, 'beaten: no touch');
  // The next point: you're back in it.
  set16(emu, 'ball_z', 16 * 4);
  emu.until((e) => e.v('goal_timer'), 200, 'the point');
  emu.until((e) => !e.v('goal_timer'), 200, 'the next serve');
  assert.equal(at(emu, 'pl_beaten', YOU), 0, 'a new ball');
});
