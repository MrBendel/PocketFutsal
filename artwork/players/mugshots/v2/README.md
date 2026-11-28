# Transparent 16×16 scoreboard heads

Revised native export of the original four head-only compositions. Index 0 is transparent; indices 1–3 are the only visible colors: skin, accessory/hair highlight, and navy outline/hair/facial features. Skin shadows are merged into the skin color so index 0 never creates holes inside the face. Cream caps and red bandanas retain their own accessory color; spiky and crop use that slot for a hair highlight.

There is no reserved border. Each head is proportionally fitted to the full 16×16 canvas, keeping the original aspect ratio. The narrower dimension may naturally retain clear pixels. Mouth landmarks are projected again at the larger scale.

Run `node tools/export-scoreboard-mugshots.mjs 2` to reproduce these outputs. Use argument `1` to reproduce the earlier opaque study. Source art and the built-in ImageGen prompt are reused unchanged from `../v1/generated-source.png` and `../v1/prompt.txt`; this is a palette/scale conversion, with no new image generation.

`mugshots-16.png` is the transparent 32×32 atlas (spiky/cap, crop/bandana). Individual `<character>-16.png` files are genuinely transparent RGBA images. `mugshots-16.json` stores indexed pixels, palettes and source landmarks. Each `<character>-16.2bpp` file contains four row-major 8×8 tiles / 64 bytes, low/high bitplanes per row. All four heads total 256 bytes, excluding palettes.

The enlarged `mugshots-preview.png` composites the icons on a checkerboard for inspection; the checkerboard is not baked into the native assets. `scoreboard-native.png` and its enlarged preview composite them onto team-color strips without rectangular portrait backgrounds. These remain layout studies with the existing bitmap font for sample score text.

The native icon files reserve palette 0 for transparency. When integrating into the background/window layer, whose index 0 is opaque, composite that slot to the panel color or use the appropriate background palette. Hardware sprite integration must repack the row-major tiles into the engine's two 8×16 columns. Runtime integration remains separate review work.
