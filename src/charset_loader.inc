; Builds the live charset from the graphics and text sources when a scene starts.
; Only the live copy is changed during play. Glyphs are selected at assembly time;
; both source fonts travel with the PRG.
; Call with music/IRQs stopped. A/X/Y, flags and ROW_PTR/DEST_PTR may change;
; the display stays blank until the scene has finished drawing.
;
; Quick reference:
;   line 17: glyph-range copy macro
;   line 29: gameplay charset composition
;   line 46: menu text charset
;   line 54: modal-screen setup
;   line 84: eight-byte glyph copy loop

.segment "CHARSETCODE"

; Copy complete glyphs from [source_id, source_id+count) into the destination range.
.macro COPY_CHARSET_RANGE source_charset, source_id, destination_id, glyph_count
    .assert (glyph_count > 0) .and (glyph_count < 256), error, "Invalid glyph count"
    .assert source_id+glyph_count <= SOURCE_CHARSET_GLYPH_COUNT, error, "Source range overflow"
    .assert destination_id+glyph_count <= CHARSET_GLYPH_COUNT, error, "Live charset overflow"
    SET_WORD ROW_PTR, source_charset + source_id*CHARSET_GLYPH_BYTES
    SET_WORD DEST_PTR, CHARSET_ADDRESS + destination_id*CHARSET_GLYPH_BYTES
    ldx #glyph_count
    jsr copy_charset_glyphs
.endmacro

; Gameplay: copy 68 graphics slots, then import digits and H/O/P/T/X/colon/S/C/R/E from
; source 2. Slots 22..39 remain blank.
load_game_charset:
    jsr blank_charset_display
    COPY_CHARSET_RANGE graphics_charset, 0, 0, CHARSET_GLYPH_COUNT
    COPY_CHARSET_RANGE text_charset, TEXT_DIGIT_0, GLYPH_DIGIT_0, 10
    COPY_CHARSET_RANGE text_charset, TEXT_H, GLYPH_H, 1
    COPY_CHARSET_RANGE text_charset, TEXT_O, GLYPH_O, 2
    COPY_CHARSET_RANGE text_charset, TEXT_T, GLYPH_T, 1
    COPY_CHARSET_RANGE text_charset, TEXT_X, GLYPH_X, 1
    COPY_CHARSET_RANGE text_charset, TEXT_COLON, GLYPH_COLON, 1
    COPY_CHARSET_RANGE text_charset, TEXT_S, GLYPH_S, 1
    COPY_CHARSET_RANGE text_charset, TEXT_C, GLYPH_C, 1
    COPY_CHARSET_RANGE text_charset, TEXT_R, GLYPH_R, 1
    COPY_CHARSET_RANGE text_charset, TEXT_E, GLYPH_E, 1
    rts

; Menus: copy source 2's first 64 glyphs, including all letters, digits, and basic
; symbols.
load_text_charset:
    jsr blank_charset_display
    COPY_CHARSET_RANGE text_charset, 0, 0, TEXT_QUESTION+1
    rts
.delmacro COPY_CHARSET_RANGE

; Disable display and sprites, load the text page, and clear both matrices. Clear using
; text space (32), not game blank (0, which is @ in this font).
prepare_modal_screen:
    jsr load_text_charset
    lda #0
    sta VIC_SPRITE_ENABLE
    SHOW_SCREEN SCREEN_A_D018
    ldy #TEXT_SPACE
    jmp clear_screens_with_character

; Keep natural phase 3; gameplay and menus share display mode/D018, with Color RAM 0..7
; for text cells. Hide/show without selecting either source charset in D018.
blank_charset_display:
    lda #(VIC_CONTROL_1_TEXT & $ef)
    sta VIC_CONTROL_1
    lda #$18
    sta VIC_CONTROL_2
    lda #COLOR_BLACK
    sta VIC_BACKGROUND_COLOR
    lda #COLOR_WHITE
    sta VIC_MULTICOLOR_1
    lda #COLOR_GRAY
    sta VIC_MULTICOLOR_2
    rts
enable_charset_display:
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    rts

; Input: ROW_PTR=source, DEST_PTR=output, X=glyph count; X=0 copies nothing.
; Output: both pointers advance by initial X*8, X=0; write only the specified range,
; including across pages.
copy_charset_glyphs:
    cpx #0
    beq @done
@glyph:
    ldy #0
@byte:
    lda (ROW_PTR), y
    sta (DEST_PTR), y
    iny
    cpy #CHARSET_GLYPH_BYTES
    bne @byte
    clc
    lda ROW_PTR
    adc #CHARSET_GLYPH_BYTES
    sta ROW_PTR
    bcc @source_ready
    inc ROW_PTR+1
@source_ready:
    clc
    lda DEST_PTR
    adc #CHARSET_GLYPH_BYTES
    sta DEST_PTR
    bcc @destination_ready
    inc DEST_PTR+1
@destination_ready:
    dex
    bne @glyph
@done:
    rts
charset_code_end:
.assert charset_code_end <= $5400, error, "Charset loader exceeds reserved memory"
