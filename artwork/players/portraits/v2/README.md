# Opaque dialogue portraits

Four 32×33 portraits with four opaque colors per character. These are technical reductions of the approved high-fidelity artwork in `../v1/generated-source.png`, generated there using built-in ImageGen; its original prompt is `../v1/prompt.txt`. No new image generation or changes to character identity are involved in this version.

Run `node tools/export-opaque-portraits.mjs` after the v1 exporter. Outputs include individual native PNGs, a 64×66 atlas, indexed JSON, individual 2bpp tile archives, enlarged portraits and dialogue-panel mockups. The transparent v1 assets remain available for comparison.

Palette roles:

- 0: opaque warm backdrop, also reused for skin shadows.
- 1: skin base.
- 2: blue or red team detail.
- 3: dark navy outline, hair, eyes and mouth.

Removing transparency permits palette index 0 to participate in shading. It does not create a fifth color: the backdrop and skin shadows share one color. Source-derived nose, ear and neck shadows are retained through area sampling; mouths retain the explicit small-size landmarks from v1. Crop uses a darker backdrop/shadow palette to preserve brown skin. All exported native pixels have alpha 255.

The 32×33 tile archives use row-major 8×8 tiles with low/high bitplanes per scanline, padded to 32×40 (20 tiles, 320 bytes). Padding uses palette index 0. In a background/window dialogue panel all four colors are drawable; a sprite layer would reserve index 0 for transparency, so these are intended for the dialogue background/window layer.

These are review assets and mockups. Runtime dialogue placement, palette allocation and tile uploads remain separate integration work. The existing source art, gameplay sprites and live UI are unchanged.
