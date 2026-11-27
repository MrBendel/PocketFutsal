# Goal silhouette refinement

The first native pass preserved a narrow 80×54 front frame, giving it a nearly square, boxy appearance unlike the approved broad goal in `../study-v1/indoor-arena-taller-goal.png`. This version prioritizes the comp's proportions and three-dimensional construction over the previous goal rectangle.

Run `node tools/export-shootout-arena-study.mjs --v2`. The revised art is in `tools/art/shootout-arena-study-v2.mjs`; it reuses the venue textures captured before the v1 goal was painted. Running the exporter without `--v2` still reproduces the original native study.

Changes:

- Front frame is 104×46, with a broad 96×42 opening, chamfered round joints, cream highlights, inner shading and small oval footplates.
- The receding rear frame is 80px wide. Its separate roof and sloped side-net planes provide visible depth rather than an inner rectangular border.
- Sparse rear mesh stops at the rear floor bar. Open teal court remains visible inside the mouth; no mesh closes off the floor like a cage.
- The goal plane moves back to y=80, leaving more foreground. Keeper standing feet remain at y=86, just in front of the goal.
- Court, seats, roof and wall reuse remain; the distinctive goal gets additional tiles. This export uses 103 unique tiles of 176 available, with 292/360 cells using repeated tiles. Tile payload is 1,648 bytes plus 720 bytes of map/attributes.

The JSON, native PNG, 2bpp tiles and GBC attributes have the same format as v1. All six shot targets fit inside the opening. The central high-jump crown remains at y=40, with two empty rows below the crossbar ending at y=37.

**Review prototype, not live artwork.** Integration must update the shot plane and display anchors together: goal line 80, low target 74, high target 50 (30px above the goal line). The keeper overlays use those proposed target-aligned positions while retaining the actual sprite pixels and standing/central-jump placement. The current gameplay remains at its existing positions until this design is approved and integrated.
