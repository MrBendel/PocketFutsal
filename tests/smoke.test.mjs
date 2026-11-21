// The game shell (title -> play -> game over -> title) and the match. Keep at
// least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import {
  assert, matchGolden, settle, bootToTitle, startGame, served, FRAME_LIMIT,
  TITLE, PLAY, GAME_OVER, YOU, CPU, SHOT_NONE, SHOT_DRIVE, SHOT_CHIP, CHARGE_MAX,
  MOVE_KICK, MOVE_VOLLEY, MOVE_HEADER, MOVE_BICYCLE, MOVE_DIVE, MOVE_DOWN,
  PHASE_SERVE, PHASE_TOSS, PHASE_RALLY,
  px, at, set16, placeBall, placePlayer,
} from './helpers.mjs';

/** Mid-rally, the CPU standing still off to one side, you at (80, 190). */
function quietRally() {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 40, 140);
  return emu;
}

/** The ball at (x, y), z px up, moving (vx, vy) and still in height. */
function ballAt(emu, x, y, z = 0, vx = 0, vy = 0) {
  placeBall(emu, x, y);
  set16(emu, 'ball_z', z * 16);
  set16(emu, 'ball_vx', vx);
  set16(emu, 'ball_vy', vy);
}

/** Run until a goal (or `n` frames pass): the scorer, or null. */
function goalWithin(emu, n = 200) {
  for (let i = 0; i < n; i++) {
    emu.idle(1);
    if (emu.v('goal_timer')) return emu.v('last_scorer');
  }
  return null;
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

test('the match opens 0-0 on your serve: along your line, then A tosses it up and strikes it on the way down', () => {
  const emu = startGame();
  assert.deepEqual([at(emu, 'score', YOU), at(emu, 'score', CPU)], [0, 0]);
  assert.equal(emu.v('phase'), PHASE_SERVE);
  assert.equal(emu.v('server'), YOU);
  const y = at(emu, 'pl_y', YOU, 'i16'), x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('A', 1).hold('UP+LEFT', 10);
  assert.equal(emu.v('phase'), PHASE_SERVE, 'not before READY goes');
  assert.equal(at(emu, 'pl_y', YOU, 'i16'), y, 'the server keeps to the line');
  assert.ok(at(emu, 'pl_x', YOU, 'i16') < x, 'but moves along it');
  assert.ok(px(emu.v('ball_y', 'i16')) < px(y), 'the ball at your feet, in front');
  served(emu).tap('A');
  assert.equal(emu.v('phase'), PHASE_TOSS);
  emu.until((e) => e.v('phase') === PHASE_RALLY, 60, 'the strike');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_KICK);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'away up the court');
  assert.equal(emu.v('ball_power'), 0, 'never a power serve');
});

test('A readies a strike that charges while you wait, slows you, and fires in reach', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150);
  emu.hold('A', 1).idle(10);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DRIVE);
  assert.ok(at(emu, 'pl_charge', YOU) >= 10);
  const y = at(emu, 'pl_y', YOU, 'i16');
  emu.hold('UP', 8);
  assert.ok(y - at(emu, 'pl_y', YOU, 'i16') <= 8 * 8, 'a shuffle while charging');
  emu.until((e) => { e.hold('UP', 1); return at(e, 'pl_shots', YOU) > 0; }, 200, 'the strike');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE);
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('which strike it is depends on the ball\'s height: kick, volley, header, and a bicycle kick behind you', () => {
  const cases = [
    [0, -6, MOVE_KICK], [10, -6, MOVE_VOLLEY], [20, -6, MOVE_HEADER], [20, 8, MOVE_BICYCLE],
  ];
  for (const [z, behind, move] of cases) {
    const emu = quietRally();
    emu.hold('A', 1).idle(4);
    ballAt(emu, 82, 190 + behind, z);
    emu.idle(2);
    assert.equal(at(emu, 'pl_last_move', YOU), move, `z ${z}, ${behind} px behind`);
    assert.ok(emu.v('ball_vy', 'i16') < 0, `move ${move} sends it up the court`);
  }
});

test('a full meter on a drive is a power shot, on fire; RIGHT aims it right', () => {
  const emu = quietRally();
  ballAt(emu, 80, 170);
  emu.hold('A', 1).idle(CHARGE_MAX + 4);
  assert.equal(at(emu, 'pl_charge', YOU), CHARGE_MAX);
  emu.until((e) => { e.hold('UP+RIGHT', 1); return at(e, 'pl_shots', YOU) > 0; }, 200, 'the strike');
  assert.equal(emu.v('ball_power'), 1);
  assert.ok(emu.v('ball_vx', 'i16') > 0, 'to the right');
});

test('a power shot knocks down a player who meets it unready, and goes on in', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 30);
  ballAt(emu, 80, 100, 0, 0, -96);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.until((e) => at(e, 'pl_move', CPU) === MOVE_DOWN, 40, 'the knock-down');
  assert.equal(goalWithin(emu), YOU);
});

test('B chips it up and over', () => {
  const emu = quietRally();
  ballAt(emu, 80, 170);
  emu.hold('B', 1).idle(20);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_CHIP);
  emu.until((e) => { e.hold('UP', 1); return at(e, 'pl_shots', YOU) > 0; }, 200, 'the chip');
  let top = 0;
  for (let i = 0; i < 30; i++) top = Math.max(top, px(emu.idle(1).v('ball_z', 'i16')));
  assert.ok(top >= 24, `up ${top} px`);
});

test('A with the ball just out of reach, coming past, is a dive for it', () => {
  const emu = quietRally();
  ballAt(emu, 108, 176, 0, 0, 32);
  emu.until((e) => px(e.v('ball_y', 'i16')) >= 182, 30, 'the ball arriving');
  emu.tap('A');
  assert.equal(at(emu, 'pl_move', YOU), MOVE_DIVE);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 12, 'the touch');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_DIVE);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'back up the court');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('LEFT', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'then on the floor a moment');
});

test('without a strike ready, the ball bounces off you', () => {
  const emu = quietRally();
  ballAt(emu, 80, 170, 0, 0, 48);
  emu.until((e) => e.v('ball_vy', 'i16') < 0, 30, 'the bounce');
  assert.equal(emu.v('last_touch'), YOU);
  assert.equal(at(emu, 'pl_shots', YOU), 0);
});

test('the ball comes off the boards: the sides, the goal line beside the goal, and above the crossbar', () => {
  let emu = quietRally();
  ballAt(emu, 30, 150, 0, -48, 0);
  emu.until((e) => e.v('ball_vx', 'i16') > 0, 30, 'off the side');
  emu = quietRally();
  ballAt(emu, 16, 20, 0, 0, -48);
  emu.until((e) => e.v('ball_vy', 'i16') > 0, 30, 'off the end, beside the goal');
  emu = quietRally();
  ballAt(emu, 80, 20, 24, 0, -48);
  set16(emu, 'ball_vz', 30);
  emu.until((e) => e.v('ball_vy', 'i16') > 0, 30, 'off the boards above the goal');
  assert.equal(emu.v('goal_timer'), 0);
  assert.ok(emu.v('bounces') >= 1);
});

test('a goal is a point; the side that conceded serves next', () => {
  const emu = quietRally();
  ballAt(emu, 80, 30, 0, 0, -48);
  assert.equal(goalWithin(emu), YOU);
  assert.equal(at(emu, 'score', YOU), 1);
  emu.until((e) => !e.v('goal_timer'), 200, 'the celebration');
  assert.equal(emu.v('phase'), PHASE_SERVE);
  assert.equal(emu.v('server'), CPU);
  emu.write8(emu.symbols.addr('cpu_idle'), 0);
  emu.until((e) => e.v('phase') === PHASE_RALLY, 200, 'the CPU serving');
  assert.ok(emu.v('ball_vy', 'i16') > 0, 'down the court at you');
});

test('into your own goal counts for the CPU', () => {
  const emu = quietRally();
  ballAt(emu, 50, 200, 0, 0, 60);
  assert.equal(goalWithin(emu), CPU);
  assert.equal(at(emu, 'score', CPU), 1);
});

test('first to seven, by two; START at full time goes back to the title', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('score') + YOU, 6);
  emu.write8(emu.symbols.addr('score') + CPU, 6);
  ballAt(emu, 80, 30, 0, 0, -48);
  assert.equal(goalWithin(emu), YOU);
  emu.until((e) => !e.v('goal_timer'), 200, 'the celebration');
  assert.equal(emu.v('game_state'), PLAY, '7-6 is not enough');
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  placePlayer(emu, CPU, 20, 20);  // out of its serving spot
  ballAt(emu, 80, 30, 0, 0, -48);
  assert.equal(goalWithin(emu), YOU);
  settle(emu, GAME_OVER, 'full time');
  assert.equal(emu.v('winner'), YOU);
  emu.idle(40).tap('START');
  settle(emu, TITLE, 'the title');
});

test('the CPU plays: it returns your serve and can score', () => {
  const emu = served(startGame()).tap('A');
  emu.until((e) => at(e, 'pl_shots', CPU) > 0, 300, 'a CPU return');
  emu.until((e) => at(e, 'score', CPU) > 0, 3000, 'a CPU goal');
});

test('START pauses and resumes', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -30);
  emu.tap('START');
  assert.equal(emu.v('paused'), 1);
  const y = emu.v('ball_y', 'i16');
  emu.idle(20);
  assert.equal(emu.v('ball_y', 'i16'), y, 'nothing moves');
  emu.tap('START');
  assert.equal(emu.v('paused'), 0);
});

test('a busy match stays inside the frame budget', () => {
  const emu = served(startGame());
  let worst = 0;
  const pads = ['A', 'RIGHT', 'RIGHT+A', 'UP', 'LEFT+B', 'DOWN', ''];
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

test('screen: your serve', () => {
  matchGolden(startGame().idle(5), 'serve');
});

test('screen: a power shot, charged', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150);
  emu.hold('A', 1).idle(CHARGE_MAX + 4);
  matchGolden(emu, 'charged');
});

test('screen: a header', () => {
  const emu = quietRally();
  emu.hold('A', 1).idle(4);
  ballAt(emu, 82, 184, 20);
  matchGolden(emu.idle(3), 'header');
});

test('screen: the far end, the CPU diving', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 24);
  ballAt(emu, 80, 60, 0, 0, -30);
  emu.write8(emu.symbols.addr('pl_move') + CPU, MOVE_DIVE);
  emu.write8(emu.symbols.addr('pl_move_timer') + CPU, 30);
  matchGolden(emu.idle(20), 'dive');
});

test('screen: goal', () => {
  const emu = quietRally();
  ballAt(emu, 80, 30, 0, 0, -48);
  goalWithin(emu);
  matchGolden(emu.idle(20), 'goal');
});

test('screen: paused', () => {
  matchGolden(quietRally().tap('START').idle(5), 'paused');
});

test('screen: full time', () => {
  const emu = startGame();
  emu.write8(emu.symbols.addr('score') + CPU, 7);
  emu.write8(emu.symbols.addr('score') + YOU, 5);
  emu.write8(emu.symbols.addr('winner'), CPU);
  emu.write8(emu.symbols.addr('next_state'), GAME_OVER);
  matchGolden(settle(emu, GAME_OVER, 'full time').idle(20), 'full-time');
});
