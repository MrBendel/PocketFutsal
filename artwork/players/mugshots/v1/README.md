# 16×16 scoreboard mugshots

Dedicated head-only portraits of the original four PocketFutsal characters: spiky, cap, crop and bandana. Built-in ImageGen created `generated-source.png` using the original dialogue portrait source as an identity reference. The complete generation request is in `prompt.txt`. These are new head compositions, not crops of walking sprites. No Nintendo source pixels are included.

Run `node tools/export-scoreboard-mugshots.mjs` to reproduce the native exports. Each connected source head is fit proportionally into a 14×14 area within its 16×16 tile block. Area voting reduces flat materials to four palette indices. The isolated source mouth is detected and retained as a two-pixel stroke at its projected location; this prevents mouths from disappearing during reduction.

The icons are opaque background/window art: palette 0 is a warm backdrop also used for skin shadow; 1 is skin; 2 is cream cap, red bandana, or a navy hair highlight; 3 is dark hair/outline/eyes/mouth. All exported native PNG pixels have alpha 255. Palettes differ per character and must be assigned per tile when integrating. Sprite rendering would reserve palette 0 for transparency and needs a separate conversion.

- `mugshots-16.png`: 32×32 atlas, top row spiky/cap, bottom row crop/bandana.
- `mugshots-preview.png`: integer 10× enlargement with thin gutters.
- `<character>-16.png`: individual native icon.
- `<character>-16.2bpp`: four row-major 8×8 tiles, 64 bytes per character; low/high bitplanes per scanline.
- `mugshots-16.json`: indexed rows, palettes, source bounds, scale and preserved mouth landmarks.
- `scoreboard-native.png` and `scoreboard-preview.png`: 160×32 layout study and integer 4× enlargement showing two portraits per team. Sample score and project bitmap font are for context only.

All four icons use 16 tiles / 256 bytes total, excluding palettes. These are review assets. Runtime HUD placement, palette allocation, tile uploads and use of the custom sports score font remain integration work; the existing scoreboard and dialogue portraits are unchanged.
