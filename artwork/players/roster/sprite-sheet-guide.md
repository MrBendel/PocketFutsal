# Complete character sprite-sheet guide

The complete sheets combine match animations, campus walking sprites and both avatar sizes. They are authoring libraries: only the required frames are loaded into the game.

| Row (top to bottom) | Section | Native Y range | Frame size | Frames, left to right | Purpose |
| --- | --- | --- | --- | --- | --- |
| 1 | `front` | 0–31 | 16×24 | idle → run-a → run-b → ready → kick → volley → header | Match poses facing the camera. |
| 2 | `back` | 32–63 | 16×24 | idle → run-a → run-b → ready → kick → volley → header | Match poses facing away from the camera. |
| 3 | `bicycle-front` | 64–95 | 24×24 | tuck → strike → recover | Dedicated front bicycle-kick sequence. |
| 4 | `bicycle-back` | 96–127 | 24×24 | tuck → strike → recover | Dedicated back bicycle-kick sequence. |
| 5 | `side-right` | 128–159 | 16×24 | idle → run-a → run-b | Slightly turned match movement; mirror for left. |
| 6 | `win` | 160–191 | 16×24 | anticipate → raise-fist → takeoff → apex → descend → land | Fist raise and jump; frame lift is stored in JSON. |
| 7 | `loss` | 192–223 | 16×24 | stand → crouch → turn → center → turn-back → settle | Crouch and head-turn reaction. |
| 8 | `slide-right` | 224–255 | 32×24 | approach → brake → plant → extend → contact → decelerate → kneel → ready | Feet-first tackle; mirror for left. |
| 9 | `dive-low-right` | 256–287 | 32×24 | ready → plant → push-off → airborne → reach → descend → land → recover | Low keeper save; authored for Spiky only. |
| 10 | `dive-high-right` | 288–319 | 32×24 | ready → plant → push-off → airborne → reach → descend → land → recover | High keeper save; authored for Spiky only. |
| 11 | `campus-down` | 320–351 | 16×16 | idle → step-a → step-b | Campus walking toward the camera. |
| 12 | `campus-up` | 352–383 | 16×16 | idle → step-a → step-b | Campus walking away from the camera. |
| 13 | `campus-right` | 384–415 | 16×16 | idle → step-a → step-b | Campus walking right. |
| 14 | `campus-left` | 416–447 | 16×16 | idle → step-a → step-b | Campus walking left; mirrored artwork already stored. |
| 15 | `avatar` | 448–479 | 16×16 | portrait | Small scoreboard headshot, three visible colors plus transparency. |
| 16 | `avatar-32` | 480–511 | 32×32 | dialogue-portrait | Large dialogue avatar, four opaque colors including background/shading. |

Row numbers in this guide start at 1. JSON `sheetRow` values start at 0. Each native sheet is 256×512 pixels: eight 32×32 cells per row and sixteen rows. Frames retain their native dimensions, centered horizontally and aligned to the bottom of their cell. Use the frame `rect` in `complete.json` to crop actual artwork rather than treating all cells as 32×32 sprites. Unused cells are transparent. The native Y ranges exclude labels; the enlarged labeled preview uses 44-pixel rows before its 3× enlargement.

Front/back rows contain seven frames; the eighth/top-right cell is intentionally empty after removing the redundant bicycle poses. Use rows 3 and 4 for the complete bicycle sequences. Keeper rows 9 and 10 are populated only for Spiky. Other characters retain empty keeper rows so all sheets share the same layout.

Campus walks play idle → step A → idle → step B at 140 ms per pose. The avatar rows are static images. Match/reaction timing and jump heights belong to their runtime animation configuration; the sheet is not a universal timing prescription.

`complete-blue.png` and `complete-red.png` change team colors for match and campus art. Avatar rows retain their own palettes in both variants. The 16×16 avatar reserves index 0 for transparency; the 32×32 dialogue avatar uses index 0 as an opaque background/skin-shadow color. That larger avatar drops only the extra background-only 33rd row from its source. Consumers must honor each animation palette and opaque flag.

`complete.2bpp` contains the frame archive in column-major 8×8 tile order within each frame; frame byte offsets are in JSON. It is not a single resident VRAM allocation. `complete-review.html` previews the animations and per-section palettes. Rebuild PNGs, JSON, tile archives and review HTML with `node tools/export-player-library.mjs`; update this guide if the section layout changes.

The combined `complete-roster.png` places Spiky, Cap, Crop and Bandana side by side, blue/blue/red/red. Each block is 256 pixels wide.
