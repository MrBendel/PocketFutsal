# PocketFutsal

A Game Boy Color arcade futsal game in the spirit of Mario Tennis: 1-on-1 against the
CPU, each side a player and a keeper, shots you charge by pressing early. Built in C
with [GBDK-2020](https://gbdk.org/) 4.5.0 on the engine from
[RogueGB](https://github.com/MrBendel/RogueGB) (by way of Danger Noodle): its build,
VRAM queue, text, fades, sound engine, headless emulator harness, tests and playtest page.

**Status: v0.1, first playable.** One pitch, one CPU opponent, first to five goals.
The design is in [docs/design/README.md](docs/design/README.md).

## Quick start

Needs Node 22+.

```
npm run play     # build, open the playtest page, rebuild + reload on every change
npm test         # build, then run the headless tests
npm run build    # just the ROM: build/pocketfutsal.gb
```

GBDK is found in this order: `$GBDK_HOME`, `tools/gbdk/`, or a release archive
dropped in `tools/` (`gbdk-win64.zip` / `gbdk-linux64.tar.gz` / `gbdk-macos*.tar.gz`,
from <https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.5.0>), which is
extracted on first build.

## How to play

You're blue, attacking the goal on the right. The ball is always loose: you strike it
rather than carry it.

| Button | What it does |
|---|---|
| D-pad | Run (eight ways). Into the ball, you dribble it ahead; standing still, you trap it |
| A | Ready a **driven shot** along the ground |
| B | Ready a **chip**, up and over the keeper |
| UP / DOWN as the shot goes | Aim at that corner of the goal |
| START | Pause |

Press A or B *before* the ball reaches you: the shot charges for as long as the ball
takes to arrive (the meter in the HUD), then fires by itself the moment it's in reach.
While charging you only shuffle, so charging early is a gamble on where the ball goes.
A full meter is a **power shot**: the ball catches fire, and a keeper can only beat it
away, and only if it's straight at them. Keepers hold anything softer that they reach,
then throw it out. A charged chip floats over a keeper standing on its line.

## Code structure

| File | What it does |
|---|---|
| `src/main.c` | Boot and the main loop. One scene update per frame; scene changes fade out, `enter()` the new scene, fade in |
| `src/state.h` | Scene IDs (`STATE_TITLE`, `STATE_PLAY`, `STATE_GAME_OVER`), `state_change()`, `wait_frame()` |
| `src/game.h` | The match's watchable state and the pitch's measurements |
| `src/play.c` | The match: players, CPU, keepers, the ball's physics, goals, the HUD |
| `src/title.c` | Title screen. START seeds the match from the frame it lands on |
| `src/game_over.c` | Full time: who won and the score; START returns to the title |
| `src/input.c` | Reads the joypad once per frame: `joy` (held), `joy_pressed`, `joy_released` |
| `src/gfx.c` | Bank 0: the VRAM write queue (`gfx_strip`, `gfx_vram_copy`), the vblank handler, `bkg_text`, banked copy helpers |
| `src/gfx_setup.c` | Banked: loading the font and frame tiles, clearing the screen, palette fades, framed boxes, numbers |
| `src/vram_queue.s` | Assembly: writes the queued VRAM updates and palettes during vblank |
| `src/text.c` | Banked: proportional text in three fonts (compact, display, italic) from `artwork/fonts/font-sheet.png` |
| `src/scene_text.h` | `draw()`: text from a banked scene's own string constants |
| `src/sfx.c` | Banked: register-level sound effects with priorities |
| `src/palettes.c` | Banked: each scene's Color palettes |
| `tools/build.mjs` | The build: assets, compile, link, bank 0 and work RAM checks |
| `tools/assets.mjs` | PNG → tiles and 16×16 sprite animations; ASCII-art tiles, and tile maps, from `tools/tile-art.mjs` |
| `tools/tile-art.mjs` | Placeholder art: the pitch drawn in code (cut into unique tiles and a map), ASCII sprites |
| `tools/fonts.mjs` | The font sheet → glyph tables for `text.c` |
| `tools/serve.mjs` | The local playtest server (live rebuild + reload, saves replays to `replays/`) |
| `tools/bundle.mjs` | The playtest page, emulator and ROM in one HTML file |
| `harness/` | The Boytacean emulator wrapper, CLI runner (`npm run run -- --help`) and CPU profiler |
| `tests/` | Headless tests (`node:test`) and golden screen hashes |

To add a scene: add a `STATE_*` ID and its `*_enter`/`*_update` to `state.h`, add
it to both switches in `main.c`, and to the scene IDs in `tests/helpers.mjs`.

## Engine rules (from RogueGB)

**Game Boy Color only, double speed.** `cpu_fast()` at boot: a frame is 140,448 CPU
cycles, 131,328 after vblank (`FRAME_LIMIT` in `tests/helpers.mjs`). Keep per-frame
code cheap: no 32-bit math or division per frame, VRAM writes through the vblank
queue (`gfx_strip`, `gfx_strip_attr`, `gfx_vram_copy`), not directly while the screen
draws. Profile with `npm run profile -- play`.

**ROM banks.** Bank 0 (0x0000-0x3FFF) is always visible and holds hot code; one
switchable bank at a time is visible at 0x4000-0x7FFF. Cold code goes in
`#pragma bank 255` files with `BANKED` entry points; assets marked `bank: true` in
`tools/assets.mjs` go in switchable banks. Banked code must not `SWITCH_ROM` itself:
use the bank-0 helpers `banked_memcpy`, `banked_sprite_data`, `banked_bkg_data`.
Text drawing reads strings from RAM: banked scenes copy their constants first
(`scene_text.h`). The build fails if bank 0 overflows or passes 90%.

**HUD at the bottom.** The HUD is the window layer at line 136 (`HUD_WY` in `gfx.h`),
under a 160×136 pitch. The hardware draws the window to the bottom of the screen
for free; a top HUD would need a scanline interrupt every frame. `gfx_sprite_split`
keeps sprites off it.

**Palettes.** Scenes set `pal_base_bg`/`pal_base_obj` (via `palettes.c`) and call
`gfx_set_fade`; the vblank handler uploads them. The DMG palette registers do
nothing on the Color.

**Watchable state.** Keep game state in global, non-static variables: tests, the CLI
(`--watch`) and the playtest page read them by name from `build/pocketfutsal.noi`.
Set `cpu_idle` to 1 (the playtest page's watches can) to make the CPU stand still.

## Art

- `artwork/fonts/font-sheet.png`: the three proportional fonts (from RogueGB).
- `artwork/Fonts for GB Studio/`: the 8×8 tile font and the dialog frame.
- `tools/tile-art.mjs`: placeholder art. The pitch is drawn a pixel at a time in code
  (grass stripes, lines, boards, nets) and cut into unique tiles; the players, keepers,
  ball, shadow and marker are 16×16 and 8×8 ASCII sprites.
