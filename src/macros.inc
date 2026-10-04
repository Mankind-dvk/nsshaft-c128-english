; Small inline helpers for address setup, paired screen writes and register bits.
; They expand at assembly time and do not save registers. Each macro notes what
; it changes; IRQ protection stays with the caller.
;
; Quick reference:
;   line 16: 16-bit address setup
;   line 26: screen selection
;   line 34: paired screen and sprite-pointer writes
;   line 62: register bit updates
;   line 79: raster IRQ acknowledgement
;
; SET_WORD stores the immediate value low byte first. It leaves A=high byte and
; changes N/Z; X/Y and other flags are preserved. Protect IRQ-vector updates
; at the call site, since its two stores are not atomic.

.macro SET_WORD destination, value
    lda #<(value)
    sta destination
    lda #>(value)
    sta destination+1
.endmacro

; Write a complete D018 screen/charset layout; the caller updates active_screen. This
; does not change the VIC bank.
; Out: A = layout; clobbers A, N/Z; preserves X/Y and C/V/D/I.
.macro SHOW_SCREEN layout
    lda #layout
    sta VIC_MEMORY_POINTERS
.endmacro

; Store A at offset [+ X/Y] in both screen matrices; the caller writes Color RAM.
; index_register is optional or x/y; keep the resulting offset in character memory.
; Preserves A/X/Y and all flags. Expands to exactly two STA instructions.
.macro STORE_SCREEN_PAIR offset, index_register
    .ifblank index_register
        sta SCREEN_A+offset
        sta SCREEN_B+offset
    .else
        sta SCREEN_A+offset, index_register
        sta SCREEN_B+offset, index_register
    .endif
.endmacro

; Store A's 64-byte sprite block number in both screen pointer tables. slot is fixed;
; with optional x/y indexing, the caller must keep slot+index in 0..7.
; Preserves A/X/Y and all flags; no bitmap bytes are copied.
.macro STORE_SPRITE_POINTER slot, index_register
    .assert (slot >= 0) .and (slot < 8), error, "Invalid sprite slot"
    .ifblank index_register
        sta SPRITE_POINTER_A+slot
        sta SPRITE_POINTER_B+slot
    .else
        sta SPRITE_POINTER_A+slot, index_register
        sta SPRITE_POINTER_B+slot, index_register
    .endif
.endmacro

; Non-atomic read/modify/write; use only registers safe to read and write back. Avoid
; D019/CIA interrupt-status registers with read/write side effects, and D011, whose
; raster high bit has different read/write meanings. The caller masks IRQs.
; Out: A = updated register; clobbers A, N/Z; preserves X/Y and C/V/D/I.
.macro SET_REGISTER_BITS target, bits
    lda target
    ora #bits
    sta target
.endmacro

; bits identifies bits to clear; pass %00000001 to clear bit 0. The register/flag
; contract and readback restrictions are the same as above.
.macro CLEAR_REGISTER_BITS target, bits
    lda target
    and #($ff ^ (bits))
    sta target
.endmacro

; Write 1 to D019 to acknowledge the raster IRQ only; no read/modify/write.
; Out: A=1, N=0, Z=0; preserves X/Y and other flags. The IRQ handler owns register
; saving, interrupt enable state, MMU handling, and the return sequence.
.macro ACK_VIC_RASTER_IRQ
    lda #1
    sta VIC_IRQ_STATUS
.endmacro
