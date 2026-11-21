// The game shell (title -> play -> game over -> title) and the match. Keep at
// least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import {
  assert, matchGolden, settle, bootToTitle, startGame, kickedOff, FRAME_LIMIT,
  TITLE, PLAY, GAME_OVER, YOU, CPU, NOBODY, SHOT_NONE, SHOT_DRIVE, SHOT_CHIP, CHARGE_MAX,
  px, at, set16, set16At, placeBall, placePlayer,
} from './helpers.mjs';

/** A match after the whistle, the CPU standing still out of the way. */
function quietMatch() {
  const emu = kickedOff(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  placePlayer(emu, CPU, 60, 20);
  return emu;
}

/** Ready a shot (A or B), wait `charge` frames, then walk right into the ball, UP or DOWN as it goes. */
function shoot(emu, button, charge, aim = '') {
  if (button) emu.hold(button, 1);
  emu.idle(charge);
  for (let f = 0; at(emu, 'pl_shots', YOU) === 0; f++) {
    assert.ok(f < 120, 'the shot never went');
    const far = px(emu.v('ball_x', 'i16')) - px(at(emu, 'pl_x', YOU, 'i16')) > 11;
    emu.hold(far || !aim ? 'RIGHT' : `RIGHT+${aim}`, 1);
  }
  return emu;
}

/** Run until the ball is caught, parried, or goes in (or `n` frames pass). */
function outcome(emu, n = 150) {
  for (let i = 0; i < n; i++) {
    emu.idle(1);
    if (emu.v('goal_timer')) return 'goal';
    if (emu.v('ball_owner') !== NOBODY) return 'caught';
    if (emu.v('last_touch') === CPU) return 'parried';
  }
  return 'none';
}

test('boots to the title screen and runs one loop per frame', () => {
  const emu = bootToTitle();
  const f0 = emu.v('frame_count', 'u16');
  emu.run(60);
  const fc = emu.v('frame_count', 'u16') - f0;
  // One game loop per frame: a slowdown or a hang shows up here.
  assert.ok(fc >= 59 && fc <= 61, `frame_count advanced ${fc} in 60 frames`);
});

test('the title waits for START', () => {
  const emu = bootToTitle().hold('RIGHT+UP+B+SELECT', 60).idle(5);
  assert.equal(emu.v('game_state'), TITLE);
});

test('START begins a match seeded by press timing', () => {
  const play = (wait) => settle(bootToTitle().idle(wait).tap('START'), PLAY, 'play state');
  const a = play(10), b = play(37);
  assert.notEqual(a.v('game_seed', 'u16'), b.v('game_seed', 'u16'));
  // Same timing, same seed: replays reproduce matches.
  assert.equal(play(10).v('game_seed', 'u16'), a.v('game_seed', 'u16'));
});

test('the match kicks off 0-0, the ball on the spot, nobody moving until the whistle', () => {
  const emu = startGame();
  assert.deepEqual([at(emu, 'score', YOU), at(emu, 'score', CPU)], [0, 0]);
  assert.deepEqual([px(emu.v('ball_x', 'i16')), px(emu.v('ball_y', 'i16'))], [80, 68]);
  assert.ok(emu.v('kickoff_timer') > 0);
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 10);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'you wait for the whistle');
  kickedOff(emu).hold('RIGHT', 10);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') > x, 'then the d-pad runs');
});

test('running into the ball dribbles it ahead; standing still traps it', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 60, 68);
  placeBall(emu, 70, 68);
  emu.hold('RIGHT', 12);
  assert.ok(emu.v('ball_vx', 'i16') > 20, 'pushed on ahead');
  assert.ok(px(emu.v('ball_x', 'i16')) > px(at(emu, 'pl_x', YOU, 'i16')));
  assert.equal(emu.v('last_touch'), YOU);
  // A rolling ball into a still player stops at its feet.
  placePlayer(emu, YOU, 100, 68);
  placeBall(emu, 80, 68);
  set16(emu, 'ball_vx', 24);
  emu.idle(30);
  assert.ok(Math.abs(emu.v('ball_vx', 'i16')) < 8, 'trapped');
  assert.ok(Math.abs(px(emu.v('ball_x', 'i16')) - 100) < 10);
});

test('A readies a shot that charges while you wait, slows you, and fires in reach', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 84, 68);
  placeBall(emu, 100, 68);
  emu.hold('A', 1).idle(10);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DRIVE);
  assert.ok(at(emu, 'pl_charge', YOU) >= 10);
  emu.hold('RIGHT', 1);
  assert.equal(at(emu, 'pl_vx', YOU, 'i8'), 8, 'a shuffle while charging');
  shoot(emu, '', 0);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE);
  assert.equal(at(emu, 'pl_charge', YOU), 0);
  assert.ok(emu.v('ball_vx', 'i16') > 50, `struck hard: ${emu.v('ball_vx', 'i16')}`);
  assert.equal(emu.v('ball_power'), 0);
});

test('a full meter is a power shot, on fire; UP aims it high', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 88, 68);
  placeBall(emu, 100, 68);
  shoot(emu, 'A', CHARGE_MAX, 'UP');
  assert.equal(emu.v('ball_power'), 1);
  assert.ok(emu.v('ball_vx', 'i16') > 80);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'toward the top corner');
});

test('a charge held too long at full is given up', () => {
  const emu = quietMatch();
  placeBall(emu, 140, 120);
  emu.hold('A', 1).idle(CHARGE_MAX + 100);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE);
});

test('B chips it up, and a chip over a keeper on its line goes in', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 88, 68);
  placeBall(emu, 100, 68);
  shoot(emu, 'B', CHARGE_MAX);
  assert.ok(emu.v('ball_vz', 'i16') > 30, 'up in the air');
  assert.equal(outcome(emu), 'goal');
  assert.equal(at(emu, 'score', YOU), 1);
});

test('the keeper holds a soft shot, then throws it out', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 108, 68);
  placeBall(emu, 120, 68);
  shoot(emu, 'A', 0);
  assert.equal(outcome(emu), 'caught');
  assert.equal(emu.v('ball_owner'), CPU);
  emu.until((e) => e.v('ball_owner') === NOBODY, 60, 'the throw');
  assert.ok(emu.v('ball_vx', 'i16') < 0, 'thrown back up the pitch');
});

test('a power shot straight at the keeper is beaten away, not held', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 88, 68);
  placeBall(emu, 100, 68);
  shoot(emu, 'A', CHARGE_MAX);
  assert.equal(outcome(emu), 'parried');
  assert.equal(emu.v('ball_owner'), NOBODY);
  assert.equal(emu.v('ball_power'), 0);
});

test('a well-charged shot into the far corner from an angle beats the keeper; a weak one is saved', () => {
  const weak = quietMatch();
  placePlayer(weak, YOU, 108, 90);
  placeBall(weak, 120, 90);
  shoot(weak, 'A', 20, 'UP');
  assert.equal(outcome(weak), 'caught');
  const emu = quietMatch();
  placePlayer(emu, YOU, 108, 90);
  placeBall(emu, 120, 90);
  shoot(emu, 'A', 28, 'UP');
  assert.equal(outcome(emu), 'goal');
});

test('the ball comes off the boards', () => {
  const emu = quietMatch();
  placeBall(emu, 80, 30);
  set16(emu, 'ball_vy', -40);
  emu.idle(15);
  assert.ok(emu.v('ball_vy', 'i16') > 0, 'back down off the top board');
  assert.ok(emu.v('bounces') >= 1);
  // Off the end board beside the goal, not in.
  placeBall(emu, 140, 30);
  set16(emu, 'ball_vx', 40);
  emu.idle(15);
  assert.ok(emu.v('ball_vx', 'i16') < 0);
  assert.equal(at(emu, 'score', YOU), 0);
});

test('a goal: the score, a celebration, then the other side kicks off', () => {
  const emu = quietMatch();
  placeBall(emu, 150, 68);
  set16(emu, 'ball_vx', 40);
  emu.until((e) => e.v('goal_timer'), 30, 'the goal');
  assert.equal(at(emu, 'score', YOU), 1);
  assert.equal(emu.v('last_scorer'), YOU);
  emu.until((e) => e.v('kickoff_timer'), 200, 'the kickoff');
  assert.deepEqual([px(emu.v('ball_x', 'i16')), px(emu.v('ball_y', 'i16'))], [80, 68]);
  // The side that conceded starts closer to the ball.
  assert.ok(94 - px(at(emu, 'pl_x', CPU, 'i16')) <= 0);
});

test('into your own goal counts for the CPU', () => {
  const emu = quietMatch();
  placeBall(emu, 10, 68);
  set16(emu, 'ball_vx', -40);
  set16At(emu, 'kp_y', YOU, 56 * 16);  // the keeper's off to one side
  emu.until((e) => e.v('goal_timer'), 30, 'the goal');
  assert.equal(at(emu, 'score', CPU), 1);
});

test('first to five wins; START at full time goes back to the title', () => {
  const emu = quietMatch();
  emu.write8(emu.symbols.addr('score') + YOU, 4);
  placeBall(emu, 150, 68);
  set16(emu, 'ball_vx', 40);
  settle(emu, GAME_OVER, 'full time');
  assert.equal(emu.v('winner'), YOU);
  emu.idle(40).tap('START');
  settle(emu, TITLE, 'the title');
});

test('the CPU plays: it chases the ball down, shoots, and can score', () => {
  const emu = kickedOff(startGame());
  placePlayer(emu, YOU, 150, 120);  // you, out of the way
  emu.until((e) => at(e, 'pl_shots', CPU) > 0, 300, 'a CPU shot');
  emu.until((e) => at(e, 'score', CPU) > 0, 3000, 'a CPU goal');
});

test('START pauses and resumes', () => {
  const emu = quietMatch();
  set16(emu, 'ball_vx', 30);
  emu.tap('START');
  assert.equal(emu.v('paused'), 1);
  const x = emu.v('ball_x', 'i16');
  emu.idle(20);
  assert.equal(emu.v('ball_x', 'i16'), x, 'nothing moves');
  emu.tap('START');
  assert.equal(emu.v('paused'), 0);
});

test('a busy match stays inside the frame budget', () => {
  const emu = kickedOff(startGame());
  let worst = 0;
  const pads = ['RIGHT', 'RIGHT+A', 'UP', 'LEFT+B', 'DOWN', ''];
  for (let f = 0; f < 1500; f++) {
    emu.hold(pads[Math.floor(f / 25) % pads.length], 1);
    if (emu.v('game_state') !== PLAY || emu.v('in_transition')) continue;
    worst = Math.max(worst, emu.lastFrameCycles);
  }
  assert.ok(worst < FRAME_LIMIT / 2, `worst frame ${worst} cycles`);
});

test('screen: title', () => {
  matchGolden(bootToTitle().idle(10), 'title');
});

test('screen: kickoff', () => {
  matchGolden(startGame().idle(5), 'kickoff');
});

test('screen: a power shot, charged', () => {
  const emu = quietMatch();
  placePlayer(emu, YOU, 88, 60);
  placeBall(emu, 100, 68);
  emu.hold('A', 1).idle(CHARGE_MAX + 4);
  matchGolden(emu, 'charged');
});

test('screen: goal', () => {
  const emu = quietMatch();
  placeBall(emu, 150, 68);
  set16(emu, 'ball_vx', 40);
  emu.until((e) => e.v('goal_timer'), 30, 'the goal');
  matchGolden(emu.idle(20), 'goal');
});

test('screen: paused', () => {
  matchGolden(quietMatch().tap('START').idle(5), 'paused');
});

test('screen: full time', () => {
  const emu = startGame();
  emu.write8(emu.symbols.addr('score') + CPU, 5);
  emu.write8(emu.symbols.addr('score') + YOU, 3);
  emu.write8(emu.symbols.addr('winner'), CPU);
  emu.write8(emu.symbols.addr('next_state'), GAME_OVER);
  matchGolden(settle(emu, GAME_OVER, 'full time').idle(20), 'full-time');
});
