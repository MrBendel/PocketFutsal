# Club Court

First native-pixel implementation of the approved Club Court concept.
club-court-1x.png is 160x168; club-court-4x.png is its nearest-neighbor preview.
The game camera shows 160x136 pixels plus the HUD and scrolls over this map.

The authoritative editable source is the futsal branch in tools/tile-art.mjs;
palettes_futsal in src/palettes.c supplies four background palettes. Court lines,
goals, penalty boxes and center circle share the existing perspective geometry.
The generated map uses 134 unique 8x8 tiles, within the scene's 176-tile allowance.

Regenerate PNG previews: node tools/export-club-court.mjs

Current goals are background artwork with existing goal-mouth collision checks.
A suggested next step is separate goal objects with sprite posts and a background
net, adding post rebounds without using sprites for the whole net surface.
