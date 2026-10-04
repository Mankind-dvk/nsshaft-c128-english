; Animates the left/right conveyor textures while the platforms scroll upward.
; The texture advances two display pixels every three frames. player.inc applies
; the matching push to a rider. Full, upper and lower glyphs use separate slots
; so a screen flip keeps the texture intact.
;
; Quick reference:
;   line 15: texture reset
;   line 44: upper/lower fragment generation
;   line 86: full-glyph restoration
;   line 105: horizontal texture animation

.segment "GAMEPLAY"

; Reset horizontal phase, install FULL glyphs and clear all fragment slots.
initialize_conveyor_platforms:
    ; Both screens are already clear at new-game initialization, so old conveyor
    ; fragments can be reset safely. Full platforms use LEFT/RIGHT_FULL; UPPER/LOWER are
    ; generated on the first scroll step.
    lda #CONVEYOR_ANIMATION_FRAMES
    sta conveyor_animation_counter
    ldy #7
@reset_patterns:
    lda conveyor_left_source_glyph, y
    sta conveyor_left_pattern, y
    lda conveyor_right_source_glyph, y
    sta conveyor_right_pattern, y
    dey
    bpl @reset_patterns
    jsr restore_conveyor_full_glyphs
    ldy #7
    lda #$00
@clear_upper:
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_upper_glyph, y
    sta conveyor_left_lower_glyph, y
    sta conveyor_right_lower_glyph, y
    dey
    bpl @clear_upper
    rts

; Use fragment_split/fragment_count to split the current horizontal texture into upper
; and lower fragments across adjacent rows. Write only UPPER/LOWER; visible screens may
; still reference FULL.
update_conveyor_fragment_glyphs:
    ; First clear all four destination glyphs to black; rows not overwritten below
    ; remain clipped.
    ldy #7
    lda #$00
@clear:
    sta conveyor_left_lower_glyph, y
    sta conveyor_left_upper_glyph, y
    sta conveyor_right_lower_glyph, y
    sta conveyor_right_upper_glyph, y
    dey
    bpl @clear

    ; Copy the top fragment_count source rows into the previous character cell, starting
    ; at fragment_split.
    ldx #0
    ldy fragment_split
@upper:
    lda conveyor_left_pattern, x
    sta conveyor_left_upper_glyph, y
    lda conveyor_right_pattern, x
    sta conveyor_right_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ; Move the remaining source rows to the current cell's top, using an independent
    ; LOWER slot.
    ldx fragment_count
    ldy #0
@lower:
    lda conveyor_left_pattern, x
    sta conveyor_left_lower_glyph, y
    lda conveyor_right_pattern, x
    sta conveyor_right_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

restore_conveyor_full_glyphs:
    ; Call after eight-pixel coarse scrolling and before creating a bottom platform.
    ; Restore only FULL slots; preserve the old screen's UPPER/LOWER fragments. The next
    ; screen uses the full glyphs.
    ldy #7
@copy:
    lda conveyor_left_pattern, y
    sta conveyor_left_full_glyph, y
    lda conveyor_right_pattern, y
    sta conveyor_right_full_glyph, y
    dey
    bpl @copy
    rts

.assert * <= $3800, error, "conveyor-platform code exceeds reserved gameplay memory"

.segment "CHARSETCODE"

; One call per game frame; rotate paired bits without moving platform bounds.
update_conveyor_animation:
    dec conveyor_animation_counter
    bne @done
    lda #CONVEYOR_ANIMATION_FRAMES
    sta conveyor_animation_counter

    ; Put the outgoing edge bit in C, then rotate the RAM byte for a circular shift.
    ; Repeat twice per row to keep white/blue multicolor bit pairs intact.
    ldx #7
@rotate:
    lda conveyor_left_pattern, x
    asl
    rol conveyor_left_pattern, x
    lda conveyor_left_pattern, x
    asl
    rol conveyor_left_pattern, x
    lda conveyor_right_pattern, x
    lsr
    ror conveyor_right_pattern, x
    lda conveyor_right_pattern, x
    lsr
    ror conveyor_right_pattern, x
    dex
    bpl @rotate

    lda fine_scroll
    cmp #7
    beq @full
    clc
    adc #1
    sta fragment_split
    lda #7
    sec
    sbc fine_scroll
    sta fragment_count
    jmp update_conveyor_fragment_glyphs
@full:
    jmp restore_conveyor_full_glyphs
@done:
    rts

.assert * <= $5400, error, "conveyor animation exceeds charset-code memory"
