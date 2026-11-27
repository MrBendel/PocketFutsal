# 16×16 campus walking sprites

Built-in ImageGen produced a coordinated master study from the four original roster identity sheets. These are dedicated compact overworld sprites for moving around the RPG campus, with the existing blue/red outfits, dark shorts and sneakers. The master is `generated-source.png`; `prompt.txt` and `refinement-prompt.txt` preserve the complete requests. `draft-source.png` is the first draft before palette/walking corrections, retained for provenance.

The high-fidelity master has four character columns (spiky, cap, crop, bandana) and nine action rows: down idle/step A/step B, up idle/step A/step B, right idle/step A/step B. Left is a horizontal mirror of right.

Run `node tools/reduce-campus-sprites.mjs 1` to reproduce all native exports. Reduction uses whole connected silhouettes at alpha >=200 and a common aspect-preserving scale. The entire body fits inside 16×16, with transparent margins. These are not stretched 16×24 gameplay sprites. Material-aware voting flattens shading into three opaque colors plus transparency. Source eye landmarks are preserved where voting loses narrow strokes. Each direction uses a stable idle head across its walking cycle, with the generated shirt/arm/leg motion below it, keeping the small faces from wobbling.

## Files

- `roster-16.png`: native atlas, four character rows and twelve columns. Direction groups are down, up, right, left; each group contains idle, step A, step B.
- `roster-preview.png`: labeled 4× enlargement in four character panels. Direction labels DN/UP/RT/LT identify each row.
- `campus-walk-loop.gif`: all four characters facing four directions, with small preview ground marks on a neutral paving-colored backdrop. Preview labels, backdrop and shadows are separate from the native character art.
- `<character>-16.png`: native 48×64 sheet, three columns and four direction rows.
- `<character>.json` and `campus.json`: indexed 16×16 pixel rows, per-character palettes, source bounds, preserved eye landmarks, direction/phase data, tile offsets and mirroring flags.
- `<character>.2bpp`: nine stored poses (down/up/right); left reuses the right offsets with `flipX: true`.

The walk cycle is idle → step A → idle → step B at 140ms per displayed frame. All poses remain distinct after native reduction.

## Runtime footprint and future integration

Each character needs 36 8×8 tiles / 576 bytes: nine unique frames, four tiles each. All four sets use 144 tiles / 2,304 bytes. Tiles are column-major within each frame: left top, left bottom, right top, right bottom. In the engine's 8×16 sprite mode each 16×16 actor uses two hardware sprite columns. Mirroring left requires swapping the columns and applying X flip to each; the PNG/JSON pixel rows already show the complete mirrored silhouette.

Palette index 0 stays transparent for moving sprites; indices 1–3 are skin/cream, shirt/bandana, and dark navy hair/outline/shoes. Crop retains brown skin. This differs from the opaque dialogue portraits, whose background can use index 0 as a visible shade. Origin is bottom-center (8,15); campus collision and movement remain caller responsibilities.

Artwork/review assets only. This adds no sports action states, no campus map or gameplay scene, and does not replace the 16×24 match sprites. The future campus renderer should consume this separate library.
