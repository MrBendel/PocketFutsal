# Readable bicycle-kick replacements

Built-in ImageGen generated this four-character pose sheet using the original roster identity sheets and the approved slide study as references. `prompt.txt` records the first request; `refinement-prompt.txt` records the edit that replaced the seated-looking strike with an airborne scissor action. `generated-source.png` is the selected unmodified generated output. The draft is retained for provenance, not used by the exporter.

Rows: spiky, backward cap, short crop, red bandana. Columns: front tuck, front strike, front recovery, back tuck, back strike, back recovery. Front and back are separately authored views. Heads stay connected to shoulders; strike legs extend above head height, with the other leg bent. The recovery is a braced kneel.

Run `node tools/reduce-bicycle-study.mjs`, then `node tools/export-player-library.mjs`. The reducer exports native 24×24 and 32×32 sheets and indexed JSON. The complete player library uses 24×24 poses in its existing 32×32 cells, with no stretching. Its old rotated bicycle and compressed tuck frames are replaced, and obsolete win/loss placeholders are removed in favor of the generated celebration cycles.

The source contains translucent ambient haze despite the transparent-background request. The original is preserved unchanged; connected silhouettes at alpha >=240 exclude this haze from the derived sheets. The derived PNGs have binary transparency and four indexed palette entries per character. One common scale across all characters and views preserves relative proportions. Previews split each character across two rows (front then back).

`bicycle-loop.gif` shows front and back cycles for all four characters with fixed ground shadows and illustrative jump offsets. Lift metadata is preview placement, not runtime physics. Skin and outfit colors match the roster, including brown skin for crop and red bandana for bandana.

This fixes the complete authoring sheets and review viewer. The resident game tile bank in `tools/player-art.mjs` is unchanged: the 24px-wide poses require metasprite/VRAM integration before they can replace the live 16px-wide bicycle/tuck frames. Existing runtime timing and gameplay stay unchanged.
