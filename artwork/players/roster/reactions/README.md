# Fist-up jump and crouched head shake

Each of the four original characters has three native 16×24 frames per outcome.
Win frames raise one original hand to shoulder height, then point the existing
feet for a short jump. The original face is restored above the arm so it stays
readable. Loss frames hold a two-pixel crouch and turn through the approved
three-quarter head profile and its opposite view. A one-pixel roll at either
side tilts the head around the middle neck columns; the head never slides
sideways. Every pixel of the original profile, hair, hat and bandana is retained.

`tools/reaction-art.mjs` is the editable source. `tools/reaction-study.mjs` writes
`reactions.json`, the contact sheet (four character rows, win left and lose right),
and a lossless animated GIF (win left, lose right). Regenerate the studies with
`node tools/reaction-study.mjs`.

The 2v2 results screen assigns the correct outcome to both teammates. Eight
eight-frame steps create a six-pixel up/down jump with a landing pause, or a
grounded crouch and tilted head turn through center, with a small phase offset per character. Team
rows are staggered vertically to stay below the hardware sprite scanline cap.
All three relevant frames are loaded once for each character into sprite bank
one; animation changes tile references only. The compact shadows remain fixed.
Gameplay atlases and Futnet's existing point reactions are separate.
