// Shared helpers for ROM tests (node:test). Each test boots a fresh emulator.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {
  loadRom, saveScreenshot, readTyped, parseInputScript, replayInput, prepareReplay, maskFromNames, ROOT,
} from '../harness/node.mjs';

export { assert, ROOT };
const GOLDENS = path.join(ROOT, 'tests', 'goldens.json');
const OUT = path.join(ROOT, 'build', 'test-output');

/** Fresh emulator with convenience methods for tests. */
export function boot(opts = {}) {
  const emu = loadRom(opts.rom, opts);
  return Object.assign(emu, {
    /** Read a global C variable by name. type: u8|i8|u16|i16|bool */
    v(name, type = 'u8') {
      return readTyped(emu, emu.symbols.addr(name), type);
    },
    /** Hold buttons ("RIGHT+A", or [] for none) for n frames. */
    hold(buttons, n = 1) {
      const names = Array.isArray(buttons) ? buttons : String(buttons).split('+');
      emu.setButtons(maskFromNames(names));
      emu.run(n);
      return emu;
    },
    /** Release all buttons and wait n frames. */
    idle(n = 1) {
      emu.setButtons(0);
      emu.run(n);
      return emu;
    },
    /** Tap buttons for one frame, then release for one frame. */
    tap(buttons) {
      return emu.hold(buttons, 1).idle(1);
    },
    /** Play an input script like "0-60:RIGHT 30:A 31:-". */
    play(script, frames) {
      const input = replayInput({ events: parseInputScript(script) });
      const end = frames ?? parseInputScript(script).at(-1)[0] + 1;
      for (let f = 0; f < end; f++) {
        emu.setButtons(input(f));
        emu.step();
      }
      return emu;
    },
    /** Play a replay JSON file (from the playtest page) to the end. */
    replay(file) {
      const r = JSON.parse(fs.readFileSync(path.resolve(ROOT, file), 'utf8'));
      const input = prepareReplay(emu, r);
      for (let f = 0; f < r.length; f++) {
        emu.setButtons(input(f));
        emu.step();
      }
      return emu;
    },
    /** Run until predicate(emu) is true; fails after maxFrames. */
    until(pred, maxFrames = 600, what = 'condition') {
      for (let i = 0; i < maxFrames; i++) {
        if (pred(emu)) return emu;
        emu.step();
      }
      assert.fail(`timed out after ${maxFrames} frames waiting for ${what}`);
    },
    shot(name) {
      return saveScreenshot(emu, path.join(OUT, `${name}.png`));
    },
  });
}

const readGoldens = () => (fs.existsSync(GOLDENS) ? JSON.parse(fs.readFileSync(GOLDENS, 'utf8')) : {});

/**
 * Update one entry of goldens.json. Test files run in parallel processes, so the
 * read-modify-write holds a lock (a directory, created atomically); without it,
 * two files updating at once could each write back the other's old entries.
 */
function writeGolden(name, hash) {
  const lock = GOLDENS + '.lock';
  const pause = new Int32Array(new SharedArrayBuffer(4));
  for (let tries = 0; ; tries++) {
    try {
      fs.mkdirSync(lock);
      break;
    } catch (e) {
      if (e.code !== 'EEXIST') throw e;
      if (tries > 2000) throw new Error(`${lock} has been held for over 10 s; delete it if no tests are running`);
      Atomics.wait(pause, 0, 0, 5);
    }
  }
  try {
    const goldens = readGoldens();
    goldens[name] = hash;
    fs.writeFileSync(GOLDENS, JSON.stringify(goldens, null, 2) + '\n');
  } finally {
    fs.rmdirSync(lock);
  }
}

/**
 * Screen regression check. Compares a hash of the current frame against
 * tests/goldens.json. Run with UPDATE_GOLDENS=1 to accept new output.
 * A PNG of the current frame is always written to build/test-output/.
 */
export function matchGolden(emu, name) {
  const goldens = readGoldens();
  const hash = emu.frameHash();
  const png = emu.shot(name);
  if (process.env.UPDATE_GOLDENS || !(name in goldens)) {
    writeGolden(name, hash);
    return;
  }
  assert.equal(hash, goldens[name], `screen "${name}" changed; see ${path.relative(ROOT, png)} (UPDATE_GOLDENS=1 to accept)`);
}

// The CPU budget (Game Boy Color, double speed): a frame is 140,448 CPU cycles,
// and emu.lastFrameCycles counts from the end of vblank (9,120 of them), so an
// update that runs past FRAME_LIMIT misses the next frame. See README, Frame budget.
export const FRAME_CYCLES = 140448;
export const FRAME_LIMIT = FRAME_CYCLES - 9120;  // 131,328

// Scene IDs, must match src/state.h.
export const TITLE = 0, PLAY = 1, GAME_OVER = 2;
// Sides, shots and positions, must match src/game.h.
export const YOU = 0, CPU = 1, NOBODY = 0xFF;
export const SHOT_NONE = 0, SHOT_DRIVE = 1, SHOT_CHIP = 2;
export const CHARGE_MAX = 32;
/** Sixteenths of a pixel -> pixels. */
export const px = (sub) => Math.floor(sub / 16);

/** Run until the given scene is active and its fade-in has finished. */
export function settle(emu, state, what = `state ${state}`) {
  // frame_count > 0: before boot code runs, RAM reads as zeros (= TITLE, settled).
  return emu.until(
    (e) => e.v('frame_count', 'u16') > 0 && e.v('game_state') === state && !e.v('in_transition'),
    300,
    what,
  );
}

export function bootToTitle() {
  return settle(boot(), TITLE, 'title screen');
}

/** From the title, START a match on a fixed seed (so the CPU plays the same way). */
export function startGame(emu = bootToTitle(), seed = 4242) {
  emu.writeBytes(emu.symbols.addr('test_seed'), [seed & 255, seed >> 8]);
  return settle(emu.tap('START'), PLAY, 'play state');
}

/** Wait out the kickoff whistle. */
export function kickedOff(emu) {
  return emu.until((e) => e.v('kickoff_timer') === 0, 120, 'the whistle');
}

/** Write a signed 16-bit global. */
export function set16(emu, name, value) {
  const v = value & 0xffff;
  emu.writeBytes(emu.symbols.addr(name), [v & 255, v >> 8]);
}
/** Write one entry of a global array of int16 (by index). */
export function set16At(emu, name, i, value) {
  const v = value & 0xffff;
  emu.writeBytes(emu.symbols.addr(name) + 2 * i, [v & 255, v >> 8]);
}
/** Read entry i of a global array. */
export function at(emu, name, i, type = 'u8') {
  const size = type.endsWith('16') ? 2 : 1;
  const addr = emu.symbols.addr(name) + size * i;
  if (size === 1) { const b = emu.read8(addr); return type === 'i8' && b > 127 ? b - 256 : b; }
  const w = emu.read8(addr) | (emu.read8(addr + 1) << 8);
  return type === 'i16' && w > 32767 ? w - 65536 : w;
}

/** Put the ball at (x, y) px, still, on the ground. */
export function placeBall(emu, x, y) {
  set16(emu, 'ball_x', x * 16); set16(emu, 'ball_y', y * 16); set16(emu, 'ball_z', 0);
  set16(emu, 'ball_vx', 0); set16(emu, 'ball_vy', 0); set16(emu, 'ball_vz', 0);
}
/** Put a player (side) at (x, y) px. */
export function placePlayer(emu, side, x, y) {
  set16At(emu, 'pl_x', side, x * 16); set16At(emu, 'pl_y', side, y * 16);
}
