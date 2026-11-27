# Four-player pixel roster

Built-in ImageGen provided original base studies; technical conversion produces
16x24 sprites with three visible colors plus transparency. The final prompts
are stored in each character folder (spiky uses ../../v1/prompt.txt).

- spiky: blue #1, controlled player in futsal
- cap: blue teammate
- crop: red opponent, darker skin palette
- bandana: red opponent

Open review.html for all four animated move sets. Each character has front/back
idle, run A/B, ready, kick, volley, header, bicycle, win and lose, plus a 24x16 slide.
Header, bicycle and other extra poses are native pixel adaptations of the base
studies. Each full-blue.png/full-red.png is 160x48: ten columns, front/back rows.
The slide is separate. moves.json holds exact pixel rows. No new AI abilities or
ball physics are introduced by the artwork.

Futnet loads the spiky character's full move set: the original 168-tile layout
plus 48 tiles for sideways tucks, landings and get-up poses, plus a separate
8-tile knockdown and 24 tiles for side idle/run A/run B (248 total).
2v2 loads all four characters' eight basic front/back idle/run/kick frames into
VRAM bank 1 (64 tiles each, exactly 256 total). Per-character poses and palettes
are selected without uploading tiles during play. Heading/bicycle artwork is
available for all four; those ball-height moves currently belong to futnet.
2v2 also loads each character's wide slide and front/back crouch into VRAM
bank 0, tiles 20..115. Tackles now crouch, extend into a slide, then stand up.
Side idle and two side-running strides share bank 0 tiles 214..241 (28 for
the whole roster): four upper bodies and three common lower bodies. This
avoids Club Court's signed background tiles at 128..213. Right-facing art
mirrors in hardware for leftward motion. The
original kicking heads are preserved pixel-for-pixel on connected,
three-quarter bodies. The broader shoulders, two-leg idle stance and compact
running gait come from the approved front-facing sprite set; these are a slight
turn toward movement, rather than a full profile silhouette.
Horizontal movement selects these frames; vertical movement selects front/back.
The last view persists at rest, and actions keep their own poses. A small
direction threshold prevents diagonal movement from flickering between views.
Open side-review.html for animated left/right cycles at native and enlarged
sizes, or side-running-1x.png / side-running-6x.png for all three poses.

Regenerate a base study with:
node tools/export-player-study.mjs artwork/players/roster/cap
Use a final `dark` argument for crop. Then regenerate extra poses and review:
node tools/export-player-moves.mjs

The build consumes player.json through tools/player-art.mjs. Changes to its
pose construction or the character pixel rows invalidate generated C assets.

Visual effects: all players now have two-sprite ground shadows. Futnet jump
poses lift the body while the shadow stays on the court. Futsal shots use a
48-frame bright-ball effect with four fading trail sprites; passes and new
possession clear the effect. This is presentation only: charged shooting,
aerial futsal physics and slowdown windows remain separate gameplay work.

Transitions: open transition-review.html to inspect bicycle/slide sequences.
Each transitions-blue.png / transitions-red.png has tuck, landing and rising
poses in three columns, front/back in two rows. Bicycles rotate through a tuck,
airborne contact, return tuck and landing; slides use crouch, extension and
recovery. These visual phases retain the match's original contact/recovery timers.
Ready poses keep connected arms; detached hand boxes were removed from serves.

Slides now use a dedicated feet-first tackle silhouette: upright head, backward
lean, bent rear knee, extended leading leg and a bracing hand. The old rotated
standing sprite is retained only for knockdowns, not intentional slides.
slide-tackle-preview.png compares the four tackle silhouettes at 8x scale.

Slide heads reuse the original kicking frame's three-quarter profiles pixel-for-pixel,
mirrored toward the leading foot. No face resampling or generic front-facing redraw
is used. Each jaw is joined directly through its neck and shirt collar. slide-heads-1x.png is the native readability check;
slide-heads-8x.png is the enlarged pixel review. Both regenerate with the move exporter.

Ball occlusion: both match modes interleave the ball with player OAM blocks by
court depth. A nearer player can cover a ball behind them; a nearer ball covers
a farther player. Ground position determines depth, independently of ball height.

## Combined match, campus and avatar sheets

Run `node tools/export-player-library.mjs` to reproduce `complete-blue.png`, `complete-red.png`, `complete-preview.png`, `complete.json` and `complete.2bpp` for each character, plus `complete-roster.png` with all four characters side by side (spiky, cap, crop, bandana; blue/blue/red/red).

The front/back rows now end at header; their redundant eighth bicycle-strike cells are empty. The dedicated bicycle-front/back tuck, strike and recovery sequences remain. Four campus rows append the latest native 16×16 idle/step A/step B views from `../campus/v2/`, followed by the latest transparent 16×16 avatar from `../mugshots/v4/`, including Crop's eyebrows. Existing match frame pixels and the positions of the original ten rows are preserved. Native atlas cells remain 32×32; smaller assets keep their native dimensions within each cell.

Avatar animations carry their own four-entry palette (transparent plus three visible colors), used by both native PNGs and the interactive review. Team recoloring applies to match/campus shirts, while portrait cream/red accessories and hair highlights keep their original colors. Consumers must honor an animation's palette override when present. `complete.json` format version is now 2; its frame rectangles and tile offsets describe the regenerated archive. The archive is an authoring library, not a single resident VRAM allocation. The review page includes the new assets; gameplay integration of the campus and avatars remains separate.

The final `avatar-32` row adds the larger dialogue portraits for all four characters. These preserve the opaque four-color portraits from `../portraits/v2/portraits-32x33.json`, omitting only their background-only 33rd row to fit exactly 32×32. Their background/skin-shadow palette index 0 remains visible; it is not treated as transparent in the atlas, labeled preview, or interactive review. The 16×16 transparent avatars remain in the preceding row.
