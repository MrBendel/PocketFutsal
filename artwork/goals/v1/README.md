# Portable goal sprites

Both ends use the same 40x16-pixel frame: cream posts and crossbar, a dark outline,
and grey open mesh. `goals-1x.png` shows top and bottom at native resolution;
`goals-4x.png` is a nearest-neighbour preview. The preview has a court-blue background.

Source: `FUTSAL_GOAL` in `tools/tile-art.mjs`. Re-export with
`node tools/export-goal-study.mjs`. Two unique 8x16 columns build each goal;
the right post mirrors the left. `goal.2bpp` also includes the compact player
shadow. All six tiles occupy bank-zero sprite tiles 250..255.

Collision dimensions are identical at both ends: posts at court x=60 and x=100,
crossbar at height 14, with a three-pixel ball/frame contact tolerance. A hit
rebounds the ball and dislodges the goal during a short stoppage. The goal resets
at the defending team's restart. The baked background goals have been removed.
