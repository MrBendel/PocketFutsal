# PocketFutsal

A Game Boy Color arcade **futnet** game (football-tennis) in the spirit of Mario
Tennis: singles against the CPU on a court seen in perspective, a net between you,
strikes you charge by pressing early. Built in C
with [GBDK-2020](https://gbdk.org/) 4.5.0 on the engine from
[RogueGB](https://github.com/MrBendel/RogueGB) (by way of Danger Noodle): its build,
VRAM queue, text, fades, sound engine, headless emulator harness, tests and playtest page.

**Status: v0.3, futnet rules.** One court, one CPU opponent, one set to 11 (or the
best of three). Proportions measured from Mario Tennis (GBC), rules from futnet: see
the design doc.
The design is in [docs/design/README.md](docs/design/README.md).

## Main menu and 2-on-2 futsal

UP/DOWN chooses **Futnet Singles** or **2v2 Futsal**. LEFT/RIGHT or SELECT
changes options: futnet match length, or futsal Watch/Play. START/A launches.

Futsal is an arcade prototype: two outfield players per team, no dedicated
goalkeepers, first to five goals. The court has goals at both baselines and
rebounding walls outside the goal mouths. WATCH runs four AI players; PLAY gives
you fixed control of blue #1 (the overhead marker). SELECT toggles Watch/Play
in a futsal match; START pauses.

With possession, A shoots and B passes. Without possession, A slide tackles and
B bursts; both defensive actions have a cooldown. When your teammate carries,
B calls for a pass. D-pad moves and left/right aims shots. AI positioning and
match balance still need playtesting.

The 2v2 gameplay pass adds acceleration, braking and momentum when reversing.
A slide has a short crouch, then commits feet-first to its launch direction;
it steals only when the foot reaches a low ball. Passes and shots travel on
ballistic arcs, with a shadow marking their ground position. High deliveries
clear ground tackles, and a receiver controls the ball as it descends.
Ball flight uses the same vector launch and gravity model as Futnet: precise
angles toward the target, quick low exchanges over short distances, and higher
arcs on longer deliveries. Shots travel faster than passes. Bounces lose pace
and height.
Dribbling uses separate foot touches: the ball rolls ahead and slows under its
own momentum between contacts. Stopping does not pull it back to the player.
A hard cut can leave it exposed or loose, and opponents can intercept the ball
without touching the carrier. Pass/shot inputs wait briefly for kicking reach;
steer back onto the ball to play it. AI carriers also recover their next touch.

Teammates run ahead into the opposite lane and continue their run after passing.
Neutral B leads moving receivers; **D-pad + B** sends your teammate 24 court
pixels into that direction to receive in space. Try a one-two: **B to pass, run
into space, B to call for the return, then A to finish**. You can call while your
first pass is still airborne: your teammate returns it on receiving. The call
expires after 48 frames or when an opponent takes possession.
AI defenders have a brief reaction delay
and commit to tackles, so changing direction can create an opening.

Both modes use dedicated left/right side-running sprites; all four futsal
characters retain their individual profile heads. The animated sprite review
is at `artwork/players/roster/side-review.html`.

The original futnet mode and its tests remain active. Futsal logic and watchable
state live in `src/futsal.c` and `src/futsal.h`, with `fs_` prefixes for its state.
Both modes share the graphics, sprite assets, build and emulator tooling.

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

You're blue, at the bottom; the CPU is red, at the top; a net between. Futnet singles:
two touches a side and one bounce, anything but your hands. Get the ball over the net
and in (a line is in); a second bounce on their side is your point. Into the net, out,
or a third touch, and it's theirs. Sets are first to 11 by two (15 at most); SELECT on
the title switches between one set and the best of three.

| Button | What it does |
|---|---|
| D-pad | Run (eight ways) in your half. Queued, or serving: it only aims |
| A | Ready a **power shot**, straight. Serving: toss the ball up |
| B | Ready a **curve**, bending in to the side you hold |
| B then A / A then B | A **lob** over them / a **drop shot** just past halfway: press the second just before the ball arrives |
| A flashing marker | A lob, pop-up or drop shot coming to you: be there (queued, you're run there) and slam it out of the air |
| A or B again while ready | Tap for more power: the meter fills only as you press, and stays |
| SELECT while ready | Call the strike off |
| LEFT / RIGHT while ready | Angle it that way; neutral goes half as far, away from the CPU (it sticks; a marker shows where it'll land) |
| UP / DOWN while ready | Play it deep / short; neutral, away from the CPU: short of it at the back, past it up close |
| START | Pause |

Press A or B to queue a strike, any time in a rally: each press fills the meter in the
HUD (holding doesn't, and it doesn't drain); once the ball comes your way, the game runs you
to it, then it goes by itself as the ball arrives. The ball's height picks the strike: a **kick** on the ground, a **volley** at
the waist, a **header**, or a **bicycle kick** for one that's just gone over your head.
Not ready when it reaches you, your first touch **controls** it, up in front of you:
ready a strike as it comes down, and with half a meter your second touch is a **slam**.
Planted out of reach, you **slide** for it: it pops up over the net, and you're on the
floor a moment. A pop-up met in the air is a slam too, on fire, as is a full-meter strike;
meeting a ball on fire takes three eighths of a meter, or it pops up off you (or knocks
you down, if you slide to it or aren't ready). The side that lost the point serves: the ball is
struck on the way down, and must go over.

## Code structure

| File | What it does |
|---|---|
| `src/main.c` | Boot and the main loop. One scene update per frame; scene changes fade out, `enter()` the new scene, fade in |
| `src/state.h` | Scene IDs (`STATE_TITLE`, `STATE_PLAY`, `STATE_GAME_OVER`), `state_change()`, `wait_frame()` |
| `src/game.h` | The match's watchable state, the court's measurements and its perspective |
| `src/play.c` | The match: the perspective, players, strikes, the CPU, the ball's physics, the futnet rules and scoring, the HUD |
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
| `tools/tile-art.mjs` | Placeholder art: the court drawn in perspective in code (cut into unique tiles and a map), ASCII sprites |
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
over the bottom of a 160×168 court that scrolls up and down after the ball (`cam_y`). The hardware draws the window to the bottom of the screen
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
- `tools/tile-art.mjs`: placeholder art. The court is drawn a pixel at a time in code,
  in perspective (bands, lines, the net, boards), and cut into unique tiles. The player
  is a generic 16×24 ASCII sprite, Mario Tennis's size, in two views (from behind for
  you, from the front for the CPU) with ten poses each, plus a dive; they load into
  VRAM bank 1. The ball and its shadow are 8×8.
- `inspiration/` (not in the repo): Mario Tennis sheets, for proportions only.
