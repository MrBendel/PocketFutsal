# Centered stadium scoreboard

Revised to match the user's marked-up placement: a large stadium scoreboard
centered above the far goal, surrounded by seating and crowd details. This
supersedes the small off-center park sign in v2 and the foreground panel in v1.
The scoreboard is venue scenery; the main readable score UI stays separate.

The board is 80×32 native pixels, with HOME/AWAY labels, recessed numeral panels,
steel supports and original Pocket Sports score glyphs in muted gold. The
`stadium-scoreboard.json` preserves the approved study's indexed pixels,
palette and digit-field coordinates. Zero is transparent, with three opaque
ink colors. `tools/stadium-art.mjs` builds the active game reduction, aligning
the digit fields to whole tiles and simplifying seats to repeatable blocks.

The background study adds 32 pixels above the original court, producing a
160×200 full map. Existing court geometry and both goals are translated together
by 32 pixels for the composition. The board sits at (40,2); posts rise behind
the fence and goal. Seating and sparse crowd pixels establish a stadium setting.

`stadium-court-4x.png` is the full venue composition, not a 160×144 emulator
screen. `far-stands-5x.png` is a detail crop, `scoreboard-6x.png` the board alone,
and `after-goal-4x.png` changes the scenic score from 2–1 to 3–1.

The active integration adds this headroom to projection and camera scrolling.
The game board is at (40,0), with local score fields at (16,16) and (48,16).
The first eight court tiles are distinct bank-zero digit cells. Each changed
team score queues four tiles; ordinary rally frames do no scoreboard work.
The main score/control strip stays separate. The background uses 156 unique
tiles: 134 in bank zero and 22 at bank-one BG IDs 96..117. Compact goal buffers
occupy bank-one IDs 0..95; player OBJ data remains untouched. BG palette 7 is
the stadium's four colors. The board scrolls with the venue.

Regenerate with `node tools/stadium-scoreboard-study.mjs`.
