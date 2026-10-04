; Handles character selection and paddle centering before a round starts.
; POTX selects Elien, Ember or Wasser; the first FIRE locks the choice.
; The second FIRE confirms with the paddle centered. An off-center press shows
; the hurt frame as feedback, then returns to the direction preview.
;
; Quick reference:
;   line 16: selection and confirmation flow
;   line 80: three-character preview
;   line 147: centering preview
;   line 221: neutral-zone check
;   line 236: frame and color tables

.segment "SELECTCODE"

; Run select -> lock -> center -> confirm, returning only when ready to play.
run_character_selection:
    jsr initialize_character_choices

    ; Wait for any held entry button to be released; the selection preview still
    ; responds to POTX.
@initial_release:
    jsr update_character_choices_frame
    jsr action_button_pressed
    bcs @initial_release

@wait_for_lock:
    jsr update_character_choices_frame
    jsr action_button_pressed
    bcc @wait_for_lock

    ; The first FIRE locks selected_character. Require release afterward so the same
    ; held press cannot pass straight through the second stage.
    jsr initialize_centering_preview
@wait_lock_release:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcs @wait_lock_release

@wait_start_press:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcc @wait_start_press
    jsr paddle_is_centered
    bcc @invalid_press

    ; After a centered FIRE press, wait for release and sample the paddle again. If it
    ; is outside the dead zone at release, require another press.
@wait_final_release:
    jsr update_centering_preview_frame
    jsr action_button_pressed
    bcs @wait_final_release
    jsr paddle_is_centered
    bcc @wait_start_press
    rts

@invalid_press:
    ; FIRE while facing left/right shows hurt feedback instead of starting the game. A
    ; fixed timer ensures the hurt frame remains visible for at least ~0.24 seconds even
    ; after a quick release.
    jsr render_centering_hurt
    lda #CHARACTER_SELECT_HURT_FRAMES
    sta character_select_hurt_timer
@show_hurt:
    jsr wait_for_frame
    jsr read_paddle_x
    dec character_select_hurt_timer
    bne @show_hurt

    ; Keep hurt displayed while the button is held; after release, restore the latest
    ; POTX direction.
@wait_invalid_release:
    jsr wait_for_frame
    jsr read_paddle_x
    jsr action_button_pressed
    bcs @wait_invalid_release
    jsr render_centering_direction
    jmp @wait_start_press

; Configure three side-by-side normal sprites for the selection phase.
initialize_character_choices:
    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY

    ; All three X coordinates are below 256; clear only sprite 0..2 high bits,
    ; preserving other bits.
    CLEAR_REGISTER_BITS VIC_SPRITE_X_HIGH, %00000111

    lda #CHARACTER_SELECT_LEFT_X
    sta VIC_SPRITE_0_X
    lda #CHARACTER_SELECT_CENTER_X
    sta VIC_SPRITE_1_X
    lda #CHARACTER_SELECT_RIGHT_X
    sta VIC_SPRITE_2_X
    lda #CHARACTER_SELECT_Y
    sta VIC_SPRITE_0_Y
    sta VIC_SPRITE_1_Y
    sta VIC_SPRITE_2_Y

    ldx #0
@pointers:
    lda character_normal_blocks, x
    STORE_SPRITE_POINTER 0, x
    inx
    cpx #PLAYER_CHARACTER_COUNT
    bne @pointers

    lda #%00000111
    sta VIC_SPRITE_ENABLE
    jmp render_character_choices

; Sample POTX, choose one of three character ranges, and update highlight colors.
update_character_choices_frame:
    jsr wait_for_frame
    jsr read_paddle_x

    lda potx_raw
    cmp #CHARACTER_SELECT_MIDDLE_START
    bcc @elien
    cmp #CHARACTER_SELECT_RIGHT_START
    bcc @ember
    lda #CHARACTER_WASSER
    bne @store
@elien:
    lda #CHARACTER_ELIEN
    beq @store
@ember:
    lda #CHARACTER_EMBER
@store:
    sta selected_character
    jmp render_character_choices

; Gray all previews, then restore the selected character's native color.
render_character_choices:
    lda #COLOR_GRAY
    sta VIC_SPRITE_0_COLOR
    sta VIC_SPRITE_1_COLOR
    sta VIC_SPRITE_2_COLOR
    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR, y
    rts

; Keep only sprite 0, center it, and begin direction-based neutral guidance.
initialize_centering_preview:
    jsr draw_centering_prompt
    lda #%00000001
    sta VIC_SPRITE_ENABLE
    lda #CHARACTER_SELECT_CENTER_X
    sta VIC_SPRITE_0_X
    lda #CHARACTER_SELECT_Y
    sta VIC_SPRITE_0_Y
    CLEAR_REGISTER_BITS VIC_SPRITE_X_HIGH, %00000001
    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR
    jmp render_centering_direction

; Refresh the locked sprite once per PAL frame while waiting for neutral.
update_centering_preview_frame:
    jsr wait_for_frame
    jsr read_paddle_x
    jmp render_centering_direction

; Show left below POTX 108, right above 148, or forward inside 108..148.
render_centering_direction:
    ldy selected_character
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right
    lda character_normal_blocks, y
    bne @store
@left:
    lda character_left_blocks, y
    bne @store
@right:
    lda character_right_blocks, y
@store:
    STORE_SPRITE_POINTER 0
    rts

; Replace the direction frame with the selected hurt frame after bad Fire.
render_centering_hurt:
    ldy selected_character
    lda character_hurt_blocks, y
    STORE_SPRITE_POINTER 0
    rts

; Draw the 35-character neutral prompt; modal entry already installed its font.
draw_centering_prompt:
    ; show_start_screen has loaded source 2's complete basic text page; write screen
    ; codes only.

    ; Clear the old ten-character PRESS FIRE prompt, then write the centered
    ; 35-character instruction.
    ldx #SCREEN_COLUMNS-1
@clear_row:
    lda #TEXT_SPACE
    STORE_SCREEN_PAIR CENTERING_PROMPT_ROW*SCREEN_COLUMNS, x
    lda #COLOR_BLACK
    sta COLOR_RAM+CENTERING_PROMPT_ROW*SCREEN_COLUMNS, x
    dex
    bpl @clear_row

    ldx #0
@draw:
    lda centering_prompt, x
    STORE_SCREEN_PAIR CENTERING_PROMPT_OFFSET, x
    lda #COLOR_WHITE
    sta COLOR_RAM+CENTERING_PROMPT_OFFSET, x
    inx
    cpx #CENTERING_PROMPT_LENGTH
    bne @draw
    rts

; Return C=1 only for the same 108..148 neutral zone used by gameplay.
paddle_is_centered:
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @no
    cmp #POTX_DEADZONE_HIGH+1
    bcs @no
    sec
    rts
@no:
    clc
    rts

; Each table is indexed by selected_character. Player sprite 0 uses all frame tables;
; HUD sprite 4 reuses the directional tables. Selection assigns the three normal-table
; values to preview sprites 0..2.
character_normal_blocks:
    .byte PLAYER_NORMAL_BLOCK, EMBER_NORMAL_BLOCK, WASSER_NORMAL_BLOCK
character_fall_blocks:
    .byte PLAYER_FALL_BLOCK, EMBER_FALL_BLOCK, WASSER_FALL_BLOCK
character_right_blocks:
    .byte PLAYER_RIGHT_BLOCK, EMBER_RIGHT_BLOCK, WASSER_RIGHT_BLOCK
character_left_blocks:
    .byte PLAYER_LEFT_BLOCK, EMBER_LEFT_BLOCK, WASSER_LEFT_BLOCK
character_fastleft_blocks:
    .byte PLAYER_FASTLEFT_BLOCK, EMBER_FASTLEFT_BLOCK, WASSER_FASTLEFT_BLOCK
character_fastright_blocks:
    .byte PLAYER_FASTRIGHT_BLOCK, EMBER_FASTRIGHT_BLOCK, WASSER_FASTRIGHT_BLOCK
character_hurt_blocks:
    .byte PLAYER_HURT_BLOCK, EMBER_HURT_BLOCK, WASSER_HURT_BLOCK
character_color_table:
    .byte COLOR_YELLOW, COLOR_RED, COLOR_CYAN

; "PRESS FIRE WHEN CHARACTER FACES YOU": exactly 35 screen codes.
centering_prompt:
    .byte TEXT_P, TEXT_R, TEXT_E, TEXT_S, TEXT_S, TEXT_SPACE
    .byte TEXT_F, TEXT_I, TEXT_R, TEXT_E, TEXT_SPACE
    .byte TEXT_W, TEXT_H, TEXT_E, TEXT_N, TEXT_SPACE
    .byte TEXT_C, TEXT_H, TEXT_A, TEXT_R, TEXT_A, TEXT_C, TEXT_T, TEXT_E, TEXT_R
    .byte TEXT_SPACE, TEXT_F, TEXT_A, TEXT_C, TEXT_E, TEXT_S, TEXT_SPACE
    .byte TEXT_Y, TEXT_O, TEXT_U
centering_prompt_end:
.assert centering_prompt_end-centering_prompt = CENTERING_PROMPT_LENGTH, error, "centering prompt length mismatch"

character_select_code_end:

.assert character_select_code_end <= $4000, error, "character select code exceeds VIC bank 0"
