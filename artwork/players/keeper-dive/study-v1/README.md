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

For future integration, the keeper can use its current OAM block plus the
shootout's unused third-player block (6..13), keeping the game's UI at 14..39.
The existing full player atlas already occupies sprite bank one; a shootout-only
packed atlas is needed rather than appending these poses to it. The JSON includes
visible bounds, nonempty sprite columns and a leading-hand reach point. Align
that hand point to the goal target at the existing save deadline; retain the
current dive input, reaction timing and outcome calculation. Use pose 4 for the
save and the later poses for landing/recovery.

This is an artwork/reduction study. No live minigame consumes it yet.
Regenerate the trials with `node tools/reduce-keeper-dive-study.mjs`.
