# PocketFutsal design

An arcade futsal game for the Game Boy Color, played like Mario Tennis: the ball is
always loose, every touch is a strike, and the skill is in reading where the ball goes
and charging the right shot early. Proposals, not decisions: change freely.

## v0.1 (2026-11-21): first playable

- **1-on-1 vs CPU** (decided 2026-11-21): each side a player and an automatic keeper.
  Simplest to get feeling good before adding teammates.
- **One-screen pitch** with boards all round (the ball comes off them, as in indoor
  futsal arenas), a goal in each end board. 160×136 above a one-row HUD.
- **Shots, Mario Tennis style.** A readies a driven shot, B a chip. It charges until
  the ball arrives and fires by itself in reach. Charging slows you to a shuffle.
  UP / DOWN at the moment of the shot picks a corner. A full meter is a power shot,
  on fire.
- **Without a shot ready**, running into the ball dribbles it ahead; standing still
  traps it.
- **Keepers** slide along their line after the ball (staying near the middle). They
  hold soft shots, with less reach the harder the shot; a power shot they can only
  parry, and only straight at them; a chip over their head beats them.
- **Match**: first to five goals. A goal is celebrated, then the side that conceded
  kicks off from nearer the ball.
- **CPU**: gets goal-side of the ball, readies a shot (a chip one time in four) when
  close and lets it charge as it closes in, aims at a random corner or the middle.

## Ideas for next

- **Characters** with stats (speed, power, curve) and a special shot each, Mario
  Tennis's roster idea, original characters.
- **Curve**: holding LEFT/RIGHT across the shot bends it.
- **Special meter**: well-timed strikes (contact the frame the ball arrives at full
  charge) fill a special bar; a special shot the keeper can't hold at all.
- **Tackles / shoulder barges** to knock the ball off the other player.
- **2-on-2**: you control the player nearest the ball, a CPU teammate helps.
- **Match timer** and sudden death, a tournament ladder against harder CPUs.
- **Link cable** 1-on-1 versus.
- Real art: player sprites with more frames, a crowd behind the boards, goal nets
  that bulge.
