// The training mode: the hub, the mini games' shell (brief -> play -> results),
// XP and levels, and the save in battery RAM. Each game's own rules are tested
// in tests/mg_<name>.test.mjs.
import { test } from 'node:test';
import { assert, boot, settle, bootToTitle, TITLE, TRAINING, MINIGAME, matchGolden } from './helpers.mjs';
import { toTraining, startMinigame, toResults, phaseSettled, MG, MG_PH, MG_RUN, STAT, finishNow } from './mg_helpers.mjs';

test('the title has a third mode, training, and it opens the hub', () => {
  const e = bootToTitle();
  e.tap('DOWN').idle(40);
  e.tap('DOWN').idle(40);
  assert.equal(e.v('game_mode'), 2);
  matchGolden(e, 'menu-training');
  e.tap('DOWN').idle(40);
  assert.equal(e.v('game_mode'), 0, 'DOWN wraps round to futnet');
  e.tap('UP').idle(40);
  assert.equal(e.v('game_mode'), 2, 'UP wraps round to training');
  settle(e.tap('START'), TRAINING, 'the training hub');
  assert.equal(e.v('train_sel'), 0);
  e.idle(2);  // the last fade step reaches the screen
  matchGolden(e, 'training-hub');
  settle(e.tap('B'), TITLE, 'back to the title');
  assert.equal(e.v('game_mode'), 2, 'training is still chosen');
});

test('the hub chooses a game, scrolls past four, and its brief plays or goes back', () => {
  const e = toTraining();
  assert.equal(e.v('train_scroll'), 0);
  e.tap('DOWN');
  assert.equal(e.v('train_sel'), 1);
  // Stepping down to the 5th game (Cone Slalom) scrolls the 4-row window down.
  e.tap('DOWN').tap('DOWN').tap('DOWN');
  assert.equal(e.v('train_sel'), MG.SLALOM);
  assert.equal(e.v('train_scroll'), 1);
  matchGolden(e, 'training-hub-scrolled');
  // Wrapping UP from 0 jumps to the last game and scrolls to it.
  e.tap('DOWN');
  assert.equal(e.v('train_sel'), 0);
  assert.equal(e.v('train_scroll'), 0);
  e.tap('UP');
  assert.equal(e.v('train_sel'), MG.SLALOM);
  assert.equal(e.v('train_scroll'), 1);
  settle(e.tap('A'), MINIGAME, 'the brief');
  assert.equal(e.v('mg_id'), MG.SLALOM);
  assert.equal(e.v('mg_phase'), MG_PH.BRIEF);
  e.idle(25);
  settle(e.tap('B'), TRAINING, 'back to the hub');
  assert.equal(e.v('train_sel'), MG.SLALOM, 'the hub remembers the choice');
  assert.equal(e.v('train_scroll'), 1, 'the hub stays scrolled to the choice');
});

test('a game counts down, plays, pauses, and ends with XP and a new best', () => {
  const e = startMinigame(MG.WALL);
  assert.equal(e.v('mg_run'), MG_RUN.READY);
  e.idle(95);
  assert.equal(e.v('mg_run'), MG_RUN.LIVE);
  // Paused, the game's clock stops.
  e.tap('START');
  assert.equal(e.v('mg_paused'), 1);
  const t = e.v('mg_timer', 'u16');
  e.idle(30);
  assert.equal(e.v('mg_timer', 'u16'), t);
  e.tap('START');
  assert.equal(e.v('mg_paused'), 0);
  finishNow(e, 7);
  toResults(e);
  assert.equal(e.v('mg_score', 'u16'), 7);
  assert.equal(e.v('mg_new_best'), 1);
  // The wall trains AIM (all the XP) and POWER (half).
  const xp = e.v('mg_xp', 'u16');
  assert.ok(xp >= 28, `XP for 7 points: ${xp}`);
  assert.equal(e.v('stat_lv'), 1, 'POWER: half of 38 XP is not a level');
  const aimLv = e.read8(e.symbols.addr('stat_lv') + STAT.AIM);
  assert.equal(aimLv, 2, 'AIM went up a level');
  matchGolden(e, 'minigame-results');
  // A plays again, straight in.
  e.tap('A');
  phaseSettled(e, MG_PH.PLAY, 200, 'playing again');
  assert.equal(e.v('mg_run'), MG_RUN.READY);
});

test('SELECT while paused quits to the hub with no XP', () => {
  const e = startMinigame(MG.JUGGLE);
  e.idle(95).tap('START').tap('SELECT');
  settle(e, TRAINING, 'quit to the hub');
  assert.equal(e.read8(e.symbols.addr('mg_plays') + 2 * MG.JUGGLE), 0);
});

test('progress is saved to battery RAM and loaded on the next boot', () => {
  const e = startMinigame(MG.MACHINE);
  e.idle(95);
  finishNow(e, 20);
  toResults(e);
  const lv = Array.from({ length: 4 }, (_, i) => e.read8(e.symbols.addr('stat_lv') + i));
  assert.ok(lv[STAT.REFLEX] > 1, 'the machine trains REFLEX');
  const sram = e.sram();
  // A new console, the same cartridge.
  const f = boot();
  f.setSram(sram);
  settle(f, TITLE, 'title');
  assert.equal(f.v('progress_from_save'), 1);
  assert.deepEqual(Array.from({ length: 4 }, (_, i) => f.read8(f.symbols.addr('stat_lv') + i)), lv);
  assert.equal(f.read8(f.symbols.addr('mg_best') + 2 * MG.MACHINE), 20);
});

test('a blank or corrupt save starts a fresh player', () => {
  const e = bootToTitle();
  assert.equal(e.v('progress_from_save'), 0);
  assert.equal(e.v('stat_lv'), 1);
});
