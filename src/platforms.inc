; Generates platform type, width and position from one 16-bit random state.
; The same seed and call order reproduce the sequence. Difficulty selects the
; type pool; drawing also records the row description for hidden-screen rebuilds.
; randomize_platform fills platform_start/width/type. draw_platform uses them
; and ROW_PTR; random_byte returns its sample in A.
;
; Quick reference:
;   line 17: initial rows and safe spawn platform
;   line 70: seed and random-byte generator
;   line 127: difficulty and type-pool setup
;   line 173: platform selection
;   line 253: weighted candidate pools
;   line 279: platform drawing and row registration

.segment "CODE"

seed_platforms:
    ; SCREEN_A is clear and seed_random has set the random state.
    ; Fill the five starting rows, ending with a safe spawn platform.
    ; Keep X intact across calls: it also indexes the paired row-address tables.
    ldx #0
@loop:
    lda seed_platform_address_low, x
    sta ROW_PTR
    lda seed_platform_address_high, x
    sta ROW_PTR+1
    jsr randomize_platform

    ; Normal builds force a safe normal spawn platform; effect-test builds override it.
    cpx #SEED_PLATFORM_COUNT-1
    bne @draw
.ifdef FADE_PLATFORM_TEST
    ; The automated build uses a fade spawn platform to exercise activation, crumbling,
    ; and deletion.
    lda #PLATFORM_TYPE_FADE
.elseif .defined(SPRING_PLATFORM_TEST)
    ; The automated build uses a spring spawn platform for direct compression/bounce
    ; observation.
    lda #PLATFORM_TYPE_SPRING
.else
    lda #PLATFORM_TYPE_NORMAL
.endif
    sta platform_type
@draw:
    ; randomize_platform proposes a candidate; test configuration may override the spawn
    ; platform above. Update consecutive-type history only after the final platform_type
    ; is settled.
    jsr commit_generated_platform_type
    jsr draw_platform

    ; Store scoring state by relative playfield row. Mark the spawn platform already
    ; claimed, preventing a point when returning to it after departure; other initial
    ; platforms await first landing.
    ldy seed_platform_score_index, x
    lda #PLATFORM_SCORE_UNCLAIMED
    cpx #SEED_PLATFORM_COUNT-1
    bne @store_score_state
    lda #PLATFORM_SCORE_CLAIMED
@store_score_state:
    sta platform_score_rows, y

    inx
    cpx #SEED_PLATFORM_COUNT
    bne @loop
    rts

; Mix clock, CIA timers, and raster for a seed. Two RAM bytes are loaded program data,
; fixed mixing values rather than power-up RAM entropy; the KERNAL clock stops after
; game IRQ takeover.
seed_random:
    ; Mix startup/load timing into random_state_low/high so rounds get different layouts.
    ; Avoid the all-zero state, which would stop the generator.
    lda KERNAL_JIFFY_LOW
    eor CIA1_TIMER_A_LOW
    eor CIA1_TOD_TENTHS
    eor VIC_RASTER
    eor $3a23
    sta random_state_low

    lda KERNAL_JIFFY_MIDDLE
    eor CIA1_TIMER_A_HIGH
    eor CIA1_TIMER_B_LOW
    eor CIA1_TOD_SECONDS
    eor $3f57
    sta random_state_high

    lda KERNAL_JIFFY_HIGH
    eor CIA1_TOD_MINUTES
    eor random_state_low
    sta random_state_low

    lda random_state_low
    ora random_state_high
    bne @done
    lda #$a7
    sta random_state_low
    lda #$5c
    sta random_state_high
@done:
    rts

; 16-bit Galois LFSR, polynomial $b400; advance eight bits per call and return a mixed
; byte in A.
random_byte:
    ; Advance eight bits to reduce adjacent output overlap; mix the high/low state bytes
    ; for the return value. Preserve X used by seed_platforms' address-table traversal;
    ; Y counts eight feedback steps.
    ldy #8
@step:
    lsr random_state_high
    ror random_state_low
    bcc @next_step
    lda random_state_high
    eor #$b4
    sta random_state_high
@next_step:
    dey
    bne @step

    lda random_state_low
    eor random_state_high
    rts

.segment "EXTCODE"

; Reset score-gated type selection and consecutive-platform history.
initialize_platform_generation:
    lda #0
    sta platform_generation_level
    sta generated_type_run_length
    lda #$ff
    sta last_generated_platform_type
    rts

; Choose five pools by first-landing score: 0..4, 5..9, 10..19, 20..39, and 40+. Any
; nonzero upper four decimal digits guarantees a score above 40; use the highest tier.
update_platform_generation_level:
    lda score_digits
    ora score_digits+1
    ora score_digits+2
    ora score_digits+3
    bne @level_4

    lda score_digits+4
    beq @under_ten
    cmp #1
    beq @level_2
    cmp #4
    bcs @level_4
    lda #3
    bne @store

@under_ten:
    lda score_digits+5
    cmp #5
    bcc @level_0
    lda #1
    bne @store
@level_0:
    lda #0
    beq @store
@level_2:
    lda #2
    bne @store
@level_4:
    lda #PLATFORM_GENERATION_LEVEL_MAX
@store:
    sta platform_generation_level
    rts

; Widths are 7..12 characters; platforms stay between the white frame and divider, never
; overwriting UI characters.
randomize_platform:
    ; Output: platform_width = 7..12 characters; platform_start = a valid start column
    ; that keeps this width inside the playfield; platform_type = a weighted type
    ; permitted by the current score tier. Rejection sampling avoids modulo bias when
    ; width/position ranges are not powers of two.
@choose_width:
    jsr random_byte
    and #%00000111
    cmp #PLATFORM_WIDTH_COUNT
    bcs @choose_width
    clc
    adc #PLATFORM_MIN_WIDTH
    sta platform_width

    ; Available columns are PLAYFIELD_FIRST_COLUMN .. UI_DIVIDER_COLUMN-1. limit counts
    ; valid starts for this width; add the left inset to the sampled value.
    lda #UI_DIVIDER_COLUMN-PLAYFIELD_FIRST_COLUMN+1
    sec
    sbc platform_width
    sta platform_start_limit

@choose_start:
    jsr random_byte
    and #%00011111
    cmp platform_start_limit
    bcs @choose_start
    clc
    adc #PLAYFIELD_FIRST_COLUMN
    sta platform_start

    ; Type pools have length 4 or 8, allowing unbiased indexing with AND. Each tier's
    ; offset and mask map a random byte into its weighted pool. Normal platforms may
    ; repeat freely; after two repeats of another type, consume a new PRNG byte and
    ; retry to prevent a third.
@choose_type:
    jsr random_byte
    ldy platform_generation_level
    and platform_type_pool_mask, y
    clc
    adc platform_type_pool_offset, y
    tay
    lda platform_type_pool, y
    sta platform_type
    beq @type_ready
    cmp last_generated_platform_type
    bne @type_ready
    lda generated_type_run_length
    cmp #PLATFORM_REPEAT_LIMIT
    bcs @choose_type
@type_ready:
    rts

; Only platforms actually drawn enter history. Keep repeated-normal run length at 1,
; recording the latest type without overflowing in long rounds; normal types never check
; this limit.
commit_generated_platform_type:
    lda platform_type
    cmp last_generated_platform_type
    bne @new_type
    cmp #PLATFORM_TYPE_NORMAL
    beq @normal_repeat
    inc generated_type_run_length
    rts
@normal_repeat:
    lda #1
    sta generated_type_run_length
    rts
@new_type:
    sta last_generated_platform_type
    lda #1
    sta generated_type_run_length
    rts

; Normal shares in the candidate pools: 75%, 50%, 37.5%, 25%, 12.5%. Rejecting a third
; identical non-normal type changes the accepted distribution. Unlock other types
; progressively: spring -> fade -> both conveyors -> spikes.
platform_type_pool_offset:
    .byte 0, 4, 8, 16, 24
platform_type_pool_mask:
    .byte 3, 3, 7, 7, 7
platform_type_pool:
    ; Score 0..4: 75% normal, 25% spring.
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_SPRING
    ; Score 5..9: 50% normal, 25% fade, 25% spring.
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING
    ; Score 10..19: 37.5% normal; unlock left/right conveyors.
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING, PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
    ; Score 20..39: 25% normal; unlock spikes for the first time.
    .byte PLATFORM_TYPE_NORMAL, PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_SPIKES, PLATFORM_TYPE_FADE, PLATFORM_TYPE_FADE
    .byte PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
    ; Score 40+: 12.5% normal; increase spike/fade weights.
    .byte PLATFORM_TYPE_NORMAL
    .byte PLATFORM_TYPE_SPIKES, PLATFORM_TYPE_SPIKES
    .byte PLATFORM_TYPE_FADE, PLATFORM_TYPE_FADE, PLATFORM_TYPE_SPRING
    .byte PLATFORM_TYPE_CONVEYOR_LEFT, PLATFORM_TYPE_CONVEYOR_RIGHT
platform_type_pool_end:
.assert platform_type_pool_end-platform_type_pool = 32, error, "platform type pool size mismatch"

.segment "CODE"

draw_platform:
    ; Input: ROW_PTR points to the target character row; platform_start/width/type are
    ; ready.
    ; Output: write the target screen buffer and shared Color RAM. Springs use purple;
    ; conveyors use white/blue multicolor, and brick/fade platforms also set bit 3 for
    ; multicolor. Build COLOR_PTR using the matching low ten address bits of SCREEN_A,
    ; SCREEN_B, and Color RAM.
    jsr record_platform_row
    ldy platform_type
    lda platform_chars, y
    sta selected_platform_char

    ; Lookup preserves X, which seed_platforms uses as its row index.
    lda platform_colors, y
    sta selected_platform_color

    lda ROW_PTR
    sta COLOR_PTR
    lda ROW_PTR+1
    and #%00000011
    ora #>COLOR_RAM
    sta COLOR_PTR+1

    ldy platform_start
    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    iny

    lda platform_width
    sec
    sbc #2
    sta platform_middle_count
@middle:
    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    iny
    dec platform_middle_count
    bne @middle

    lda selected_platform_char
    sta (ROW_PTR), y
    lda selected_platform_color
    sta (COLOR_PTR), y
    rts
