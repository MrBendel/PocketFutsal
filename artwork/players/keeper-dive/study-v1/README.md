# Shootout keeper dive study

Built-in ImageGen created `generated-source.png` (1254×1254 RGBA) from the
original spiky player's study and the higher-fidelity reaction study as style
references. `prompt.txt` preserves the exact generation prompt. The source's
alpha channel is unchanged. It is a new keeper animation, separate from the
feet-first slide and the generic lying/fall pose.

The sheet reads left-to-right across four rows. The first two rows are eight
low-dive poses; the last two are eight high-dive poses:

1. Ready stance.
2. Plant/compress the pushing leg.
3. Push off toward the save.
4. Airborne reach, legs trailing.
5. Full save extension, hands leading the head.
6. Descend and absorb the fall.
7. Side landing/tuck.
8. Recover toward a crouch.

Both sequences face screen right. Leftward versions use horizontal mirroring.
The head stays recognizable and joined to the torso, with a bent trailing leg
and forward-reaching hands instead of an extended kicking foot.

`tools/reduce-keeper-dive-study.mjs` identifies the sixteen complete silhouettes,
removes low-alpha fringes, flattens material shading to three opaque color roles,
then reduces at a common scale. The native pose canvases are wider to accommodate
the reach. There are 32×24 and 32×32 trials, each with blue/red transparent sheets,
enlarged previews, and indexed JSON. Pale gloves currently share the skin color
role in the three-color reduction. The 32×24 trial has 121 unique 8×8 tiles
including blank, and 4–7 nonempty 8×16 sprite columns per pose.

`dive-right-loop.gif` and `dive-left-loop.gif` show LOW and HIGH saves side by
side. They use the 32×24 trial with illustrative whole-body travel/lift curves,
ground marks and the game's existing ball as separate preview layers. The ball
is not embedded in any keeper frame. These loops illustrate a parry; they do
not simulate the actual shootout's timing or save logic.

Shootout now consumes the 32×24 low/high sequences through
`tools/shootout-player-art.mjs`. Its keeper uses OAM 6..13, including the unused
third-player block, with only nonempty columns enabled. The current pose is
streamed into bank-0 tiles 64..79 through the vblank queue; the shared player
bank is unchanged. Lower sprite columns end at the true 24px baseline so blank
padding cannot consume the result banner's scanline budget.

The full-extension hand aligns with the existing goal target; dive inputs,
reaction deadlines and save outcomes are unchanged. Pose 4 is full reach, followed
by descent, landing and recovery. Generated front-facing win/loss cycles play
after the keeper recovers and immediately for the shooter. OAM 38/39 provide
separate grounded shadows. These runtime trajectories differ from the study GIFs.

Reaction streams use tiles 80..87 and 88..95. All streams stay below sprite tile
128, where bank-0 sprite data would alias the signed background tile region.

Regenerate the trials with `node tools/reduce-keeper-dive-study.mjs`.
