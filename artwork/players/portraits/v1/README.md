# Dialogue portraits

Built-in ImageGen created a coordinated high-fidelity head portrait sheet from the four original player identity references. `generated-source.png` preserves the original generated RGBA image; `prompt.txt` is the exact generation prompt. Individual `*-source.png` crops preserve source RGBA samples without resizing.

The master sheet and native sheets are two columns by two rows: spiky, backward cap, short crop, red bandana. All heads face slightly toward screen right. Each portrait includes a short connected neck and collar, with no hands or full body.

Run `node tools/export-dialogue-portraits.mjs` to reproduce:

- `portraits-32x33.png` / JSON and `<character>-32x33.png`: the preferred compact option, preserving the corrected 32px head artwork with one additional transparent bottom row. `portraits-32x33-preview.png` is the enlarged review sheet. Its tile archive pads to 32×40 (20 tiles / 320 bytes) because tile heights are multiples of eight.

- `portraits-32.png` and `portraits-48.png`: transparent 2×2 native atlases, with corresponding JSON containing indexed pixel rows, per-character palettes, atlas rectangles and original source bounds.
- `<character>-32.png` and `<character>-48.png`: individual native portraits.
- `<character>-32.2bpp` and `<character>-48.2bpp`: row-major 8×8 tile archives, low/high bitplanes per scanline. A 32px portrait uses 16 tiles / 256 bytes; a 48px portrait uses 36 tiles / 576 bytes.
- `portraits-*-preview.png`: integer nearest-neighbor enlargements over a checkerboard; checkerboard is absent from the native assets.
- `<character>-dialogue.png`: 160×56 dialogue panel mockups using the existing game font, 32×33 portraits and illustrative text. `dialogue-preview.png` enlarges the four panels for review. The names are internal character IDs, not finalized story names.

Reduction uses whole connected silhouettes, one common aspect-preserving scale per atlas, one-pixel transparent margins and material-aware four-color area sampling. Dark navy hair/outline, warm skin and team color remain distinct. Crop retains brown skin; blue collars remain blue and red collars/bandana remain red. Four-color reduction deliberately removes source shading; the 48px option retains more facial detail.

Thin mouth strokes were disappearing during area voting. The exporter now maps measured mouth landmarks from the approved source into each native portrait and preserves a minimum three-pixel dark mouth. JSON records both the source landmark bounds and native mouth rectangle. This keeps the compact portraits readable without regenerating or changing their identities.

Palette index 0 is transparent in PNGs. In a Game Boy background/window tile it must be set to the dialogue panel's backdrop color; indices 1–3 are skin, team and outline. Separate character palettes are supplied. These tile files are authoring assets, not yet registered in `tools/assets.mjs`; dialogue placement, palette allocation and VRAM uploads still need runtime integration.

Use 32px beside regular dialogue on a 160×144 screen. The 48px alternative is for larger conversation layouts or character introduction panels. Both are supplied for review; no live dialogue behavior is changed.
