; Awards one HP for every three scored platforms, before landing damage is handled.
; HP absorbs spike and ceiling hits. Falling out of the bottom ends the round
; regardless of HP. Recovery moves the player away from the contact surface
; so the same hit is not repeated on the next frame.
;
; Quick reference:
;   line 17: reset and initial display
;   line 43: score rewards
;   line 60: HP consumption
;   line 75: platform-spike bounce
;   line 92: HP digits
;   line 148: ceiling recovery

.segment "EXTCODE"

; Reset HP/progress and draw the centered HP:000 line in both screen buffers.
initialize_health:
    lda #0
    sta health_points
    sta health_score_progress
    sta health_decimal_work
    sta health_hundreds
    sta health_tens
    sta health_ones

    lda #CHAR_H
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET
    lda #CHAR_P
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+1
    lda #CHAR_COLON
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+2

    lda #COLOR_WHITE
    ldx #0
@color:
    sta COLOR_RAM+HP_DISPLAY_OFFSET, x
    inx
    cpx #3+HP_DIGIT_COUNT
    bne @color
    jmp update_health_display

; Every third platform point adds one HP, capped at 255; score is unchanged.
award_health_for_score:
    inc health_score_progress
    lda health_score_progress
    cmp #SCORES_PER_HP
    bcc @done
    lda #0
    sta health_score_progress

    lda health_points
    cmp #$ff
    beq @done
    inc health_points
    jmp update_health_display
@done:
    rts

; Consume one HP and return C=1, or request game over and return C=0.
consume_health_or_game_over:
    lda health_points
    beq @fatal
    dec health_points
    lda #PLAYER_HURT_FRAMES
    sta player_hurt_timer
    jsr update_health_display
    sec
    rts
@fatal:
    jsr request_game_over
    clc
    rts

; Spend HP for spike contact and bounce upward to prevent repeat damage.
handle_spike_damage:
    jsr consume_health_or_game_over
    bcc @done
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_on_spring
    sta player_fall_fraction
    sta player_fall_velocity
    sta conveyor_move_counter
    lda #SPIKE_DAMAGE_BOUNCE_VELOCITY
    sta player_rise_velocity
@done:
    rts

; Convert binary HP 0..255 to three decimal character cells in both buffers.
update_health_display:
    lda health_points
    sta health_decimal_work
    ldx #0
@hundreds:
    lda health_decimal_work
    cmp #100
    bcc @hundreds_done
    sec
    sbc #100
    sta health_decimal_work
    inx
    bne @hundreds
@hundreds_done:
    stx health_hundreds

    ldx #0
@tens:
    lda health_decimal_work
    cmp #10
    bcc @tens_done
    sec
    sbc #10
    sta health_decimal_work
    inx
    bne @tens
@tens_done:
    stx health_tens
    lda health_decimal_work
    sta health_ones

    lda health_hundreds
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+3
    lda health_tens
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+4
    lda health_ones
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR HP_DISPLAY_OFFSET+5
    rts

.assert * <= PLAYER_HURT_ADDRESS, error, "HP code overlaps hurt sprite block"

; EXTCODE borders the hurt sprite; place ceiling recovery in available GAMEPLAY space
; without changing calls.
.segment "GAMEPLAY"

; Absorb a ceiling hit, cancel spring/support/momentum, and set player_y to
; PLAYER_CEILING_CONTACT_Y + CEILING_DAMAGE_DROP_PIXELS (73 + 8 = 81). This places the
; head below the tips; it is not a relative eight-pixel move. Changing Y alone leaves
; support/spring state active and can carry or launch the player into spikes again next
; frame.
handle_ceiling_damage:
    jsr consume_health_or_game_over
    bcc @done
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_on_spring
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_rise_velocity
    sta conveyor_move_counter
    lda #(PLAYER_CEILING_CONTACT_Y+CEILING_DAMAGE_DROP_PIXELS)
    sta player_y
    sta VIC_SPRITE_0_Y
@done:
    rts
