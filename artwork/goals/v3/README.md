# Dimensional game goal

`simplified-source.png` is the approved built-in imagegen study; `prompt.txt`
preserves its generation prompt. `native.json` is the active 40x24 game asset:
three opaque colors plus transparent holes, symmetric posts, a sparse four-bay
rear net, and visible roof and side depth. `native-1x.png`, `native-4x.png` and
`native-8x.png` preview it on the blue court color.

Rebuild the reduction with `node tools/import-goal-study.mjs`. The importer uses
a fixed crop and area sampling of the source image, then strengthens the front
frame and restores thin net strands that disappear during reduction. The white
crossbar stays at the existing 14-pixel collision height; the eight extra rows
add depth behind it. Width, centre and physics remain unchanged.

`tools/goal-art.mjs` reads the native pixels and `tools/goal-background.mjs`
composites them into the court for both goals. Normal and motion buffers use
BG bank-one tile IDs 0..95, outside player OBJ memory. Only the at-most-24 inked
tiles of each 32-cell patch occupy goal VRAM; IDs 96..127 are reserved for the
stadium background. Far and near pose tables
occupy separate ROM assets. Motion uploads six tiles per frame and swaps only
a complete goal pose, preserving the scanline-clipping fix.
