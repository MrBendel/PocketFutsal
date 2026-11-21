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
| ready | crouched, charging a strike; also tossing the serve |
| kick | a ground strike (the ball under 5 px up) |
| volley | the ball 5-15 px up: faster and flatter |
| header | the ball 16-27 px up, in front: knocked down at the other half |
| bicycle | the ball 10-31 px up and just *behind* you (a lob over your head): a power strike if charged half way |
| dive | (one set, both views) stretching for a ball out of reach; then lying there a moment |
| win / lose | the end of the match |

### Rules

- **Strikes, Mario Tennis style.** A readies a driven strike, B a chip (a lob). It
  charges until the ball arrives and fires by itself in reach; which strike depends on
  the ball's height. Charging slows you to a shuffle. LEFT / RIGHT as it goes aims at
  that corner of the goal. A full meter on a drive is a **power shot**: on fire, and a
  player who meets it without at least half a meter is knocked down.
- **Dive**: A or B with the ball just out of reach, coming past you: a lunge sideways.
  Catch it and it's a weak return; miss and you're on the floor for a moment.
- **Without a strike ready**, the ball bounces off you.
- **The court**: each goal is two thirds of its back line; boards round the rest, and
  above the crossbar, so the ball only stops for a goal.
- **Serve**: the side that conceded serves (you first). A or B tosses it up; it's
  struck on the way down, aimed with LEFT / RIGHT.
- **Scoring**: a goal is a point. First to 7, by 2.
- **CPU**: works out where the ball will cross its line (every 8 frames), goes there,
  readies a strike as it comes (a chip one time in four), and aims away from you. It
  dives for balls it can't reach. It's a little slower than you.

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
