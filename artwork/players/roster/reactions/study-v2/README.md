# Generated reaction studies — higher-fidelity first

Built-in ImageGen created two original 1536×1024 RGBA studies using the four
approved `generated-source.png` character sheets as identity references.
`win-source.png` also references the new loss sheet for matching style/layout.
The complete generation prompts are `win-prompt.txt` and `loss-prompt.txt`.
The generated source files retain their alpha channels unchanged.

Both sheets have four character rows: spiky, cap, crop, bandana. Each row has
six poses. The win sequence anticipates, raises one fist, lifts off, reaches
the apex, descends and lands. The loss sequence stands, crouches, turns/tilts
through a three-quarter head pose, returns through center, turns the other
way and settles. All heads and limbs are constructed in the generated study,
rather than pasted from other game poses or shifted a pixel at runtime.

`tools/reduce-reaction-study.mjs` exports the two sheets at 32×48 and 16×24:

- `native-16x24.json` / `native-32x48.json`: indexed frames, palettes, source
  silhouette bounds and measured jump offsets. These are study data only.
- `win-16x24.png` / `loss-16x24.png`: transparent native-size sheets, six columns
  and four rows. There are corresponding 32×48 files.
- `*-preview.png`: nearest-neighbor enlarged previews over a checkerboard.
- `native-16x24-loop.gif`: win left, loss right, four character rows. The win
  loop applies whole-body jump offsets measured from the source sheet; the
  loss loop stays in the crouched part of the sequence rather than standing
  fully on every repeat. Ground marks in the GIF are preview guides only.

The reduction identifies the 24 complete connected silhouettes in each source,
including poses crossing nominal cell edges. It then uses one common scale
across both sheets, anchors each pose by its feet, removes low-alpha halos,
flattens material shading to the game's three opaque color roles, and takes
area votes per native pixel. Pose anatomy comes from the generated artwork;
the importer does not construct new limbs or turn heads. The 16×24 pass is a
readability/cleanup trial, not approved production art. The 32×48 pass makes
the effect of the native resolution limit easier to compare.

Regenerate with `node tools/reduce-reaction-study.mjs`. No live-game asset or
runtime currently consumes these studies. Review the source motion and native
reduction before selecting frames and integrating them into the game.
