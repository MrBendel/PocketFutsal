# Campus walk motion study, v2

Studied the local Mario Tennis Alex/Harry and Brian overworld rows at integer enlargement. Brian keeps the cap and eye line quite stable while arms and feet alternate; Harry also changes the hair/cheek/shoulder silhouette between contacts. These are tiny, connected full-body poses, rather than a separately animated face. The source is reference only; no Nintendo pixels, crops, or character shapes are included in this export.

The v1 reducer held the entire top 10–11 rows stationary. V2 preserves our original face and head proportions but lowers the head, neck, and shoulders together by one native pixel on each planted step. The passing/idle pose rises back up. Lower-body step poses retain their foot contact, and the preview shadow stays on the ground. This is an original motion experiment informed by the reference, not a reconstruction of its exact timing or frames. Raw per-pose source heads were also examined, but their scale drift becomes excessive at 16×16.

Source art and generation prompts are reused unchanged from [v1](../v1/README.md); this revision is a deterministic edit of our native pixel export, with no new image generation. Reproduce with `node tools/reduce-campus-sprites.mjs 2`; reproduce the stationary-head version with `node tools/reduce-campus-sprites.mjs 1`.

The atlas, individual sheets, JSON and 2bpp formats match v1. The JSON records `upperBodyOffset` for planted poses and moves eye landmarks with the face. Native frames remain 16×16, with three visible colors plus transparency and mirrored left-facing cycles. Each character still uses 576 bytes / 36 tiles. The four-frame preview cycle is passing → contact A → passing → contact B, 140ms per pose. `campus-walk-loop.gif` shows all four characters and directions; `roster-preview.png` shows the frame contacts enlarged.

Review artwork only; these sprites are not connected to a campus scene yet.
