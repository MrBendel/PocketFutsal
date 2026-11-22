// Helpers for the training mode's tests (tests/minigames.test.mjs, tests/mg_*.test.mjs).
import { settle, bootToTitle, TRAINING, MINIGAME } from './helpers.mjs';

// Must match src/progress.h and src/minigame.h.
export const MG = { MACHINE: 0, WALL: 1, JUGGLE: 2, SHOOTOUT: 3 };
export const STAT = { POWER: 0, CONTROL: 1, AIM: 2, REFLEX: 3 };
export const MG_PH = { BRIEF: 0, PLAY: 1, RESULT: 2 };
export const MG_RUN = { READY: 0, LIVE: 1, OVER: 2 };
export const MG_READY_FRAMES = 90;
// OAM slots (src/minigame.h).
export const OAM_MG_BALL = 0, OAM_MG_SHADOW = 1, OAM_MG_PLAYER = 2, OAM_MG_FREE = 14;

/** Title -> the training hub (the title's third mode). */
export function toTraining(emu = bootToTitle()) {
  emu.tap('UP').idle(40);  // UP from futnet wraps round to training
  return settle(emu.tap('START'), TRAINING, 'the training hub');
}

/** The hub -> a game's brief card. */
export function openGame(id, emu = toTraining()) {
  emu.write8(emu.symbols.addr('train_sel'), id);
  return settle(emu.tap('A'), MINIGAME, 'the brief');
}

/**
 * Run until the shell is in `phase` with its screen faded in. The phase changes
 * a frame before the fade to it starts, so step past that before waiting.
 */
export function phaseSettled(emu, phase, frames = 400, what = `phase ${phase}`) {
  emu.until((e) => e.v('mg_phase') === phase, frames, what);
  emu.idle(2);
  return emu.until((e) => !e.v('in_transition'), 120, what);
}

/** Straight into a game (its countdown just started), on a fixed seed. */
export function startMinigame(id, seed = 4242) {
  const emu = openGame(id);
  emu.writeBytes(emu.symbols.addr('test_seed'), [seed & 255, seed >> 8]);
  emu.idle(25);  // the card ignores buttons for a moment
  emu.tap('A');
  return phaseSettled(emu, MG_PH.PLAY, 200, 'the game');
}

/** Through the countdown: the game is live. */
export function goLive(emu) {
  return emu.until((e) => e.v('mg_run') === MG_RUN.LIVE, MG_READY_FRAMES + 10, 'GO!');
}

/** End the game now with this score, as if it had called mg_finish. */
export function finishNow(emu, score) {
  emu.writeBytes(emu.symbols.addr('mg_score'), [score & 255, score >> 8]);
  emu.writeBytes(emu.symbols.addr('mg_timer'), [0, 0]);
  emu.write8(emu.symbols.addr('mg_run'), MG_RUN.OVER);
  return emu;
}

/** Run until the results card is up (and taking buttons). */
export function toResults(emu, frames = 400) {
  phaseSettled(emu, MG_PH.RESULT, frames, 'the results');
  return emu.idle(25);
}
