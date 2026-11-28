# Bandana character sheet

See the [shared sprite-sheet guide](../sprite-sheet-guide.md) for cell coordinates, palettes, mirroring, timing and tile archive conventions.

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
| 9 | `dive-low-right` | 256–287 | — | Empty / not authored | Low keeper save; authored for Spiky only. |
| 10 | `dive-high-right` | 288–319 | — | Empty / not authored | High keeper save; authored for Spiky only. |
| 11 | `campus-down` | 320–351 | 16×16 | idle → step-a → step-b | Campus walking toward the camera. |
| 12 | `campus-up` | 352–383 | 16×16 | idle → step-a → step-b | Campus walking away from the camera. |
| 13 | `campus-right` | 384–415 | 16×16 | idle → step-a → step-b | Campus walking right. |
| 14 | `campus-left` | 416–447 | 16×16 | idle → step-a → step-b | Campus walking left; mirrored artwork already stored. |
| 15 | `avatar` | 448–479 | 16×16 | portrait | Small scoreboard headshot, three visible colors plus transparency. |
| 16 | `avatar-32` | 480–511 | 32×32 | dialogue-portrait | Large dialogue avatar, four opaque colors including background/shading. |

Files: `complete-blue.png` / `complete-red.png` are native atlases; `complete-preview.png` is the labeled enlargement; `complete.json` stores native frame rectangles, palettes and byte offsets; `complete.2bpp` stores packed tiles.
