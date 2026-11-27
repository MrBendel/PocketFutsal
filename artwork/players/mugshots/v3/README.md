# Native face cleanup

Revision of the transparent 16×16 scoreboard heads, addressing the marked eyes, mouths and dangling chin/neck pixels. Hair, cap, bandana, skin palettes and transparent index 0 are retained. Lower faces are cleaned at native pixel resolution: paired one-pixel-wide eyes, a restrained two-pixel mouth, and a continuous jaw contour without a neck stump. There is no reserved canvas border; natural clear space below the jaw is intentional rather than stretching the face to fill it.

`face-cleanup.json` is the editable native source for this revision. It records replacement rows and explicit eye/mouth coordinates. Run `node tools/export-scoreboard-mugshots.mjs 3` to regenerate all exports. The reducer first produces the v2 full-canvas reduction, then applies these authored rows before packing tiles and rendering previews. The prior opaque and transparent reductions remain reproducible with arguments `1` and `2`.

No new ImageGen call was used for this revision. The original generated head study and its prompt remain at `../v1/generated-source.png` and `../v1/prompt.txt`. This is a direct edit of the existing indexed-pixel artwork.

Outputs follow v2: transparent native atlas and individual PNGs, indexed JSON with face landmarks, and four row-major 8×8 tiles / 64 bytes per character. Palette 0 is transparent; the three visible colors are skin, accessory/hair highlight, and dark navy. Enlarged previews composite onto a checkerboard or sample scoreboard; those backdrops are not baked into the icons. Background/window integration must composite transparent slots to the target panel color. Hardware sprite integration must repack row-major tiles into the engine's 8×16 columns.

These are review assets, not a runtime HUD change.
