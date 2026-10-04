; Adds one point on the first landing on each platform, including spikes.
; Row claim flags prevent repeat points after a spring bounce or another landing.
; Six separate decimal digits drive the HUD and leaderboard. Score thresholds
; 20/40/60/80 increase scrolling speed and music tempo together.
;
; Quick reference:
;   line 15: score and speed reset
;   line 49: scroll phase accumulator
;   line 64: first-landing claims and scoring
;   line 127: difficulty thresholds
;   line 153: HUD digits and speed tables

.segment "CODE"

initialize_score:
    ; Start at 000000 with no claimed platforms and the base scroll/music speed.
    ; Draw SCORE: and its digits on separate HUD rows.
    lda #0
    sta scroll_accumulator
    ldx #SCORE_DIGIT_COUNT-1
@clear_digits:
    sta score_digits, x
    dex
    bpl @clear_digits

    ldx #PLAYFIELD_ROW_COUNT-1
@clear_platform_rows:
    sta platform_score_rows, x
    dex
    bpl @clear_platform_rows

    lda #SCROLL_RATE_LEVEL_0
    sta scroll_rate
    lda #MUSIC_TEMPO_LEVEL_0
    sta music_tempo_increment

    ldx #0
@label:
    lda score_label, x
    STORE_SCREEN_PAIR SCORE_DISPLAY_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+SCORE_DISPLAY_OFFSET, x
    sta COLOR_RAM+SCORE_DIGITS_OFFSET, x
    inx
    cpx #SCORE_DIGIT_COUNT
    bne @label
    jmp render_score_digits

scroll_step_ready:
    ; Input: scroll_accumulator, scroll_rate. Output: updated accumulator; C=1 means
    ; call scroll_one_pixel_up this frame, C=0 holds position. Maximum speed uses rate=0
    ; as the 256/256 marker and always returns C=1; other tiers use the accumulator.
    lda scroll_rate
    beq @every_frame
    clc
    adc scroll_accumulator
    sta scroll_accumulator
    rts
@every_frame:
    sec
    rts

; Award a point only if collision_row names an unclaimed platform row.
award_platform_landing_score:
    ; collision_row is an absolute screen row; subtract PLAYFIELD_TOP_ROW for playfield
    ; index 0..19. Return immediately for empty, out-of-range, or already-claimed rows.
    lda collision_row
    sec
    sbc #PLAYFIELD_TOP_ROW
    bcc @done
    cmp #PLAYFIELD_ROW_COUNT
    bcs @done
    tax
    lda platform_score_rows, x
    beq @done
    and #PLATFORM_SCORE_CLAIMED_MASK
    bne @done
    lda #PLATFORM_SCORE_CLAIMED
    sta platform_score_rows, x
    jmp add_platform_score
@done:
    rts

advance_platform_score_rows:
    ; Copy entries 1..19 to 0..18 and clear the last row. If generate_bottom_row creates
    ; a platform, it then marks that row PLATFORM_SCORE_UNCLAIMED.
    ldx #0
@shift:
    lda platform_score_rows+1, x
    sta platform_score_rows, x
    inx
    cpx #PLAYFIELD_ROW_COUNT-1
    bne @shift
    lda #0
    sta platform_score_rows+PLAYFIELD_ROW_COUNT-1
    rts

; Add one newly landed platform, update difficulty, and redraw six digits.
add_platform_score:
    ; Carry decimal digits from units leftward; separate digit bytes require only CMP
    ; #10.
    ldx #SCORE_DIGIT_COUNT-1
@increment:
    inc score_digits, x
    lda score_digits, x
    cmp #10
    bcc @updated
    lda #0
    sta score_digits, x
    dex
    bpl @increment

    ; Saturate at 999999 instead of wrapping to 000000 and reducing speed.
    lda #9
    ldx #SCORE_DIGIT_COUNT-1
@saturate:
    sta score_digits, x
    dex
    bpl @saturate

@updated:
    jsr award_health_for_score
    jsr update_scroll_rate_from_score
    jsr update_platform_generation_level
    jmp render_score_digits

update_scroll_rate_from_score:
    ; At 100+ retain maximum speed. For 0..99, compare only the tens digit against
    ; 2/4/6/8; X becomes the number of thresholds crossed, the speed-table index 0..4.
    lda score_digits
    ora score_digits+1
    ora score_digits+2
    ora score_digits+3
    bne @maximum
    lda score_digits+4
    ldx #0
@threshold:
    cmp score_level_tens, x
    bcc @load
    inx
    cpx #SCORE_LEVEL_THRESHOLD_COUNT
    bne @threshold
@maximum:
    ldx #SCROLL_LEVEL_MAX
@load:
    lda scroll_rate_table, x
    sta scroll_rate
    lda music_tempo_rate_table, x
    sta music_tempo_increment
    rts

; Copy the six decimal digits to both screen buffers as custom glyph codes.
render_score_digits:
    ; Update both screens together or the next $d018 flip briefly displays an old score.
    ldx #0
@digit:
    lda score_digits, x
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR SCORE_DIGITS_OFFSET, x
    inx
    cpx #SCORE_DIGIT_COUNT
    bne @digit
    rts

score_label:
    .byte CHAR_S, CHAR_C, CHAR_O, CHAR_R, CHAR_E, CHAR_COLON
.assert *-score_label = SCORE_DIGIT_COUNT, error, "Score label/color span mismatch"

scroll_rate_table:
    .byte SCROLL_RATE_LEVEL_0, SCROLL_RATE_LEVEL_1, SCROLL_RATE_LEVEL_2
    .byte SCROLL_RATE_LEVEL_3, SCROLL_RATE_LEVEL_4
scroll_rate_table_end:
.assert scroll_rate_table_end-scroll_rate_table = SCROLL_LEVEL_MAX+1, error, "scroll rate table size mismatch"

; Use the same index as scrolling's 1/1.25/1.5/1.75/2 multipliers. Modulo-512 phase
; represents 55 and 77 exactly for quarter-step multipliers, without alternating integer
; rates.
music_tempo_rate_table:
    .byte MUSIC_TEMPO_LEVEL_0, MUSIC_TEMPO_LEVEL_1, MUSIC_TEMPO_LEVEL_2
    .byte MUSIC_TEMPO_LEVEL_3, MUSIC_TEMPO_LEVEL_4
music_tempo_rate_table_end:
.assert music_tempo_rate_table_end-music_tempo_rate_table = SCROLL_LEVEL_MAX+1, error, "music tempo table size mismatch"

score_level_tens:
    .byte 2, 4, 6, 8
score_level_tens_end:
.assert score_level_tens_end-score_level_tens = SCORE_LEVEL_THRESHOLD_COUNT, error, "score threshold table size mismatch"
