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

UP/DOWN chooses **Futnet Singles**, **2v2 Futsal**, or **Mini Games**.
For futsal, LEFT/RIGHT chooses **Casual / Normal / Expert** pace and SELECT
switches **Watch / Play** independently. START/A launches. Futnet's LEFT/RIGHT
changes its match length, and SELECT switches **You play / AI plays**: the AI
plays your side with your buttons, to watch how fair the match is.

Futsal defaults to **Casual: 75%** of the original match speed. **Normal: 87.5%**
is the middle setting; **Expert: 100%** preserves the original pace. This scales
players, the ball, action windows and animations together, giving more time to
read passes and react without changing the physical relationships or AI tactics.
These are pace settings, independent of future AI skill tiers. Quick A/B taps
and their aim are buffered across held simulation frames; pause stays immediate.

Futsal is an arcade prototype: two outfield players per team, no dedicated
goalkeepers, first to five goals. The court has goals at both baselines and
rebounding walls outside the goal mouths. Both ends now use the same 40x24
dimensional portable-goal artwork and matching collision dimensions. Posts and the crossbar
rebound the ball and knock the goal out of position. A short FRAME! stoppage
lets the rebound and displacement read, then resets the goal and gives the
defending team the restart. A frame hit never counts as a goal. Goals are drawn with background tiles so
the ball, players, shadows and effects cannot push their net/frame out of the
Game Boy's ten-sprites-per-scanline list. Their collision and displacement
remain separate from the renderer.

A stadium stand and centered scoreboard sit behind the far goal. The board is
part of the scrolling background and uses the original Pocket Sports numerals
in muted gold. Each score changes through a queued six-tile update; the main
score/control strip stays separate. The extra 32 pixels of stadium headroom
leave court physics unchanged. Sources are `tools/stadium-art.mjs` and
`artwork/scoreboard/v3`.

The lob landing ring reserves the first two sprite slots so its complete
outline survives crowded scanlines. Players and the ball retain their depth
order behind the targeting cue; shadows, trails and contact effects follow.

At full time, all four players have win/lose reactions: one fist lifted and a
short up/down jump for the winning pair, a duck and head shake for the losing
pair. The original faces and outfits stay intact. Three frames per
outcome loop on the results screen, with fixed ground shadows. The native art
and animated study are in `artwork/players/roster/reactions`.

Shooting precision depends on distance: close finishes have very little scatter,
medium shots spread wider, and efforts from more than 144 court pixels away are
wild clearances aimed outside the goal mouth. Missed deliveries shed speed on
landing so an unreceived pass does not keep rolling into a distant goal.
The active goal artwork is in `artwork/goals/v3`: the approved generated study
reduced to native pixels, with a sparse recessed net and roof/side depth.

WATCH runs four AI players; PLAY gives
you fixed control of blue #1 (the overhead marker). SELECT toggles Watch/Play
in a futsal match; START pauses.

With possession, A shoots; tap B for a quick pass, or hold B for at least
12 simulation ticks (about 0.27 seconds on Casual, 0.2 on Expert) and release
for a high lob. Holding a lob slows the carrier
and flashes their crouch when ready. Without possession, A slide tackles and
B bursts; both defensive actions have a cooldown. When your teammate carries,
tap B for a quick return, or hold/release B to call a lob into your forward run.
D-pad moves and left/right aims shots. AI positioning and
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

Teammates offer a wide diagonal outlet about 24 court pixels ahead, hold their
lane through central dribbles, and continue their run after passing.
Neutral B leads moving receivers; **D-pad + B** sends your teammate 24 court
pixels into that direction to receive in space. Try a one-two: **B to pass, run
into space, B to call for the return, then A to finish**. You can call while your
first pass is still airborne: your teammate returns it on receiving. The call
expires after 48 frames (80 for a lob) or when an opponent takes possession.
**Crossover:** while dribbling, cut left-right-left (or right-left-right).
Let your body actually change direction between cuts; two reversals within
28 frames can wrong-foot a presser within 28 court pixels who is still moving
into your feint. They skid, crouch, and recover over 22 frames, opening a pass
or shot. Stationary button spam, a single cut, and distant or balanced defenders
do not trigger it. A 90-frame cooldown prevents repeated stuns; the ball keeps
its normal independent movement, so an uncontrolled cut can still lose it.

Only one defender presses at a time. Their partner covers the other attacker
from the goal side and keeps a separate lane when attackers bunch up. The press
changes hands only when the partner is clearly closer, so a small juke does not
send both defenders after the ball. Defenders have a brief reaction delay and
commit to tackles, so changing direction can create an opening.
The perception schedule gets a seeded phase at each kickoff instead of always
giving the same player slots the same opening. At most one AI makes a full
tactical decision per frame; all four players still move every frame. This
breaks the repeated red counterattack without changing either team's stats.
Run `node tools/futsal-balance.mjs 16` for a reproducible Watch-mode audit of
wins, goals, shot opportunities, possession and the full-match frame budget.

A high lob marks its landing zone with a gold ellipse. Get into the zone; as
the ball descends the ellipse pulses orange. Press A to jump for an aerial
finish, with a bicycle kick when the delivery is behind you. The jump winds
up before contact, keeps the ground shadow fixed and recovers afterward.
It only shoots if the descending ball actually reaches you in the air.
A recent defensive burst does not block this jump, so you can sprint into
position. Try **tap B, run forward, hold/release B for the lob return, then A
in the pulsing zone**. AI receivers can attempt aerial finishes too.

Shot contact now uses the small hit burst from `artwork/BURST_VFX`: a brief
flash for a normal shot, longer for an aerial finish. It stays at the contact
point and respects the camera and pause. Misses and regular passes do not
trigger it. `tools/export-futsal-fx.mjs` preserves the pack pixels and uses
hardware flips to fit four frames into eight tiles (242..249), with two
effect sprites (33/34). The purchased source pack remains ignored; converted
Game Boy tile data is committed so builds do not require the source pack.

Both modes use dedicated left/right side-running sprites; all four futsal
characters retain their individual profile heads. The animated sprite review
is at `artwork/players/roster/side-review.html`.

The original futnet mode and its tests remain active. Futsal logic and watchable
state live in `src/futsal.c` and `src/futsal.h`, with `fs_` prefixes for its state.
Both modes share the graphics, sprite assets, build and emulator tooling.

## Training: mini games and your player's stats

The title's third mode, **Training**, opens a hub: your **player level**, four stats
(**POWER**, **CONTROL**, **AIM**, **REFLEX**) each with a level and an XP bar, and the
mini games with your best in each. UP/DOWN chooses, A plays, B goes back.

| Mini game | The idea | Trains |
|---|---|---|
| **Ball Machine** | A machine fires balls over the net; return them inside the lines. Three misses and you're out | REFLEX + CONTROL |
| **Target Wall** | Hit the ball at a wall: only the panel that lights up scores. Against the clock | AIM + POWER |
| **Keepy-Uppy** | Juggle: time A to your foot (B to your head) as it drops. How many in a row? | CONTROL + REFLEX |
| **Shootout** | Five penalties each: aim and time your shot, then guess the CPU's side and dive | POWER + REFLEX |
| **Cone Slalom** | Dribble through timed cone gates; heavy touches knock the ball loose | REFLEX + CONTROL |

Every game runs through one shell (`src/minigame.c`, the contract in `src/minigame.h`): a
brief card, READY / GO!, START to pause (SELECT, paused, quits without XP), then a
results card with the score, your best and the XP won. A game's score is its XP (times
a per-game rate, plus bonuses for a new best or a win): all of it to the first stat it
trains, half to the second. Levels go to 20; the save is in battery RAM
(`src/progress.c`), so it survives power off. The match modes can read `stat_lv[]`
(`src/progress.h`) to make your player feel the levels.

Each game is `src/mg_<name>.c`, its art `tools/art/mg_<name>.mjs` and its tests
`tests/mg_<name>.test.mjs`; the shell's own tests are in `tests/minigames.test.mjs`.

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
or a third touch, and it's theirs. Sets are first to 11 by two (15 at most); LEFT/RIGHT on
the title switch between one set and the best of three.

| Button | What it does |
|---|---|
| D-pad | Run (eight ways) in your half. Ready (from the press), or serving: it only aims |
| A | Ready a **power shot**, straight. Serving: toss the ball up |
| B | Ready a **curve**, bending in to the side you hold |
| A, B / B, A in a quick roll | A **lob** over them / a **drop shot** just past halfway: the second button within a moment (12 frames) of the first; it stays one till it's struck |
| A flashing marker | A lob or pop-up coming to you: three tight rings round the spot, the light chasing in to it, quicker as it comes down (a drop shot: one small ring). Be there (queued, you're run there) and mash: with four presses you jump and bicycle-kick it, a **slam**, the fastest ball in the game (a lob floats down slowly to give you time). A for a red one (straight), B for blue (bending), both buttons for **gold** (faster; only a full meter meets it clean) |
| Up at the net, mash | A ball of the CPU's coming over high (14 px up or more) while you're within 30 px of the net: four presses and you leap and bicycle-kick it down, a **smash**: at a slam's speed, steep and short, kicking up high off its bounce. Like gold, only a full meter meets it clean: the CPU fends it up for you to slam |
| A or B again while ready | Tap for more power: the meter fills only as you press, and stays. The ball's speed follows it: a drive, then on fire (eight presses, a red trail), then a slam; you flash from half a meter |
| A + B, the power meter full | The **power slam**: your next clean strike goes gold and fast. Each clean strike in a rally fills the meter a little: four pips show under your feet for a second as it does, each one filled with a blip a note higher. Full, the pips stay, your outline glows gold and "A+B" shows over you the first time; armed, it all blinks faster. The CPU's is a bar under its feet, flashing when it's full |
| A or B as a power slam reaches you | Your one try: within 5 frames of it reaching you, you send it back on fire; too early, too late or never, it knocks you down |
| The other button while ready (or SELECT) | Call the strike off: readied with A, B cancels, and the other way round |
| LEFT / RIGHT while ready | Angle it that way; neutral goes half as far, away from the CPU (it sticks; a marker shows where it'll land) |
| UP / DOWN while ready | Play it deep / short; neutral, away from the CPU: short of it at the back, past it up close |
| START | Pause |

Press A or B to queue a strike, any time in a rally: each press fills the meter (you
flash brighter; holding doesn't, and it doesn't drain); once the ball comes your way, the game runs you
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
| `src/play.c` | The match: the perspective, players, strikes, the CPU, the ball's physics, the futnet rules and scoring |
| `src/play_hud.c` | The score: the board in the court art, the box with the faces before each serve, the banner for a point's call, the power gauges |
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

**No HUD row in a match.** The court (160×168, scrolling up and down after the ball,
`cam_y`) has the whole screen. The score is on a board in the court's art, above the far
end, its digits swapped through the vblank queue (as Mario Tennis has it). The window
layer is an overlay along the bottom only for a moment: the score box with each
player's face before each serve, a banner for a point's call or PAUSED (`play_hud.c`).
The hardware draws the window to the bottom of the screen for free; at the top it would
need a scanline interrupt every frame. Other scenes keep a HUD row at `HUD_WY` (`gfx.h`).

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
