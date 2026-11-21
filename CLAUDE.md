# Notes for Claude sessions on PocketFutsal

- A Game Boy Color arcade futsal game (Mario Tennis-style shots) on the engine from
  RogueGB (https://github.com/MrBendel/RogueGB), taken over from Danger Noodle
  (../DangerNoodle when checked out beside this one): look there for systems not
  carried over yet (sprite slots, camera/scrolling, actors, save files in SRAM, far
  RAM, dialogue, music hooks) before writing new ones.
- The design: `docs/design/README.md`. The match is `src/play.c`; its state and the
  pitch's measurements are in `src/game.h` (tools/tile-art.mjs draws the pitch to match).
- Engine: GBDK-2020 4.5.0. Reference docs are in `docs/gbdk/` (start with `docs/gbdk/README.md`).
- The user builds and tests with `npm run play` / `npm test` (Node 22+).
- In a cloud session: download GBDK into `tools/` first
  (`curl -fSL -o tools/gbdk-linux64.tar.gz https://github.com/gbdk-2020/gbdk-2020/releases/download/4.5.0/gbdk-linux64.tar.gz`);
  `node tools/build.mjs` extracts it on first use.
- Verify every change with `node tools/test.mjs`. Look at screenshots in
  `build/test-output/` with Read when visuals change; `node tools/test.mjs --update`
  accepts new golden screens.
- Balance changes (keeper reach, shot speeds, the CPU): the tests pin a few outcomes
  (a weak corner shot is saved, a charged one scores, a chip beats a keeper on its
  line, the CPU can score). Try a spread of shots in a scratch script before and after.
- The HUD goes at the bottom, on the window layer (`HUD_WY` in `src/gfx.h`): free in hardware,
  where a top HUD needs a scanline interrupt every frame. Don't move it to the top.
- Keep watchable game state in global, non-static variables.
- Positions are int16 in sixteenths of a pixel (`SUB`, `PX()`, `TO_SUB()` in game.h).
- Bank 0 is for hot code; cold code/data goes in `#pragma bank 255` files or `bank: true`
  assets (README, "Engine rules"). The build fails if bank 0 overflows or passes 90%.
- Game Boy Color only, double speed: a frame is 140,448 CPU cycles (131,328 after vblank,
  `FRAME_LIMIT` in tests/helpers.mjs). Palettes go through `pal_base_*` and `gfx_set_fade`.
- Frame budget: per-frame code avoids 32-bit math and division; VRAM writes go through the
  vblank queue (`gfx_strip`, `gfx_strip_attr`, `gfx_vram_copy`).
- Never commit build output or the extracted `tools/.gbdk-*` folders.
