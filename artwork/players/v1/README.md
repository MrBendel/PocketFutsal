# Player study v1

First pass translating the approved art exploration into real 16x24 pixel frames.
Artwork source: built-in image generation using the exact prompt in prompt.txt.
The exporter then samples the generated reference into aligned native cells,
quantizes to three opaque colors, and writes binary transparency and 2bpp data.

Open review.html for front/back run animation, pose selection, and 1x / 8x views.
preview-8x.png shows blue front/back followed by red front/back.
blue-sheet.png and red-sheet.png are 64x48 transparent sheets, 4 columns x 2 rows.
Columns: idle, run A, run B, kick. Rows: front, back.
Individual PNGs are exactly 16x24, with one transparent row below the feet.
Palette indices: 0 transparent, 1 skin, 2 team kit, 3 dark outline/hair/shorts/boots.
Colors are multiples of 8 for GBC palette compatibility.

player.json includes editable pixel rows and frame metadata. player.2bpp is
1024 bytes: 8 frames, 128 bytes per frame (8 tiles). Each frame stores left
column top/middle/bottom/blank followed by right top/middle/bottom/blank.
This is the engine's per-frame tile layout, but NOT its full animation ordering:
the current game expects back views first, ten poses per view, plus a wide slide.
These eight frames have not been wired into the ROM. Pass/shoot currently share
a kick; slide, other directions, and extra character variants remain to draw.

Regenerate exports: node tools/export-player-study.mjs
The exporter validates dimensions, palette count, and pixel-for-pixel 2bpp decoding.
