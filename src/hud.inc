; Draws the POTX readout and the selected character's direction indicator (sprite 4).
; The indicator shares the player's frame tables and uses the same POTX sample.
; Write text and sprite pointers to both screens; Color RAM has one shared copy.
;
; Quick reference:
;   line 15: POTX label setup
;   line 35: decimal POTX digits
;   line 94: direction-indicator sprite setup
;   line 126: five paddle ranges and frame selection

.segment "CODE"

; POTX numeric display

initialize_hud:
    ; Output: write the five-character POTX: label to both screens and color all eight
    ; display cells white. update_potx_display supplies the three digits.
    ldx #0
@label:
    lda potx_label, x
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET, x
    inx
    cpx #5
    bne @label

    ldx #0
    lda #COLOR_WHITE
@color:
    sta COLOR_RAM+POTX_DISPLAY_OFFSET, x
    inx
    cpx #8
    bne @color
    rts

update_potx_display:
    ; Split potx_raw into hundreds, tens and units for the three-digit display.
    ; Repeated subtraction keeps this small for the 0..255 input range.
    lda potx_raw
    sta decimal_work
    ldx #0

@hundreds:
    lda decimal_work
    cmp #100
    bcc @hundreds_done
    sec
    sbc #100
    sta decimal_work
    inx
    bne @hundreds

@hundreds_done:
    txa
    sta potx_hundreds

    ldx #0
@tens:
    lda decimal_work
    cmp #10
    bcc @tens_done
    sec
    sbc #10
    sta decimal_work
    inx
    bne @tens

@tens_done:
    txa
    sta potx_tens

    lda decimal_work
    sta potx_ones

    ; Gameplay digit glyphs are imported from source 2, outside the platform fragment
    ; area; these characters therefore remain fixed at the upper right.
    lda potx_hundreds
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+5

    lda potx_tens
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+6

    lda potx_ones
    clc
    adc #CHAR_DIGIT_0
    STORE_SCREEN_PAIR POTX_DISPLAY_OFFSET+7
    rts

; Character direction indicator

initialize_dashboard:
    ; Sprite 4 uses existing character frames at the lower right. Change only X-high bit
    ; 4, preserving player bit 0.
    lda #HUD_CHARACTER_X_LOW
    sta VIC_SPRITE_4_X
    lda #HUD_CHARACTER_Y
    sta VIC_SPRITE_4_Y

    ldy selected_character
    lda character_color_table, y
    sta VIC_SPRITE_4_COLOR

    SET_REGISTER_BITS VIC_SPRITE_X_HIGH, %00010000
    CLEAR_REGISTER_BITS VIC_SPRITE_X_EXPAND, %00010000
    CLEAR_REGISTER_BITS VIC_SPRITE_Y_EXPAND, %00010000
    jsr update_dashboard_direction

    SET_REGISTER_BITS VIC_SPRITE_ENABLE, %00010000
    rts

; Share the player's five direction ranges; only switch the sprite block number.
; Synchronize both screen pointer tables as for sprite 0, preventing old frames after a
; D018 flip.
update_dashboard_direction:
    jsr select_direction_sprite_block
    STORE_SPRITE_POINTER 4
    rts

; Input: potx_raw. Return the selected character's sprite block number in A; Y becomes
; the character index. Thresholds match player_movement_ready; supported player
; direction frames and the HUD use the same ranges. Airborne and hurt player frames take
; priority.
select_direction_sprite_block:
    ldy selected_character
    lda potx_raw
    cmp #POTX_FAR_LEFT_END
    bcc @fastleft
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcc @normal
    cmp #POTX_FAR_RIGHT_START
    bcc @right
    lda character_fastright_blocks, y
    rts
@fastleft:
    lda character_fastleft_blocks, y
    rts
@left:
    lda character_left_blocks, y
    rts
@normal:
    lda character_normal_blocks, y
    rts
@right:
    lda character_right_blocks, y
    rts
