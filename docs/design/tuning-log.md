# Overnight tuning log

## Morning summary

(Updated as the loop goes; read this first.)

- **1. You run to the ball on your own** (kept). With nothing queued and the d-pad
  let go, you now head for where you'll meet a ball coming to you, the way you jog
  home between shots. A press only readies the strike. The d-pad still overrides.
  This is a feel change worth checking by hand.

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
