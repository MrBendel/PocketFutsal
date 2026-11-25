// The game shell (title -> play -> game over -> title) and the match: futnet
// singles. Keep at least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import {
  assert, matchGolden, settle, bootToTitle, startGame, served, FRAME_LIMIT,
  TITLE, PLAY, GAME_OVER, YOU, CPU, SHOT_NONE, SHOT_POWER, SHOT_CURVE, SHOT_LOB, SHOT_DROP, SHOT_GOLD,
  CHARGE_MAX, TAP_CHARGE, TAP_MAX, YOU_TAP, MOVE_KICK, MOVE_VOLLEY, MOVE_HEADER, MOVE_BICYCLE, MOVE_SLIDE, MOVE_DOWN, MOVE_CONTROL,
  PHASE_SERVE, PHASE_TOSS, PHASE_RALLY, HALFWAY, POINT_NET, POINT_OUT, POINT_BOUNCE, POINT_TOUCHES,
  px, at, set16, placeBall, placePlayer,
} from './helpers.mjs';

/** Mid-rally, the ball in your half and the CPU (which struck last) standing still off to one side, you at (80, 190). */
const POWER_MAX = 64;

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

/** You walk to where the ball's coming before you press (you're locked once queued). */
function standAtMeet(emu) {
  emu.idle(4);
  placePlayer(emu, YOU, at(emu, 'meet_x', YOU, 'i16'), at(emu, 'meet_y', YOU, 'i16'));
  emu.idle(2);
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

test('the title waits for START; LEFT/RIGHT switch one set and the best of three, SELECT you or the AI', () => {
  const emu = bootToTitle();
  assert.equal(emu.v('match_sets'), 1);
  emu.tap('RIGHT');
  assert.equal(emu.v('match_sets'), 2);
  emu.tap('LEFT');
  assert.equal(emu.v('match_sets'), 1);
  assert.equal(emu.v('you_ai'), 0);
  emu.tap('SELECT');
  assert.equal(emu.v('you_ai'), 1);
  assert.equal(emu.v('match_sets'), 1, 'SELECT leaves the sets alone');
  emu.tap('SELECT');
  assert.equal(emu.v('you_ai'), 0);
  emu.hold('UP+B', 60).idle(5);
  assert.equal(emu.v('game_state'), TITLE);
});

test('PLAYER: AI: the AI plays your side by your buttons, serving and returning', () => {
  const emu = startGame();
  emu.write8(emu.symbols.addr('you_ai'), 1);
  let points = 0, yours = 0;
  for (let f = 0; f < 6000; f++) {
    const g = emu.v('goal_timer');
    emu.idle(1);
    if (!g && emu.v('goal_timer')) points++;
  }
  yours = at(emu, 'pl_shots', YOU);
  assert.ok(points >= 1, `points played: ${points}`);
  assert.ok(yours >= 10, `your side struck it ${yours} times`);
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
  emu.until((e) => e.v('holder') === CPU, 120, 'over the net');
  assert.equal(emu.v('ball_serve'), 0);
});

test('A queues a strike any time: it waits until the ball comes (the d-pad only aims), you are run to it, and it goes', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_POWER, 'queued, with it going away');
  const x0 = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('RIGHT', 6);
  assert.equal(at(emu, 'pl_charge', YOU), YOU_TAP, 'the press’s charge, no more');
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x0, 'the d-pad aims, it doesn’t walk you');
  assert.equal(at(emu, 'pl_aim', YOU), 2, 'aimed right');
  placePlayer(emu, YOU, 80, 190);
  ballAt(emu, 80, 120, 0, 0, 40);
  emu.idle(4);
  assert.equal(at(emu, 'pl_charge', YOU), YOU_TAP, 'it doesn’t fill by itself, coming either');
  const x = at(emu, 'pl_x', YOU, 'i16');
  emu.hold('LEFT', 6);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'planted: LEFT does nothing (the ball is straight at you)');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE);
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_KICK);
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('your meter fills only as you press (not waiting, not holding); full, the strike is on fire, aimed with RIGHT', () => {
  const emu = quietRally();
  incoming(emu, 80, 40, 20, 40);
  emu.tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), YOU_TAP, 'the first press');
  emu.idle(12);
  assert.equal(at(emu, 'pl_charge', YOU), YOU_TAP, 'waiting adds nothing');
  emu.hold('A', 12);
  assert.equal(at(emu, 'pl_charge', YOU), 2 * YOU_TAP, 'holding adds nothing after its press');
  emu.idle(1).tap('A').tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), 4 * YOU_TAP, 'each press adds');
  for (let k = 0; k < 8; k++) emu.tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), CHARGE_MAX, 'pressed full: no cap on presses');
  // RIGHT aims once the ball's in your half (before, it would run you).
  emu.until((e) => { e.hold(px(e.v('ball_y', 'i16')) >= 112 ? 'RIGHT' : [], 1); return at(e, 'pl_shots', YOU) > 0; }, 160, 'the strike');
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

test('an angled ball: stand where it crosses your line, queue, and you strike it', () => {
  const emu = quietRally();
  placePlayer(emu, YOU, 40, 190);
  incoming(emu, 20, 120, 24, 20);
  set16(emu, 'ball_vx', 24);  // angled across: it crosses your line about 50 px to your right
  standAtMeet(emu);
  emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 240, 'the strike');
  assert.equal(at(emu, 'pl_shots', YOU), 1, 'struck, not missed');
  assert.notEqual(at(emu, 'pl_last_move', YOU), MOVE_SLIDE, 'there, no slide needed');
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
    emu.idle(18);
    assert.ok(oam(emu, 11, 0) > 0, 'the first dot is drawn');
    assert.equal(oam(emu, 11, 3) & 7, pal, 'in its color');
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
    assert.equal(emu.v('ball_trail'), 0, 'over at the bounce');
    emu.idle(20);
    assert.equal(emu.v('trail_len'), 0, 'faded');
    assert.equal(oam(emu, 11, 0), 0, 'hidden');
  }
});

test('no HUD in a rally: the score box (faces, points) before each serve, a banner for the call, the board in the court art', () => {
  const BOARD_ROW = 2, BOARD_PTS = 7;
  const gauge = (e) => e.read8(0x8000 + 141 * 16 + 2);  // your shadow's lower tile, the bar's outline row: its columns
  const win = (e) => (e.read8(0xFF40) & 0x20 ? e.read8(0xFF4A) : -1);  // the window's line, or -1: off
  const board = (e) => [0, 1, 3, 4].map((k) => e.read8(0x9800 + BOARD_ROW * 32 + BOARD_PTS + k) - 16);  // font: '0' is 16
  const emu = served(startGame());
  emu.idle(1);
  assert.equal(win(emu), 112, 'the box, four rows at the bottom, before the serve');
  assert.deepEqual(board(emu), [0, 0, 0, 0], 'the board: 00-00');
  emu.tap('A');
  emu.idle(2);
  assert.equal(win(emu), -1, 'tossed: gone, the court has the whole screen');
  const rally = quietRally();
  rally.idle(2);
  assert.equal(win(rally), -1, 'none in a rally');
  assert.equal(gauge(rally), 0, 'an empty power meter: no gauge');
  rally.write8(rally.symbols.addr('pl_power') + YOU, 40);
  rally.idle(2);
  assert.equal(gauge(rally), 0xEE, 'a strike added to it: four pips show a moment');
  assert.equal(rally.read8(0x8000 + 141 * 16 + 3), 0, 'the first two filled (40 of 64: gold to 7 of 12 px)');
  rally.idle(70);
  assert.equal(gauge(rally), 0, 'then gone till the next');
  const hint = () => rally.read8(0xFE00 + 35 * 4);
  assert.equal(hint(), 0, 'no hint yet');
  rally.write8(rally.symbols.addr('pl_power') + YOU, POWER_MAX);
  rally.idle(2);
  assert.equal(gauge(rally), 0xEE, 'full: the slam ready, the pips in your shadow while it lasts');
  assert.equal(rally.read8(rally.symbols.addr('sfx_last')), 15, 'the ready sound (SFX_READY)');
  assert.ok(hint() > 0, 'the first time in a match: A+B over you');
  assert.ok(rally.read8(rally.symbols.addr('glow_level')) <= 3, 'your outline pulsing gold');
  rally.idle(130);
  assert.equal(hint(), 0, 'the hint gone after a moment');
  assert.equal(gauge(rally), 0xEE, 'the pips still there: ready');
  // A point: the call along the bottom, the board's digits swapped, then the box.
  for (const [n, v] of [['last_touch', YOU], ['holder', CPU], ['ball_landings', 1], ['ball_netted', 0], ['ball_serve', 0]]) rally.write8(rally.symbols.addr(n), v);
  ballAt(rally, 100, 60, 2);
  assert.equal(pointWithin(rally, 30), YOU);
  rally.idle(2);
  assert.equal(win(rally), 136, 'the call: one row');
  assert.deepEqual(board(rally), [0, 1, 0, 0], 'the board: 01-00');
  rally.until((e) => e.v('goal_timer') === 0, 400, 'the next serve').idle(3);
  assert.equal(win(rally), 112, 'the box again');
  rally.tap('START').idle(1);
  assert.equal(win(rally), 136, 'PAUSED along the bottom');
});

test('a quick roll (B, A or A, B) is a lob or a drop shot, and it stays one; the other button later calls the strike off', () => {
  let emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);  // going away: nothing to strike yet
  emu.tap('B').tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_LOB, 'B, A: a lob');
  emu.idle(40);
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_LOB, 'it stays a lob');
  const c = at(emu, 'pl_charge', YOU);
  emu.tap('B');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_LOB, 'B again fills the meter, still a lob');
  assert.equal(at(emu, 'pl_charge', YOU), c + YOU_TAP);
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE, 'A later: called off');
  emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.tap('A').tap('B');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DROP, 'A, B: a drop shot');
  emu.tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DROP, 'A again: still a drop shot');
  for (const [first, other] of [['A', 'B'], ['B', 'A']]) {
    emu = quietRally();
    ballAt(emu, 80, 150, 0, 0, -40);
    emu.tap(first).tap(first).idle(20).tap(other);
    assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE, `${first} readied, ${other} later: called off`);
    assert.equal(at(emu, 'pl_charge', YOU), 0, 'the meter emptied');
    emu.tap(other);
    assert.equal(at(emu, 'pl_shot', YOU), first === 'A' ? 2 : 1, 'then it readies afresh');
  }
  emu = quietRally();
  ballAt(emu, 80, 60, 0, 0, -10);  // far off in the CPU's half: nothing to strike yet
  emu.tap('B').tap('A').idle(40);
  ballAt(emu, 82, 184);  // in reach: struck now
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 10, 'the strike');
  assert.equal(emu.v('ball_lob'), 1, 'rolled long before it came: still a lob');
});

test('readied, you are locked from the press: the ball still in the CPU\'s half, the d-pad only aims', () => {
  const emu = quietRally();
  ballAt(emu, 80, 60, 0, 0, -10);
  emu.tap('A');
  const x = at(emu, 'pl_x', YOU, 'i16'), y = at(emu, 'pl_y', YOU, 'i16');
  emu.hold('LEFT+UP', 20);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'not run left');
  assert.equal(at(emu, 'pl_y', YOU, 'i16'), y, 'nor up');
  assert.equal(at(emu, 'pl_aim', YOU), 0, 'aimed left');
  assert.equal(at(emu, 'pl_depth', YOU), 2, 'and deep');
  emu.tap('B').hold('LEFT', 10);
  assert.ok(at(emu, 'pl_x', YOU, 'i16') < x, 'B called it off: you run');
});

test('B then A is a lob, dropping deep behind the CPU', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 24, 40);
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('B').tap('A');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_LOB);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the lob');
  let top = 0;
  emu.until((e) => { top = Math.max(top, px(e.v('ball_z', 'i16'))); return top > 4 && e.v('ball_z', 'i16') === 0; }, 120, 'it landing');
  assert.ok(top >= 24, `up ${top} px`);
  const y = px(emu.v('ball_y', 'i16'));
  assert.ok(y <= 24 && y >= 8, `lands at ${y}, well behind the CPU at 40`);
});

test('a lob coming down to you: a flashing marker where to meet it; there and queued, you slam it out of the air', () => {
  const emu = quietRally();
  // The marker: the slam zone's middle sprite (29-34) for a slam, the small ring (19) for a drop shot.
  const marker = (e) => e.read8(e.symbols.addr('shadow_OAM') + 4 * 31) || e.read8(e.symbols.addr('shadow_OAM') + 4 * 19);
  incoming(emu, 60, 50, 26, 28);
  set16(emu, 'ball_vx', 4);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  standAtMeet(emu);
  emu.tap('A');
  for (let k = 0; k < 3; k++) emu.tap('A');  // half a meter by then: a slam
  emu.idle(4);
  assert.equal(at(emu, 'meet_slam', YOU), 1, 'the slam spot');
  assert.ok(marker(emu) > 0, 'the marker shows');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 120, 'the slam');
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_BICYCLE, 'met in the air: a jumping bicycle kick');
  assert.equal(emu.v('ball_power'), 1, 'a slam, on fire');
  assert.equal(marker(emu), 0, 'the marker gone');
  const low = quietRally();
  incoming(low, 60, 50, 26, 28);
  set16(low, 'ball_vx', 4);
  low.write8(low.symbols.addr('ball_lob'), 1);
  standAtMeet(low);
  low.tap('A');
  low.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 120, 'the header');
  assert.equal(at(low, 'pl_last_move', YOU), MOVE_HEADER, 'less than half a meter: still out of the air');
  assert.equal(low.v('ball_power'), 0, 'a header, not a slam');
});

test('the slam zone: three rings, 48 px across, the lit one stepping in to the spot as the lob comes; its palette borrowed and given back', () => {
  const emu = quietRally();
  const oam = (n) => [emu.read8(emu.symbols.addr('shadow_OAM') + 4 * n + 1), emu.read8(emu.symbols.addr('shadow_OAM') + 4 * n)];
  const pal = () => [1, 2, 3].map((k) => at(emu, 'pal_base_obj', 4 * 4 + k, 'u16'));  // the curve trail's, palette 4
  const before = pal();
  incoming(emu, 60, 50, 26, 28);
  set16(emu, 'ball_vx', 4);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  emu.idle(6);
  assert.equal(at(emu, 'meet_slam', YOU), 1, 'the slam spot');
  assert.equal(emu.v('zone_on'), 1, 'the zone shows');
  assert.equal(oam(34)[0] - oam(29)[0], 40, 'six sprites in a row: 48 px across');
  assert.equal(oam(29)[1], oam(34)[1], 'one row: six on its lines');
  assert.equal(emu.v('zone_lit'), 0, 'far: the outer ring lit');
  assert.notDeepEqual(pal(), before, 'the palette borrowed');
  matchGolden(emu, 'slam-zone');
  emu.until((e) => e.v('zone_lit') === 1, 200, 'the middle ring');
  emu.until((e) => Math.abs(px(e.v('ball_y', 'i16')) - at(e, 'meet_y', YOU, 'i16')) <= 4, 200, 'it reaching the spot');
  assert.equal(emu.v('zone_lit'), 2, 'there: the spot lit');
  assert.equal(oam(19)[1], 0, 'no small ring meanwhile');
  emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
  emu.idle(2);
  assert.equal(oam(29)[1], 0, 'gone once it lands');
  assert.deepEqual(pal(), before, 'the palette given back');
});

test('charged for a slam (four presses), you leap at a lob coming down well outside your reach', () => {
  const leap = (charge) => {
    const emu = quietRally();
    incoming(emu, 104, 196, 0, -6);  // 24 px across, 6 behind you, coming down
    set16(emu, 'ball_z', 30 * 16);
    emu.write8(emu.symbols.addr('ball_lob'), 1);
    readied(emu, SHOT_POWER, charge);
    emu.idle(3);
    return emu;
  };
  const yes = leap(16);
  assert.equal(at(yes, 'pl_shots', YOU), 1, 'leapt and struck it');
  assert.equal(at(yes, 'pl_last_move', YOU), MOVE_BICYCLE, 'a slam: a jumping bicycle kick');
  assert.equal(yes.v('ball_power'), 1, 'on fire');
  assert.equal(at(leap(8), 'pl_shots', YOU), 0, 'two presses: no leap (a slam takes mashing)');
});

test('a slam by the button: A red, B blue (bending), both gold (faster; only a full meter meets it clean)', () => {
  const slam = (first, second) => {
    const emu = quietRally();
    incoming(emu, 60, 50, 26, 28);
    set16(emu, 'ball_vx', 4);
    emu.write8(emu.symbols.addr('ball_lob'), 1);
    standAtMeet(emu);  // the slam spot shows
    emu.tap(first);
    if (second) emu.tap(second);
    const queued = at(emu, 'pl_shot', YOU);
    for (let k = 0; k < 4; k++) emu.tap(first === 'B' ? 'B' : 'A');  // the same button: more charge
    emu.until((e) => at(e, 'pl_shots', YOU) > 0 || e.v('goal_timer'), 160, 'the slam');
    assert.equal(at(emu, 'pl_last_move', YOU), MOVE_BICYCLE, 'a jumping bicycle kick');
    assert.equal(emu.v('ball_power'), 1, 'on fire');
    return { emu, queued, speed: Math.abs(emu.v('ball_vx', 'i16')) + Math.abs(emu.v('ball_vy', 'i16')) };
  };
  const red = slam('A'), blue = slam('B'), gold = slam('A', 'B'), both = slam('A+B');
  assert.equal(red.emu.v('ball_trail'), 1, 'red');
  assert.equal(red.emu.v('ball_curve'), 0, 'straight');
  assert.equal(blue.emu.v('ball_trail'), 2, 'blue');
  assert.notEqual(blue.emu.v('ball_curve'), 0, 'bending');
  assert.equal(gold.queued, SHOT_GOLD, 'A then B, a slam coming: GLD');
  assert.equal(both.queued, SHOT_GOLD, 'A and B at once too');
  assert.equal(gold.emu.v('ball_gold'), 1);
  assert.equal(gold.emu.v('ball_trail'), 3, 'gold');
  assert.ok(gold.speed > red.speed, `faster: ${gold.speed} against ${red.speed}`);
  // Gold, met with three quarters of a meter: popped up (a red slam's met clean).
  const meetIt = (gold) => {
    const emu = quietRally();
    incoming(emu, 80, 20, 48, 20);
    emu.write8(emu.symbols.addr('ball_power'), 1);
    emu.write8(emu.symbols.addr('ball_gold'), gold);
    standAtMeet(emu);
    emu.tap('A');
    for (let k = 0; k < 5; k++) emu.tap('A');  // 24 of 32
    emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the touch');
    return emu.v('ball_pop');
  };
  assert.equal(meetIt(0), 0, 'fire, three quarters: clean');
  assert.equal(meetIt(1), 1, 'gold, three quarters: popped up');
});

test('a drop shot coming to you shows where to meet it, after its bounce too', () => {
  const emu = quietRally();
  const marker = (e) => e.read8(e.symbols.addr('shadow_OAM') + 4 * 19);
  placePlayer(emu, YOU, 80, 200);
  incoming(emu, 80, 60, 24, 22);
  emu.write8(emu.symbols.addr('ball_drop'), 1);
  emu.idle(4);
  assert.ok(marker(emu) > 0, 'shown in flight');
  emu.until((e) => e.v('ball_landings') === 1, 120, 'the bounce');
  emu.idle(4);
  assert.ok(marker(emu) > 0, 'still shown off the bounce');
});

test('bursts: an impact star off a slam and sparks where it lands, dust off a slide, sparks off the net', () => {
  const oam = (e) => e.read8(e.symbols.addr('shadow_OAM') + 4 * 20);
  const VFX = { SLAM: 0, FIRE: 1, DUST: 2, SPARK: 3, SPRAY: 4 };
  let emu = quietRally();
  incoming(emu, 60, 50, 26, 28);
  set16(emu, 'ball_vx', 4);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  standAtMeet(emu);
  emu.tap('A');
  for (let k = 0; k < 3; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 120, 'the slam');
  assert.equal(emu.v('vfx_kind'), VFX.SLAM);
  emu.idle(3);
  assert.ok(oam(emu) > 0 || emu.read8(emu.symbols.addr('shadow_OAM') + 4 * 21) > 0, 'drawn');
  emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'the slam landing');
  assert.equal(emu.v('vfx_kind'), VFX.SPRAY, 'sparks fly up where it lands');
  emu.idle(3);
  const vis = [];
  for (let k = 20; k < 28; k++) if (emu.read8(emu.symbols.addr('shadow_OAM') + 4 * k)) vis.push(k);
  assert.ok(vis.length >= 1, `drawn in the burst's 8 sprites: ${vis}`);
  emu.idle(24);
  assert.equal(emu.v('vfx_kind'), 255, 'over');
  for (let k = 20; k < 28; k++) assert.equal(emu.read8(emu.symbols.addr('shadow_OAM') + 4 * k), 0, `sprite ${k} hidden`);
  emu = quietRally();
  ballAt(emu, 112, 150, 0, 0, 48);
  emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
  emu.until((e) => at(e, 'pl_move', YOU) === MOVE_SLIDE, 40, 'the slide');
  assert.equal(emu.v('vfx_kind'), VFX.DUST);
  emu = quietRally();
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  ballAt(emu, 80, 130, 4, 0, -48);
  emu.until((e) => e.v('ball_netted'), 20, 'the net');
  assert.equal(emu.v('vfx_kind'), VFX.SPARK);
});

test('a lob sits up where it lands: slowed to a quarter, and a high bounce to turn and reach', () => {
  const emu = quietRally();
  incoming(emu, 80, 120, 20, 20);
  emu.write8(emu.symbols.addr('ball_lob'), 1);
  emu.until((e) => e.v('ball_landings') === 1, 120, 'it landing');
  assert.ok(emu.v('ball_vy', 'i16') <= 5, `slowed: vy ${emu.v('ball_vy', 'i16')}`);
  let top = 0;
  for (let k = 0; k < 28; k++) top = Math.max(top, px(emu.idle(1).v('ball_z', 'i16')));
  assert.ok(top >= 12, `up ${top} px`);
});

test('A then B is a drop shot, just over the net and barely bouncing: unreached, your point', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('A').tap('B');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_DROP);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the drop shot');
  emu.until((e) => e.v('ball_landings') > 0, 120, 'it landing');
  const y = px(emu.v('ball_y', 'i16'));
  assert.ok(y >= HALFWAY - 30 && y < HALFWAY, `lands at ${y}, just over`);
  assert.equal(pointWithin(emu, 60), YOU);
  assert.equal(emu.v('point_reason'), POINT_BOUNCE);
});

test('queued, you are locked where you stand: a ball in reach, you return it', () => {
  const emu = quietRally();
  ballAt(emu, 92, 120, 0, 0, 30);  // 12 px across: in reach
  emu.tap('A');
  const x = at(emu, 'pl_x', YOU, 'i16'), y = at(emu, 'pl_y', YOU, 'i16');
  emu.hold('RIGHT', 8);
  assert.equal(at(emu, 'pl_x', YOU, 'i16'), x, 'the d-pad aims, it does not move you');
  assert.equal(at(emu, 'pl_y', YOU, 'i16'), y);
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.notEqual(at(emu, 'pl_last_move', YOU), MOVE_SLIDE, 'reached without a slide');
  assert.ok(emu.v('ball_vy', 'i16') < 0, 'returned');
});

test('out of reach you always slide: near, you return it; at full stretch, a pop-up; too far, a miss', () => {
  const slideAt = (off) => {
    const emu = quietRally();
    ballAt(emu, 80 + off, 130, 0, 0, 40);
    emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
    emu.write8(emu.symbols.addr('pl_charge') + YOU, 12);
    let slid = false;
    for (let f = 0; f < 60 && !at(emu, 'pl_shots', YOU); f++) { emu.idle(1); slid ||= at(emu, 'pl_move', YOU) === MOVE_SLIDE; }
    return { emu, slid, touched: at(emu, 'pl_shots', YOU) > 0, pop: emu.v('ball_pop') };
  };
  const near = slideAt(28), stretch = slideAt(48), far = slideAt(70);
  assert.ok(near.slid && near.touched && !near.pop, 'near: slide and return');
  assert.ok(near.emu.v('ball_vy', 'i16') < 0, 'into the other half');
  assert.equal(at(near.emu, 'pl_move', YOU), MOVE_SLIDE, 'still on the floor');
  const x = at(near.emu, 'pl_x', YOU, 'i16');
  near.emu.hold('LEFT', 20);
  assert.equal(at(near.emu, 'pl_x', YOU, 'i16'), x, 'on the floor');
  assert.ok(stretch.slid && stretch.touched && stretch.pop, 'full stretch: slid, popped up');
  assert.ok(far.slid, 'too far: you still try');
  assert.ok(!far.touched, 'and miss');
  assert.equal(at(far.emu, 'pl_shot', YOU), 0, 'the strike called off');
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

test('a ball on fire: met with under three eighths of a meter it pops up; with more, a clean strike', () => {
  let emu = quietRally();
  ballAt(emu, 80, 140, 0, 0, 96);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 30, 'the touch');
  assert.equal(emu.v('ball_pop'), 1, 'popped up');
  emu = quietRally();
  incoming(emu, 80, 20, 48, 20);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.tap('A');
  for (let k = 0; k < 8; k++) emu.tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 80, 'the strike');
  assert.equal(emu.v('ball_pop'), 0, 'struck clean');
  assert.ok(emu.v('ball_vy', 'i16') < 0);
});

test('a ball on fire knocks down a player who meets it unready, and goes on', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 30);
  emu.write8(emu.symbols.addr('last_touch'), YOU);
  emu.write8(emu.symbols.addr('holder'), CPU);
  ballAt(emu, 80, 100, 8, 0, -48);
  set16(emu, 'ball_vz', 5);
  emu.write8(emu.symbols.addr('ball_power'), 1);
  emu.until((e) => at(e, 'pl_move', CPU) === MOVE_DOWN, 80, 'the knock-down');
  assert.equal(pointWithin(emu, 240), YOU);
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

test('A plays it deep, deeper the more charged; B curves to mid-court', () => {
  const land = (shot, charge) => {
    const emu = quietRally();
    ballAt(emu, 82, 184);
    readied(emu, shot, charge);
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 160, 'it landing');
    return HALFWAY - px(emu.v('ball_y', 'i16'));  // px past the net
  };
  const a8 = land(SHOT_POWER, 8), a24 = land(SHOT_POWER, 24), b8 = land(SHOT_CURVE, 8);
  assert.ok(a8 >= 60 && a8 <= 80, `A, two presses: deep, ${a8} px past the net`);
  assert.ok(a24 > a8 + 4 && a24 <= 100, `A, six presses: deeper, ${a24}`);
  assert.ok(b8 >= 40 && b8 <= 64, `B: mid-court, ${b8}`);
});

test('aim is a direction from where you are: neutral goes half across, away from the CPU; LEFT / RIGHT angle all the way', () => {
  const land = (x, keys, cpuX = 20) => {
    const emu = quietRally();
    placePlayer(emu, CPU, cpuX, 20);
    placePlayer(emu, YOU, x, 190);
    ballAt(emu, x + 2, 184);
    readied(emu, SHOT_POWER, 16);
    emu.hold(keys, 1);
    emu.until((e) => e.v('ball_landings') > 0 || e.v('goal_timer'), 120, 'it landing');
    return px(emu.v('ball_x', 'i16'));
  };
  const away = land(80, []), awayL = land(80, [], 140), right = land(80, ['RIGHT']);
  assert.ok(away > 92 && away < right, `neutral from x 80, the CPU at 20: right, at ${away} (RIGHT ${right})`);
  assert.ok(awayL < 68, `the CPU at 140: left, at ${awayL}`);
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
  assert.equal(pointWithin(emu, 160), CPU);
  assert.equal(emu.v('shot_seen'), 11, 'MIS on the HUD');
});

test('SELECT calls off your queued strike', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, -40);
  emu.tap('A').tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), 2 * YOU_TAP);
  emu.tap('SELECT');
  assert.equal(at(emu, 'pl_shot', YOU), SHOT_NONE, 'called off');
  assert.equal(at(emu, 'pl_charge', YOU), 0, 'the meter emptied');
});

test('the first bounce in your half always gets up where you can play it, even off a low ball or a drop shot', () => {
  for (const drop of [0, 1]) {
    const emu = quietRally();
    ballAt(emu, 80, 150, 1, 0, 20);  // skimming in, barely off the ground
    set16(emu, 'ball_vz', -2);
    emu.write8(emu.symbols.addr('ball_drop'), drop);
    emu.until((e) => e.v('ball_landings') === 1, 40, 'the bounce');
    let top = 0;
    for (let k = 0; k < 24; k++) top = Math.max(top, px(emu.idle(1).v('ball_z', 'i16')));
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
  const emu = settle(bootToTitle().tap('RIGHT'), TITLE);
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

test('running has Mario Tennis momentum: a ramp up, a long coast when let go, a skid turning round', () => {
  const emu = quietRally();
  ballAt(emu, 140, 20, 0, 24, 12);  // far off, coming, high, and going out wide: no run home or to it
  set16(emu, 'ball_vz', 60);
  emu.idle(2);
  assert.equal(at(emu, 'meet_out', YOU), 1, 'landing out: you let it go');
  const vx = () => (emu.read8(emu.symbols.addr('pl_vx')) << 24) >> 24;
  emu.hold('RIGHT', 4);
  assert.ok(vx() > 6 && vx() < 22, `speeding up: ${vx()}`);
  emu.hold('RIGHT', 6);
  assert.equal(vx(), 22, 'full speed');
  emu.idle(10);
  assert.ok(vx() >= 10, `let go, still coasting: ${vx()}`);
  emu.idle(14);
  assert.equal(vx(), 0, 'stopped');
  emu.hold('RIGHT', 10).hold('LEFT', 6);
  assert.ok(vx() > 10, `turning: skidding ${vx()}`);
  emu.hold('LEFT', 12);
  assert.ok(vx() < -10, `then pushing off the other way: ${vx()}`);
});

test('the ball coming to you: you stay put, pressed or not (no autopilot)', () => {
  for (const press of [false, true]) {
    const emu = quietRally();
    placePlayer(emu, YOU, 40, 190);
    incoming(emu, 110, 90, 30, 20);  // across the court from you, coming
    if (press) emu.tap('A');
    emu.idle(30);
    assert.equal(px(at(emu, 'pl_x', YOU, 'i16')), 40, `you stay where you are (${press ? 'queued' : 'nothing pressed'})`);
  }
});

test('running side to side costs you: a turn skids first', () => {
  const emu = quietRally();
  const vx = () => (emu.read8(emu.symbols.addr('pl_vx')) << 24) >> 24;
  placePlayer(emu, YOU, 24, 190);
  emu.hold('RIGHT', 12);
  assert.ok(vx() >= 16, `running right: ${vx()}`);
  emu.hold('LEFT', 5);
  assert.ok(vx() > 0, `skidding, not turned on the spot: ${vx()}`);
  emu.hold('LEFT', 25);
  assert.ok(vx() < -8, `then off the other way: ${vx()}`);
});

test('rushing the net: a ball over it high, mashed four times, is smashed down short, and the CPU can only pop it up', () => {
  const shot = (youY, presses) => {
    const emu = quietRally();
    placePlayer(emu, YOU, 78, youY);
    placePlayer(emu, CPU, 80, 30);
    ballAt(emu, 80, 70, 16, 0, 44);
    set16(emu, 'ball_vz', 8);
    for (let k = 0; k < presses; k++) emu.tap('A').idle(1);
    emu.until((e) => at(e, 'pl_shots', YOU) > 0, 60, 'the strike');
    emu.idle(1);
    return emu;
  };
  const speed = (e) => Math.abs(e.v('ball_vx', 'i16')) + Math.abs(e.v('ball_vy', 'i16'));
  const emu = shot(132, 4);
  assert.equal(at(emu, 'pl_last_move', YOU), MOVE_BICYCLE, 'a bicycle kick');
  assert.equal(speed(emu), 72, "at your slam's speed");
  assert.equal(emu.v('ball_smash'), 1);
  emu.until((e) => e.v('ball_landings') > 0, 30, 'it landing');
  const landY = px(emu.v('ball_y', 'i16'));
  assert.ok(landY > HALFWAY - 48 && landY < HALFWAY, `down short, past the net: ${landY}`);
  assert.ok(emu.v('ball_vz', 'i16') >= 28, 'and it kicks up high');
  emu.write8(emu.symbols.addr('cpu_idle'), 0);
  emu.until((e) => at(e, 'pl_shots', CPU) > 0 || e.v('goal_timer'), 200, 'the CPU at it');
  if (!emu.v('goal_timer')) assert.equal(emu.v('ball_pop'), 1, 'only a full meter meets it clean: the CPU pops it up');
  assert.ok(speed(shot(132, 2)) < 72, 'two presses: no smash');
  assert.ok(speed(shot(160, 4)) < 72, 'back from the net: no smash');
});

test('between shots you stay where you are; a volley right at the net stays low and lands in', () => {
  let emu = quietRally();
  placePlayer(emu, YOU, 40, 124);  // up at the net, off to the left
  ballAt(emu, 80, 60, 20, 0, -4);  // going away, slowly, high
  set16(emu, 'ball_vz', 40);
  const x0 = px(at(emu, 'pl_x', YOU, 'i16')), y0 = px(at(emu, 'pl_y', YOU, 'i16'));
  emu.idle(60);
  assert.equal(emu.v('goal_timer'), 0, 'still in play');
  assert.equal(px(at(emu, 'pl_x', YOU, 'i16')), x0, 'no jog to the middle');
  assert.equal(px(at(emu, 'pl_y', YOU, 'i16')), y0, 'or back');
  emu.hold('DOWN', 12);
  assert.ok(px(at(emu, 'pl_y', YOU, 'i16')) > y0 + 4, 'the d-pad takes you');
  emu = quietRally();
  placePlayer(emu, YOU, 50, 122);
  ballAt(emu, 52, 116, 10, 0, 6);  // dropping onto your foot at net height
  emu.tap('A').tap('A').tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 40, 'the volley');
  let top = 0;
  emu.until((e) => { top = Math.max(top, px(e.v('ball_z', 'i16'))); return e.v('ball_landings') > 0 || e.v('goal_timer'); }, 120, 'it landing');
  assert.ok(top < 40, `up ${top} px (it went up 111 and long)`);
  assert.equal(emu.v('goal_timer'), 0, 'in');
});

test('cues: the ball coming and nothing queued, you blink; each press ticks, half and full chime', () => {
  const emu = quietRally();
  incoming(emu, 80, 40, 20, 30);
  emu.idle(3);
  assert.equal(emu.v('go_cue'), 1, 'press!');
  emu.tap('A');
  assert.equal(emu.v('go_cue'), 0, 'queued: no more cue');
  assert.equal(emu.v('sfx_last'), 10, 'a tick');
  emu.tap('A').tap('A').tap('A');
  assert.equal(at(emu, 'pl_charge', YOU), CHARGE_MAX / 2);
  assert.equal(emu.v('sfx_last'), 11, 'half: a slam is ready');
});

test('the CPU mishits only under pressure: a long run for a fast shot', () => {
  const meet = (fromX) => {
    const emu = quietRally();
    placePlayer(emu, CPU, 80, 40);
    emu.write8(emu.symbols.addr('pl_shot') + CPU, SHOT_POWER);
    emu.write8(emu.symbols.addr('pl_charge') + CPU, 20);
    ballAt(emu, 80, 70, 2, 0, -40);
    emu.write8(emu.symbols.addr('last_touch'), YOU);
    emu.write8(emu.symbols.addr('holder'), CPU);
    set16(emu, 'cpu_from_x', fromX);
    set16(emu, 'cpu_from_y', 40);
    const r0 = emu.v('cpu_rattled');
    emu.until((e) => at(e, 'pl_shots', CPU) > 0, 40, 'the CPU meeting it');
    return emu.v('cpu_rattled') - r0;
  };
  assert.equal(meet(80), 0, 'where it stood: clean');
  assert.equal(meet(-2000), 1, 'a long run: rattled (popped up or scuffed)');
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

test('the power meter: clean strikes in a rally fill it; full, A+B arms the power slam (SP!), gold and fast', () => {
  const emu = quietRally();
  ballAt(emu, 82, 186, 0);
  readied(emu, SHOT_POWER, 12);
  emu.idle(2);
  assert.equal(at(emu, 'pl_power', YOU), 1, 'a clean strike adds one');
  const arm = quietRally();
  arm.write8(arm.symbols.addr('pl_power') + YOU, POWER_MAX);
  incoming(arm, 82, 130, 40, 0);
  arm.hold('A+B', 1);
  assert.equal(at(arm, 'pl_special', YOU), 1, 'armed');
  assert.equal(at(arm, 'pl_power', YOU), 0, 'the meter spent');
  arm.idle(1);
  assert.ok(arm.v('glow_level') >= 1, 'armed: your outline glowing (a fast pulse, brighter)');
  arm.until((e) => { e.idle(1); return at(e, 'pl_shots', YOU) > 0; }, 80, 'the strike');
  arm.idle(1);
  assert.equal(arm.v('ball_special'), 1, 'the power slam');
  assert.equal(arm.v('ball_gold'), 1);
  assert.equal(Math.abs(arm.v('ball_vx', 'i16')) + Math.abs(arm.v('ball_vy', 'i16')), 88, 'faster than a gold slam');
  assert.ok(arm.v('ball_vy', 'i16') < 0, 'up the court');
  assert.equal(at(arm, 'pl_special', YOU), 0, 'used');
  arm.idle(2);
  assert.equal(arm.v('glow_level'), 5, 'struck: the glow snaps off in a white flash');
  arm.idle(8);
  assert.equal(arm.v('glow_level'), 0, 'then the outline as it was');
  const notFull = quietRally();
  notFull.write8(notFull.symbols.addr('pl_power') + YOU, POWER_MAX - 1);
  incoming(notFull, 82, 130, 40, 0);
  notFull.hold('A+B', 1);
  assert.equal(at(notFull, 'pl_special', YOU), 0, 'not full: no power slam');
});

test("the CPU's power slam: only a press timed to within 5 frames of it reaching you returns it; else down you go", () => {
  const comeAt = (pressAt) => {
    const emu = quietRally();
    ballAt(emu, 80, 116, 4, 0, 64);
    for (const n of ['ball_power', 'ball_gold', 'ball_special']) emu.write8(emu.symbols.addr(n), 1);
    emu.write8(emu.symbols.addr('sp_try') + YOU, 0);
    emu.write8(emu.symbols.addr('ball_landings'), 0);
    for (let f = 0; f < 80; f++) {
      const d = 190 - px(emu.v('ball_y', 'i16'));
      for (const at_ of pressAt) if (d <= at_ && d > at_ - 4) { emu.hold('A', 1); }
      emu.idle(1);
      if (at(emu, 'pl_move', YOU) === MOVE_DOWN) return 'down';
      if (at(emu, 'pl_shots', YOU)) return emu.v('ball_vy', 'i16') < 0 && emu.v('ball_power') ? 'returned' : 'touched';
    }
    return 'past';
  };
  assert.equal(comeAt([]), 'down', 'no press');
  assert.equal(comeAt([48]), 'down', 'too early');
  assert.equal(comeAt([20]), 'returned', 'timed: back on fire');
  assert.equal(comeAt([48, 20]), 'down', 'one try: a second press does not count');
});

test('screen: the power slam ready: the pips, your outline gold, A+B over you', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('pl_power') + YOU, POWER_MAX);
  incoming(emu, 80, 40, 30, 60);
  emu.tap('A').tap('A');
  matchGolden(emu.idle(4), 'power-meter');
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
  matchGolden(emu.idle(20), 'trail-power');
});

test('screen: a curve trail', () => {
  const emu = quietRally();
  ballAt(emu, 80, 150, 0, 0, 30);
  emu.tap('B');
  for (let k = 0; k < 8; k++) emu.tap('B');
  emu.until((e) => { e.hold('RIGHT', 1); return at(e, 'pl_shots', YOU) > 0; }, 80, 'the strike');
  matchGolden(emu.idle(28), 'trail-curve');
});

test('screen: the far end, the CPU sliding', () => {
  const emu = quietRally();
  placePlayer(emu, CPU, 80, 24);
  ballAt(emu, 80, 60, 0, 0, -30);
  emu.write8(emu.symbols.addr('pl_move') + CPU, MOVE_SLIDE);
  emu.write8(emu.symbols.addr('pl_move_timer') + CPU, 30);
  matchGolden(emu.idle(20), 'slide');
});

test('your slide or knockdown leaves the CPU standing: it is not drawn lying, nor split in three', () => {
  const emu = quietRally();
  emu.write8(emu.symbols.addr('pl_move') + YOU, MOVE_DOWN);
  emu.write8(emu.symbols.addr('pl_move_timer') + YOU, 40);
  emu.idle(3);
  // The CPU's body in the displayed OAM: bank-1 tiles in its red palette (2).
  const rows = (pal) => {
    const ys = new Set();
    for (let n = 0; n < 40; n++) {
      const a = 0xFE00 + n * 4, y = emu.read8(a), p = emu.read8(a + 3);
      if (y && (p & 8) && (p & 7) === pal) ys.add(y);
    }
    return ys.size;
  };
  assert.equal(rows(0), 1, 'you lie down: one row');
  assert.equal(rows(2), 2, 'the CPU stands: two rows');
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
