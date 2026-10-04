; Starts and updates a disappearing platform after the first landing.
; It crumbles at 25 frames and disappears at 50. Only one activated platform is
; tracked at a time; activating another removes the previous one.
; Effects change both screens and the row description used for reconstruction.
;
; Quick reference:
;   line 15: reset
;   line 30: first-landing activation
;   line 55: lifetime and crumble stages
;   line 78: scroll tracking and removal
;   line 130: screen-code changes

.segment "EFFECTS"

initialize_fade_platforms:
    ; Reset lifetime, crumble stage, and support flags for a new game; untriggered
    ; platforms need no registration.
    lda #0
    sta fade_platform_active
    sta fade_platform_row
    sta fade_platform_timer
    sta player_on_fade_platform
    lda #FADE_STAGE_CRACKED
    sta fade_stage
    rts

; collision_row is the logical anchor row containing the lower scrolling fragment. Use
; the same row when the platform is full; it survives pixel phases and decreases by one
; per coarse scroll.
activate_fade_platform:
    ; Input: collision_row identifies the newly contacted platform anchor.
    ; Output: record its row/lifetime and replace intact bricks with stage-2 glyphs in
    ; both screens.
    lda fade_platform_active
    beq @start
    ; Only one independent countdown is tracked. If the player quickly lands on another
    ; fade platform, delete the old one before starting a full countdown for the new
    ; one.
    jsr remove_active_fade_platform
@start:
    lda collision_row
    sta fade_platform_row
    lda #FADE_PLATFORM_LIFETIME
    sta fade_platform_timer
    lda #FADE_STAGE_CRACKED
    sta fade_stage
    lda #1
    sta fade_platform_active

    lda #FADE_EFFECT_ACTIVATE
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers

; Advance the countdown each frame; enter stage 3 at half lifetime and delete at zero.
update_fade_platform:
    ; Both fragment sets share the same scroll phase; crumbling changes appearance but
    ; retains support until deletion.
    lda fade_platform_active
    beq @done

    dec fade_platform_timer
    beq remove_active_fade_platform

    lda fade_platform_timer
    cmp #FADE_PLATFORM_BROKEN_TIME
    bne @done
    lda #FADE_STAGE_BROKEN
    sta fade_stage
    lda #FADE_EFFECT_ADVANCE
    sta fade_effect_mode
    jmp apply_fade_effect_to_buffers
@done:
    rts

; Coarse scrolling moves the full platform up one character row; move the effect anchor
; with it. When its anchor moves above PLAYFIELD_TOP_ROW (row 3), the newly visible
; screen omits it; stop the countdown and release any remaining fade support.
advance_fade_platform_scroll_position:
    ; Call after each eight-pixel coarse scroll. Screen contents have moved up one row,
    ; so decrement the stored logical anchor to keep stage changes/deletion on the same
    ; platform.
    lda fade_platform_active
    beq @done
    lda fade_platform_row
    cmp #(PLAYFIELD_TOP_ROW+1)
    bcs @move_up

    lda #0
    sta fade_platform_active
    jsr release_player_from_fade_platform
    rts
@move_up:
    dec fade_platform_row
@done:
    rts

remove_active_fade_platform:
    ; Clear all three glyph forms of the active platform in both buffers; if it still
    ; supports the player, release support so gravity resumes on the next frame.
    lda fade_platform_active
    beq @done
    lda #FADE_EFFECT_REMOVE
    sta fade_effect_mode
    jsr apply_fade_effect_to_buffers

    lda #0
    sta fade_platform_active
    sta fade_platform_timer
    jsr release_player_from_fade_platform
@done:
    rts

; Deleting a fading platform starts a fall only if that platform still supports the
; player. If the player has moved onto a normal platform, the old countdown must not
; affect the new support.
release_player_from_fade_platform:
    lda player_on_fade_platform
    beq @done
    lda #0
    sta player_on_fade_platform
    sta player_on_platform
    sta player_fall_fraction
    sta player_fall_velocity
@done:
    rts

; Buffers may retain different full/fragment forms, so scan three rows around the
; anchor. Platforms are spaced at least four rows apart, preventing this window from
; reaching another untriggered fade.
apply_fade_effect_to_buffers:
    ; Input: fade_platform_row, fade_effect_mode, fade_stage.
    ; Output: update SCREEN_A/B. Scan row-1,row,row+1 because a transitioning logical
    ; platform may occupy two rows and the buffers may hold different representation
    ; phases.
    jsr update_fade_row_description
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B

    ldx fade_platform_row
    dex
@seek_first_row:
    cpx #0
    beq @pointers_ready
    jsr advance_copy_pointers
    dex
    bne @seek_first_row
@pointers_ready:
    lda #3
    sta fade_rows_remaining

@row:
    ldy #PLAYFIELD_LAST_COLUMN
@column:
    lda (ROW_PTR), y
    jsr transform_fade_character
    sta (ROW_PTR), y

    lda (DEST_PTR), y
    jsr transform_fade_character
    sta (DEST_PTR), y

    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @column

    jsr advance_copy_pointers
    dec fade_rows_remaining
    bne @row
    rts

; Input/output: screen code in A. Activation converts full platforms only; stage changes
; and deletion match the two activated glyph sets explicitly, without bit masks or
; guessed ranges.
transform_fade_character:
    ; Input: A=screen code. Output: A=converted screen code. This is a pure
    ; single-character mapping routine. Outer code handles addressing/loops, so
    ; activation, stage changes, and deletion share one traversal framework.
    sta fade_char_work
    ldx fade_effect_mode
    beq @activate

    ; X identifies FULL/UPPER/LOWER across both exact active-code sets.
    ldx #2
@find_active:
    cmp fade_cracked_chars, x
    beq @active
    cmp fade_broken_chars, x
    beq @active
    dex
    bpl @find_active
    jmp @unchanged
@active:
    lda fade_effect_mode
    cmp #FADE_EFFECT_ADVANCE
    beq @select_stage
    lda #CHAR_BLANK
    rts

@select_stage:
    lda fade_stage
    beq @cracked
    lda fade_broken_chars, x
    rts
@cracked:
    lda fade_cracked_chars, x
    rts

@activate:
    lda fade_char_work
    ldx #2
@find_inactive:
    cmp fade_inactive_chars, x
    beq @select_stage
    dex
    bpl @find_inactive

@unchanged:
    lda fade_char_work
    rts

fade_inactive_chars:
    .byte CHAR_FADE_FULL, CHAR_FADE_UPPER, CHAR_FADE_LOWER
fade_cracked_chars:
    .byte CHAR_FADE_CRACKED_FULL, CHAR_FADE_CRACKED_UPPER, CHAR_FADE_CRACKED_LOWER
fade_broken_chars:
    .byte CHAR_FADE_BROKEN_FULL, CHAR_FADE_BROKEN_UPPER, CHAR_FADE_BROKEN_LOWER

.assert * <= $3000, error, "fade-platform effects overlap sprite memory"
