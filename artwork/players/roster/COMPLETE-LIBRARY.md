# Complete player sprite sheets

Regenerate with `node tools/export-player-library.mjs` or the existing `node tools/export-player-moves.mjs` command. Open `complete-review.html` for animation selection, team palettes and left/right mirroring.

Each character folder now contains `complete-blue.png`, `complete-red.png`, `complete.json`, `complete.2bpp`, and a labeled nearest-neighbor enlarged `complete-preview.png`. The transparent sheets are 256×320 pixels with eight 32×32 cells per row. Native sprites are centered horizontally and bottom-aligned, with no rescaling or stretching. JSON records each native rectangle, local foot origin, original pixel rows, animation membership, optional lift/contact information and occupied sprite columns. Labels belong only to the enlarged review image.

Rows, top to bottom:

1. Front moves (eight), with a generated overhead bicycle strike replacing the rotated placeholder.
2. Back moves (eight), with a separately authored rear-view bicycle strike.
3. Generated front bicycle cycle: tuck, strike, recover.
4. Generated back bicycle cycle: tuck, strike, recover.
5. Existing side-running poses (three), mirrored for left.
6. Generated win cycle (six), including measured jump lift.
7. Generated loss cycle (six).
8. Generated feet-first slide (eight), mirrored for left.
9. Generated low keeper dive (eight), spiky only; mirror for left.
10. Generated high keeper dive (eight), spiky only; mirror for left.

There are 61 frames for spiky and 45 each for cap, crop and bandana: 196 total. The obsolete win/loss placeholders have been removed; the generated cycles occupy their own rows. The two keeper rows are empty for the three characters without authored keeper artwork. Review labels identify those unpopulated rows explicitly.

The 2bpp archive stores each frame as 8×8 tiles in column-major order within that frame, low bitplane then high bitplane per scanline. JSON `tiles.byteOffset`, `tiles.width` and `tiles.height` describe each frame. These complete archives exceed the resident player tile budget and must be uploaded in animation-sized subsets when integrated. Do not substitute the archive directly for the existing runtime tile bank.

`tools/player-animation-library.mjs` combines existing moves with the native reductions from `reactions/study-v2`, `players/slide/study-v1`, `players/keeper-dive/study-v1` and `players/bicycle/v1`. Sources and generation prompts stay in those versioned directories. Team color changes affect only palette index 2; character skin tones remain intact. Bicycle lift values are illustrative preview placement, separate from game physics.

This change packages the artwork into the player library. It does not yet change live animation selection, tackle flip mapping, keeper timing, metasprites or VRAM allocation. Existing runtime sheets and tile offsets remain compatible.
