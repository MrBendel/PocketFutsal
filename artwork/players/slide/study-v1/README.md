# Feet-first slide study

Created with built-in ImageGen using the original four character sheets and the approved keeper-dive study as references. The unmodified transparent result is `generated-source.png`; the exact generation instructions are in `prompt.txt`.

Rows: spiky, backward cap, short crop, bandana. Each source row has eight poses, all facing screen right: approach, braking crouch, hand plant, leg extension, full contact, deceleration, kneeling recovery, ready. The leading foot reaches the ball; the rear knee stays folded and the supporting hand stays behind the hip. The head and shoulders were generated together rather than assembled from rotated standing sprites.

Run `node tools/reduce-slide-study.mjs` to reproduce the derived assets. Connected silhouettes avoid clipping hair or copying neighbouring poses. Alpha below 200 is excluded from the reductions; source material colors are flattened before area sampling. All poses in each trial use one common scale. The generated source retains its original alpha.

- `native-32x24.png` / JSON: eight poses per character, eight columns by four rows. Enlarged preview wraps each character onto two rows of four.
- `contact-24x16.png` / JSON: extension and contact only, two columns by four character rows. This compares the important poses at the current slide dimensions without squeezing the upright recovery into a short canvas.
- `slide-right-loop.gif` and `slide-left-loop.gif`: all four characters with illustrative travel and timing. Ground marks and the existing game ball are separate preview layers, absent from the source and native character sheets. Leftward playback mirrors the entire demonstration.

JSON includes source bounding boxes, leading-boot positions and occupied 8x16 sprite columns. Palette entries are transparent, skin, team, outline; the crop character retains a darker skin palette.

## Integration notes

Review artwork only: no runtime consumes these files. The 32x24 trial uses up to seven occupied 8x16 sprite columns per pose and requires metasprite and VRAM planning; the current wide-pose renderer allocates four OAM slots. Preserve tackle/contact/recovery gameplay timers when integrating.

The old base slide in `tools/player-art.mjs` has feet left and head right. Tackles currently invert their rendering flip in `src/futsal.c`. This new base art has feet right, so its flip mapping must change when wired into the game.
