# Complete player sprite sheets

Regenerate with `node tools/export-player-library.mjs` or the existing `node tools/export-player-moves.mjs` command. Open `complete-review.html` for animation selection, team palettes and left/right mirroring.

Each character folder now contains `complete-blue.png`, `complete-red.png`, `complete.json`, `complete.2bpp`, and a nearest-neighbor enlarged `complete-preview.png`. The transparent sheets are 320×320 pixels with ten 32×32 cells per row. Native sprites are centered horizontally and bottom-aligned, with no rescaling or stretching. JSON records each native rectangle, local foot origin, original pixel rows, animation membership, optional lift/contact information and occupied sprite columns.

Rows, top to bottom:

1. Existing front moves (ten).
2. Existing back moves (ten).
3. Existing front transitions (three).
4. Existing back transitions (three).
5. Existing side-running poses (three), mirrored for left.
6. Generated win cycle (six), including measured jump lift.
7. Generated loss cycle (six).
8. Generated feet-first slide (eight), mirrored for left.
9. Generated low keeper dive (eight), spiky only; mirror for left.
10. Generated high keeper dive (eight), spiky only; mirror for left.

There are 65 frames for spiky and 49 each for cap, crop and bandana: 212 total. Old win/loss placeholders remain explicitly named `win-legacy` and `loss-legacy` in the original move rows; the generated cycles have their own rows. The two keeper rows are empty for the three characters without authored keeper artwork.

The 2bpp archive stores each frame as 8×8 tiles in column-major order within that frame, low bitplane then high bitplane per scanline. JSON `tiles.byteOffset`, `tiles.width` and `tiles.height` describe each frame. These complete archives exceed the resident player tile budget and must be uploaded in animation-sized subsets when integrated. Do not substitute the archive directly for the existing runtime tile bank.

`tools/player-animation-library.mjs` combines existing moves with the approved native reductions from `reactions/study-v2`, `players/slide/study-v1` and `players/keeper-dive/study-v1`. Sources and generation prompts stay in those versioned directories. Team color changes affect only palette index 2; character skin tones remain intact.

This change packages the artwork into the player library. It does not yet change live animation selection, tackle flip mapping, keeper timing, metasprites or VRAM allocation. Existing runtime sheets and tile offsets remain compatible.
