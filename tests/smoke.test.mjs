// The game shell (title -> play -> game over -> title) and the match. Keep at
// least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import {
  assert, matchGolden, settle, bootToTitle, startGame, served, FRAME_LIMIT,
  TITLE, PLAY, GAME_OVER, YOU, CPU, SHOT_NONE, SHOT_POWER, SHOT_CURVE, SHOT_LOB, SHOT_DROP,
  CHARGE_MAX, TAP_CHARGE, MOVE_KICK, MOVE_VOLLEY, MOVE_HEADER, MOVE_BICYCLE, MOVE_SLIDE, MOVE_DOWN, MOVE_BLOCK,
  PHASE_SERVE, PHASE_TOSS, PHASE_RALLY, HALFWAY, POINT_GOAL, POINT_NET, POINT_BOUNCE,
  px, at, set16, placeBall, placePlayer,
} from './helpers.mjs';

/** Mid-rally, the CPU (which struck last) standing still off to one side, you at (80, 190). */
function quietRally() {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 40, 140);
  return emu;
}

/** A ball struck by the CPU, in the air: from (x, y) at vy, lifted by vz (SUB a frame). */
function incoming(emu, x, y, vy, vz) {
  ballAt(emu, x, y, 0, 0, vy);
  set16(emu, 'ball_vz', vz);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('ball_landings'), 0);
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

test('A plants you to ready a strike, only with the ball coming your way, and it goes as the ball arrives', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE, 'not with it going away');
  ballAt(emu, 80, 120, 0, 0, 40);
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_POWER);
  const x = at(emu, 'pl_x', YOU, 'i16'), y = at(emu, 'pl_y', YOU, 'i16');
  emu.hold('UP+LEFT', 6);
  assert.deepEqual([at(emu, 'pl_x', YOU, 'i16'), at(emu, 'pl_y', YOU, 'i16')], [x, y], 'planted');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE);
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_KICK);
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('the meter fills as you wait, faster as you tap; full, the strike is on fire, aimed with RIGHT', () => {
  const emu = quietRally();
  incoming(emu, 80, 40, 40, 80);
  emu.tap('A');
  const c0 = at(emu, 'pl_charge', YOU);
  emu.idle(6);
  const waited = at(emu, 'pl_charge', YOU) - c0;
  assert.ok(waited >= 2 && waited <= 4, `waiting: +${waited} in 6 frames`);
  emu.tap('A').tap('A');
  assert.ok(at(emu, 'pl_charge', YOU) >= c0 + waited + 2 * TAP_CHARGE, 'taps add to it');
  for (let k = 0; k < 8; k++) emu.tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), CHARGE_MAX);
  emu.until((e) => { e.hold('RIGHT', 1); return at(e, 'pl_shots', YOU) > 0; }, 80, 'the strike');
  assert.equal(emu.v('ball_power'), 1);
  assert.ok(emu.v('ball_vx', 'i16') > 0, 'to the right');
});

test('which strike it is depends on the ball\'s height: kick, volley, header, and a bicycle kick behind you', () => {
  const cases = [
    [0, -6, MOVE_KICK], [10, -6, MOVE_VOLLEY], [20, -6, MOVE_HEADER], [20, 8, MOVE_BICYCLE],
  ];
  for (const [z, behind, move] of cases) {
    const emu = quietRally();
    ballAt(emu, 82, 190 + behind, z);
    emu.hold('A', 1).idle(1);
    assert.equal(at(emu, 'pl_last_move', YOU), move, `z ${z}, ${behind} px behind`);
    assert.ok(emu.v('ball_vy', 'i16') < 0, `move ${move} sends it up the court`);
  }
});

test('B is a curve: set off wide, it bends in to the side you hold', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('B');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_CURVE);
  emu.until((e) => { e.hold('RIGHT', 1); return at(e, 'pl_shots', YOU) > 0; }, 80, 'the strike');
  assert.ok(emu.v('ball_curve', 'i8') > 0, 'bending right');
  const vx0 = emu.v('ball_vx', 'i16');
  emu.until((e) => px(e.v('ball_y', 'i16')) < 20 || e.v('goal_timer'), 120, 'the far end');
  assert.ok(emu.v('ball_vx', 'i16') > vx0, 'more to the right as it goes');
  assert.ok(px(emu.v('ball_x', 'i16')) > 95, `ends up right: x ${px(emu.v('ball_x', 'i16'))}`);
});

test('B then A is a lob, dropping just behind the CPU', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 24, 40);
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('B').tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_LOB);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the lob');
  let top = 0;
  emu.until((e) => { top = Math.max(top, px(e.v('ball_z', 'i16'))); return top > 4 && e.v('ball_z', 'i16') === 0; }, 80, 'it landing');
  assert.ok(top >= 24, `up ${top} px`);
  const y = px(emu.v('ball_y', 'i16'));
  assert.ok(y <= 40 && y >= 20, `lands at ${y}, just behind the CPU at 40`);
});

test('A then B is a drop shot, just over the net and barely bouncing: unreached, your point', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('A').tap('B');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DROP);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the drop shot');
  emu.until((e) => e.v('ball_landings') > 0, 60, 'it landing');
  const y = px(emu.v('ball_y', 'i16'));
  assert.ok(y >= HALFWAY - 30 && y < HALFWAY, `lands at ${y}, just over`);
  assert.equal(goalWithin(emu, 30), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
});

test('planted out of reach, you slide for it: it pops up, and you stay down a moment', () => {
  const emu = quietRally();
  ballAt(emu, 100, 120, 0, 0, 40);
  emu.tap('A');
  emu.until((e) => at(e, 'pl_move', YOU) === MOVE_SLIDE, 60, 'the slide');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 12, 'the touch');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_SLIDE);
  assert.equal(emu.v('ball_pop'), 1);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'up into the other half');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('LEFT', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'on the floor');
});

test('a pop-up met in the air is a slam, on fire', () => {
  const emu = quietRally();
  ballAt(emu, 82, 184, 12);
  emu.write8(emu.symbols.addr('ball_pop'), 1);
  emu.hold('A', 1).idle(1);
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_VOLLEY);
  assert.equal(emu.v('ball_power'), 1);
});

test('a ball on fire: met with under three quarters of a meter it pops up; with more, a clean strike', () => {
  let emu = quietRally();
  ballAt(emu, 80, 140, 0, 0, 96);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 30, 'the touch');
  assert.equal(emu.v('ball_pop'), 1, 'popped up');
  emu = quietRally();
  incoming(emu, 80, 20, 96, 40);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.tap('A');
  for (let k = 0; k < 8; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 40, 'the strike');
  assert.equal(emu.v('ball_pop'), 0, 'struck clean');
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('a ball on fire knocks down a player who meets it unready, and goes on in', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 30);
  ballAt(emu, 80, 100, 0, 0, -96);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.until((e) => at(e, 'pl_move', CPU) === MOVE_DOWN, 40, 'the knock-down');
  assert.equal(goalWithin(emu), YOU);
});

test('unready, the ball pops up off you', () => {
  const emu = quietRally();
  ballAt(emu, 80, 170, 0, 0, 48);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 30, 'the block');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_BLOCK);
  assert.equal(emu.v('ball_pop'), 1);
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});


test('LEFT or RIGHT while planted sets your aim, and it sticks', () => {
  const emu = quietRally();
  ballAt(emu, 80, 120, 0, 0, 40);
  emu.tap('A').hold('LEFT', 2).idle(1);
  assert.equal(at(emu, 'pl_aim', YOU), 0);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.ok(emu.v('ball_vx', 'i16') < 0, 'to the left, nothing held as it went');
});

test('charging, you flash: your palette, white, and fire from half a meter', () => {
  const emu = quietRally();
  incoming(emu, 80, 40, 40, 80);
  const pals = new Set();
  emu.tap('A');
  for (let k = 0; k < 4; k++) emu.tap('A');
  for (let k = 0; k < 16; k++) pals.add(emu.idle(1).read8(emu.symbols.addr('shadow_OAM') + 4 * 1 + 3) & 7);
  assert.deepEqual([...pals].sort(), [0, 1, 6], 'you, flash, fire');
});

test('the net: low across halfway, the ball is stopped and drops back', () => {
  const emu = quietRally();
  ballAt(emu, 80, 130, 0, 0, -48);
  emu.until((e) => e.v('ball_vy', 'i16') > 0, 20, 'the net');
  assert.ok(px(emu.v('ball_y', 'i16')) >= HALFWAY);
});

test('barely charged, a strike is scuffed into the net: the CPU\'s point', () => {
  const emu = quietRally();
  ballAt(emu, 82, 184);
  emu.hold('A', 1);
  assert.equal(goalWithin(emu, 60), CPU);
  assert.equal(emu.v('point_reason'), POINT_NET);
});

test('a second bounce in their half is the striker\'s point', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  ballAt(emu, 100, 60, 12, 0, -10);
  assert.equal(goalWithin(emu, 60), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
  assert.ok(emu.v('ball_landings') >= 1);
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

test('screen: planted, charged full', () => {
  const emu = quietRally();
  ballAt(emu, 80, 40, 0, 0, 30);
  emu.tap('A');
  for (let k = 0; k < 8; k++) emu.tap('A');
  matchGolden(emu.idle(4), 'charged');
});

test('screen: a header', () => {
  const emu = quietRally();
  ballAt(emu, 82, 184, 20);
  emu.hold('A', 1);
  matchGolden(emu.idle(3), 'header');
});

test('screen: the far end, the CPU sliding', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 24);
  ballAt(emu, 80, 60, 0, 0, -30);
  emu.write8(emu.symbols.addr('pl_move') + CPU, MOVE_SLIDE);
  emu.write8(emu.symbols.addr('pl_move_timer') + CPU, 30);
  matchGolden(emu.idle(20), 'slide');
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
