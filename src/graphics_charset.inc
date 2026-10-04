; The 2 KB source font for platform, frame and ceiling graphics.
; Unused slots are zero-filled. Text comes from text_charset.inc; scene setup
; copies the needed glyphs from both sources into the live charset.
;
; Quick reference:
;   line 21: shared brick bitmap
;   line 28: blank and full platform glyphs
;   line 78: upper/lower fragment templates
;   line 139: frame and ceiling glyphs
;   line 147: unused-slot padding and size checks

.segment "GRAPHICS_CHARSET"
graphics_charset:
.macro GRAPHICS_SLOT glyph_id
    .assert (*-graphics_charset) = (glyph_id)*CHARSET_GLYPH_BYTES, error, "Graphics slot/order mismatch"
.endmacro

; Normal platforms and the frame share this brick bitmap: 00=black, 01=white, 10=gray,
; 11=the cell's local color (red for bricks). A multicolor pixel is two display pixels
; wide; eight rows still scroll by 1 px.
.macro BRICK_GLYPH
    .byte $f7,$f7,$55,$7f,$7f,$55,$f7,$f7
.endmacro

; --- Blank and bodies ---
; 00: Blank
    GRAPHICS_SLOT GLYPH_BLANK
graphics_slot_00:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 01: Normal red-brick platform
    GRAPHICS_SLOT GLYPH_PLATFORM_FULL
graphics_slot_01:
    BRICK_GLYPH

; 02: White/green spikes; each bit pair is one color pixel: 00=empty, 01=white,
; 11=green.
    GRAPHICS_SLOT GLYPH_SPIKE_FULL
graphics_slot_02:
    .byte %00000100 ; empty empty white empty
    .byte %00000100 ; empty empty white empty
    .byte %00001100 ; empty empty green empty
    .byte %00110101 ; empty green white white
    .byte %00011101 ; empty white green white
    .byte %00010111 ; empty white white green
    .byte %11011101 ; green white green white
    .byte %01110111 ; white green white green

; 03: Relaxed spring, two-pixel-wide zigzag coil
    GRAPHICS_SLOT GLYPH_SPRING_FULL
graphics_slot_03:
    .byte $ff,$0c,$30,$f0,$0f,$0c,$30,$ff

; 04: Compressed spring, solid lower half
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_FULL
graphics_slot_04:
    .byte $00,$00,$00,$00,$ff,$ff,$ff,$ff

; 05: Left-conveyor FULL texture, mirrored from slot 6 by multicolor pixel pairs.
; Its scrolling LOWER fragment uses the separate slot 66.
    GRAPHICS_SLOT GLYPH_CONVEYOR_LEFT_FULL
graphics_slot_05:
    .byte $f5,$d7,$5f,$7d,$7d,$5f,$d7,$f5

; 06: Right-conveyor FULL texture. Its scrolling LOWER fragment uses slot 67.
    GRAPHICS_SLOT GLYPH_CONVEYOR_RIGHT_FULL
graphics_slot_06:
    .byte $5f,$d7,$f5,$7d,$7d,$f5,$d7,$5f

; 07: Cracked fade platform with partially missing gray mortar
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_FULL
graphics_slot_07:
    .byte $fb,$f3,$20,$3f,$3f,$02,$f3,$fb

; --- Initial scrolling fragments ---
; 08: Brick platform UPPER
    GRAPHICS_SLOT GLYPH_NORMAL_UPPER
graphics_slot_08:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 09: Brick platform LOWER
    GRAPHICS_SLOT GLYPH_NORMAL_LOWER
graphics_slot_09:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 10: Spike UPPER
    GRAPHICS_SLOT GLYPH_SPIKE_UPPER
graphics_slot_10:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 11: Spike LOWER
    GRAPHICS_SLOT GLYPH_SPIKE_LOWER
graphics_slot_11:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 12: Spring UPPER
    GRAPHICS_SLOT GLYPH_SPRING_UPPER
graphics_slot_12:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 13: Spring LOWER
    GRAPHICS_SLOT GLYPH_SPRING_LOWER
graphics_slot_13:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 14: Compressed spring UPPER
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_UPPER
graphics_slot_14:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 15: Compressed spring LOWER
    GRAPHICS_SLOT GLYPH_SPRING_COMPRESSED_LOWER
graphics_slot_15:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 16: Cracked fade UPPER
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_UPPER
graphics_slot_16:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 17: Cracked fade LOWER
    GRAPHICS_SLOT GLYPH_FADE_CRACKED_LOWER
graphics_slot_17:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 18: Left conveyor UPPER
    GRAPHICS_SLOT GLYPH_CONVEYOR_LEFT_UPPER
graphics_slot_18:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; 19: Right conveyor UPPER
    GRAPHICS_SLOT GLYPH_CONVEYOR_RIGHT_UPPER
graphics_slot_19:
    .byte $00,$00,$00,$00,$00,$00,$00,$00

; --- Frame and ceiling spikes ---
; 20: Frame and UI divider share the red/white multicolor brick texture.
    GRAPHICS_SLOT GLYPH_UI_FRAME
graphics_slot_20:
    BRICK_GLYPH

; 21: Slot-2 spike flipped vertically, without horizontal mirroring.
    GRAPHICS_SLOT GLYPH_CEILING_SPIKE
graphics_slot_21:
    .byte $77,$dd,$17,$1d,$35,$0c,$04,$04

graphics_defined_end:
.assert graphics_defined_end-graphics_charset = GRAPHICS_GLYPH_COUNT*CHARSET_GLYPH_BYTES, error, "Unexpected graphics count"
    ; 22..39 are unused output slots; 40..55 receive HUD text from source charset 2.
    .res (GLYPH_FADE_FULL-GRAPHICS_GLYPH_COUNT)*CHARSET_GLYPH_BYTES, 0
    GRAPHICS_SLOT GLYPH_FADE_FULL
    ; Stage 1: red bricks and gray mortar; 11=red, 10=gray.
    .byte $fb,$fb,$aa,$bf,$bf,$aa,$fb,$fb
    GRAPHICS_SLOT GLYPH_FADE_UPPER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_LOWER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_FULL
    ; Stage 3: only red bricks remain, with blank mortar gaps.
    .byte $f3,$f3,$00,$3f,$3f,$00,$f3,$f3
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_UPPER
    .res 8, 0
    GRAPHICS_SLOT GLYPH_FADE_BROKEN_LOWER
    .res 8, 0
    .res SOURCE_CHARSET_BYTES-(*-graphics_charset), 0
graphics_charset_end:
.delmacro GRAPHICS_SLOT
.delmacro BRICK_GLYPH
.assert graphics_charset = GRAPHICS_CHARSET_ADDRESS, error, "Graphics source moved"
.assert graphics_charset_end-graphics_charset = SOURCE_CHARSET_BYTES, error, "Graphics source must be 2 KB"
