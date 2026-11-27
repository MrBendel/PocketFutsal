; Sprite art kept in WRAM bank 2 (0xD000-0xDFFF with SVBK = 2; nothing else uses
; banks 2-7): the big poses' tiles (slide_art, reaction_art), sent to VRAM a frame
; at a time by the GBC's general DMA, which reads RAM at 16-byte boundaries.
;
; GBDK's stack is in 0xD000-0xDFFF too (it starts at 0xE000), in bank 1, so while
; bank 2 is switched in neither routine pushes, pops, calls, or reads a variable:
; anything in 0xD000-0xDFFF would be bank 2's. Bank 1 is put back before they return.

    .module art_ram
    .globl _dmas, _dma_count, _art_copy_dst, _art_copy_src, _art_copy_len

SVBK = 0x70         ; WRAM bank (ldh)
HDMA1 = 0x51        ; source, high and low
HDMA2 = 0x52
HDMA3 = 0x53        ; destination in VRAM, high and low
HDMA4 = 0x54
HDMA5 = 0x55        ; length / 16 - 1, bit 7 clear: all at once, the CPU waiting

    .area _CODE

; void dma_queue_write(void): the queued DMAs (gfx_dma), in vblank. dmas[]: src (2
; bytes), dst (2), blocks (1). The addresses are latched first, from bank 1; only
; the last write, the one that starts it, needs bank 2 in.
_dma_queue_write::
    ld a, (_dma_count)
    or a, a
    ret z
    ld b, a
    ld hl, #_dmas
1$:
    ld a, (hl+)
    ldh (HDMA2), a
    ld a, (hl+)
    ldh (HDMA1), a
    ld a, (hl+)
    ldh (HDMA4), a
    ld a, (hl+)
    ldh (HDMA3), a
    ld a, (hl+)
    dec a
    ld e, a
    ld a, #2
    ldh (SVBK), a
    ld a, e
    ldh (HDMA5), a
    ld a, #1
    ldh (SVBK), a
    dec b
    jr nz, 1$
    xor a, a
    ld (_dma_count), a
    ret

; void art_copy(void): art_copy_len bytes (not 0) from art_copy_src (ROM, its bank
; switched in, or 0xC000-0xCFFF) to art_copy_dst in bank 2. With interrupts off.
_art_copy::
    ld hl, #_art_copy_len
    ld a, (hl+)
    ld c, a
    ld b, (hl)
    ld hl, #_art_copy_src
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_art_copy_dst
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    ld a, #2
    ldh (SVBK), a
    inc b
    inc c
    jr 3$
2$:
    ld a, (de)
    inc de
    ld (hl+), a
3$:
    dec c
    jr nz, 2$
    dec b
    jr nz, 2$
    ld a, #1
    ldh (SVBK), a
    ret
