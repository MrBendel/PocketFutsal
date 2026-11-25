// CONE SLALOM (src/mg_slalom.c): TimberMan-style two-lane dribble sprint,
// alternating-foot dribble animation, cone time penalties, Gold/Silver/Bronze
// time medals at 40m, stat scaling, frame budget, and golden screens.
import { test } from 'node:test';
import { assert, matchGolden, FRAME_LIMIT, set16 } from './helpers.mjs';
import { openGame, phaseSettled, startMinigame, goLive, toResults, MG, MG_PH, MG_RUN, STAT } from './mg_helpers.mjs';

const CONE_NONE = 0, CONE_LEFT = 1, CONE_RIGHT = 2;
const MEDAL = { NONE: 0, BRONZE: 1, SILVER: 2, GOLD: 3 };

function liveSlalom(seed = 4242) {
  return goLive(startMinigame(MG.SLALOM, seed));
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

test('LEFT and RIGHT dribble forward step-by-step with alternating foot & ball push animation', () => {
  const e = liveSlalom(4242);
  e.idle(2);
  matchGolden(e, 'slalom-live');

  assert.equal(e.v('mg_slalom_dist'), 0);
  assert.equal(e.v('mg_slalom_target'), 40);
  const sy0 = e.v('scroll_y');
  const byRest = e.v('mg_slalom_by', 'i16');

  // Clear row 1 & row 2 so we can test pure RIGHT then LEFT dribble steps cleanly.
  e.write8(e.symbols.addr('mg_slalom_row') + 1, CONE_LEFT);   // safe on RIGHT
  e.write8(e.symbols.addr('mg_slalom_row') + 2, CONE_RIGHT);  // safe on LEFT

  e.tap('RIGHT');
  assert.equal(e.v('mg_slalom_lane'), 1, 'RIGHT switches to right dribble lane');
  assert.equal(e.v('mg_slalom_dist'), 1, 'advances 1m forward');
  assert.equal(e.v('mg_slalom_streak'), 1);
  assert.ok(e.v('mg_slalom_drib_anim') > 0, 'dribble kick animation active');
  assert.ok(e.v('mg_slalom_by', 'i16') < byRest, 'ball pushes forward ahead of boots on dribble touch');
  const foot1 = e.v('mg_slalom_foot');

  e.idle(3);
  matchGolden(e, 'slalom-clear');
  assert.notEqual(e.v('scroll_y'), sy0, 'pitch scrolls smoothly on dribble step');
  assert.equal(e.v('mg_slalom_px', 'i16'), 108, 'player glided to right lane (x=108)');

  // Next press: LEFT dodges row 1 (which shifted from row 2 = CONE_RIGHT) and alternates foot!
  e.tap('LEFT');
  assert.equal(e.v('mg_slalom_lane'), 0, 'LEFT switches to left dribble lane');
  assert.equal(e.v('mg_slalom_dist'), 2);
  assert.equal(e.v('mg_slalom_streak'), 2);
  assert.notEqual(e.v('mg_slalom_foot'), foot1, 'alternates dribbling foot each tap');
});

test('stepping into a cone bonks it, stumbles briefly, and adds time penalty (+2.0s)', () => {
  const e = liveSlalom(1337);
  e.write8(e.symbols.addr('mg_slalom_streak'), 5);
  // Put a cone in the RIGHT lane at row 1, then press RIGHT straight into it!
  e.write8(e.symbols.addr('mg_slalom_row') + 1, CONE_RIGHT);
  const tBefore = e.v('mg_slalom_tenths', 'u16');
  const pen = e.v('mg_slalom_pen_tenths');
  assert.equal(pen, 20, '2.0s (20 tenths) base penalty at CONTROL lv 1');

  e.tap('RIGHT');
  assert.equal(e.v('mg_slalom_hits'), 1, 'cone hit counted');
  assert.equal(e.v('mg_slalom_streak'), 0, 'cone hit resets streak');
  assert.ok(e.v('mg_slalom_stumble') > 0, 'stumble freeze triggered');
  assert.ok(e.v('mg_slalom_tenths', 'u16') >= tBefore + pen, 'added +2.0s penalty to total time');

  e.idle(2);
  matchGolden(e, 'slalom-hit');
});

test('dodging all 40m quickly wins GOLD medal; slower/penalized times award SILVER and BRONZE', () => {
  // 1) Fast bot reads row[1] every 4 frames and taps the open lane -> finishes in ~6.5s -> GOLD!
  const e = liveSlalom(4242);
  let steps = 0;
  for (let f = 0; f < 600 && e.v('mg_run') === MG_RUN.LIVE; f++) {
    if (f % 3 === 0 && e.v('mg_slalom_stumble') === 0) {
      const r1 = e.read8(e.symbols.addr('mg_slalom_row') + 1);
      // If cone is on LEFT (1), go RIGHT; otherwise go LEFT!
      e.tap(r1 === CONE_LEFT ? 'RIGHT' : 'LEFT');
      steps++;
    } else {
      e.idle(1);
    }
  }
  assert.equal(e.v('mg_run'), MG_RUN.OVER, '40m sprint finished');
  assert.equal(e.v('mg_slalom_dist'), 40);
  assert.equal(e.v('mg_slalom_hits'), 0, 'bot dodged every cone cleanly');
  assert.equal(e.v('mg_slalom_medal'), MEDAL.GOLD);
  assert.equal(e.v('mg_won'), 1);
  assert.ok(readDetail(e).startsWith('GOLD! '), `expected GOLD detail, got "${readDetail(e)}"`);
  toResults(e);
  assert.equal(e.v('mg_phase'), MG_PH.RESULT);
  assert.ok(e.v('mg_xp', 'u16') > 30, 'awarded strong XP + GOLD win bonus');

  // 2) SILVER (14.5s = 145 tenths) & BRONZE (19.0s = 190 tenths) checks at step 39 -> 40.
  const checkTier = (tenths, expectedMedal, prefix) => {
    const u = liveSlalom(777);
    u.write8(u.symbols.addr('mg_slalom_dist'), 39);
    u.write8(u.symbols.addr('mg_slalom_row') + 1, CONE_NONE);
    set16(u, 'mg_slalom_tenths', tenths);
    u.tap('LEFT');
    assert.equal(u.v('mg_slalom_medal'), expectedMedal);
    assert.ok(readDetail(u).startsWith(prefix), `expected "${prefix}", got "${readDetail(u)}"`);
  };
  checkTier(145, MEDAL.SILVER, 'SILVER! 14.5S');
  checkTier(190, MEDAL.BRONZE, 'BRONZE! 19.0S');
  checkTier(245, MEDAL.NONE, 'TIME 24.5S');
});

test('REFLEX shortens cone stumble; CONTROL reduces time penalty; sprint stays well inside frame budget', () => {
  const low = liveSlalom(303);
  const stum1 = low.v('mg_slalom_stumble_max');
  const pen1 = low.v('mg_slalom_pen_tenths');

  const high = openGame(MG.SLALOM);
  high.write8(high.symbols.addr('stat_lv') + STAT.REFLEX, 16);
  high.write8(high.symbols.addr('stat_lv') + STAT.CONTROL, 16);
  high.writeBytes(high.symbols.addr('test_seed'), [303 & 255, 303 >> 8]);
  high.idle(25).tap('A');
  phaseSettled(high, MG_PH.PLAY, 200, 'high stats slalom');
  goLive(high);

  assert.ok(high.v('mg_slalom_stumble_max') < stum1, 'REFLEX shortens cone stumble');
  assert.ok(high.v('mg_slalom_pen_tenths') < pen1, 'CONTROL reduces cone time penalty');

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
  assert.ok(worst < FRAME_LIMIT / 2, `worst frame ${worst} cycles exceeded half budget ${FRAME_LIMIT / 2}`);
});
