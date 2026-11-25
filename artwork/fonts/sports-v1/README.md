# Pocket Sports — original HUD lettering study

An original hand-pixelled uppercase sports alphabet, inspired by the broad,
outlined arcade lettering in the user's tennis references. No reference glyphs
are copied. Clipped outside corners, heavy stems, and open counters are drawn
on a six-column face. A one-pixel outline and a deeper lower shadow bring each
glyph to at most 8×16 pixels, compatible with the current text renderer.

`sports.json` is the authoritative palette-indexed asset: A–Z, 0–9, ! ? - : .,
with a four-pixel space. Index 0 is transparent; indices 1–3 are gold, orange,
and dark outline. The palette can be recolored without redrawing the letters.
`glyph-atlas.png` is a viewing sheet on black, not an alpha sprite import.

Rebuild with `node tools/sports-font-study.mjs`. The source contains the editable
binary letter faces and writes the native JSON, atlas, specimen at 1× and 4×,
and four court alert mockups at 3× with nearest-neighbor scaling. Court mockups
use `build/test-output/futsal-object-goals.png`; run the game tests first if it
is absent. The examples are design composites, not emulator screenshots of
an implemented HUD. This study does not change the live game's text.

Long calls can stack into two 16-pixel lines; tiny persistent scores and hints
should keep the existing compact font. The specimen includes single-line
COURT CHANGE to check spacing, with its two-line treatment in the court study.
