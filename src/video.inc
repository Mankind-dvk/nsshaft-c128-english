; Scrolling, screen-buffer swaps, fixed UI drawing and title-screen controls.
; D011 stays at phase 3; rebuilt upper/lower glyphs provide the pixel movement.
; Large transitions are drawn in the hidden screen before changing D018.
; Color RAM is shared, so its colors are updated separately from the screen flip.
;
; Quick reference:
;   line 16: world scroll and supported-player movement
;   line 66: vertical fragment generation
;   line 112: buffer copy and transition helpers
;   line 201: screen swap and new bottom row
;   line 253: title and FIRE controls
;   line 321: screen clearing and fixed UI

.segment "CODE"

scroll_one_pixel_up:
    ; Input: fine_scroll=0..7, active_screen, player_on_platform. Move a supported
    ; player up first. On ceiling contact, handle damage and return without advancing
    ; the world; otherwise scroll it upward one pixel. Phase 7 starts a row transition;
    ; phases 6..1 rebuild glyphs only; phase 0 completes coarse scrolling and flips
    ; screens.
    lda player_on_platform
    beq @scroll_playfield
    dec player_y
    lda player_y
    sta VIC_SPRITE_0_Y
    ; Check platform-carried ceiling contact within this pixel step instead of delaying
    ; damage/detection one frame.
    cmp #(PLAYER_CEILING_CONTACT_Y+1)
    bcs @scroll_playfield
    jmp handle_ceiling_damage

@scroll_playfield:
    lda fine_scroll
    cmp #7
    bne @fragments_exist

    dec fine_scroll
    jsr update_fragment_glyphs
    ; Buffer commit 1 (7 -> 6): build upper/lower fragments in the hidden screen before
    ; switching display.
    jsr start_fragment_transition
    jmp flip_hidden_screen

@fragments_exist:
    lda fine_scroll
    beq @coarse_step  ; Phase 0: complete the character-row transition.
    dec fine_scroll   ; Advance one pixel.
    jmp update_fragment_glyphs   ; Rebuild upper/lower glyph fragments.

@coarse_step:
    ; Buffer commit 2 (0 -> 7): move logical platforms up one row and rebuild full
    ; glyphs in the hidden screen. Finish the bottom row and state updates before
    ; flipping; the previous visible screen becomes the next back buffer.
    lda #7
    sta fine_scroll
    jsr advance_platform_score_rows
    jsr complete_fragment_transition
    jsr advance_fade_platform_scroll_position
    jsr advance_spring_platform_scroll_position
    jmp flip_hidden_screen

; VIC vertical scroll stays at natural phase 3 so all 25 character rows cover the
; visible area. Normal bricks and three fade stages have independent fragments; spikes,
; springs, and both conveyors each generate shape-preserving dynamic fragments.
update_fragment_glyphs:
    ; Input: fine_scroll. Output: rebuild normal, spike, fade, spring, and conveyor
    ; fragments. Changing glyph RAM updates every cell using that glyph without changing
    ; screen codes, so the six intermediate pixel phases need no full-matrix traversal.
    lda fine_scroll
    clc
    adc #1
    sta fragment_split
    lda #7
    sec
    sbc fine_scroll
    sta fragment_count

    jsr update_brick_and_fade_fragments

    ldy #0
    lda #$00
@clear_spikes:
    sta spike_upper_glyph, y
    sta spike_lower_glyph, y
    iny
    cpy #8
    bne @clear_spikes

    ldx #0
    ldy fragment_split
@spike_upper:
    lda spike_source_glyph, x
    sta spike_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @spike_upper

    ldx fragment_count
    ldy #0
@spike_lower:
    lda spike_source_glyph, x
    sta spike_lower_glyph, y
    inx
    iny
    cpx #8
    bne @spike_lower
    jsr update_conveyor_fragment_glyphs
    jmp update_spring_fragment_glyphs

prepare_screen_b_from_a:
copy_active_to_hidden:
    ; Input: active_screen. Output: an exact 1000-byte copy of the visible matrix in the
    ; hidden screen. Mainly used at new-game startup; ongoing transitions reconstruct
    ; the hidden playfield from platform descriptions.
    jsr set_screen_copy_pointers
    ldx #SCREEN_ROWS
@row:
    ldy #SCREEN_COLUMNS-1
@column:
    lda (ROW_PTR), y
    sta (DEST_PTR), y
    dey
    bpl @column
    jsr advance_copy_pointers
    dex
    bne @row
    rts

set_screen_copy_pointers:
    ; Output: ROW_PTR=visible screen, DEST_PTR=hidden screen. Shared indirect addressing
    ; avoids separate A->B and B->A copy loops.
    lda active_screen
    beq @a_to_b
    SET_WORD ROW_PTR, SCREEN_B
    SET_WORD DEST_PTR, SCREEN_A
    rts
@a_to_b:
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B
    rts

advance_copy_pointers:
    ; Advance ROW_PTR and DEST_PTR by 40. CLC before ADC; INC the high byte when the low
    ; byte overflows, the standard 6502 pattern for 16-bit addition.
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @source_ready
    inc ROW_PTR+1
@source_ready:
    clc
    lda DEST_PTR
    adc #SCREEN_COLUMNS
    sta DEST_PTR
    bcc @done
    inc DEST_PTR+1
@done:
    rts

advance_transition_pointers:
    ; During UI construction, NEXT_ROW_PTR points to Color RAM; advance all three row
    ; pointers together.
    jsr advance_copy_pointers
    clc
    lda NEXT_ROW_PTR
    adc #SCREEN_COLUMNS
    sta NEXT_ROW_PTR
    bcc @done
    inc NEXT_ROW_PTR+1
@done:
    rts

; Rebuild hidden LOWER/UPPER spans from platform descriptions, not screen cells.
start_fragment_transition:
    jmp render_hidden_platforms

; When fine_scroll goes from 0 to 7, UPPER becomes a full character one row higher and
; LOWER disappears; a new platform can then enter at the bottom. Shift descriptions,
; rebuild FULL spans, and generate the new bottom row.
complete_fragment_transition:
    ; Shift canonical rows alongside score claims, then rebuild FULL spans.
    jsr advance_platform_rows
    jsr render_hidden_platforms

    ; Update independent conveyor FULL slots before flipping, preserving LOWER fragments
    ; referenced by the old screen. Use the current horizontal phase rather than
    ; resetting surface animation to its source pattern.
    jsr restore_conveyor_full_glyphs

    lda active_screen
    beq @bottom_b
    SET_WORD ROW_PTR, (SCREEN_A + PLAYFIELD_BOTTOM_OFFSET)
    jmp generate_bottom_row
@bottom_b:
    SET_WORD ROW_PTR, (SCREEN_B + PLAYFIELD_BOTTOM_OFFSET)
    jmp generate_bottom_row

flip_hidden_screen:
    ; Finish hidden-screen rendering before calling; this routine neither copies screens
    ; nor waits for the raster. main_loop's wait_for_frame provides synchronization;
    ; scrolling processing performs the later flip. active_screen ^= 1; EOR #1 toggles
    ; 0/1, then $d018 selects the corresponding matrix. The charset address is
    ; unchanged, so two precomputed layouts suffice.
    lda active_screen
    eor #1
    sta active_screen
    beq @show_screen_a

    SHOW_SCREEN SCREEN_B_D018
    rts

@show_screen_a:
    SHOW_SCREEN SCREEN_A_D018
    rts

generate_bottom_row:
    ; Input: ROW_PTR points to the hidden screen's bottom playfield row after coarse
    ; scrolling.
    ; Output: clear that row; when rows_until_platform=0, generate a random platform and
    ; reload spacing.
    lda #CHAR_BLANK
    ldy #PLAYFIELD_LAST_COLUMN
@clear:
    sta (ROW_PTR), y
    dey
    cpy #PLAYFIELD_FIRST_COLUMN
    bcs @clear

    lda rows_until_platform
    beq @add_platform
    dec rows_until_platform
    rts

@add_platform:
    jsr randomize_platform
    jsr commit_generated_platform_type
    jsr draw_platform

    ; The new platform occupies the last logical playfield row, awaiting its
    ; first-landing point.
    lda #PLATFORM_SCORE_UNCLAIMED
    sta platform_score_rows+PLAYFIELD_ROW_COUNT-1

    lda #PLATFORM_GAP_ROWS
    sta rows_until_platform
    rts

; Non-scrolling start and game-over screens

show_start_screen:
    ; Modal screen: disable sprites, select SCREEN_A, clear both screens, then draw
    ; title/prompt.
    jsr prepare_modal_screen

    ldx #0
@title:
    lda start_title, x
    STORE_SCREEN_PAIR TITLE_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+TITLE_OFFSET, x
    inx
    cpx #8
    bne @title
    jsr draw_action_prompt
    jmp enable_charset_display

; Compatibility entry point for the current text-only leaderboard screen.
show_game_over_screen:
    jmp show_highscore_table
draw_action_prompt:
    ; Write the same ten-character prompt to A/B so either previous D018 value yields
    ; consistent menu contents.
    ldx #0
@prompt:
    lda action_prompt, x
    STORE_SCREEN_PAIR PROMPT_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+PROMPT_OFFSET, x
    inx
    cpx #10
    bne @prompt
    rts

; Accept paddle buttons 1/2 and joystick FIRE on control port 1. Require release before
; and after the press to prevent one action from skipping two screens.
wait_for_action_button:
    ; Wait for released -> pressed -> released, providing software debounce and edge
    ; triggering. Waiting only for a press would let a held button from the previous
    ; screen skip the next.
@wait_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_release
@wait_press:
    jsr wait_for_frame
    jsr action_button_pressed
    bcc @wait_press
@wait_final_release:
    jsr wait_for_frame
    jsr action_button_pressed
    bcs @wait_final_release
    rts

action_button_pressed:
    ; Output: C=1 if any action button is pressed, C=0 if all are released. Inputs are
    ; active-low, so all PB2..PB4 bits must be 1 to indicate no press.
    lda CIA1_PORT_B
    and #%00011100
    cmp #%00011100
    beq @not_pressed
    sec
    rts
@not_pressed:
    clc
    rts

; Clear both 1 KiB matrices and the complete 1 KiB Color RAM region.
clear_screens_and_colors:
    ldy #CHAR_BLANK
; Reset platform descriptions; Y is the fill code (game 0 or text space 32).
clear_screens_with_character:
    ; X increments from 0 through wraparound, covering 256 offsets; four pages cover
    ; each 1 KB screen and Color RAM. The last 24 invisible bytes, including sprite
    ; pointer tables, are filled too, so initialize_player/dashboard must restore
    ; pointers afterward.
    tya
    pha
    jsr reset_platform_rows
    pla
    tay
    ldx #0
@loop:
    tya
    sta SCREEN_A+$000, x
    sta SCREEN_A+$100, x
    sta SCREEN_A+$200, x
    sta SCREEN_A+$300, x
    sta SCREEN_B+$000, x
    sta SCREEN_B+$100, x
    sta SCREEN_B+$200, x
    sta SCREEN_B+$300, x

    lda #COLOR_BLACK
    sta COLOR_RAM+$000, x
    sta COLOR_RAM+$100, x
    sta COLOR_RAM+$200, x
    sta COLOR_RAM+$300, x
    inx
    bne @loop
    rts

; Fixed character frame: black hardware border, brick rows 1/23 and columns 1/38. Column
; 29 separates the playfield from the eight-column HUD; ceiling spikes occupy playfield
; row 2. Rows 0/24 and columns 0/39 remain black, matching the reference image's outer
; margin.
initialize_ui:
    ; Draw the same frame, divider and ceiling spikes in both screens.
    ; Start with the top and bottom rows, then walk down the side/divider columns.
    ldx #0
@horizontal:
    lda #CHAR_UI_FRAME
    STORE_SCREEN_PAIR UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    STORE_SCREEN_PAIR UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    lda #UI_FRAME_COLOR
    sta COLOR_RAM+UI_FRAME_TOP_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    sta COLOR_RAM+UI_FRAME_BOTTOM_ROW*SCREEN_COLUMNS+UI_FRAME_LEFT_COLUMN, x
    inx
    cpx #UI_FRAME_WIDTH
    bne @horizontal

    ; Then draw the left frame, divider, and right frame row by row. The pointers
    ; address screen A, screen B, and Color RAM; skip row 0 to reach the first frame
    ; row, 1.
    SET_WORD ROW_PTR, SCREEN_A
    SET_WORD DEST_PTR, SCREEN_B
    SET_WORD NEXT_ROW_PTR, COLOR_RAM
    jsr advance_transition_pointers

    ldx #UI_FRAME_HEIGHT
@vertical:
    ldy #UI_FRAME_LEFT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #UI_FRAME_COLOR
    sta (NEXT_ROW_PTR), y

    ldy #UI_DIVIDER_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #UI_FRAME_COLOR
    sta (NEXT_ROW_PTR), y

    ldy #UI_FRAME_RIGHT_COLUMN
    lda #CHAR_UI_FRAME
    sta (ROW_PTR), y
    sta (DEST_PTR), y
    lda #UI_FRAME_COLOR
    sta (NEXT_ROW_PTR), y

    jsr advance_transition_pointers
    dex
    bne @vertical

    jmp draw_ceiling_spikes

advance_row_pointer:
    ; Advance ROW_PTR for ceiling/UI drawing; collision queries use row lookup tables.
    clc
    lda ROW_PTR
    adc #SCREEN_COLUMNS
    sta ROW_PTR
    bcc @done
    inc ROW_PTR+1
@done:
    rts

; Draw the fixed hazard only during scene construction; neither scroll path writes this
; row, and no sprite is used. Draw once into both matrices, using the same white/green
; multicolor palette.
.segment "EXTCODE"
draw_ceiling_spikes:
    ldx #PLAYFIELD_FIRST_COLUMN
@column:
    lda #CHAR_CEILING_SPIKE
    STORE_SCREEN_PAIR CEILING_SPIKE_ROW*SCREEN_COLUMNS, x
    lda #SPIKE_CELL_COLOR
    sta COLOR_RAM+CEILING_SPIKE_ROW*SCREEN_COLUMNS, x
    inx
    cpx #PLAYFIELD_LAST_COLUMN+1
    bne @column
    rts
