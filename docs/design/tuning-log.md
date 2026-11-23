# Overnight tuning log

## Morning summary

The loop stopped at about 23:30 (two rounds running with nothing worth keeping):
two changes kept, both worth feeling by hand.

- **1. You run to the ball on your own** (bec60b7). With nothing queued and the
  d-pad let go, you head for where you'll meet a ball coming to you; a press only
  readies the strike; the d-pad still overrides. The late-pressing bot went from
  12% of points to 65%: it had been lunging at balls it reached before its first
  press, popping them up for the CPU to slam.
- **2. Side to side costs you, run or steered** (ee891fc; asked for in the chat).
  The game's run to the ball turned on the spot; it now skids turning round like
  your own running. Wins 53%. Speed, push and grip are per player (pl_top,
  pl_accel, pl_grip): a SPEED stat in training would set them (Codex's save and
  menus, so left for that session).

Where it stands (eight matches): you win 56% of points (target 35-55); points are
long, median 25.6 s, a quarter over 45 s (target 8-20 s, none over 45); slams come
up about 64 a match, and the CPU returns 7 in 10 cleanly. Ten tries at those,
below, didn't help. My read: rally length comes from the CPU retrieving nearly
everything, slams included, and slam-pop-slam chains; a fix is a design call (how
often the CPU should lose a rally, whether a slam should usually win) more than a
constant. Worth deciding after playing.

## How it's measured

`build/scratch/metrics.mjs` (not committed: build output) plays four matches (seeds
1, 4242, 777, 99) with a bot modelled on the 2026-11-22 recordings: its first press
comes 28-42 frames after the CPU strikes, then 1-2 more 8 frames apart, B a third of
the time, and it holds a direction (LEFT, RIGHT, UP or a mix) from its first press.

Targets: it wins 35-55% of points; points mostly 8-20 s, none over 45 s; fewer
"CPU fire past you, you down"; a few slam chances a match, two-press slams landing;
your shots within about 8 px of their aim; none out on neutral aim.

## Iterations

### 2026-11-22 22:30: you make for the ball before you press

Measured (before): wins 12% (6-44). Points median 20.4 s, 90th percentile 42.6 s.
Lost points: CPU fire past you, knocked down 17; CPU drive past you as you slid 12;
CPU fire past you as you slid 9. Your touches: 105 pop-ups of 405, 89 of them slides
with an empty meter: the ball got to you before the first press, you lunged and
popped it up, and the CPU slammed it.

Tried first: the CPU's fire shots at 40 instead of 48 (about 60 frames to cross, not
50). Wins 12% to 15%: not the cause. Reverted.

Changed: with nothing queued, the d-pad let go and the ball coming to you, you run
(full speed, easing in) to where you'll meet it, as the queued auto-run does.

After: wins 65% (42-23). Pop-ups 17 (from 105). Knocked down by fire 13 (from 17).
Points median 23.5 s, 90th percentile 65.3 s, 15 over 45 s (from 3): rallies now
last, and slams are very common (62 a match, from 15): next to tune.

### 2026-11-22 23:10: side to side costs you when the game runs you too

Asked for in the chat ("going side to side has a little cost"). Your own running
already ramped up (about 8 frames), coasted (22) and skidded turning round (9); the
game's run to the ball braked hard and turned on the spot, and since iteration 1
it runs you most of the time.

Changed: fc_step_move's firm 2 (yours, run for): it still stops firmly where it
means to, but a turn skids at the coasting rate for FC_SKID_FRAMES first. The CPU
keeps firm 1.

After: wins 53% (39-35), in the target. Pop-ups 84 (from 17: more late arrivals
after a turn), knocked down by fire 18 (from 13). Points median 23.4 s, 90th
percentile 61.9 s, 15 over 45 s; slams 62 a match: still the open gaps.

### 2026-11-22 23:20: long rallies (nothing kept)

Measured: points median 23.4 s, 90th percentile 61.9 s, 15 of 74 over 45 s. Slams
62 a match, and after one of yours the CPU returned it cleanly 7 times in 10 (it won
you the point 30 times in 250).

Tried, each reverted (4 seeds):
- The CPU needs a full meter to meet your slam clean: wins 96%. Every popped-up
  return was another slam for you.
- ... three quarters of a meter (24): wins 67%, points no shorter (slam, pop-up,
  slam chains).
- Your slam aimed away from the CPU in depth (40 or 92 past the net, not 64): wins
  47%, points longer: it volleys them out of the air.
- The CPU rattled sooner (PRESSURE_FREE 30 to 22): points median 19.8 s, 8 over
  45 s, but wins 70%; at 26, no clear change.
- Slams faster (54, not 48): wins 43%, points as long.

The measure was too noisy at four matches (about 10 points of win rate either way),
so it's eight from here (seeds 1, 4242, 777, 99, 3, 11, 22, 33). Baseline on eight:
wins 56% (75-58), points median 25.6 s, 90th percentile 70 s, 32 of 133 over 45 s;
slams 64 a match.

### 2026-11-22 23:30: presses while down (not kept); the loop stops

Measured (eight matches): the commonest lost point, 27 of 58, was "CPU fire past
you, knocked down", nearly all the CPU's bicycle slams and headers off your pop-ups
(as in the recordings). Presses made while on the floor after a slide were ignored.

Tried: a press mid-move readies the next strike. Unqueued knockdowns 27 to 3, but
17 now came while queued (still on the floor as the slam came), so lost points 58
to 57; wins 60%. Not kept: slide, pop-up, slam is the game's own sequence.

Stopped: nothing kept two rounds running; what's left is a design call.

### 2026-11-23 morning: the run to the ball undone (playtest)

Played by hand: "almost seems like the player is on autopilot... automatically moves
to the ball and kicks but it never goes over the net". Iteration 1 ran you to every
ball coming your way, nothing pressed, and a ball reaching you unready is a control
touch (a first touch up in front of you, to play with a second): with nothing
pressed after it, it dropped on your side. A match pressing only the serve: 11
control touches, 11 points lost to the bounce.

Changed: nothing pressed, you stay put (the d-pad moves you; a press runs you to it,
as before the loop). The skid turning round stays.

After (four matches, the late-pressing bot): wins 15% (8-44), points median 15.6 s,
4 over 45 s, slams 5 a match: late presses lose again, by design for now; the power
meter and an easier CPU are the next ideas.

### 2026-11-23: locked once queued; slide and return

The user's mode of play: "player positions himself after kick to where they think
the ball is landing next, they press a key to power up A or B or both. The player is
locked, when they're near the ball at return distance they either return or slide
and return or if they're too far they miss."

Changed: queued, you're no longer run to the ball (only a loose ball in your half
walks you to it); the CPU still is. A slide keeps the readied strike and, getting
there, returns the ball by its height (a fire ball without the meter: knocked down)
instead of popping it up; a slide that misses calls the strike off.

Measured with build/scratch/posbot.mjs (a player who lines up across with the
d-pad, 12-24 frames after the CPU strikes, at depth 190, then presses 2-3 times),
four matches: wins 33% (22-45), points median 13.6 s, 90th percentile 29.5 s.
Lost: CPU bounce as you volleyed 18, slid 14, bicycle 12 (the ball got past).
The late-pressing metrics bot (it never moves itself) now wins 4%: no longer
the measure.
