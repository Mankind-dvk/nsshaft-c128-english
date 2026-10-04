.setcpu "6502"

.include "constants.inc"
.include "macros.inc"

; Program entry, machine setup and the main game loop.
; A round runs from character selection through gameplay to the leaderboard,
; then returns to selection. The included files implement the individual systems.
; Calls may change A/X/Y and flags unless a routine says otherwise.
;
; Quick reference:
;   line 20: PRG header and BASIC SYS loader
;   line 36: C128 setup and initial score loading
;   line 99: round reset
;   line 146: frame update order
;   line 173: game-over transition
;   line 180: raster synchronization
;   line 196: subsystem includes

.segment "LOADADDR"
    ; PRG load address, followed by the BASIC loader below.
    .word $1c01

.segment "BASIC"
    ; BASIC line 10: SYS 7424. RUN enters the machine-code setup at $1d00.
    .word basic_end
    .word 10
    .byte $9e
    .byte "7424"
    .byte 0
basic_end:
    .word 0

.segment "CODE"

start:
    ; Keep IRQs off while setting up memory and hardware. initialize_music enables
    ; them when the first round is ready.
    sei
    cld

    ; Use bank 0 and 1 MHz for the VIC display. Set the bank explicitly before writing
    ; screen, charset or sprite memory; BASIC may have left a different mapping.
    lda #MMU_CONFIG_BANK0_IO
    sta MMU_CONFIG
    jsr highscore_storage_bootstrap
    CLEAR_REGISTER_BITS VIC_CPU_SPEED, %00000001
    CLEAR_REGISTER_BITS MMU_RAM_CONFIG, %01000000

    ; Select the first 16 KB VIC bank ($0000-$3fff) in RAM bank 0.
    SET_REGISTER_BITS CIA2_DDR_A, %00000011
    SET_REGISTER_BITS CIA2_PORT_A, %00000011

    ; Disable all sprites during initialization to avoid random images from BASIC's
    ; register state.
    lda #0
    sta VIC_SPRITE_ENABLE

    ; Mix multicolor and hires characters: disable ECM; Color RAM bit 3 selects each
    ; cell's mode. Keep the natural phase 3; bricks and spikes are multicolor, text and
    ; springs remain hires.
    lda #$18
    sta VIC_CONTROL_2
    lda #VIC_CONTROL_1_TEXT
    sta VIC_CONTROL_1
    SHOW_SCREEN SCREEN_A_D018

    lda #COLOR_BLACK
    sta VIC_BACKGROUND_COLOR
    lda #COLOR_WHITE
    sta VIC_MULTICOLOR_1
    lda #COLOR_GRAY
    sta VIC_MULTICOLOR_2
    lda #COLOR_GRAY
    sta VIC_BACKGROUND_COLOR_3
    ; Keep the hardware border black; the brick frame is drawn entirely with characters.
    lda #COLOR_BLACK
    sta VIC_BORDER_COLOR

    jsr initialize_paddles
    jsr stop_music
    jsr initialize_highscores
.ifdef AUTO_START_TEST
    ; Display/physics tests bypass title buttons but still execute the normal game loop.
    jmp new_game
.endif
.ifdef MUSIC_REGRESSION_TEST
    ; The regression build bypasses title buttons for timed VICE startup and SID-write
    ; checks.
    jmp new_game
.endif
    jsr highscore_load_at_start
start_screen:
    jsr show_start_screen
    jsr run_character_selection
    jmp new_game

; Reset round state, generate a new world, and start music from bar one.
new_game:
    ; Explicitly reset each round's state instead of relying on values left after game
    ; over; otherwise round two inherits fade timers, scroll phases, or music steps.
    ; Preserve the selected character and the in-memory leaderboard between rounds.
    lda #0
    sta VIC_SPRITE_ENABLE
    sta active_screen
    sta game_over_flag
    lda #7
    sta fine_scroll
    ; Recompose gameplay glyphs from the two source charsets before enabling IRQs.
    jsr load_game_charset
    SHOW_SCREEN SCREEN_A_D018

    jsr clear_screens_and_colors
    jsr initialize_fade_platforms
    jsr initialize_spring_platforms
    jsr initialize_conveyor_platforms
    jsr seed_random
    jsr initialize_score
    jsr initialize_health
    jsr initialize_platform_generation
    jsr seed_platforms
    jsr initialize_ui
    jsr initialize_hud
    jsr read_paddle_x
    jsr update_potx_display
    lda #2
    sta rows_until_platform

    jsr initialize_player
    jsr initialize_dashboard
    jsr initialize_music

    ; Screen B starts as an identical hidden copy of the visible game screen and UI.
    jsr prepare_screen_b_from_a
    jsr enable_charset_display

.ifdef MUSIC_REGRESSION_TEST
    ; Freeze gameplay in the test build so VICE can capture all 16 bars and the loop
    ; transition.
music_regression_loop:
    jsr wait_for_frame
    jmp music_regression_loop
.endif

; Target one update per PAL frame; game_over_flag is handled after updates.
main_loop:
    ; This is the fixed frame schedule. Input, physics, effects, and surface animation
    ; update every frame. A fixed-point phase accumulator schedules upward pixel
    ; scrolling; only first landings add score.
    jsr wait_for_frame
    jsr read_paddle_x
    jsr update_dashboard_direction
    jsr update_potx_display
    jsr update_player_horizontal
    jsr update_player_vertical
    jsr update_fade_platform
    jsr update_spring_platform
    jsr update_conveyor_animation
    jsr update_player_sprite_frame
    lda game_over_flag
    beq @continue_game
    jmp game_over_screen
@continue_game:

    jsr scroll_step_ready
    bcc main_loop
    jsr scroll_one_pixel_up
    lda game_over_flag
    bne game_over_screen
    jmp main_loop

; Stop music, run the leaderboard flow, then return to character selection.
game_over_screen:
    ; After the end screen, return to selection; new_game completely regenerates the
    ; next world.
    jsr stop_music
    jsr run_highscore_game_over
    jmp start_screen

wait_for_frame:
    ; Both waits matter: if the raster is already at 250 on entry, wait until it leaves,
    ; then wait for the next arrival at 250 to prevent two main-loop updates in one
    ; frame.
@wait_until_away:
    lda VIC_RASTER
    cmp #FRAME_SYNC_RASTER
    beq @wait_until_away
@wait_until_line:
    lda VIC_RASTER
    cmp #FRAME_SYNC_RASTER
    bne @wait_until_line
    rts

; Subsystem file order does not change runtime call order; ca65 resolves all forward
; references made by the scheduling code above.
.include "video.inc"
.include "platform_materials.inc"
.include "platforms.inc"
.include "platform_rows.inc"
.include "input.inc"
.include "hud.inc"
.include "score.inc"
.include "health.inc"
.include "character_select.inc"
.include "player.inc"
.include "fade_platforms.inc"
.include "spring_platforms.inc"
.include "conveyor_platforms.inc"
.include "music.inc"
.include "assets.inc"
.include "state.inc"
.include "highscores.inc"
.include "highscore_storage.inc"
