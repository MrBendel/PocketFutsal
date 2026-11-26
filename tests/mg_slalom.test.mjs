// CONE SLALOM (src/mg_slalom.c): TimberMan-style two-lane dribble sprint,
// ball-behind-player hardware occlusion, tight 28px/24px lane spacing,
// guaranteed-solvable cone generator (open crossover rows between side switches),
// Solo Gold/Silver/Bronze time medals, 1v1 VS CPU head-to-head race, stat scaling,
// frame budget, and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, set16 } from './helpers.mjs';
import { openGame, phaseSettled, startMinigame, goLive, toResults, MG, MG_PH, MG_RUN, STAT } from './mg_helpers.mjs';

const CONE_NONE = 0, CONE_LEFT = 1, CONE_RIGHT = 2;
const MEDAL = { NONE: 0, BRONZE: 1, SILVER: 2, GOLD: 3 };

function liveSlalom(seed = 4242, vs = false) {
  return goLive(startMinigame(MG.SLALOM, seed, { vs }));
}

function readDetail(e) {
  const base = e.symbols.addr('mg_detail');
  let s = '';
  for (let i = 0; i < 20; i++) {
    const ch = e.read8(base + i);
    if (!ch) break;
    s += String.fromCharCode(ch);
  }
  return s;
}

test('ball renders ahead of & behind player in OAM, lanes are tight (66/94), and dribble animates', () => {
  const e = liveSlalom(4242, false);
  e.idle(2);
  matchGolden(e, 'slalom-live');

  // Verify ball is NOT in OAM slot 0 (which would draw over the player's head),
  // and IS in OAM slot 14 (> player slots 2..5) ahead of the player's head (y <= 89 < 95)!
  assert.equal(e.read8(0xFE00 + 0 * 4), 0, 'OAM slot 0 hidden so ball never draws over player');
  const ballOamY = e.read8(0xFE00 + 14 * 4);
  assert.ok(ballOamY > 0 && ballOamY < 160, `ball drawn in occluded OAM slot 14 (y=${ballOamY})`);
  assert.ok(e.v('mg_slalom_by', 'i16') <= 89, 'ball rests cleanly ahead of player head (y <= 89)');
  assert.equal(e.v('mg_slalom_px', 'i16'), 66, 'starts in tight Solo left lane (x=66)');

  // Verify Solo cones (OAM slots 18..24) are actually visible on screen!
  let visibleCones = 0;
  for (let slot = 18; slot <= 24; slot++) {
    const cy = e.read8(0xFE00 + slot * 4);
    if (cy > 0 && cy < 160) visibleCones++;
  }
  assert.ok(visibleCones >= 2, `Solo mode displays visible cones on pitch (found ${visibleCones})`);

  e.write8(e.symbols.addr('mg_slalom_row') + 1, CONE_LEFT);   // safe on RIGHT
  e.write8(e.symbols.addr('mg_slalom_row') + 2, CONE_RIGHT);  // safe on LEFT

  const byRest = e.v('mg_slalom_by', 'i16');
  e.tap('RIGHT');
  assert.equal(e.v('mg_slalom_lane'), 1);
  assert.equal(e.v('mg_slalom_dist'), 1);
  assert.ok(e.v('mg_slalom_by', 'i16') < byRest, 'ball pushes further ahead of boots on dribble kick');
  const foot1 = e.v('mg_slalom_foot');

  e.idle(4);
  matchGolden(e, 'slalom-clear');
  assert.equal(e.v('mg_slalom_px', 'i16'), 94, 'glided to tight Solo right lane (x=94, 28px gap)');

  e.tap('LEFT');
  assert.equal(e.v('mg_slalom_lane'), 0);
  assert.equal(e.v('mg_slalom_dist'), 2);
  assert.notEqual(e.v('mg_slalom_foot'), foot1, 'alternates dribbling foot each tap');
});

test('cone generator never places opposite-side cones on consecutive rows without an open gap', () => {
  for (const seed of [101, 202, 303, 404, 505, 606, 707, 808]) {
    const e = liveSlalom(seed, false);
    let prev = CONE_NONE;
    for (let step = 0; step < 40; step++) {
      const r1 = e.read8(e.symbols.addr('mg_slalom_row') + 1);
      if (prev !== CONE_NONE && r1 !== CONE_NONE) {
        assert.equal(r1, prev, `seed ${seed} step ${step}: opposite cones (${prev} -> ${r1}) without open crossover gap!`);
      }
      prev = r1;
      // Follow a purely single-lane-hold until forced to switch on an open gap or before cone:
      const curLane = e.v('mg_slalom_lane');
      const goRight = r1 === CONE_LEFT ? true : r1 === CONE_RIGHT ? false : Boolean(curLane);
      e.tap(goRight ? 'RIGHT' : 'LEFT');
      assert.equal(e.v('mg_slalom_hits'), 0, `seed ${seed} step ${step}: 100% clean dodge without hitting any cone`);
    }
  }
});

test('stepping into a cone bonks it, stumbles briefly, and adds time penalty (+2.0s in Solo)', () => {
  const e = liveSlalom(1337, false);
  e.write8(e.symbols.addr('mg_slalom_streak'), 5);
  e.write8(e.symbols.addr('mg_slalom_row') + 1, CONE_RIGHT);
  const tBefore = e.v('mg_slalom_tenths', 'u16');
  const pen = e.v('mg_slalom_pen_tenths');
  assert.equal(pen, 20);

  e.tap('RIGHT');
  assert.equal(e.v('mg_slalom_hits'), 1);
  assert.equal(e.v('mg_slalom_streak'), 0);
  assert.ok(e.v('mg_slalom_stumble') > 0);
  assert.ok(e.v('mg_slalom_tenths', 'u16') >= tBefore + pen);

  e.idle(2);
  matchGolden(e, 'slalom-hit');
});

test('Solo 40m medals (GOLD / SILVER / BRONZE) and 1v1 VS CPU head-to-head race', () => {
  // 1) Solo GOLD run
  const solo = liveSlalom(4242, false);
  for (let f = 0; f < 600 && solo.v('mg_run') === MG_RUN.LIVE; f++) {
    if (f % 3 === 0 && solo.v('mg_slalom_stumble') === 0) {
      const r1 = solo.read8(solo.symbols.addr('mg_slalom_row') + 1);
      solo.tap(r1 === CONE_LEFT ? 'RIGHT' : 'LEFT');
    } else {
      solo.idle(1);
    }
  }
  assert.equal(solo.v('mg_slalom_medal'), MEDAL.GOLD);
  assert.ok(readDetail(solo).startsWith('GOLD! '));

  // 2) SILVER & BRONZE tiers
  const checkTier = (tenths, expectedMedal, prefix) => {
    const u = liveSlalom(777, false);
    u.write8(u.symbols.addr('mg_slalom_dist'), 39);
    u.write8(u.symbols.addr('mg_slalom_row') + 1, CONE_NONE);
    set16(u, 'mg_slalom_tenths', tenths);
    u.tap('LEFT');
    assert.equal(u.v('mg_slalom_medal'), expectedMedal);
    assert.ok(readDetail(u).startsWith(prefix));
  };
  checkTier(145, MEDAL.SILVER, 'SILVER! 14.5S');
  checkTier(190, MEDAL.BRONZE, 'BRONZE! 19.0S');

  // 3) 1v1 VS CPU Slalom Race: side-by-side split track (40/64 vs 96/120), first to 40m wins!
  const vs = liveSlalom(4242, true);
  assert.equal(vs.v('mg_vs'), 1);
  assert.equal(vs.v('mg_slalom_px', 'i16'), 40, 'VS YOU starts on left track (x=40)');
  assert.equal(vs.v('mg_slalom_cx', 'i16'), 96, 'VS CPU starts on right track (x=96)');
  vs.idle(20);  // let CPU take its first step so both racers are moving!
  assert.ok(vs.v('mg_slalom_cdist') >= 1, 'CPU racer advances down right track');
  matchGolden(vs, 'slalom-vs-live');

  // Sprint YOU across 40m ahead of CPU -> 1ST place win!
  for (let f = 0; f < 600 && vs.v('mg_run') === MG_RUN.LIVE; f++) {
    if (f % 3 === 0 && vs.v('mg_slalom_stumble') === 0) {
      const r1 = vs.read8(vs.symbols.addr('mg_slalom_row') + 1);
      vs.tap(r1 === CONE_LEFT ? 'RIGHT' : 'LEFT');
    } else {
      vs.idle(1);
    }
  }
  assert.equal(vs.v('mg_run'), MG_RUN.OVER);
  assert.equal(vs.v('mg_won'), 1, 'beating CPU to 40m wins the race');
  assert.ok(readDetail(vs).startsWith('1ST! '), `expected 1ST! detail, got "${readDetail(vs)}"`);
  toResults(vs);
  assert.ok(vs.v('mg_xp', 'u16') > 30, 'earned race XP + win bonus');
});

test('REFLEX & CONTROL scale recovery/penalty and VS 2-track race fits well inside frame budget', () => {
  const low = liveSlalom(303, false);
  const stum1 = low.v('mg_slalom_stumble_max');
  const pen1 = low.v('mg_slalom_pen_tenths');

  const high = openGame(MG.SLALOM);
  high.write8(high.symbols.addr('stat_lv') + STAT.REFLEX, 16);
  high.write8(high.symbols.addr('stat_lv') + STAT.CONTROL, 16);
  high.writeBytes(high.symbols.addr('test_seed'), [303 & 255, 303 >> 8]);
  high.idle(25).tap('RIGHT').idle(2).tap('A');  // toggle to VS CPU mode too!
  phaseSettled(high, MG_PH.PLAY, 200, 'high stats VS slalom');
  goLive(high);

  assert.equal(high.v('mg_vs'), 1);
  assert.ok(high.v('mg_slalom_pen_tenths') < pen1, 'CONTROL reduces Solo penalty');
  const lowVs = liveSlalom(303, true);
  assert.ok(high.v('mg_slalom_stumble_max') < lowVs.v('mg_slalom_stumble_max'), 'stats shorten VS stumble');

  let worst = 0;
  for (let f = 0; f < 180 && high.v('mg_run') === MG_RUN.LIVE; f++) {
    if (f % 3 === 0 && high.v('mg_slalom_stumble') === 0) {
      const r1 = high.read8(high.symbols.addr('mg_slalom_row') + 1);
      high.hold(r1 === CONE_LEFT ? 'RIGHT' : 'LEFT', 1);
    } else {
      high.idle(1);
    }
    if (high.lastFrameCycles > worst) worst = high.lastFrameCycles;
  }
  assert.ok(worst < FRAME_LIMIT / 2, `worst VS race frame ${worst} cycles exceeded half budget ${FRAME_LIMIT / 2}`);
});
