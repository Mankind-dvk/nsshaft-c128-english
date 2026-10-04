; Selects control port 1 and samples SID POTX once per frame.
; The player and HUD share potx_raw. Movement thresholds are applied by their
; own routines; the FIRE-button helper is in video.inc.
;
; Quick reference:
;   line 13: CIA1 port setup
;   line 27: POTX sample and settling delay

.segment "CODE"

; Paddle input

initialize_paddles:
    ; Set PA6/PA7 as SID port-select outputs and PB2..PB4 as button inputs.
    ; Leave the other CIA pins unchanged.
    SET_REGISTER_BITS CIA1_DDR_A, %11000000
    lda CIA1_PORT_A
    and #%00111111
    ora #PADDLE_PORT_1_SELECT
    sta CIA1_PORT_A

    ; Paddle buttons and joystick FIRE are active-low PB2..PB4 inputs.
    CLEAR_REGISTER_BITS CIA1_DDR_B, %00011100
    rts

; Read one SID POTX sample into potx_raw; test builds may override it.
read_paddle_x:
    ; Keep one 0..255 sample in potx_raw for the player and HUD.
.ifdef FADE_PLATFORM_TEST
    ; Automated effect tests hold the paddle centered to keep the player on the spawn
    ; platform.
    lda #128
    sta potx_raw
    rts
.endif
    lda CIA1_PORT_A
    and #%00111111
    ora #PADDLE_PORT_1_SELECT
    sta CIA1_PORT_A

    ; Give the SID paddle reading time to settle after selecting the port.
    ldx #$a0
@settle:
    dex
    bne @settle

    lda SID_POTX
    sta potx_raw
    rts
