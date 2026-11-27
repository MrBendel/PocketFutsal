# Second native face cleanup

Targeted fixes to the marked v3 areas: Spiky's mouth moves one pixel left to separate it from the right eye and cheek; Cap's crown becomes a solid cream shape with a simple three-pixel dark opening, removing the face-like arch; Crop gets an even forehead/hair boundary without protruding dark pixels. Bandana is unchanged.

`face-cleanup.json` contains the complete editable cleanup, including v3's eyes and closed jaws. Reproduce with `node tools/export-scoreboard-mugshots.mjs 4`. Versions 1–3 remain reproducible with their respective arguments. Generated source art and its prompt remain unchanged in `../v1/`; this is native indexed-pixel editing, with no new image generation.

Each head remains 16×16 with transparent index 0 and three visible colors. Individual PNGs and `mugshots-16.png` have genuine alpha; enlarged previews use a checkerboard only for inspection. Each row-major 2bpp file is 64 bytes/four tiles. JSON includes palettes, pixels and facial landmarks. See `../v3/README.md` for export and integration details. Review assets only; no runtime HUD changes.
