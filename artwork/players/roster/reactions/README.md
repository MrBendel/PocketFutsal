# Subtle win and lose reactions

Each of the four original characters has three native 16×24 frames per outcome.
The approved idle supplies all pixels: no new facial features or oversized arms.
Win frames lift the shoulders by one pixel and make a small original-arm shift;
lose frames slump one pixel, then dip the original head one further pixel.
Hair, hats, faces and bandanas move intact. Feet remain planted.

`tools/reaction-art.mjs` is the editable source. `tools/reaction-study.mjs` writes
`reactions.json`, the contact sheet (four character rows, win left and lose right),
and a lossless animated GIF (win left, lose right). Regenerate the studies with
`node tools/reaction-study.mjs`.

The 2v2 results screen assigns the correct outcome to both teammates and loops
0–1–2–1 at 16 frames per step, with a small phase offset per character. Team
rows are staggered vertically to stay below the hardware sprite scanline cap.
All three relevant frames are loaded once for each character into sprite bank
one; animation changes tile references only. The compact shadows remain fixed.
Gameplay atlases and Futnet's existing point reactions are separate.
