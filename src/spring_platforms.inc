; Compresses a spring on landing, waits briefly, then restores it and launches
; the player. Walking off before launch cancels the bounce and restores the glyphs.
; Full, upper and lower forms are updated in both screens and the row description.
;
; Quick reference:
;   line 17: normal/compressed fragments
;   line 59: reset, compression and cancellation
;   line 106: launch timing
;   line 132: scroll tracking
;   line 151: screen-code changes

.segment "GAMEPLAY"

; Use video.inc's fragment_split/fragment_count to rebuild both normal and compressed
; upper/lower fragments. Zero is black background; copied 1 bits display the purple
; spring.
update_spring_fragment_glyphs:
    ; Input: fragment_split, fragment_count. Output: four writable dynamic glyphs. Clear
    ; first so uncopied parts remain transparent; place the source head in UPPER and
    ; tail in LOWER. Generate normal/compressed shapes in the same loop to reduce loop
    ; overhead.
    ldy #0
    lda #$00
@clear:
    sta spring_upper_glyph, y
    sta spring_lower_glyph, y
    sta spring_compressed_upper_glyph, y
    sta spring_compressed_lower_glyph, y
    iny
    cpy #8
    bne @clear

    ldx #0
    ldy fragment_split
@upper:
    lda spring_source_glyph, x
    sta spring_upper_glyph, y
    lda spring_compressed_source_glyph, x
    sta spring_compressed_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ldx fragment_count
    ldy #0
@lower:
    lda spring_source_glyph, x
    sta spring_lower_glyph, y
    lda spring_compressed_source_glyph, x
    sta spring_compressed_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

; Reset compression, support, and upward-speed state for a new round.
initialize_spring_platforms:
    ; Clear compression countdown and rise velocity for a new game so old bounce state
    ; is not inherited.
    lda #0
    sta player_on_spring
    sta player_rise_velocity
    sta spring_platform_row
    sta spring_compression_timer
    rts

; collision_row always identifies the logical anchor containing the lower fragment/full
; character.
compress_spring_platform:
    ; Input: collision_row. If not already compressed, record the anchor, load an
    ; eight-frame timer, and replace the platform's three normal codes with compressed
    ; codes in both screens.
    lda player_on_spring
    bne @done
    lda collision_row
    sta spring_platform_row
    lda #SPRING_COMPRESSION_FRAMES
    sta spring_compression_timer
    lda #1
    sta player_on_spring
    lda #SPRING_EFFECT_COMPRESS
    sta spring_effect_mode
    jmp apply_spring_effect_to_buffers
@done:
    rts

; Walking off the spring restores its appearance without launching the player.
cancel_spring_platform:
    ; Call when the player walks off before launch: restore glyphs and clear the timer
    ; without setting rise velocity.
    lda player_on_spring
    beq @done
    lda #SPRING_EFFECT_RESTORE
    sta spring_effect_mode
    jsr apply_spring_effect_to_buffers
    lda #0
    sta player_on_spring
    sta spring_compression_timer
@done:
    rts

; After compression expires, restore relaxed glyphs and enter the separate upward-motion
; state.
update_spring_platform:
    ; Call each frame. At zero, restore relaxed glyphs, release support, and set
    ; player_rise_velocity; the next frame's update_player_vertical performs actual
    ; upward movement.
    lda player_on_spring
    beq @done
    dec spring_compression_timer
    bne @done

    lda #SPRING_EFFECT_RESTORE
    sta spring_effect_mode
    jsr apply_spring_effect_to_buffers

    lda #0
    sta player_on_spring
    sta player_on_platform
    sta player_on_fade_platform
    sta player_fall_fraction
    sta player_fall_velocity
    lda #SPRING_LAUNCH_VELOCITY
    sta player_rise_velocity
@done:
    rts

; After each coarse scroll, move the anchor up one row. When it leaves the playfield,
; the newly visible screen omits it, so discard the spring lifecycle.
advance_spring_platform_scroll_position:
    ; Decrement the anchor, or discard state when it would move above row 3.
    lda player_on_spring
    beq @done
    lda spring_platform_row
    cmp #(PLAYFIELD_TOP_ROW+1)
    bcs @move_up
    lda #0
    sta player_on_spring
    sta spring_compression_timer
    rts
@move_up:
    dec spring_platform_row
@done:
    rts

; One buffer may hold full glyphs while the other holds fragments. Scan three rows
; around the anchor and convert only the spring's six codes, leaving neighboring
; platforms unchanged.
apply_spring_effect_to_buffers:
    ; Input: spring_platform_row, spring_effect_mode. Output: synchronize the three rows
    ; around the anchor in A/B. Color RAM stays purple because compression does not
    ; change color.
    jsr update_spring_row_description
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B

    ldx spring_platform_row
    dex
@seek_first_row:
    cpx #0
    beq @pointers_ready
    jsr advance_copy_pointers
    dex
    bne @seek_first_row
@pointers_ready:
    lda #3
    sta spring_rows_remaining

@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    jsr transform_spring_character
    sta (ROW_PTR), y
    lda (DEST_PTR), y
    jsr transform_spring_character
    sta (DEST_PTR), y
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column

    jsr advance_copy_pointers
    dec spring_rows_remaining
    bne @row
    rts

transform_spring_character:
    ; Input/output: A=screen code. mode=0 maps normal to compressed; mode=1 reverses the
    ; mapping. Return non-spring characters unchanged.
    sta spring_char_work
    lda spring_effect_mode
    bne @restore

@compress:
    lda spring_char_work
    cmp #CHAR_SPRING_FULL
    beq @compressed_full
    cmp #CHAR_SPRING_UPPER
    beq @compressed_upper
    cmp #CHAR_SPRING_LOWER
    bne @unchanged
    lda #CHAR_SPRING_COMPRESSED_LOWER
    rts
@compressed_full:
    lda #CHAR_SPRING_COMPRESSED_FULL
    rts
@compressed_upper:
    lda #CHAR_SPRING_COMPRESSED_UPPER
    rts

@restore:
    lda spring_char_work
    cmp #CHAR_SPRING_COMPRESSED_FULL
    beq @full
    cmp #CHAR_SPRING_COMPRESSED_UPPER
    beq @upper
    cmp #CHAR_SPRING_COMPRESSED_LOWER
    bne @unchanged
    lda #CHAR_SPRING_LOWER
    rts
@full:
    lda #CHAR_SPRING_FULL
    rts
@upper:
    lda #CHAR_SPRING_UPPER
    rts

@unchanged:
    lda spring_char_work
    rts

.assert * <= $3800, error, "gameplay code exceeds reserved memory"
