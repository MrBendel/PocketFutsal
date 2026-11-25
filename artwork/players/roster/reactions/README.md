# Fist-up jump and crouched head shake

Each of the four original characters has three native 16×24 frames per outcome.
Win frames raise one original hand to shoulder height, then point the existing
feet for a short jump. The original face is restored above the arm so it stays
readable. Loss frames duck down one to two pixels and shake the original head
left/right by one pixel. The bandana's wide silhouette uses center/right poses
to retain every head pixel. Hair, hats, faces and bandanas move intact.

`tools/reaction-art.mjs` is the editable source. `tools/reaction-study.mjs` writes
`reactions.json`, the contact sheet (four character rows, win left and lose right),
and a lossless animated GIF (win left, lose right). Regenerate the studies with
`node tools/reaction-study.mjs`.

The 2v2 results screen assigns the correct outcome to both teammates. Eight
eight-frame steps create a six-pixel up/down jump with a landing pause, or a
grounded crouch and head shake, with a small phase offset per character. Team
rows are staggered vertically to stay below the hardware sprite scanline cap.
All three relevant frames are loaded once for each character into sprite bank
one; animation changes tile references only. The compact shadows remain fixed.
Gameplay atlases and Futnet's existing point reactions are separate.
