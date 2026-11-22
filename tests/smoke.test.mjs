// The game shell (title -> play -> game over -> title) and the match: futnet
// singles. Keep at least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import {
  assert, matchGolden, settle, bootToTitle, startGame, served, FRAME_LIMIT,
  TITLE, PLAY, GAME_OVER, YOU, CPU, SHOT_NONE, SHOT_POWER, SHOT_CURVE, SHOT_LOB, SHOT_DROP,
  CHARGE_MAX, TAP_CHARGE, TAP_MAX, MOVE_KICK, MOVE_VOLLEY, MOVE_HEADER, MOVE_BICYCLE, MOVE_SLIDE, MOVE_DOWN, MOVE_CONTROL,
  PHASE_SERVE, PHASE_TOSS, PHASE_RALLY, HALFWAY, POINT_NET, POINT_OUT, POINT_BOUNCE, POINT_TOUCHES,
  px, at, set16, placeBall, placePlayer,
} from './helpers.mjs';

/** Mid-rally, the ball in your half and the CPU (which struck last) standing still off to one side, you at (80, 190). */
function quietRally() {
  const emu = served(startGame());
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
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
  emu.write8(emu.symbols.addr('holder'), y < HALFWAY ? CPU : YOU);
  emu.write8(emu.symbols.addr('ball_landings'), 0);
}

/** The ball at (x, y), z px up, moving (vx, vy) and still in height. */
function ballAt(emu, x, y, z = 0, vx = 0, vy = 0) {
  placeBall(emu, x, y);
  set16(emu, 'ball_z', z * 16);
  set16(emu, 'ball_vx', vx);
  set16(emu, 'ball_vy', vy);
}

/** Your strike, already readied and charged: shot and charge set directly. */
function readied(emu, shot, charge) {
  emu.write8(emu.symbols.addr('pl_shot') + YOU, shot);
  emu.write8(emu.symbols.addr('pl_charge') + YOU, charge);
}

/** Run until a point (or `n` frames pass): who won it, or null. */
function pointWithin(emu, n = 200) {
  for (let i = 0; i < n; i++) {
    emu.idle(1);
    if (emu.v('goal_timer')) return emu.v('last_scorer');
  }
  return null;
}

/** Score a point for `side` straight away: the ball dropped dead in the other half. */
function pointTo(emu, side) {
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 20, 200);
  emu.write8(emu.symbols.addr('last_touch'), side);
  emu.write8(emu.symbols.addr('holder'), side ^ 1);
  emu.write8(emu.symbols.addr('ball_landings'), 1);
  emu.write8(emu.symbols.addr('ball_netted'), 0);
  emu.write8(emu.symbols.addr('ball_serve'), 0);
  ballAt(emu, 100, side === YOU ? 60 : 170, 2);
  assert.equal(pointWithin(emu, 30), side);
  emu.until((e) => !e.v('goal_timer'), 200, 'the celebration');
}

test('boots to the title screen and runs one loop per frame', () => {
  const emu = bootToTitle();
  const f0 = emu.v('frame_count', 'u16');
  emu.run(60);
  const fc = emu.v('frame_count', 'u16') - f0;
  // One game loop per frame: a slowdown or a hang shows up here.
  assert.ok(fc >= 59 && fc <= 61, `frame_count advanced ${fc} in 60 frames`);
});

test('the title waits for START; SELECT switches between one set and the best of three', () => {
  const emu = bootToTitle();
  assert.equal(emu.v('match_sets'), 1);
  emu.tap('SELECT');
  assert.equal(emu.v('match_sets'), 2);
  emu.tap('SELECT');
  assert.equal(emu.v('match_sets'), 1);
  emu.hold('UP+B', 60).idle(5);
  assert.equal(emu.v('game_state'), TITLE);
});

test('START begins a match seeded by press timing', () => {
  const play = (wait) => settle(bootToTitle().idle(wait).tap('START'), PLAY, 'play state');
  const a = play(10), b = play(37);
  assert.notEqual(a.v('game_seed', 'u16'), b.v('game_seed', 'u16'));
  // Same timing, same seed: replays reproduce matches.
  assert.equal(play(10).v('game_seed', 'u16'), a.v('game_seed', 'u16'));
});

test('the match opens 0-0 on your serve: the d-pad aims it, A tosses it up and strikes it on the way down', () => {
  const emu = startGame();
  assert.deepEqual([at(emu, 'score', YOU), at(emu, 'score', CPU)], [0, 0]);
  assert.equal(emu.v('phase'), PHASE_SERVE);
  assert.equal(emu.v('server'), YOU);
  const y = at(emu, 'pl_y', YOU, 'i16'), x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('A', 1).hold('UP+LEFT', 10);
  assert.equal(emu.v('phase'), PHASE_SERVE, 'not before READY goes');
  assert.deepEqual([at(emu, 'pl_x', YOU, 'i16'), at(emu, 'pl_y', YOU, 'i16')], [x, y], 'the server stays put');
  assert.ok(px(emu.v('ball_y', 'i16')) < px(y), 'the ball at your feet, in front');
  served(emu).hold('LEFT+A', 1);
  assert.equal(emu.v('phase'), PHASE_TOSS);
  emu.until((e) => e.v('phase') === PHASE_RALLY, 60, 'the strike');
  assert.ok(emu.v('ball_vx', 'i16') < 0, 'aimed left');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_KICK);
  assert.equal(emu.v('ball_serve'), 1);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'away up the court');
  assert.equal(emu.v('ball_power'), 0, 'never a power serve');
  emu.until((e) => e.v('holder') === CPU, 60, 'over the net');
  assert.equal(emu.v('ball_serve'), 0);
});

test('A queues a strike any time: it waits until the ball comes (the d-pad only aims), then charges, you are run to it, and it goes', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_POWER, 'queued, with it going away');
  const x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 6);
  assert.equal(at(emu, 'pl_charge', YOU), 0, 'no charge yet');
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x0, 'the d-pad aims, it doesn’t walk you');
  assert.equal(at(emu, 'pl_aim', YOU), 2, 'aimed right');
  placePlayer(emu, YOU, 80, 190);
  ballAt(emu, 80, 120, 0, 0, 40);
  emu.idle(4);
  assert.ok(at(emu, 'pl_charge', YOU) > 0, 'charging now it is coming');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('LEFT', 6);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'planted: LEFT does nothing (the ball is straight at you)');
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
  assert.equal(at(emu, 'pl_taps', YOU), TAP_MAX, 'taps stop counting at TAP_MAX');
  emu.until((e) => at(e, 'pl_charge', YOU) === CHARGE_MAX, 20, 'a full meter');
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
    readied(emu, SHOT_POWER, 12);
    emu.idle(1);
    assert.equal(at(emu, 'pl_last_move', YOU), move, `z ${z}, ${behind} px behind`);
    assert.ok(emu.v('ball_vy', 'i16') < 0, `move ${move} sends it up the court`);
  }
});

test('you reach a body width either side, and head a ball up to 35 px', () => {
  for (const [dx, z, move] of [[15, 0, MOVE_KICK], [-15, 10, MOVE_VOLLEY], [4, 34, MOVE_HEADER]]) {
    const emu = quietRally();
    ballAt(emu, 80 + dx, 184, z);
    readied(emu, SHOT_POWER, 12);
    emu.idle(1);
    assert.equal(at(emu, 'pl_last_move', YOU), move, `${dx} px across, ${z} up`);
  }
});

test('queued, you are run to where an angled ball crosses your line, and you strike it', () => {
  const emu = quietRally();
  placePlayer(emu, YOU, 40, 190);
  incoming(emu, 20, 120, 48, 40);
  set16(emu, 'ball_vx', 48);  // angled across: it crosses your line about 50 px to your right
  emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 120, 'the strike');
  assert.equal(at(emu, 'pl_shots', YOU), 1, 'struck, not missed');
  assert.notEqual(at(emu, 'pl_last_move', YOU), MOVE_SLIDE, 'run there, no slide needed');
});

test('every strike arcs over the net', () => {
  for (const charge of [6, 16, 31]) {
    const emu = quietRally();
    ballAt(emu, 80, 186);
    readied(emu, SHOT_POWER, charge);
    emu.idle(1);
    emu.until((e) => e.v('holder') === CPU || e.v('ball_netted') || e.v('goal_timer'), 60, 'the net');
    assert.equal(emu.v('holder'), CPU, `charge ${charge} goes over`);
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
  emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
  assert.ok(emu.v('ball_vx', 'i16') > vx0, 'more to the right as it goes');
  assert.ok(px(emu.v('ball_x', 'i16')) > 100, `lands right: x ${px(emu.v('ball_x', 'i16'))}`);
});

test('a struck ball leaves a trail: red off A, blue off B, longer charged; it fades after the bounce', () => {
  const oam = (e, n, b) => e.read8(e.symbols.addr('shadow_OAM') + 4 * n + b);
  for (const [button, kind, pal] of [['A', 1, 3], ['B', 2, 4]]) {
    const emu = quietRally();
    ballAt(emu, 80, 150, 0, 0, 30);
    emu.tap(button);
    for (let k = 0; k < 8; k++) emu.tap(button);
    emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
    assert.equal(emu.v('ball_trail'), kind, button);
    assert.ok(emu.v('trail_len') >= 3, `charged: ${emu.v('trail_len')} dots`);
    emu.idle(9);
    assert.ok(oam(emu, 11, 0) > 0, 'the first dot is drawn');
    assert.equal(oam(emu, 11, 3) & 7, pal, 'in its color');
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
    assert.equal(emu.v('ball_trail'), 0, 'over at the bounce');
    emu.idle(10);
    assert.equal(emu.v('trail_len'), 0, 'faded');
    assert.equal(oam(emu, 11, 0), 0, 'hidden');
  }
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

test('a lob sits up where it lands: slowed to a quarter, and a high bounce to turn and reach', () => {
  const emu = quietRally();
  incoming(emu, 80, 120, 40, 40);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  emu.until((e) => e.v('ball_landings') === 1, 60, 'it landing');
  assert.ok(emu.v('ball_vy', 'i16') <= 10, `slowed: vy ${emu.v('ball_vy', 'i16')}`);
  let top = 0;
  for (let k = 0; k < 14; k++) top = Math.max(top, px(emu.idle(1).v('ball_z', 'i16')));
  assert.ok(top >= 12, `up ${top} px`);
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
  assert.equal(pointWithin(emu, 30), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
});

test('planted, the game steps you toward the ball', () => {
  const emu = quietRally();
  ballAt(emu, 104, 120, 0, 0, 30);
  emu.tap('A');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.idle(16);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') > x + 4 * 16, 'over to the right, toward it');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.notEqual(at(emu, 'pl_last_move', YOU), MOVE_SLIDE, 'reached without a slide');
});

test('planted too late to get there, you slide for it: it pops up over the net, and you stay down a moment', () => {
  const emu = quietRally();
  ballAt(emu, 112, 150, 0, 0, 96);
  emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
  emu.until((e) => at(e, 'pl_move', YOU) === MOVE_SLIDE, 20, 'the slide');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 12, 'the touch');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_SLIDE);
  assert.equal(emu.v('ball_pop'), 1);
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'up into the other half');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('LEFT', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'on the floor');
});

test('unready, your first touch controls it, up in front of you; your second can slam it', () => {
  const emu = quietRally();
  ballAt(emu, 80, 170, 0, 0, 48);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 30, 'the control');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_CONTROL);
  assert.equal(emu.v('touches'), 1);
  assert.equal(emu.v('ball_pop'), 1);
  assert.ok(emu.v('ball_vz', 'i16') > 0, 'up');
  emu.until((e) => at(e, 'pl_move_timer', YOU) === 0, 30, 'the control pose');
  emu.tap('A');
  for (let k = 0; k < 4; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 1, 40, 'the second touch');
  assert.equal(emu.v('touches'), 2);
  assert.equal(emu.v('ball_power'), 1, 'a slam, on fire');
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('a third touch is a fault', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('touches'), 2);
  ballAt(emu, 82, 184);
  readied(emu, SHOT_POWER, 12);
  assert.equal(pointWithin(emu, 5), CPU);
  assert.equal(emu.v('point_reason'), POINT_TOUCHES);
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

test('a ball on fire knocks down a player who meets it unready, and goes on', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 30);
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  ballAt(emu, 80, 100, 8, 0, -96);
  set16(emu, 'ball_vz', 10);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.until((e) => at(e, 'pl_move', CPU) === MOVE_DOWN, 40, 'the knock-down');
  assert.equal(pointWithin(emu, 120), YOU);
});

test('LEFT or RIGHT while planted sets your aim, and it sticks', () => {
  const emu = quietRally();
  ballAt(emu, 80, 120, 0, 0, 40);
  emu.tap('A').hold('LEFT', 2).idle(1);
  assert.equal(at(emu, 'pl_aim', YOU), 0);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.ok(emu.v('ball_vx', 'i16') < 0, 'to the left, nothing held as it went');
});

test('UP plays it deep, DOWN short; LEFT and RIGHT near the sidelines', () => {
  const land = (keys) => {
    const emu = quietRally();
    ballAt(emu, 82, 184);
    readied(emu, SHOT_POWER, 16);
    emu.hold(keys, 1);
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
    return [px(emu.v('ball_x', 'i16')), px(emu.v('ball_y', 'i16'))];
  };
  const [, deep] = land(['UP']), [, mid] = land([]), [, short] = land(['DOWN']);
  assert.ok(deep < mid - 12 && mid < short, `deep ${deep}, as charged ${mid}, short ${short}`);
  assert.ok(land(['LEFT'])[0] < 40 && land(['RIGHT'])[0] > 120, 'near the sidelines');
});

test('aim is a direction from where you are: from the side, neutral goes straight and LEFT / RIGHT angle away', () => {
  const land = (x, keys) => {
    const emu = quietRally();
    placePlayer(emu, YOU, x, 190);
    ballAt(emu, x + 2, 184);
    readied(emu, SHOT_POWER, 16);
    emu.hold(keys, 1);
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
    return px(emu.v('ball_x', 'i16'));
  };
  const straight = land(40, []);
  assert.ok(Math.abs(straight - 42) <= 6, `straight on from x 40: lands at ${straight}`);
  assert.ok(land(40, ['RIGHT']) > 80, 'RIGHT from the left side: across the court');
  assert.ok(land(120, ['LEFT']) < 80, 'LEFT from the right side: across the court');
});

test('queued, a marker shows where your strike will land; it follows your aim', () => {
  const emu = quietRally();
  const marker = () => [emu.read8(emu.symbols.addr('shadow_OAM') + 4 * 10), emu.read8(emu.symbols.addr('shadow_OAM') + 4 * 10 + 1)];
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.idle(1);
  assert.equal(marker()[0], 0, 'hidden, nothing queued');
  emu.tap('A');
  const [y, x] = marker();
  assert.ok(y > 0, 'shown');
  emu.hold('LEFT', 1).idle(1);
  assert.ok(marker()[1] < x - 16, 'over to the left');
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

test('into the net: it drops back, dead, the other side\'s point', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  ballAt(emu, 80, 130, 4, 0, -48);
  emu.until((e) => e.v('ball_vy', 'i16') > 0, 20, 'the net');
  assert.ok(px(emu.v('ball_y', 'i16')) >= HALFWAY);
  assert.equal(emu.v('ball_netted'), 1);
  assert.equal(pointWithin(emu, 30), CPU);
  assert.equal(emu.v('point_reason'), POINT_NET);
});

test('barely charged, a strike is scuffed: it never gets over, and it\'s the CPU\'s point', () => {
  const emu = quietRally();
  ballAt(emu, 82, 184);
  readied(emu, SHOT_POWER, 0);
  assert.equal(pointWithin(emu, 80), CPU);
});

test('the first bounce in your half always gets up where you can play it, even off a low ball or a drop shot', () => {
  for (const drop of [0, 1]) {
    const emu = quietRally();
    ballAt(emu, 80, 150, 1, 0, 40);  // skimming in, barely off the ground
    set16(emu, 'ball_vz', -4);
    emu.write8(emu.symbols.addr('ball_drop'), drop);
    emu.until((e) => e.v('ball_landings') === 1, 20, 'the bounce');
    let top = 0;
    for (let k = 0; k < 12; k++) top = Math.max(top, px(emu.idle(1).v('ball_z', 'i16')));
    assert.equal(emu.v('goal_timer'), 0, 'not dead');
    assert.ok(top >= (drop ? 5 : 8), `${drop ? 'a drop shot' : 'a low ball'} gets ${top} px up`);
  }
});

test('a second bounce in a half is the other side\'s point', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  ballAt(emu, 100, 60, 12);
  emu.until((e) => e.v('ball_landings') === 1, 30, 'the first bounce');
  assert.equal(emu.v('goal_timer'), 0, 'one bounce is fine');
  assert.equal(pointWithin(emu, 60), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
});

test('out on the full is the striker\'s fault; in, then out, is the striker\'s point', () => {
  let emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  ballAt(emu, 80, 20, 12, 0, -64);  // over the far baseline before it lands
  assert.equal(pointWithin(emu, 40), CPU);
  assert.equal(emu.v('point_reason'), POINT_OUT);
  emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  emu.write8(emu.symbols.addr('ball_landings'), 1);
  ballAt(emu, 140, 60, 12, 40, 0);  // bounced in already, now off the side
  assert.equal(pointWithin(emu, 40), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
});

test('a point: the side that lost it serves next', () => {
  const emu = quietRally();
  pointTo(emu, YOU);
  assert.equal(at(emu, 'score', YOU), 1);
  assert.equal(emu.v('phase'), PHASE_SERVE);
  assert.equal(emu.v('server'), CPU);
  emu.write8(emu.symbols.addr('cpu_idle'), 0);
  emu.until((e) => e.v('phase') === PHASE_RALLY, 200, 'the CPU serving');
  assert.ok(emu.v('ball_vy', 'i16') > 0, 'down the court at you');
});

test('a set is first to 11 by two, 15 at most; one set is the match; START at full time goes back to the title', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('score') + YOU, 10);
  emu.write8(emu.symbols.addr('score') + CPU, 10);
  pointTo(emu, YOU);
  assert.equal(emu.v('game_state'), PLAY, '11-10 is not enough');
  emu.write8(emu.symbols.addr('score') + YOU, 14);
  emu.write8(emu.symbols.addr('score') + CPU, 14);
  pointTo(emu, CPU);
  settle(emu, GAME_OVER, 'full time: 15-14 is');
  assert.equal(emu.v('winner'), CPU);
  emu.idle(40).tap('START');
  settle(emu, TITLE, 'the title');
});

test('the best of three: a set won starts the next from 0-0', () => {
  const emu = settle(bootToTitle().tap('SELECT'), TITLE);
  startGame(emu);
  assert.equal(emu.v('match_sets'), 2);
  emu.write8(emu.symbols.addr('score') + YOU, 10);
  pointTo(emu, YOU);
  assert.equal(emu.v('game_state'), PLAY);
  assert.deepEqual([at(emu, 'sets', YOU), at(emu, 'sets', CPU)], [1, 0]);
  assert.deepEqual([at(emu, 'score', YOU), at(emu, 'score', CPU)], [0, 0]);
  emu.write8(emu.symbols.addr('score') + YOU, 10);
  pointTo(emu, YOU);
  settle(emu, GAME_OVER, 'full time');
  assert.equal(emu.v('winner'), YOU);
});

test('the CPU plays: it returns your serve and can win a point', () => {
  const emu = served(startGame()).tap('A');
  emu.until((e) => at(e, 'pl_shots', CPU) > 0, 300, 'a CPU return');
  emu.until((e) => at(e, 'score', CPU) > 0, 3000, 'a CPU point');
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
  incoming(emu, 80, 40, 30, 60);
  emu.tap('A');
  for (let k = 0; k < 8; k++) emu.tap('A');
  matchGolden(emu.idle(4), 'charged');
});

test('screen: a header', () => {
  const emu = quietRally();
  ballAt(emu, 82, 184, 20);
  readied(emu, SHOT_POWER, 12);
  matchGolden(emu.idle(3), 'header');
});

test('screen: a power shot trail', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('A');
  for (let k = 0; k < 8; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  matchGolden(emu.idle(10), 'trail-power');
});

test('screen: a curve trail', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('B');
  for (let k = 0; k < 8; k++) emu.tap('B');
  emu.until((e) => { e.hold('RIGHT', 1); return at(e, 'pl_shots', YOU) > 0; }, 80, 'the strike');
  matchGolden(emu.idle(14), 'trail-curve');
});

test('screen: the far end, the CPU sliding', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 24);
  ballAt(emu, 80, 60, 0, 0, -30);
  emu.write8(emu.symbols.addr('pl_move') + CPU, MOVE_SLIDE);
  emu.write8(emu.symbols.addr('pl_move_timer') + CPU, 30);
  matchGolden(emu.idle(20), 'slide');
});

test('screen: a point', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  ballAt(emu, 100, 60, 12);
  pointWithin(emu, 60);
  matchGolden(emu.idle(20), 'point');
});

test('screen: paused', () => {
  matchGolden(quietRally().tap('START').idle(5), 'paused');
});

test('screen: full time', () => {
  const emu = startGame();
  emu.write8(emu.symbols.addr('score') + CPU, 11);
  emu.write8(emu.symbols.addr('score') + YOU, 7);
  emu.write8(emu.symbols.addr('winner'), CPU);
  emu.write8(emu.symbols.addr('next_state'), GAME_OVER);
  matchGolden(settle(emu, GAME_OVER, 'full time').idle(20), 'full-time');
});
