# Legacy portable goal sprite study

The original flat 40x16-pixel frame used: thick cream posts and crossbar, a dark outline,
and dark open mesh for contrast on the blue court. `goals-1x.png` shows top and bottom at native resolution;
`goals-4x.png` is a nearest-neighbour preview. The preview has a court-blue background.

Source: `FUTSAL_GOAL` in `tools/tile-art.mjs`. Re-export with
`node tools/export-goal-study.mjs`. Two unique 8x16 columns build each goal;
the right post mirrors the left. `goal.2bpp` also includes the compact player
shadow. The active dimensional goal is now `../v3/native.json`. This legacy authoring
sheet supplies only its compact player shadow at OBJ tiles 254..255.
Goals do not use hardware OBJ sprites.
`tools/goal-background.mjs` composites the goal with its underlying court into
bank-one BG tiles. Two resident rest poses and two shared motion buffers use
signed BG tile IDs 0..95, separate from player OBJ data. The stadium uses
IDs 96..127; compact goal buffers hold only inked cells of each pose.
Precomputed knockback poses upload six tiles per frame during the stoppage;
the tilemap switches only once a whole pose is ready. Reset switches straight
back to the resident normal pose. Scrolling needs no goal uploads. This avoids
the ten-objects-per-scanline limit while preserving physical goal movement.

Collision dimensions are identical at both ends: posts at court x=60 and x=100,
crossbar at height 14, with a three-pixel ball/frame contact tolerance. A hit
rebounds the ball and dislodges the goal during a short stoppage. The goal resets
at the defending team's restart. The baked background goals have been removed. Frames centre at court x=80 after accounting for the hardware sprite origin.
The bottom rail sits just inside each projected baseline; the white crossbar
aligns with collision height 14 above that line.
