; Player movement, sprite frames and platform collision.
; X is an absolute nine-bit VIC coordinate; Y is the sprite's top edge.
; Vertical velocity uses 1/8-pixel units. Two probes below the feet find support;
; platforms are one-way, so the player can rise through them.
;
; Quick reference:
;   line 18: spawn setup
;   line 52: hurt, airborne and direction frames
;   line 128: POTX movement
;   line 205: gravity, support and landing
;   line 476: collision probes
;   line 596: conveyor push and movement helpers

.segment "GAMEPLAY"

; Player sprite

initialize_player:
    ; seed_platforms leaves the lowest platform's start/width; fine_scroll starts at 7.
    ; Set up sprite 0 and place the player at the center of that row-22 platform.
    ldy selected_character
    lda character_normal_blocks, y
    STORE_SPRITE_POINTER 0

    lda #0
    sta VIC_SPRITE_MULTICOLOR
    sta VIC_SPRITE_X_EXPAND
    sta VIC_SPRITE_Y_EXPAND
    sta VIC_SPRITE_PRIORITY
    sta player_move_counter
    sta player_hurt_timer
    sta conveyor_move_counter

    ; Spritemate bitmaps describe transparent/solid pixels; sprite 0's color register
    ; supplies the actual color.
    lda character_color_table, y
    sta VIC_SPRITE_0_COLOR

    lda #PLAYFIELD_BOTTOM_ROW
    sta spawn_row
    jsr position_player_on_platform

    ; Enable player sprite 0 first; initialize_dashboard later enables
    ; direction-indicator sprite 4.
    lda #%00000001
    sta VIC_SPRITE_ENABLE
    rts

; Frame priority: hurt, then fall for any airborne state (including ascent), then one of
; five paddle-direction frames while supported. Synchronize both screen pointer tables
; before a D018 buffer flip.
update_player_sprite_frame:
    ; Input: selected_character, player_on_platform, potx_raw, player_hurt_timer.
    ; Output: sprite 0 pointers in both A/B screens; decrement player_hurt_timer if
    ; nonzero. Switch bitmap block numbers instead of copying 63 bytes per frame.
    ldy selected_character
    lda player_hurt_timer
    beq @select_motion
    dec player_hurt_timer
    lda character_hurt_blocks, y
    bne @store

@select_motion:
    lda player_on_platform
    beq @fall

    jsr select_direction_sprite_block
    jmp @store

@fall:
    lda character_fall_blocks, y
    bne @store
@store:
    STORE_SPRITE_POINTER 0
    rts

; Place the player on the spawn_row platform described by platform_start/platform_width.
position_player_on_platform:
    ; Input: spawn_row, platform_start, platform_width.
    ; Output: player_x/player_y and hardware sprite coordinates; enter the
    ; platform-supported state. Center the 24-pixel-wide sprite horizontally:
    ; x = 24 + start*8 + (width*8 - 24)/2
    ;   = 12 + start*8 + width*4.
    lda platform_start
    asl
    asl
    asl
    clc
    adc #12
    sta player_x_low
    lda #0
    adc #0
    sta player_x_high

    lda platform_width
    asl
    asl
    clc
    adc player_x_low
    sta player_x_low
    bcc @x_ready
    inc player_x_high
@x_ready:
    jsr write_player_x

    ; Platform top = SCREEN_ROW0_BASE_Y + fine_scroll + row*8.
    lda spawn_row
    asl
    asl
    asl
    clc
    adc fine_scroll
    adc #SCREEN_ROW0_BASE_Y-PLAYER_HEIGHT
    sta player_y
    sta VIC_SPRITE_0_Y

    lda #0
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_on_fade_platform
    sta player_on_spring
    sta player_rise_velocity
    sta conveyor_move_counter
    lda #1
    sta player_on_platform
    rts

update_player_horizontal:
    ; potx_raw selects direction and the wait between two-pixel steps.
    ; Neutral clears the counter, so movement starts immediately when the paddle leaves
    ; the dead zone. Fast movement shortens the wait without increasing the step size.
    lda potx_raw
    cmp #POTX_DEADZONE_LOW
    bcc @left
    cmp #POTX_DEADZONE_HIGH+1
    bcs @right

    ; 108..148 is the stationary neutral dead zone.
    lda #0
    sta player_move_counter
    rts

@left:
    jsr player_movement_ready
    bcc @done

@move_left:
    ; Check bounds before subtraction so invalid X=0/1 cannot underflow a 16-bit
    ; coordinate and teleport the sprite rightward. Clamp the final normal leftward step
    ; to the actual left edge.
    lda player_x_high
    bne @subtract_left
    lda player_x_low
    cmp #PLAYER_MIN_X+PLAYER_MOVE_STEP
    bcs @subtract_left
@clamp_left:
    lda #PLAYER_MIN_X
    sta player_x_low
    lda #0
    sta player_x_high
    jmp write_player_x

@subtract_left:
    sec
    lda player_x_low
    sbc #PLAYER_MOVE_STEP
    sta player_x_low
    lda player_x_high
    sbc #0
    sta player_x_high
@write_left:
    jmp write_player_x

@right:
    jsr player_movement_ready
    bcc @done

@move_right:
    clc
    lda player_x_low
    adc #PLAYER_MOVE_STEP
    sta player_x_low
    lda player_x_high
    adc #0
    sta player_x_high
    cmp #PLAYER_MAX_X_HIGH
    bcc @write_right
    bne @clamp_right
    lda player_x_low
    cmp #PLAYER_MAX_X_LOW+1
    bcc @write_right
@clamp_right:
    lda #PLAYER_MAX_X_LOW
    sta player_x_low
    lda #PLAYER_MAX_X_HIGH
    sta player_x_high
@write_right:
    jmp write_player_x

@done:
    rts

; NS-Shaft vertical movement

update_player_vertical:
    ; The vertical state machine processes three mutually exclusive states in order:
    ; 1. player_on_platform != 0: check support.
    ; 2. player_rise_velocity != 0: rise after a spring launch or spike bounce.
    ; 3. Otherwise: apply gravity and test landing.
    ; No return value is used; set game_over_flag when needed and let main_loop handle
    ; the transition. All states first check ceiling spikes; consume HP and move clear
    ; if available, otherwise end the game.
    lda player_y
    cmp #(PLAYER_CEILING_CONTACT_Y+1)
    bcs @below_ceiling
    jmp handle_ceiling_damage
@below_ceiling:
    lda player_on_platform
    bne @supported
    jmp @airborne
@supported:

    ; Start falling after walking beyond the current supporting platform.
    jsr find_platform_below_player
    cmp #COLLISION_NORMAL
    beq @supported_normal
    cmp #COLLISION_SPIKES
    beq @hit_spikes
    cmp #COLLISION_FADE_INACTIVE
    beq @activate_supported_fade
    cmp #COLLISION_FADE_ACTIVE
    beq @supported_fade
    cmp #COLLISION_SPRING
    beq @supported_spring
    cmp #COLLISION_CONVEYOR_LEFT
    beq @supported_conveyor
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @supported_conveyor
    jmp @begin_fall
@hit_spikes:
    jmp handle_spike_damage
@supported_normal:
    jsr cancel_spring_platform
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    rts
@activate_supported_fade:
    jsr cancel_spring_platform
    jsr activate_fade_platform
    jmp @mark_supported_fade
@supported_fade:
    jsr cancel_spring_platform
@mark_supported_fade:
    lda #0
    sta conveyor_move_counter
    lda #1
    sta player_on_fade_platform
    rts
@supported_spring:
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    lda player_on_spring
    bne @spring_ready
    jsr compress_spring_platform
@spring_ready:
    rts
@supported_conveyor:
    ; Conveyors provide normal support and use a separate counter to push X two pixels
    ; every three frames. Paddle movement runs first, allowing opposite input to
    ; counteract the belt or matching input to add speed.
    jsr cancel_spring_platform
    lda #0
    sta player_on_fade_platform
    jsr apply_conveyor_motion
    rts
@begin_fall:
    jsr cancel_spring_platform
    lda #0
    sta player_on_platform
    sta player_on_fade_platform
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_rise_velocity
    sta conveyor_move_counter
    rts

@airborne:
    ; Clear the conveyor throttle when airborne so the next conveyor landing immediately
    ; receives its first push.
    lda #0
    sta conveyor_move_counter
    lda player_rise_velocity
    beq @falling

; Upward motion uses 1/8-pixel units like gravity. Launch velocities exceed 8, allowing
; multiple pixels per frame; per-pixel stepping prevents fast ascent from passing
; through fixed ceiling spikes.
@rising:
    lda player_fall_fraction
    clc
    adc player_rise_velocity
    sta player_fall_fraction
@rise_pixel:
    lda player_fall_fraction
    cmp #8
    bcc @decelerate_rise
    sec
    sbc #8
    sta player_fall_fraction
    dec player_y
    lda player_y
    sta VIC_SPRITE_0_Y
    cmp #(PLAYER_CEILING_CONTACT_Y+1)
    bcs @rise_pixel
    jmp handle_ceiling_damage
@decelerate_rise:
    dec player_rise_velocity
    bne @done
    lda #0
    sta player_fall_fraction
    sta player_fall_velocity
    rts

@falling:
    lda player_y
    cmp #PLAYER_BOTTOM_GAME_OVER_Y
    bcc @falling_in_bounds
    jmp request_game_over
@falling_in_bounds:

    lda player_fall_velocity
    cmp #GRAVITY_MAX
    bcs @velocity_ready
    inc player_fall_velocity
@velocity_ready:
    lda player_fall_fraction
    clc
    adc player_fall_velocity
    cmp #8
    bcc @store_fraction
    sbc #8
    sta player_fall_fraction
    inc player_y
    lda player_y
    sta VIC_SPRITE_0_Y

    ; Gravity this frame may bring the sprite bottom onto the bottom frame; test
    ; immediately rather than waiting for the next update_player_vertical call to end
    ; the game.
    cmp #PLAYER_BOTTOM_GAME_OVER_Y
    bcc @check_landing
    jmp request_game_over

@check_landing:
    jsr find_platform_below_player
    cmp #COLLISION_NORMAL
    beq @land
    cmp #COLLISION_SPIKES
    beq @fall_on_spikes
    cmp #COLLISION_FADE_INACTIVE
    beq @land
    cmp #COLLISION_FADE_ACTIVE
    beq @land
    cmp #COLLISION_SPRING
    beq @land
    cmp #COLLISION_CONVEYOR_LEFT
    beq @land
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @land
    jmp @fall_done
@fall_on_spikes:
    ; Spike contact still reaches a new platform: claim its one-time row point before HP
    ; damage/bounce or death. The claimed flag prevents repeated points from the same
    ; spikes.
    jsr award_platform_landing_score
    jmp handle_spike_damage
@fall_done:
    rts

@store_fraction:
    sta player_fall_fraction
@done:
    rts

@land:
    ; Award the collision row only on its first airborne landing. The platform's claimed
    ; bit scrolls with its row, preventing repeat points after spring bounces or
    ; returning to an old platform.
    jsr award_platform_landing_score

    lda collision_row
    sta spawn_row

    ; Keep X unchanged and snap the sprite's feet to the platform top.
    lda spawn_row
    asl
    asl
    asl
    clc
    adc fine_scroll
    adc #SCREEN_ROW0_BASE_Y-PLAYER_HEIGHT
    sta player_y
    sta VIC_SPRITE_0_Y

    lda #0
    sta player_fall_fraction
    sta player_fall_velocity
    sta player_rise_velocity

    lda collision_type
    cmp #COLLISION_FADE_INACTIVE
    beq @activate_landed_fade
    cmp #COLLISION_FADE_ACTIVE
    beq @mark_landed_fade
    cmp #COLLISION_SPRING
    beq @compress_landed_spring
    cmp #COLLISION_CONVEYOR_LEFT
    beq @landed_conveyor
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @landed_conveyor
    lda #0
    sta player_on_fade_platform
    sta player_on_spring
    sta conveyor_move_counter
    beq @finish_landing
@activate_landed_fade:
    ; If another platform is already counting down, activation deletes the old one
    ; first.
    lda #0
    sta player_on_fade_platform
    jsr activate_fade_platform
@mark_landed_fade:
    lda #1
    sta player_on_fade_platform
    lda #0
    sta player_on_spring
    sta conveyor_move_counter
    beq @finish_landing
@compress_landed_spring:
    lda #0
    sta player_on_fade_platform
    sta conveyor_move_counter
    jsr compress_spring_platform
    jmp @finish_landing
@landed_conveyor:
    ; Apply the first conveyor displacement on the landing frame to avoid a pause before
    ; movement starts.
    lda #0
    sta player_on_fade_platform
    sta player_on_spring
    jsr apply_conveyor_motion
@finish_landing:
    lda #1
    sta player_on_platform
    rts

request_game_over:
    ; Do not clear screens directly from physics routines. Set the flag and return to
    ; the main loop instead of jumping out of nested JSR calls and leaving unmatched
    ; return addresses.
    lda #1
    sta game_over_flag
    rts

; Return A=0 empty, 1 normal, 2 spike, 3 untriggered fade, 4 triggered fade, 5 spring, 6
; left conveyor, 7 right conveyor. Relaxed/compressed springs share a collision type.
; Probe at the sprite's feet with a two-pixel crossing-detection window.
;
; Smooth scrolling displays a logical platform across two adjacent cells: LOWER keeps
; the original logical row, while UPPER draws only pixels crossing the character
; boundary. Always anchor collision to LOWER's row; treating UPPER as another platform
; makes the player land one character row early, visually eight pixels above the real
; platform top.
find_platform_below_player:
    ; Input: player_x, player_y, fine_scroll, active_screen.
    ; Output: collision type in A. On a valid row, store collision_row/type for effects.
    ; An early no-collision return sets A=0 but may leave those scratch bytes unchanged.
    ; Return support only when feet are 0..2 pixels relative to the platform top,
    ; preventing falling players from snapping onto the platform's side or underside.
    lda player_y
    clc
    adc #PLAYER_HEIGHT
    sta collision_probe_y

    lda fine_scroll
    clc
    adc #SCREEN_ROW0_BASE_Y
    sta collision_screen_top

    lda collision_probe_y
    cmp collision_screen_top
    bcc @none
    sec
    sbc collision_screen_top
    sta collision_relative_y
    and #%00000111
    cmp #3
    bcs @none

    lda collision_relative_y
    lsr
    lsr
    lsr
    cmp #PLAYFIELD_TOP_ROW
    bcc @none
    cmp #PLAYFIELD_BOTTOM_ROW+1
    bcs @none
    sta collision_row
    jsr set_collision_row_pointer

    lda #5
    jsr sample_platform_at_player_offset
    sta collision_type
    lda #18
    jsr sample_platform_at_player_offset
    cmp collision_type
    bcc @return_saved
    sta collision_type
@return_saved:
    lda collision_type
    rts

@none:
    lda #0
    rts

set_collision_row_pointer:
    ; Input: collision_row, active_screen. Output: ROW_PTR points to the corresponding
    ; visible-screen row. The caller already bounds the row to the playfield, so reuse
    ; platform row-address tables instead of adding 40 bytes repeatedly from the screen
    ; base.
    lda collision_row
    sec
    sbc #PLAYFIELD_TOP_ROW
    tax

    lda playfield_row_low, x
    sta ROW_PTR
    lda active_screen
    beq @screen_a
    lda playfield_row_high, x
    ora #>SCREEN_B
    bne @store_high
@screen_a:
    lda playfield_row_high, x
    ora #>SCREEN_A
@store_high:
    sta ROW_PTR+1
    rts

; Input A is an X offset inside the 24-pixel-wide sprite.
sample_platform_at_player_offset:
    ; Input: A=pixel offset from the sprite's left edge, currently 5 or 18.
    ; Output: A=logical platform type below that foot point. Subtract VIC visible-area X
    ; origin 24, then right-shift the 16-bit coordinate three times, an unsigned
    ; division by 8 converting pixel X to a character column. UPPER fragments
    ; intentionally provide no support.
    clc
    adc player_x_low
    sta collision_x_low
    lda player_x_high
    adc #0
    sta collision_x_high

    sec
    lda collision_x_low
    sbc #24
    sta collision_x_low
    lda collision_x_high
    sbc #0
    sta collision_x_high

    lsr collision_x_high
    ror collision_x_low
    lsr collision_x_high
    ror collision_x_low
    lsr collision_x_high
    ror collision_x_low

    ldy collision_x_low
    lda (ROW_PTR), y
    cmp #CHARSET_GLYPH_COUNT
    bcs @none
    tay
    lda collision_type_by_char, y
    rts
@none:
    lda #COLLISION_NONE
    rts

; collision_type comes from this frame's foot sampling. Conveyor motion has its own
; throttle: after a push, skip two frames for two pixels every three frames; clamp at
; shaft boundaries.
apply_conveyor_motion:
    lda conveyor_move_counter
    beq @ready
    dec conveyor_move_counter
    rts
@ready:
    lda #CONVEYOR_MOVE_DELAY
    sta conveyor_move_counter

    lda collision_type
    cmp #COLLISION_CONVEYOR_LEFT
    beq @left
    cmp #COLLISION_CONVEYOR_RIGHT
    beq @right
    rts

@left:
    lda player_x_high
    bne @subtract_left
    lda player_x_low
    cmp #PLAYER_MIN_X+CONVEYOR_PUSH_STEP
    bcs @subtract_left
    lda #PLAYER_MIN_X
    sta player_x_low
    lda #0
    sta player_x_high
    jmp write_player_x
@subtract_left:
    sec
    lda player_x_low
    sbc #CONVEYOR_PUSH_STEP
    sta player_x_low
    lda player_x_high
    sbc #0
    sta player_x_high
    jmp write_player_x

@right:
    clc
    lda player_x_low
    adc #CONVEYOR_PUSH_STEP
    sta player_x_low
    lda player_x_high
    adc #0
    sta player_x_high
    cmp #PLAYER_MAX_X_HIGH
    bcc @write_right
    bne @clamp_right
    lda player_x_low
    cmp #PLAYER_MAX_X_LOW+1
    bcc @write_right
@clamp_right:
    lda #PLAYER_MAX_X_LOW
    sta player_x_low
    lda #PLAYER_MAX_X_HIGH
    sta player_x_high
@write_right:
    jmp write_player_x

; Check whether paddle movement is due this frame; reload the normal or fast interval
; when ready.
player_movement_ready:
    ; Output: C=1 permits movement this frame, C=0 means waiting; the caller tests with
    ; BCC. SEC/CLC is a Boolean return value here, not carry preparation for later
    ; arithmetic.
    lda player_move_counter
    beq @ready
    dec player_move_counter
    clc
    rts
@ready:
    ; Normal ranges move two pixels every three frames. Extreme ranges reduce the wait
    ; from 2 to 1, giving two pixels every two frames, exactly 1.5 times the average
    ; speed.
    lda #PLAYER_MOVE_DELAY
    ldx potx_raw
    cpx #POTX_FAR_LEFT_END
    bcc @fast
    cpx #POTX_FAR_RIGHT_START
    bcc @store_delay
@fast:
    lda #PLAYER_FAST_MOVE_DELAY
@store_delay:
    sta player_move_counter
    sec
    rts

write_player_x:
    ; Write the nine-bit software coordinate to VIC: low byte to $d000, high bit to
    ; $d010 bit 0. AND clears only player bit 0; preserve bit 4 used by the right-side
    ; direction sprite.
    lda player_x_low
    sta VIC_SPRITE_0_X

    lda VIC_SPRITE_X_HIGH
    and #%11111110
    ldx player_x_high
    beq @store_high
    ora #%00000001
@store_high:
    sta VIC_SPRITE_X_HIGH
    rts
