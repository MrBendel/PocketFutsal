# PocketFutsal design

An arcade **futnet** game (football-tennis) for the Game Boy Color, played like Mario
Tennis: singles, top down, a net between the halves. The ball is always loose, every
touch is a strike, and the skill is in reading where the ball goes and charging the
right strike early. Proposals, not decisions: change freely.

## Training mode (2026-11-22): mini games and stats

The RPG side: your player has four stats, raised by playing training mini games, and
(next) the matches read them. Mini games aren't side content: they're how you level up.

- **Stats**: POWER (how hard you strike), CONTROL (touch: first touches, juggling), AIM
  (where the ball goes), REFLEX (getting there: returns, saves). Each has a level, 1 to
  20, and XP toward the next (30 XP from 1 to 2, 15 more each level after). Your
  **player level** is 1 plus every stat level gained.
- **XP**: a mini game's score times its rate (machine 3, wall 4, keepy-uppy 2, shootout
  5), +10 for a new best, +20 for a win, at most 400 from the score; all of it to the
  game's first stat, half to its second.
- **Feeding back**: each game gets a little kinder as its stats rise (wider timing
  windows, more reach), so the levels are felt in training too.
- **The save**: battery RAM, a tagged, versioned, checksummed block; a bad or old save
  starts a fresh player.
- **The games**, from the user's ideas (2026-11-22):
  1. **Ball Machine**: a machine shoots the ball; return it in bounds.
  2. **Target Wall**: only the part of the wall that lights up scores (after Mario
     Tennis on the Game Boy Advance).
  3. **Keepy-Uppy**: juggle a consecutive number of times.
  4. **Shootout**: score on the CPU, then keep its shots out: timing, and guessing the
     direction it will shoot.

### More mini game ideas

- **Cone Slalom** (REFLEX + CONTROL): dribble through gates against the clock; touches
  too heavy run away from you.
- **Crossbar Challenge** (AIM + POWER): chip from distance to ring the crossbar; wind
  and distance grow.
- **Rondo** (CONTROL + REFLEX): keep the ball from two CPU players in a circle with
  one-touch passes to teammates.
- **Cross & Volley** (POWER + AIM): crosses come in from the wings; volley or head them
  past a keeper before they land.
- **Rhythm Footwork** (REFLEX): a rhythm game of d-pad and button taps to the beat:
  training montage.
- **Futnet Serve Targets** (AIM): serve onto targets on the far court, using the match's
  own serve.

## v0.3 (2026-11-21): futnet

Decided 2026-11-21: the game follows the rules of futnet singles (UNIF, the governing
body; nohejbal in Czech, where it began in the 1920s). The goals and boards go; the
court has lines, and a ball landing out is a fault. Sources:
[UNIF, Futnet at a Glance](https://www.futnetworld.com/futnet.html),
[Football tennis (Wikipedia)](https://en.wikipedia.org/wiki/Football_tennis). The full
UNIF laws (serve, line calls, who serves) weren't read yet: where they're silent, our
calls are marked *(ours)*.

| Futnet singles | PocketFutsal |
|---|---|
| Court 9 m x 12.8 m: each half a little wider than deep | 144 x 224 flat units (a half 144 x 112) |
| Net 1.10 m (about 0.6 of a player's height: 14 px at our scale) | 10 px, lower to play well on a small screen |
| One player, **two touches** (one after the other is fine in singles), **one bounce** | The same: `touches`, `ball_landings` per `holder` |
| Any part of the body but the hands and arms | Feet, volleys, headers, bicycle kicks |
| Faults: in the net, out of the court, touching the net, a third touch | Net, out, third touch (players can't reach the net) |
| A ball off the net can't be played again | `ball_netted`: dead, the other side's point when it lands |
| Sets to 11 by two, at most 15 (15-14); best of three sets | One set by default; LEFT/RIGHT on the title for the best of three |
| Serve: not in the sources | *(ours)* toss and strike from your back line; it must go over; the side that lost the point serves |
| A ball on a line: not in the sources | *(ours)* in, as in tennis |

The two touches give the game its set-up-and-attack move: reached unready, your first
touch **controls** the ball up in front of you (not over the net); ready a strike as
it comes down, and with two presses your second touch is a **slam**, on fire.

The HUD says how each point ended: NET!, OUT!, 2 BOUNCES!, 3 TOUCHES!, FAULT! (a serve
that didn't go over), and SET TO ... at the end of a set.

## v0.2 (2026-11-21): the tennis-style rebuild

Decided 2026-11-21: the tennis version over a Super Dodge Ball-style passing game
(which needs teammates with real AI; it could be a later mode). v0.1 was a sideways
pitch with keepers; v0.2 drops the keepers, turns the court vertical and puts you at
the bottom.

### Proportions, measured from Mario Tennis (GBC)

From the sheets in `inspiration/` (reference only, never committed or copied):

| | Mario Tennis | PocketFutsal |
|---|---|---|
| Court on screen | 154 px wide at the near baseline, 134 at the far one (x 0.87) | 144 and 125 (`PROJ_S0` 111/128) |
| Near half | 71 px tall | 64 px |
| Far half | 47 px tall (the far half is squashed: perspective) | 48 px |
| Lines | 2 px in the near half, 1 px in the far | the same |
| Background | 256 x 256 map; the camera scrolls up and down after the ball | 160 x 168 map; scrolls 32 px |
| Players | 14-16 x 23 px standing; 29 x 21 diving | 16 x 24 standing (4 sprites, 8x16 mode); 24 x 16 diving (3) |
| Views | the near player from behind, the far one from the front | the same: two sets of frames |

### Pace, measured from the Mario Tennis ROM (2026-11-22)

From the cartridge run headless in our own emulator (`inspiration/rom/`, gitignored;
scripts in build/scratch/mt/): sprite positions plus scroll, a frame at a time, and a bot
that rallies against its CPU. Only measurements were taken; nothing was copied.

| | Mario Tennis | PocketFutsal |
|---|---|---|
| A rally shot, one player to the other | 60-80 frames (about 1.1 s); the shadow 1.1-1.6 px a frame down a court about 120 px long | 53 frames median, 74 at p90 (was 33: twice as fast) |
| A serve | about 49 frames, 2.75 px a frame | |
| Running | the same speed every way (it looks half as fast up and down: the perspective) | 1.4 px a frame (`SPEED_YOU` 22 SUB) |
| Starting | a straight ramp, full speed in 7 frames | 8 frames (`ACCEL` 3) |
| Letting go | a coast of 20 frames, easing down evenly | 22 frames, about 15 px (`GRIP` 1) |
| Turning round | a skid at the coasting rate for 8-10 frames, then the full push: about 20 frames | 9 frames' skid (`SKID_FRAMES`), then `ACCEL`: about 21 |
| A trail behind the ball | 5 afterimages | 4 dots, red or blue |

So the whole ball plays at half speed: gravity 3/4 SUB a frame and every speed and lift
halved, which keeps each arc's shape and height and doubles its time. Players accelerate,
coast and turn (`approach()`). The running rows come from the velocity itself, read
from its RAM a frame at a time (build/scratch/mt/vel.mjs, 2026-11-22: from the playtest,
"a real nice loose easing ... a real momentum to moving around the field"); the first
pass, from sprite positions, had the coast at 8 frames. Each player's speed, push and
grip (`pl_top`, `pl_accel`, `pl_grip`) are their own, for upgrades later (Mario
Tennis's RPG mode raises them). The auto-run (the game running you to the ball) and the
CPU brake firmly instead (`BRAKE` 4), so they stop where they mean to; it's your own
running that glides.

The court is played in flat coordinates (`game.h`) and drawn in perspective: each depth
has a screen row and a width scale (`proj_y`, `proj_s` in `play.c`, the same formula as
`tools/tile-art.mjs`). Heights aren't scaled.

### Moves

Mario Tennis's frames per character, per view: preparing to serve (2 x 3), idle (3),
running (4 directions x 2), hitting (forehand and backhand, 3 each), dive (dive, lying,
getting up), win, lose. PocketFutsal's, per view:

| Frame | Used for |
|---|---|
| stand | idle |
| run A / run B | running |
| ready | planted, charging a strike; also tossing the serve |
| kick | a ground strike (the ball under 5 px up) |
| volley | the ball 5-15 px up: faster and flatter |
| header | the ball 16-27 px up, in front: driven down at the other half |
| bicycle | the ball 10-31 px up and just *behind* you (a lob over your head): on fire if charged half way |
| lying | (one set, both views) a slide, feet first, for a ball out of reach; knocked down |
| win / lose | the end of the match |

### Rules

Decided 2026-11-21 (the second pass): A and B are straight vs. curve, Mario Tennis's
topspin and slice turned into football; readying plants you; mashing adds power;
the game slides you to a ball out of reach; a ball only just reached pops up, to be
slammed (the Captain Tsubasa feel).

- **Queueing a strike** (2026-11-22, from the playtest: "I should be able to return the
  ball as long as I'm near it, as long as you queue it up"): A or B any time in a
  rally. **Queued, you're locked** where you stand, the d-pad only aiming (2026-11-23,
  from the playtest, replacing the game running you to the ball: "player positions
  himself after kick to where they think the ball is landing next, they press a key to
  power up A or B or both. The player is locked, when they're near the ball at return
  distance they either return or slide and return or if they're too far they miss").
  Getting there is yours, with the d-pad; only a ball loose in your half walks you to
  it. **The lock starts as the ball comes over the net** into your half (2026-11-23,
  playtest: "I keep getting stuck from the power up before I think I should"): queued
  while it's still over their side, you still run, and the meter fills as you press;
  over the net, you're planted and the d-pad aims. (A bot that presses as the CPU
  strikes and then runs there wins 63% of points; it had been stuck where it pressed.) The CPU plays by the same lock: it gets there, then readies
  (or readies anyway with the ball 40 px off), its meter starting at 8 (two of your
  presses) and filling a point every 4 frames. The strike goes by itself as the ball arrives;
  the ball's height picks kick, volley, header or bicycle kick. Strikes go at most half
  as far across as down the court (about 27 degrees). You only play the ball over your
  own half: no reaching over the net.
- **The meter fills only as you press** (2026-11-22, from the playtest: "repeatedly
  pressing A should fill the gauge, not holding it, and it shouldn't go down"): each
  press of A or B adds 4, the first too, as many as you like; waiting and holding add
  nothing, and it never drains or gives up. SELECT calls the strike off. The CPU's
  still fills as it waits (a step every 4 frames, and up to 5 taps of 3) and gives up
  a strike held full for 90 frames. Meeting a ball on fire cleanly takes 8 (two
  presses; the CPU 12); less, and it pops up off you.
- **Aim** (2026-11-22, from the playtest: "it doesn't feel like I can control the
  ball", then "the reticle's left but the ball goes straight"): a direction from where
  you are. LEFT / RIGHT angle it as far across as a strike goes (half as far as down
  the court), kept in the court; UP plays it deep, DOWN short. Neutral goes away from
  the CPU (from the playtest: "no matter what, I always hit the ball to them"): half as
  far across, to the side it isn't on, and in depth short of it when it's back by its
  baseline (40 px past the net at most), past it when it's up near the net. Headers and
  bicycle kicks are aimed the same way (they used to land at a fixed depth).
  Held as it goes, or last held while queued (they stick). Queued (and serving), the
  d-pad only aims: it doesn't walk you, so aiming left never leaves you on the left
  sideline striking straight. A yellow marker on the CPU's side shows where it will land
  (lobs and drop shots too).
- **A: power**, straight where the d-pad points.
- **B: curve**: set off wide and bent back in to the side the d-pad holds.
- **B then A: lob**, over the other player and deep: 24 px behind them, at least 60
  past the net (a lob at a player up at the net sends them back for it). **A then B: drop shot**,
  dying just past halfway. (Mario Tennis has these the other way round.) The second
  button arms it for 24 frames (about 0.4 s), so press it just before the ball arrives;
  struck later it's the curve or the power shot again (2026-11-22, from the playtest:
  "every shot I shoot is a lob"). The first button again taps the charge up.
- **A full meter** (on A or B, or a header) sets the ball on fire. Meeting a ball on
  fire cleanly takes 20 of the meter's 32.
- **The power meter and the power slam** (2026-11-23, playtest: "fill it up for successful
  rallies. When it's full you can press A + B to unleash a powerful slam that requires
  super precise timing to hit"). Each clean strike in a rally adds 1 to pl_power (64
  full: about one power slam a side every five points in the bot matches); the white
  bar under your charge on the HUD, flashing when full; a ! by CPU when its is. Full,
  A + B together (one pressed, the other held) arms it, SP! on the HUD; the next clean
  strike is the power slam: a gold bicycle kick at speed 64 (a gold slam is 56), deep.
  Reaching the other player it knocks them down unless their first press since it was
  struck came within 5 frames of it reaching them (one try: no mashing), when they
  send it back on fire. The CPU arms it one time in two as it readies with a full
  meter, and times the return 2 times in 8; the player AI one in two or so (it times
  its press, 0-7 frames out). Later: equippable power slams (pl_special holds which;
  fc_gold_speed already takes the speed).
- **The AI on your side** (2026-11-23): SELECT on the title (Futnet) switches YOU PLAY / AI PLAYS.
  The AI presses your buttons and nothing else (play.c ai_input): 12-27 frames after the
  CPU strikes it picks where it will meet the ball, walks there, presses A (B one time in
  four) then once or twice more, holding the d-pad away from the CPU. A fairness check:
  over four seeds it wins 65% of points, median point 45 s (build/scratch/aimatch.mjs).
- **Return, slide and return, or miss** (2026-11-23): you reach a body width either
  side (16 px), a header 35 px up (a jump), a bicycle kick 39. A ball going past out of
  reach (not going out, up to 23 px high), you always slide for it, toward where it'll
  cross your line, at the last moment (playtest: "the player should always make the
  attempt even if they're too far... err on the side of helping the player"). What the
  ball does is by the slide: got there in the lunge's first 10 frames (up to 46 px
  across), you return it with the strike you readied; in its last 5 (to 61 px), at full
  stretch, it pops up; further, you miss and you're down longer. On fire without the
  meter to meet it, you're knocked down. (Before, the game ran you to the
  ball from anywhere in your half, and a slide only popped it up.) (Measured with build/scratch/misses.mjs: balls getting past within
  30 px of a queued player went from 32 to 15 in seven matches.)
- **Slam spot** (2026-11-22, from the playtest: "you should be able to knock lobs out
  of the air"; Mario Tennis shows a flashing circle): a lob or a pop-up coming down over
  your half has a flashing marker where it drops to 26 px up. Get there (a charged slam
  leaps up to 28 px across to it); met in the air with two presses (8; the CPU half a meter) it's a **slam**, on fire, deep; with less, a
  header, still out of the air. A drop shot gets the marker too, off its bounce as well,
  and hops higher (about 11 px). The CPU taps hard against a ball on fire, so a slam can
  be met clean.
- **Slams by the button** (2026-11-22, from the playtest: "if it's A then it's a red
  slam, if it's B it's a blue slam, and if it's A+B it's a gold slam"): every slam is a
  jump into a bicycle kick. A: red, straight (48). B: blue, bending the curve's way, as
  fast. Both (the other button while the slam spot shows, or both at once): **GLD** on
  the HUD, gold, straight and faster (56), its trail in the fire's colors; only a full
  meter meets a gold slam clean, less pops it up (a slam back, if you're ready). A lob or
  drop shot queued slams red. Gold is yours: the CPU slams red or blue.
- **Bursts** (2026-11-22): an impact star where a slam is met (big) or a strike goes on
  fire (small), dust where a slide or a knockdown starts, sparks off the net. From the
  BURST_VFX pack (a paid pack: the PNGs stay out of the repo; tools/export-vfx.mjs turns
  the chosen sheets into src/vfx_tiles.c, which is committed). One at a time, sprites
  20-27, a row of sprites placed a frame at most (src/vfx.c).
- **Pop-ups**: a slide at full stretch, a block, or a ball on fire met with too little meter pops the
  ball up high, to drop just behind the other player. Met in the air, it's a **slam**,
  on fire. A slam met with three eighths of a meter (a press and a moment) is returned clean; less, and it
  pops up again; slide to it, or meet it unready, and you're knocked down and it goes on.
- **Unready**, your first touch controls it (v0.3); a ball on fire knocks you down.
- **Bounces** (2026-11-22, from the playtest: some returns died where they landed): a
  bounce keeps three quarters of its height and the court takes a quarter of its pace;
  the first bounce in a half always gets up 9 px (a futnet ball bounces over half a
  metre), a drop shot's 6 px, and a **lob sits up**: slowed to a quarter, a 16 px bounce.
- **The arc** (2026-11-21, after the first playtest: the ball went dead in play): every
  strike arcs, to come down at a depth set by its charge (deeper the more charged), so
  the ball is in the air with its shadow under it, and it's lifted to clear the net
  (fast flat strikes land deeper, or long). A strike with almost no meter (under 4:
  readied at the last moment) is scuffed: it comes down short, on your own side (MIS on
  the HUD). Drives, curves and headers arc for at most 60 frames (about 22 px up): a
  weak one is sped up instead of looping up like a pop-up; drop shots, 100.
- **The CPU under pressure** (2026-11-22, from "how can we improve playability": points
  went on and on, a player who didn't aim getting about 70 touches a point): the CPU,
  meeting your shot, may mishit it. Out of 64: a quarter of how far it ran for it (from
  where it stood as you struck it) plus half the shot's speed, past 30. It runs about
  74 px for a shot at the median, at speed 28: no chance; a wide fast one (100 px, 40)
  about a quarter. Mostly it pops up (a slam for you); a quarter of the time it's scuffed
  into its own half. Aiming bots now win their points in about 15 s, 11-8 or 9-6.
- **Cues** (the same playtest): the CPU's struck it to you and nothing's queued, your
  player blinks white and the HUD says **GO!** (a fire shot met unready knocks you down:
  the commonest lost point left). Each press of A or B ticks; two presses (a slam
  ready) and full have their own chimes (SFX_TICK, SFX_HALF, SFX_CHARGE). The slam spot
  is a 16x8 ring (two sprites, S_RING at tile 28, after the ball's spin tiles).
- **From two recorded matches** (2026-11-22, played on a phone, lost 3-11 and 1-11;
  tools/match-report.mjs replays a recording and breaks it down): you press 2 or 3 times
  before the ball arrives (first press about 35 frames after the CPU's strike, the ball
  there at about 55), so the meter sat at 4-8. At 12 to meet fire, a third of your
  touches popped up, and the CPU slammed them past you (half the points lost). Of 25
  slam chances you reached the ring for 9 and slammed 2: the rest were headers, short
  of half a meter. Three shots went out by the left sideline, aimed 6 px inside it: a
  long kick lands up to 10 px past its aim. So: two presses meet fire (FIRE_MEET_YOU 8)
  and slam (SLAM_CHARGE 8; the chime moved there), and aim stays 16 px inside the lines
  (AIM_IN). The CPU's thresholds are unchanged.
- **Deep by the button, a slam zone, the ball's spin** (2026-11-22, from the playtest:
  "near impossible to get the lob shots before they landed in the zone", "it always
  seems to shoot the balls short... A shots the deepest where B shots should aim to
  curve mid field", "the ball always rotates the same direction"):
  - Your A lands 64-80 px past the net by the meter (UP 16 deeper, to 96; DOWN 24
    shorter); B 48-56, mid-court. The CPU's depths are unchanged.
  - Charged for a slam (two presses), you leap at a lob or a pop-up coming down from
    up to 28 px across and 20 in depth (SLAM_LEAP_X, SLAM_LEAP_FWD; a strike reaches
    16 and 4-8), moving up to 12 px, and strike it the next frame (a leap and a strike
    in one frame was a match's busiest frame).
  - Shots landed long: arc() sped up a slow shot with >> on a negative, which rounds
    away from zero, so your shots (going up the court) went a tenth faster than their
    time in the air allowed: serves 20 px long. Sped up as a size now, the time cut by
    103/128 a step and z0 / t from a table: within 6 px, no division.
  - The ball's frames roll its face down the screen (toward you); going up it, they
    play backwards.
- **The slam zone** (2026-11-22, from the playtest: "something bigger that acts like a
  target to say get here now", then "we don't need the arrow"): round the slam spot's
  ring, 8 dots (sprites 29-36, tile 30, set once a match: a frame only moves them) as
  far out as your leap reaches while the ball's 64 px or more from the spot in depth,
  closing on the ring as it comes: closed, it's there. A drop shot's marker has none.
- **Home between shots** (2026-11-22, from the playtest: "I keep losing 2 or 3 to 11"):
  with nothing queued, the d-pad let go and the ball going away, you jog back to the
  middle, 196 deep (the CPU's home, mirrored). A bot that plays like a person (it reacts
  10-20 frames after the CPU's touch and presses three times; build/scratch/humanbot.mjs)
  was within 16 px of the net for three quarters of the CPU's strikes: the auto-run only
  ever takes you toward the ball, so a rally crept you up to the net, where lobs and
  drives went over your head. Half its lost points were its own volleys, out: struck
  right at the net, at its height, the net check (`net_clear`) asked for the clearance in
  one frame and launched the ball 111 px up and long. It now allows at least 6 frames.
  The bot went from losing every game to winning 31 points to 15.
- **Aim sticks**: LEFT or RIGHT while planted sets the aim until the strike goes.
- **Charging flashes**: your palette, white, and (from half a meter) fire, faster as the
  meter fills.
- **Shot trails** (Mario Tennis style): a struck ball streaks a trail of dots, taper
  and fading behind it: red off A (or anything on fire), blue off B. Two dots, three
  from half a meter, four on fire. Lobs, drop shots and scuffs leave none; the trail
  fades out at the bounce or the next touch.
- **Shot label** (2026-11-22, a debugging aid: "I'm hitting lobs when I mean curves"): the
  HUD says your shot where it says YOU: PWR, CRV, LOB or DRP while queued, then for a
  second what went (HDR and BIK go straight, SLM a slam, POP a pop-up, CTL a control).
- **Serve**: the side that lost the point serves (you first). A or B tosses it up; it's
  struck on the way down, aimed with LEFT / RIGHT, and must go over.
- **CPU**: traces the ball's flight 12 frames a step (gravity and a bounce, not the curve,
  so curves fool it) to where it lands and meets it just after the bounce; it lets a
  ball that will land out go. It plants once it's in place with the ball within 96 px
  (or within 40, in place or not), taps now and then (every 16 frames, 3 times in 4), lobs you if you're up near the
  net, drops it short if you're back on your line, curves one time in four, and aims
  away from you. Same slides, controls and pop-ups as you. A little slower than you.
- **Balance so far** (build/scratch/sim2.mjs, a bot that re-reads the ball every 12
  frames) and build/scratch/queuebot.mjs (one that queues early and taps, as you would
  now): against the queueing one, close sets, the CPU edging them (5-11 to 10-12), 6,000-
  9,000 frames (about 2 minutes). At half speed (2026-11-22): closer still (1-6, 4-5,
  5-5, 3-4, 7-6 after 15,000 frames each), rallies about 13 crossings. Not yet tested
  by hand.
- **Frame budget**: the busiest frame is right at half a frame (the tests' line, kept as
  headroom); the next per-frame feature needs room made first (`node harness/profile.mjs busy`).

## v0.1 (2026-11-21): first playable (replaced)

A sideways one-screen pitch, each side a player and an automatic keeper, first to five
goals. The keepers and the sideways view went in v0.2.

## Ideas for next

- **Characters** with stats (speed, power, curve) and a special strike each, Mario
  Tennis's roster idea, original characters.
- **Curve**: holding LEFT/RIGHT across the strike bends it.
- **Special meter**: well-timed strikes fill a special bar.
- **2-on-2 / Super Dodge Ball mode**: passing and positioning, once the strikes feel good.
- **Match timer**, a tournament ladder against harder CPUs, **link cable** versus.
- **Doubles and triples** (futnet has both: three touches a side, not two in a row by
  one player).
- Real art: more frames (serve, run sideways, get up, a control touch), a crowd.
- **The name**: PocketFutsal is now a futnet game.
