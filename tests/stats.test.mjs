// The players' stats (play.c, "the players' stats"): Speed and Control, levels 1-8,
// written at the title before the match starts (as the RPG will set them).
import { test } from 'node:test';
import {
  assert, bootToTitle, startGame, served, YOU, CPU, SHOT_POWER, MOVE_SLIDE, PHASE_RALLY,
  at, set16, placeBall, placePlayer,
} from './helpers.mjs';

/** A match started with these levels: { speed: [you, cpu], control: [you, cpu] }. */
function withStats({ speed = [4, 4], control = [4, 4] } = {}) {
  const emu = bootToTitle();
  emu.writeBytes(emu.symbols.addr('pl_speed_lv'), speed);
  emu.writeBytes(emu.symbols.addr('pl_control_lv'), control);
  return served(startGame(emu));
}

/** Mid-rally, the CPU (which struck last) standing still, you at (x, 190), the ball loose far off. */
function quiet(emu, x = 80) {
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, x, 190);
  placeBall(emu, 40, 140);
  for (const n of ['ball_z', 'ball_vx', 'ball_vy', 'ball_vz']) set16(emu, n, 0);
  return emu;
}

test('the middle levels are the old tuning: your 22 a frame, the CPU 20, fire met at 8 and 12', () => {
  const emu = withStats();
  assert.equal(at(emu, 'pl_top', YOU, 'i8'), 22);
  assert.equal(at(emu, 'pl_top', CPU, 'i8'), 20);
  assert.equal(at(emu, 'pl_fire_meet', YOU), 8);
  assert.equal(at(emu, 'pl_fire_meet', CPU), 12);
  assert.equal(emu.v('cpu_calm', 'i8'), 30);
});

test('Speed: a faster player runs further, and takes longer to stop', () => {
  const run = (lv) => {
    const emu = quiet(withStats({ speed: [lv, 4] }), 40);
    const x0 = at(emu, 'pl_x', YOU, 'i16');
    emu.hold('RIGHT', 24);
    const ran = at(emu, 'pl_x', YOU, 'i16') - x0;
    let coast = 0;
    while (at(emu, 'pl_vx', YOU, 'i8') && coast < 60) { emu.idle(1); coast++; }
    return { ran, coast };
  };
  const slow = run(1), mid = run(4), fast = run(8);
  assert.ok(slow.ran < mid.ran && mid.ran < fast.ran, `ran ${slow.ran} ${mid.ran} ${fast.ran}`);
  assert.ok(slow.coast < mid.coast && mid.coast < fast.coast, `coasted ${slow.coast} ${mid.coast} ${fast.coast} frames`);
});

test('Speed: up off a missed slide sooner (level 8) or later (level 1)', () => {
  const floor = (lv) => {
    const emu = quiet(withStats({ speed: [lv, 4] }));
    emu.write8(emu.symbols.addr('pl_move') + YOU, MOVE_SLIDE);
    emu.write8(emu.symbols.addr('pl_move_timer') + YOU, 64);
    let f = 0;
    while (at(emu, 'pl_move_timer', YOU) && f < 200) { emu.idle(1); f++; }
    return f;
  };
  const slow = floor(1), mid = floor(4), fast = floor(8);
  assert.equal(mid, 64, 'the middle: as before');
  assert.ok(fast <= 50 && slow >= 80, `on the floor ${slow} ${mid} ${fast} frames`);
});

test('Control, the defensive stat: the charge that meets a ball on fire cleanly', () => {
  // Your strike readied with this much charge, a ball on fire rolling at you.
  const meet = (lv, charge) => {
    const emu = quiet(withStats({ control: [lv, 4] }));
    emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
    emu.write8(emu.symbols.addr('pl_charge') + YOU, charge);
    placeBall(emu, 80, 150);
    set16(emu, 'ball_vy', 64);
    emu.write8(emu.symbols.addr('ball_power'), 1);
    emu.until((e) => at(e, 'pl_shots', YOU) > 0, 40, 'the touch');
    return emu.v('ball_pop') ? 'popped' : 'clean';
  };
  assert.equal(meet(4, 6), 'popped', 'the middle: 8 to meet it');
  assert.equal(meet(4, 8), 'clean');
  assert.equal(meet(8, 6), 'clean', 'level 8: 4 is enough');
  assert.equal(meet(1, 12), 'popped', 'level 1: it takes 14');
  assert.equal(at(withStats({ control: [4, 8] }), 'pl_fire_meet', CPU), 8, "the CPU's too");
  assert.equal(withStats({ control: [4, 1] }).v('cpu_calm', 'i8'), 18, 'and it mishits under less pressure');
});

/** Your drive (a power shot, two presses' charge) from (x, 190), aimed LEFT: where it first lands, across. */
function driveLeft(control, x) {
  const emu = withStats({ control: [control, 4] });
  quiet(emu, x);
  emu.write8(emu.symbols.addr('pl_beaten') + CPU, 1);
  placePlayer(emu, CPU, 80, 2);
  placeBall(emu, x, 160);
  set16(emu, 'ball_vy', 32);
  set16(emu, 'ball_vx', 0);
  emu.write8(emu.symbols.addr('pl_shot') + YOU, SHOT_POWER);
  emu.write8(emu.symbols.addr('pl_charge') + YOU, 8);
  emu.until((e) => { e.hold('LEFT', 1); return at(e, 'pl_shots', YOU) > 0; }, 60, 'the strike');
  emu.until((e) => { e.write8(e.symbols.addr('pl_beaten') + CPU, 1); return e.v('ball_landings') > 0; }, 200, 'it landing');
  return emu.v('ball_x', 'i16') >> 4;
}

test('Control takes your aim to the line: at 8, from mid-court, a drive lands on it', () => {
  const mid = driveLeft(4, 80), top = driveLeft(8, 80);
  assert.ok(mid - 8 >= 16, `Control 4: well inside the sideline (${mid - 8} px)`);
  assert.ok(top - 8 >= 0 && top - 8 <= 6, `Control 8: on the line, or nearly (${top - 8} px)`);
});

/** Your drive, two presses, at a ball coming, with Power `power` and the difficulty `pace`. */
function driveWithPower(power, pace = 1) {
  const emu = bootToTitle();
  emu.writeBytes(emu.symbols.addr('pl_power_lv'), [power, 4]);
  emu.write8(emu.symbols.addr('fn_pace'), pace);
  served(startGame(emu));
  emu.write8(emu.symbols.addr('cpu_idle'), 1);
  emu.write8(emu.symbols.addr('phase'), PHASE_RALLY);
  emu.write8(emu.symbols.addr('last_touch'), CPU);
  emu.write8(emu.symbols.addr('holder'), YOU);
  placePlayer(emu, CPU, 20, 20);
  placePlayer(emu, YOU, 80, 190);
  placeBall(emu, 80, 124);
  set16(emu, 'ball_vy', 36);
  emu.tap('A').tap('A');
  emu.until((e) => at(e, 'pl_shots', YOU) > 0, 60, 'the strike');
  return emu;
}

test('Power: a stronger player drives faster, landing sooner; the difficulty moves the CPU\'s two levels', () => {
  const pace = (e) => Math.abs(e.v('ball_vx', 'i16')) + Math.abs(e.v('ball_vy', 'i16'));
  const weak = driveWithPower(1), mid = driveWithPower(4), strong = driveWithPower(8);
  assert.ok(pace(weak) < pace(mid) && pace(mid) < pace(strong), `${pace(weak)} < ${pace(mid)} < ${pace(strong)}`);
  assert.ok(weak.v('arc_t', 'i16') > mid.v('arc_t', 'i16') && mid.v('arc_t', 'i16') > strong.v('arc_t', 'i16'), 'in the air longer, weaker');
  assert.equal(at(mid, 'pl_pace', YOU, 'i8'), 0, 'the middle level: the old tuning');
  assert.equal(at(mid, 'pl_pace', CPU, 'i8'), 0, 'Normal: the CPU at its own level');
  assert.equal(at(driveWithPower(4, 0), 'pl_pace', CPU, 'i8'), -12, 'Casual: two levels down');
  assert.equal(at(driveWithPower(4, 2), 'pl_pace', CPU, 'i8'), 8, 'Expert: two up');
});
