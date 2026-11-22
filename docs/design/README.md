# PocketFutsal design

An arcade **futnet** game (football-tennis) for the Game Boy Color, played like Mario
Tennis: singles, top down, a net between the halves. The ball is always loose, every
touch is a strike, and the skill is in reading where the ball goes and charging the
right strike early. Proposals, not decisions: change freely.

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
| Sets to 11 by two, at most 15 (15-14); best of three sets | One set by default; SELECT on the title for the best of three |
| Serve: not in the sources | *(ours)* toss and strike from your back line; it must go over; the side that lost the point serves |
| A ball on a line: not in the sources | *(ours)* in, as in tennis |

The two touches give the game its set-up-and-attack move: reached unready, your first
touch **controls** the ball up in front of you (not over the net); ready a strike as
it comes down, and with half a meter your second touch is a **slam**, on fire.

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
  rally. It waits, and you move freely, until the ball comes your way; then it charges
  (a step every other frame, each tap of the button again adds 3, up to 5 taps) and the
  game runs you to where to meet it, half as fast again as running, anywhere in your
  half (the d-pad doesn't move you then). The strike goes by itself as the ball arrives;
  the ball's height picks kick, volley, header or bicycle kick. Strikes go at most half
  as far across as down the court (about 27 degrees).
- **Aim** (2026-11-22, from the playtest: "it doesn't feel like I can control the
  ball", then "the reticle's left but the ball goes straight"): a direction from where
  you are. Neutral goes straight on; LEFT / RIGHT angle it as far across as a strike
  goes (half as far as down the court), kept in the court; UP plays it deep, DOWN short.
  Held as it goes, or last held while queued (they stick). Queued (and serving), the
  d-pad only aims: it doesn't walk you, so aiming left never leaves you on the left
  sideline striking straight. A yellow marker on the CPU's side shows where it will land
  (lobs and drop shots too).
- **A: power**, straight where the d-pad points.
- **B: curve**: set off wide and bent back in to the side the d-pad holds.
- **B then A: lob**, dropping just behind the other player. **A then B: drop shot**,
  dying just past halfway. (Mario Tennis has these the other way round.)
- **A full meter** (on A or B, or a header) sets the ball on fire. Meeting a ball on
  fire cleanly takes 20 of the meter's 32.
- **No distance limits** (2026-11-22, from the playtest): you're run to the ball from
  anywhere in your half, to where it crosses your line if it's low enough to play
  there (worked out exactly before it bounces), else to just behind its bounce. You
  reach a body width either side (16 px), a header 35 px up (a jump), a bicycle kick 39.
  At the last moment, if a slide gets there (your reach plus a 36 px lunge, toward where
  it'll cross your line, a ball up to 23 px high), you slide for it; further, you miss.
  A slide that gets there pops the ball up, and you're down a short while; miss and
  you're down longer. (Measured with build/scratch/misses.mjs: balls getting past within
  30 px of a queued player went from 32 to 15 in seven matches.)
- **Pop-ups**: a slide, a block, or a ball on fire met with too little meter pops the
  ball up high, to drop just behind the other player. Met in the air, it's a **slam**,
  on fire. A slam met with three quarters of a meter is returned clean; less, and it
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
  readied at the last moment) is scuffed: it comes down short, on your own side.
- **Aim sticks**: LEFT or RIGHT while planted sets the aim until the strike goes.
- **Charging flashes**: your palette, white, and (from half a meter) fire, faster as the
  meter fills.
- **Shot trails** (Mario Tennis style): a struck ball streaks a trail of dots, taper
  and fading behind it: red off A (or anything on fire), blue off B. Two dots, three
  from half a meter, four on fire. Lobs, drop shots and scuffs leave none; the trail
  fades out at the bounce or the next touch.
- **Serve**: the side that lost the point serves (you first). A or B tosses it up; it's
  struck on the way down, aimed with LEFT / RIGHT, and must go over.
- **CPU**: traces the ball's flight every 8 frames (gravity and a bounce, not the curve,
  so curves fool it) to where it lands and meets it just after the bounce; it lets a
  ball that will land out go. It plants once it's in place with the ball within 96 px
  (or within 40, in place or not), taps now and then, lobs you if you're up near the
  net, drops it short if you're back on your line, curves one time in four, and aims
  away from you. Same slides, controls and pop-ups as you. A little slower than you.
- **Balance so far** (build/scratch/sim2.mjs, a bot that re-reads the ball every 12
  frames) and build/scratch/queuebot.mjs (one that queues early and taps, as you would
  now): against the queueing one, close sets, the CPU edging them (5-11 to 10-12), 6,000-
  9,000 frames (about 2 minutes). Not yet tested by hand.
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
