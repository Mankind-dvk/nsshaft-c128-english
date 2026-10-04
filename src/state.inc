; Shared RAM state for gameplay, input, music and platform effects.
; Round setup resets gameplay fields; selected_character is kept between rounds.
; Leaderboard buffers live in highscores.inc. Flags use 0=false, nonzero=true.
;
; Quick reference:
;   line 17: frame and scrolling state
;   line 46: platform generation and row descriptions
;   line 87: score and HP
;   line 116: input and music state
;   line 146: selection, player and collision state
;   line 207: platform effects and random state

.segment "CODE"

; --- Frame scheduling and double buffering ---
; 0 displays SCREEN_A; 1 displays SCREEN_B.
active_screen:
    .byte 0
; Any physics branch may set 1; the main loop performs the shared game-over transition.
game_over_flag:
    .byte 0
; Software scroll phase 7..0 describes dynamic glyph splits; it is not written directly
; to $d011.
fine_scroll:
    .byte 7
; Upper/lower split position and pixel-row copy count computed by
; update_fragment_glyphs.
fragment_split:
    .byte 7
fragment_count:
    .byte 1
; Eight-bit scroll phase: accumulator += rate each frame; carry moves the world up one
; pixel.
scroll_accumulator:
    .byte 0
scroll_rate:
    .byte SCROLL_RATE_LEVEL_0
; Number of empty logical rows still needed at the bottom; 0 generates a platform this
; time.
rows_until_platform:
    .byte 2

; --- Platform currently being generated ---
; start/width use character columns; type: 0 normal, 1 spike, 2 fade, 3 spring, 4 left
; conveyor, 5 right conveyor.
platform_start:
    .byte 0
platform_width:
    .byte 0
platform_type:
    .byte 0
; First-landing score updates the generation tier. Normal platforms have no repeat
; limit; last/run_length prevents a third consecutive platform of any other identical
; type.
platform_generation_level:
    .byte 0
last_generated_platform_type:
    .byte $ff
generated_type_run_length:
    .byte 0
platform_start_limit:
    .byte 0
platform_middle_count:
    .byte 0
selected_platform_char:
    .byte 0
selected_platform_color:
    .byte COLOR_BLACK

; One platform per logical row; width=0 means empty. FULL encodes material and
; fade/spring state, independently of either screen's fragment layout.
platform_row_start:
    .res PLAYFIELD_ROW_COUNT, 0
platform_row_width:
    .res PLAYFIELD_ROW_COUNT, 0
platform_row_full:
    .res PLAYFIELD_ROW_COUNT, 0
platform_render_end:
    .byte 0
platform_render_bank:
    .byte 0

; --- Paddle, HUD, and music ---
; Six independent decimal digits run from most to least significant, avoiding 16-bit
; division and decimal mode that could affect SID IRQs; the HUD converts each digit
; directly to a character.
score_digits:
    .res SCORE_DIGIT_COUNT, 0
score_digits_end:
.assert score_digits_end-score_digits = SCORE_DIGIT_COUNT, error, "score digit count mismatch"

; HP counts absorbable platform/ceiling-spike hits; bottom overflow is always fatal.
; Each new platform point advances progress; after three points add 1 HP without
; reducing score.
health_points:
    .byte 0
health_score_progress:
    .byte 0
health_decimal_work:
    .byte 0
health_hundreds:
    .byte 0
health_tens:
    .byte 0
health_ones:
    .byte 0

; Each logical playfield row has scoring state: 0=no platform, $01=unclaimed, $81=first
; landing already scored. Coarse scrolling moves this table with character rows.
platform_score_rows:
    .res PLAYFIELD_ROW_COUNT, 0
platform_score_rows_end:
.assert platform_score_rows_end-platform_score_rows = PLAYFIELD_ROW_COUNT, error, "platform score row count mismatch"

; Raw SID POTX, 0..255; around 128 is neutral.
potx_raw:
    .byte 128
decimal_work:
    .byte 0
potx_hundreds:
    .byte 1
potx_tens:
    .byte 2
potx_ones:
    .byte 8
; music_step is an eighth-note index 0..127. Music uses a nine-bit modulo-512
; accumulator; low holds eight bits, high is only 0/1, and the score tier updates
; increment.
music_step:
    .byte 0
music_tempo_accumulator_low:
    .byte 0
music_tempo_accumulator_high:
    .byte 0
music_tempo_increment:
    .byte MUSIC_TEMPO_LEVEL_0
music_gate_timer:
    .byte 0
music_voice3_delay:
    .byte 0

; --- Player position and vertical physics ---
; Title-locked character index: 0=Elien, 1=Ember, 2=Wasser. new_game preserves this
; value, keeping the same character for gameplay, the HUD, and hurt feedback until the
; next selection.
selected_character:
    .byte CHARACTER_ELIEN
; Minimum hurt-feedback duration after FIRE outside neutral; retain hurt until the
; button is released.
character_select_hurt_timer:
    .byte 0
; VIC sprite X is nine bits: low byte at $d000, ninth bit at $d010 bit 0.
player_x_low:
    .byte 0
player_x_high:
    .byte 0
player_move_counter:
    .byte 0
; Force the hurt bitmap briefly after HP loss; decrement on frame selection and restore
; movement frames at zero.
player_hurt_timer:
    .byte 0
; Separate from paddle player_move_counter; 2 skips two frames after the current push.
conveyor_move_counter:
    .byte 0
player_y:
    .byte 0
; Three Booleans distinguish general support, fade support, and a compressing spring.
player_on_platform:
    .byte 0
player_on_fade_platform:
    .byte 0
player_on_spring:
    .byte 0
; Vertical velocity uses 1/8-pixel fixed point; fraction retains subpixel remainder.
; Separate nonnegative fall/rise velocities avoid signed velocity arithmetic.
player_fall_fraction:
    .byte 0
player_fall_velocity:
    .byte 0
player_rise_velocity:
    .byte 0
spawn_row:
    .byte 0

; --- Collision-query temporary values ---
; find_platform_below_player converts foot Y to a character row and samples left/right
; foot points. type: 0 empty, 1 normal, 2 spike, 3 untriggered fade, 4 triggered fade, 5
; spring, 6 left conveyor, 7 right conveyor.
collision_probe_y:
    .byte 0
collision_screen_top:
    .byte 0
collision_relative_y:
    .byte 0
collision_row:
    .byte 0
collision_type:
    .byte 0
collision_x_low:
    .byte 0
collision_x_high:
    .byte 0

; --- Disappearing-platform lifecycle ---
; Track one activated platform at a time; row anchors its lower fragment/full character.
fade_platform_active:
    .byte 0
fade_platform_row:
    .byte 0
fade_platform_timer:
    .byte 0
fade_stage:
    .byte FADE_STAGE_CRACKED
fade_effect_mode:
    .byte FADE_EFFECT_ACTIVATE
fade_rows_remaining:
    .byte 0
fade_char_work:
    .byte 0

; --- Spring lifecycle ---
; Record the anchor/countdown on landing; restore original glyphs in both buffers on
; departure or launch.
spring_platform_row:
    .byte 0
spring_compression_timer:
    .byte 0
spring_effect_mode:
    .byte SPRING_EFFECT_COMPRESS
spring_rows_remaining:
    .byte 0
spring_char_work:
    .byte 0

; --- 16-bit pseudorandom state ---
random_state_low:
    .byte 0
random_state_high:
    .byte 0

; --- Conveyor surface animation ---
; Horizontal phase lives in two writable full patterns, separate from immutable sources
; and vertical fragments.
conveyor_animation_counter:
    .byte CONVEYOR_ANIMATION_FRAMES
conveyor_left_pattern:
    .res 8, 0
conveyor_right_pattern:
    .res 8, 0

.assert * <= CHARSET_ADDRESS, error, "Main code/state overlaps live charset"
