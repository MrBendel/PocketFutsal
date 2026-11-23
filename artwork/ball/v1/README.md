# Soccer ball study

`generated-source.png` is the original six-frame sprite study made with the
built-in image generator. The exact generation prompt is in `prompt.txt`.

`node tools/export-ball-study.mjs` produces aligned transparent sheets at 8, 12
and 16 pixels, plus `scaling-preview.png` and a self-contained `review.html`.
The technical conversion uses one shared scale, centers each ball, quantizes
to three opaque colors, removes soft alpha, and locks the silhouette.

At 8px, direct reduction merges seams into large blobs (`sheet-8-raw.png`).
The native cleanup keeps the generated palette and panel motif but simplifies
it to a travelling 2x2 panel and smaller side panels. The current game size
remains 8x8; the 12px and 16px sheets are comparison studies.

`ball.json` holds the editable native pixel rows consumed by the build.
The first two frames keep tile slots 0 and 2. Four extra frames occupy tiles
20..27 in Futnet, before its effects, and 116..123 in futsal, before the signed
background tile range. Player actions, the court, shadows and shot trails keep
their existing tile allocations. Spin follows court distance and freezes at rest.
