# PocketFutsal design

An arcade futsal game for the Game Boy Color, played like Mario Tennis: 1-on-1, top
down, each player keeping to their own half and defending the goal behind them. The
ball is always loose, every touch is a strike, and the skill is in reading where the
ball goes and charging the right strike early. It's less about scoring goals than
getting the ball past the other player. Proposals, not decisions: change freely.

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

- **Readying a strike** is only possible with the ball coming your way (or loose in
  your half), and plants you until it goes. The meter fills as you wait (a step every
  other frame) and each tap of the button again adds 3, up to 8 taps. The strike goes
  by itself as the ball arrives; the ball's height picks kick, volley, header or
  bicycle kick. Plant too early in the wrong place and you'll be sliding for it.
- **A: power**, straight where the d-pad points (LEFT / RIGHT: that corner of the goal).
- **B: curve**: set off wide and bent back in to the side the d-pad holds.
- **B then A: lob**, dropping just behind the other player. **A then B: drop shot**,
  dying just past halfway. (Mario Tennis has these the other way round.)
- **A full meter** (on A or B) sets the ball on fire. Meeting a ball on fire cleanly
  takes three quarters of a meter.
- **Out of reach, planted**: you slide for it (24 px either side, more than your reach
  of 10). Get there and it pops up, and you're down a short while; miss and you're down
  longer.
- **Pop-ups**: a slide, a block, or a ball on fire met with too little meter pops the
  ball up high, to drop just behind the other player. Met in the air, it's a **slam**,
  on fire. A slam met with three quarters of a meter is returned clean; less, and it
  pops up again; slide to it, or meet it unready, and you're knocked down and it goes on.
- **Unready**, the ball pops up off you (a ball on fire knocks you down).
- **The net and the bounce** (decided 2026-11-21, after the first playtest: the ball
  went dead in play): tennis rules over a low net (futnet, football-tennis, is a real
  sport). A 6 px net across halfway. Every strike arcs, to come down at a depth set by
  its charge (deeper the more charged), so the ball is in the air with its shadow under
  it. Landing first on the striker's own side (scuffed, or off the net) is the other
  side's point; a second bounce, or a ball that dies, is the striker's. A strike with
  almost no meter (under 4: readied at the last moment) is scuffed into the net.
- **The court**: each goal is two thirds of its back line, and into it is a point
  straight away; boards round the rest, and above the crossbar.
- **Aim sticks**: LEFT or RIGHT while planted sets the aim until the strike goes.
- **Charging flashes**: your palette, white, and (from half a meter) fire, faster as the
  meter fills.
- **Serve**: the side that conceded serves (you first). A or B tosses it up; it's
  struck on the way down, aimed with LEFT / RIGHT.
- **Scoring**: a goal is a point. First to 7, by 2.
- **CPU**: works out where the ball will cross its line (every 8 frames, in a straight
  line, so curves fool it), goes there, and plants once it's in place with the ball
  within 72 px (or at the last moment). It taps now and then, lobs you if you're up near
  halfway, drops it short if you're back on your line, curves one time in four, and aims
  away from you. Same slides and pop-ups as you. A little slower than you.
- **Balance so far** (build/scratch/sim2.mjs, a bot that re-reads the ball every 12
  frames): with the net, points take 400-600 frames, most won on a second bounce; the
  CPU beats the bot (which is slow to get back for deep balls).

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
- Real art: more frames (serve, run sideways, get up), a crowd, goal nets that bulge.
