# Native indoor Shootout arena study

A code-authored 160×144 tile reconstruction of the approved high-fidelity concept in `../study-v1/indoor-arena-taller-goal.png`. The generated inspiration is retained there with its prompts; this native pass uses the engine's existing pixel/tile authoring workflow rather than shrinking the whole illustration. Characters and ball are absent from the background asset.

Run `node tools/export-shootout-arena-study.mjs`. The editable art is `tools/art/shootout-arena-study.mjs`, using the same `canvas` helpers as the other mini-game backgrounds. This is review art; the live game is unchanged.

## Native constraints and reuse

- 20×18 map cells, 8×8 pixels each; the live HUD covers the last eight screen rows.
- 100 unique tiles out of the 176 scene tiles available after font/frame reservations.
- Six scene palettes, four opaque colors per tile. The two UI palettes remain reserved.
- 304 of 360 map cells (84.4%) use tiles placed more than once. 50 cells additionally reuse a mirrored tile through GBC map attributes.
- Reusable court panels, roof panels/trusses, padded-wall quadrants, seats, plank strips, square mesh and lights. Identical indexed pixels can also share a tile under a different palette.
- Unique tiles are spent on the white goal frame, receding roof/side edges, floor contact, curved court line and scoreboard. Fewer roof beams and larger wall panels keep the background quiet at native scale.
- Tile payload: 1,600 bytes. Relative map and attributes: 360 bytes each. Six RGB555 palettes require another 48 bytes when integrated.

`background.png` is the actual native asset; `background-4x.png` enlarges it without smoothing. `texture-kit.png` shows the named source patterns. `tile-atlas.png` shows all packed unique tiles in their first-used palette. `reuse-map.png` uses amber for single-use tiles, blue for tiles used two to four times, and green for more frequent reuse.

## Keeper clearance

The existing goal line (y=88), penalty spot (80,118), shot columns (52,80,108), and target heights (y=82/52) are retained. The goal frame is narrower, from x=40 through 119; the crossbar starts at y=34 and ends at y=37. All six targets remain inside the mouth.

`keeper-clearance-3x.png` overlays the real approved shooter, keeper and ball pixels, with the window HUD row indicated. The four views are ready, full leftward low dive, full rightward high dive and the central high jump. These use the same peak placement as the current runtime; the highest central crown is at y=40, leaving empty rows 38 and 39 below the crossbar. Ball and actors in this proof are separate overlays, not baked into the background. The exporter asserts this clearance and reconstructs every tile/map pixel to check flip packing.

The high-fidelity concept's tall ceiling is compressed to make room for the existing scoreboard and playable foreground. Keeping the real actor scale takes priority over a literal full-screen resize.

## Export format / integration

`arena.json` contains indexed tiles, relative map, GBC attributes, RGB palettes, geometry and usage stats. `arena.2bpp` stores row-major 8×8 tiles with low/high bitplanes per scanline. `arena.map` has relative tile indices; add the existing scene base of 80 at load/generation time. `arena.attr` includes palette index bits 0–2 and X/Y flips in bits 5/6, with VRAM bank zero.

To integrate after review, register this map/palette set as the Shootout background while retaining the existing SPR/HUD assets and save rules. The first two rows reuse the current kick scoreboard. The present study imports that header from the existing art module; an integration can use the exported static JSON or extract the shared header helper without a circular import. Repetition is handled by the tile map, with no texture-rendering work per frame.
