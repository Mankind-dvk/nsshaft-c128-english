; Shared addresses, screen layout and tuning values for the native C128 game.
; Keep changes here so input, drawing and physics use the same settings.
;
; Quick reference:
;   line 11: screen and playfield layout
;   line 45: VIC, MMU, CIA and SID registers
;   line 180: sprite addresses and block numbers
;   line 333: paddle thresholds and movement timing
;   line 380: shared zero-page pointers

SCREEN_A                = $0400
SCREEN_B                = $0c00
COLOR_RAM               = $d800
SCREEN_COLUMNS          = 40
SCREEN_ROWS             = 25
UI_FRAME_TOP_ROW        = 1
UI_FRAME_BOTTOM_ROW     = 23
UI_FRAME_LEFT_COLUMN    = 1
UI_FRAME_RIGHT_COLUMN   = 38
UI_FRAME_WIDTH          = UI_FRAME_RIGHT_COLUMN - UI_FRAME_LEFT_COLUMN + 1
UI_FRAME_HEIGHT         = UI_FRAME_BOTTOM_ROW - UI_FRAME_TOP_ROW + 1
CEILING_SPIKE_ROW       = UI_FRAME_TOP_ROW + 1
PLAYFIELD_TOP_ROW       = CEILING_SPIKE_ROW + 1
PLAYFIELD_BOTTOM_ROW    = UI_FRAME_BOTTOM_ROW - 1
PLAYFIELD_ROW_COUNT     = PLAYFIELD_BOTTOM_ROW - PLAYFIELD_TOP_ROW + 1
PLAYFIELD_FIRST_COLUMN  = UI_FRAME_LEFT_COLUMN + 1
UI_DIVIDER_COLUMN       = 29
STATUS_FIRST_COLUMN     = UI_DIVIDER_COLUMN + 1
PLAYFIELD_LAST_COLUMN   = UI_DIVIDER_COLUMN - 1
HUD_CHARACTER_ROW       = 20
POTX_ROW                = UI_FRAME_TOP_ROW + 1
SCORE_ROW               = POTX_ROW + 2
HP_ROW                  = SCORE_ROW + 2
HP_FIRST_COLUMN         = STATUS_FIRST_COLUMN + 1
PLAYFIELD_BOTTOM_OFFSET = PLAYFIELD_BOTTOM_ROW * SCREEN_COLUMNS
POTX_DISPLAY_OFFSET     = POTX_ROW * SCREEN_COLUMNS + STATUS_FIRST_COLUMN
SCORE_DISPLAY_OFFSET    = SCORE_ROW * SCREEN_COLUMNS + STATUS_FIRST_COLUMN
SCORE_DIGITS_OFFSET     = SCORE_DISPLAY_OFFSET + SCREEN_COLUMNS
HP_DISPLAY_OFFSET       = HP_ROW * SCREEN_COLUMNS + HP_FIRST_COLUMN
SCORE_DIGIT_COUNT       = 6
HP_DIGIT_COUNT          = 3

; VIC-IIe, MMU, and sprite registers used by rendering. $d000-$d02e is memory-mapped
; I/O: STA to these addresses configures the video chip instead of writing RAM.
VIC_SPRITE_0_X          = $d000
VIC_SPRITE_0_Y          = $d001
VIC_SPRITE_1_X          = $d002
VIC_SPRITE_1_Y          = $d003
VIC_SPRITE_2_X          = $d004
VIC_SPRITE_2_Y          = $d005
VIC_SPRITE_3_X          = $d006
VIC_SPRITE_3_Y          = $d007
VIC_SPRITE_4_X          = $d008
VIC_SPRITE_4_Y          = $d009
VIC_SPRITE_5_X          = $d00a
VIC_SPRITE_5_Y          = $d00b
VIC_SPRITE_6_X          = $d00c
VIC_SPRITE_6_Y          = $d00d
VIC_SPRITE_7_X          = $d00e
VIC_SPRITE_7_Y          = $d00f
VIC_SPRITE_X_HIGH       = $d010
VIC_CONTROL_1           = $d011
VIC_CONTROL_2           = $d016
VIC_RASTER              = $d012
VIC_SPRITE_ENABLE       = $d015
VIC_SPRITE_Y_EXPAND     = $d017
VIC_MEMORY_POINTERS     = $d018
VIC_SPRITE_PRIORITY     = $d01b
VIC_SPRITE_MULTICOLOR   = $d01c
VIC_SPRITE_X_EXPAND     = $d01d
VIC_IRQ_STATUS          = $d019
VIC_IRQ_ENABLE          = $d01a
VIC_BORDER_COLOR        = $d020
VIC_BACKGROUND_COLOR    = $d021
VIC_MULTICOLOR_1        = $d022
VIC_MULTICOLOR_2        = $d023
VIC_BACKGROUND_COLOR_3  = $d024
VIC_SPRITE_0_COLOR      = $d027
VIC_SPRITE_1_COLOR      = $d028
VIC_SPRITE_2_COLOR      = $d029
VIC_SPRITE_3_COLOR      = $d02a
VIC_SPRITE_4_COLOR      = $d02b
VIC_SPRITE_5_COLOR      = $d02c
VIC_SPRITE_6_COLOR      = $d02d
VIC_SPRITE_7_COLOR      = $d02e
VIC_CPU_SPEED           = $d030
MMU_RAM_CONFIG          = $d506
MMU_CONFIG              = $ff00
MMU_CONFIG_BANK0_IO     = $0e
; The natural vertical phase in 25-row mode is 3; the character matrix starts at raster
; $33 and covers all 200 visible lines without sprites filling phase gaps at the top or
; bottom.
VIC_CONTROL_1_TEXT      = $1b

; CIA ports and timers. A DDR bit of 1 means output; 0 means input. CIA2 port A bits
; 0..1 select VIC bank 0.
CIA1_PORT_A             = $dc00
CIA1_PORT_B             = $dc01
CIA1_DDR_A              = $dc02
CIA1_DDR_B              = $dc03
CIA2_PORT_A             = $dd00
CIA2_DDR_A              = $dd02

CIA1_TIMER_A_LOW        = $dc04
CIA1_TIMER_A_HIGH       = $dc05
CIA1_TIMER_B_LOW        = $dc06
CIA1_TOD_TENTHS         = $dc08
CIA1_TOD_SECONDS        = $dc09
CIA1_TOD_MINUTES        = $dc0a
CIA1_INTERRUPT_CONTROL  = $dc0d
CIA2_INTERRUPT_CONTROL  = $dd0d

; Mix the KERNAL-maintained clock bytes into the game's PRNG seed.
KERNAL_JIFFY_HIGH       = $a0
KERNAL_JIFFY_MIDDLE     = $a1
KERNAL_JIFFY_LOW        = $a2
KERNAL_IRQ_VECTOR       = $0314
KERNAL_IRQ_RETURN       = $ff33

SID_POTX                = $d419
PADDLE_PORT_1_SELECT    = $40

; Registers for the three-voice SID player. Each hardware voice has frequency,
; waveform/gate, and ADSR registers. Music at $2a20 avoids charset and sprite memory;
; the raster IRQ calls the player once per PAL frame.
SID_VOICE_1_FREQ_LOW    = $d400
SID_VOICE_1_FREQ_HIGH   = $d401
SID_VOICE_1_PULSE_LOW   = $d402
SID_VOICE_1_PULSE_HIGH  = $d403
SID_VOICE_1_CONTROL     = $d404
SID_VOICE_1_ATTACK      = $d405
SID_VOICE_1_SUSTAIN     = $d406
SID_VOICE_2_FREQ_LOW    = $d407
SID_VOICE_2_FREQ_HIGH   = $d408
SID_VOICE_2_CONTROL     = $d40b
SID_VOICE_2_ATTACK      = $d40c
SID_VOICE_2_SUSTAIN     = $d40d
SID_VOICE_3_FREQ_LOW    = $d40e
SID_VOICE_3_FREQ_HIGH   = $d40f
SID_VOICE_3_CONTROL     = $d412
SID_VOICE_3_ATTACK      = $d413
SID_VOICE_3_SUSTAIN     = $d414
SID_FILTER_CUTOFF_LOW   = $d415
SID_FILTER_CUTOFF_HIGH  = $d416
SID_FILTER_CONTROL      = $d417
SID_VOLUME              = $d418

; Music tempo uses a 9-bit phase accumulator modulo 512. The base rate is 44/512;
; increments 55/66/77/88 give exactly 1.25/1.5/1.75/2 times the base tempo.
MUSIC_TEMPO_LEVEL_0     = 44
MUSIC_TEMPO_LEVEL_1     = 55
MUSIC_TEMPO_LEVEL_2     = 66
MUSIC_TEMPO_LEVEL_3     = 77
MUSIC_TEMPO_LEVEL_4     = 88
MUSIC_TEMPO_WRAP_HIGH   = 2
MUSIC_PATTERN_MASK      = $7f
MUSIC_LEAD_WAVE         = $40
MUSIC_LEAD_GATE         = $41
MUSIC_BASS_WAVE         = $10
MUSIC_BASS_GATE         = $11
MUSIC_ARP_WAVE          = $20
MUSIC_ARP_GATE          = $21
MUSIC_NOISE_WAVE        = $80
MUSIC_NOISE_GATE        = $81

; The music IRQ must precede the main-loop synchronization raster. If both use 250, the
; IRQ may interrupt before wait_for_frame reads $D012 and return after raster 250,
; occasionally forcing the main loop to wait an extra frame and causing visible stutter.
MUSIC_IRQ_RASTER        = 240
FRAME_SYNC_RASTER       = 250

; VIC memory pointers and fixed 64-byte sprite slots. Bitmap data uses the first 63
; bytes (24x21 bits); byte 64 is padding. Screen offsets $3f8..$3ff hold eight sprite
; block numbers. Scrolling switches D018 between screens, so both pointer tables must
; remain synchronized.
SCREEN_A_D018           = $1a
SCREEN_B_D018           = $3a
SPRITE_POINTER_A        = SCREEN_A + $03f8
SPRITE_POINTER_B        = SCREEN_B + $03f8
PLAYER_NORMAL_ADDRESS  = $3000
PLAYER_FALL_ADDRESS    = $3040
PLAYER_RIGHT_ADDRESS   = $3080
PLAYER_LEFT_ADDRESS    = $30c0
PLAYER_HURT_ADDRESS    = $3a00
EMBER_NORMAL_ADDRESS   = $3a40
EMBER_FALL_ADDRESS     = $3a80
EMBER_RIGHT_ADDRESS    = $3ac0
EMBER_LEFT_ADDRESS     = $3b00
EMBER_HURT_ADDRESS     = $3b40
WASSER_NORMAL_ADDRESS  = $3b80
WASSER_FALL_ADDRESS    = $3bc0
WASSER_RIGHT_ADDRESS   = $3c00
WASSER_LEFT_ADDRESS    = $3c40
WASSER_HURT_ADDRESS    = $3c80
PLAYER_SETS_END_ADDRESS = $3cc0
; Six fast-direction frames occupy the last six consecutive 64-byte blocks of VIC bank
; 0.
PLAYER_FASTLEFT_ADDRESS  = $3e80
PLAYER_FASTRIGHT_ADDRESS = $3ec0
EMBER_FASTLEFT_ADDRESS   = $3f00
EMBER_FASTRIGHT_ADDRESS  = $3f40
WASSER_FASTLEFT_ADDRESS  = $3f80
WASSER_FASTRIGHT_ADDRESS = $3fc0
PLAYER_NORMAL_BLOCK    = PLAYER_NORMAL_ADDRESS / 64
PLAYER_FALL_BLOCK      = PLAYER_FALL_ADDRESS / 64
PLAYER_RIGHT_BLOCK     = PLAYER_RIGHT_ADDRESS / 64
PLAYER_LEFT_BLOCK      = PLAYER_LEFT_ADDRESS / 64
PLAYER_HURT_BLOCK      = PLAYER_HURT_ADDRESS / 64
EMBER_NORMAL_BLOCK     = EMBER_NORMAL_ADDRESS / 64
EMBER_FALL_BLOCK       = EMBER_FALL_ADDRESS / 64
EMBER_RIGHT_BLOCK      = EMBER_RIGHT_ADDRESS / 64
EMBER_LEFT_BLOCK       = EMBER_LEFT_ADDRESS / 64
EMBER_HURT_BLOCK       = EMBER_HURT_ADDRESS / 64
WASSER_NORMAL_BLOCK    = WASSER_NORMAL_ADDRESS / 64
WASSER_FALL_BLOCK      = WASSER_FALL_ADDRESS / 64
WASSER_RIGHT_BLOCK     = WASSER_RIGHT_ADDRESS / 64
WASSER_LEFT_BLOCK      = WASSER_LEFT_ADDRESS / 64
WASSER_HURT_BLOCK      = WASSER_HURT_ADDRESS / 64
PLAYER_FASTLEFT_BLOCK   = PLAYER_FASTLEFT_ADDRESS / 64
PLAYER_FASTRIGHT_BLOCK  = PLAYER_FASTRIGHT_ADDRESS / 64
EMBER_FASTLEFT_BLOCK    = EMBER_FASTLEFT_ADDRESS / 64
EMBER_FASTRIGHT_BLOCK   = EMBER_FASTRIGHT_ADDRESS / 64
WASSER_FASTLEFT_BLOCK   = WASSER_FASTLEFT_ADDRESS / 64
WASSER_FASTRIGHT_BLOCK  = WASSER_FASTRIGHT_ADDRESS / 64
; HUD columns 30..37 correspond to sprite X=264..327; center the 24-pixel character at
; X=284.
HUD_CHARACTER_X_LOW     = 28       ; 284-256; $d010 bit 4 supplies the ninth bit.
HUD_CHARACTER_Y         = SCREEN_CHARACTER_TOP_Y + HUD_CHARACTER_ROW*8 + 1

; Legacy coordinates retained in this source; the current text-only leaderboard does not
; use these constants or enable a game-over character sprite.
GAME_OVER_PLAYER_X      = 172
GAME_OVER_PLAYER_Y      = 124

; The title displays three characters. POTX left/middle/right ranges map to characters
; 0/1/2. After locking, keep only centered sprite 0 and use left/normal/right frames to
; request paddle centering.
PLAYER_CHARACTER_COUNT        = 3
CHARACTER_ELIEN               = 0
CHARACTER_EMBER               = 1
CHARACTER_WASSER              = 2
CHARACTER_SELECT_MIDDLE_START = 85
CHARACTER_SELECT_RIGHT_START  = 171
CHARACTER_SELECT_LEFT_X       = 96
CHARACTER_SELECT_CENTER_X     = 172
CHARACTER_SELECT_RIGHT_X      = 248
CHARACTER_SELECT_Y            = 124
CHARACTER_SELECT_HURT_FRAMES  = 12

; Gameplay glyph/screen codes and separate menu text codes; no fragment/letter aliases.
.include "charset_ids.inc"

; Start/end text positions are offsets relative to either screen buffer.
TITLE_OFFSET            = 8 * SCREEN_COLUMNS + 16
PROMPT_OFFSET           = 14 * SCREEN_COLUMNS + 15
CENTERING_PROMPT_ROW    = 14
CENTERING_PROMPT_COLUMN = 2
CENTERING_PROMPT_LENGTH = 35
CENTERING_PROMPT_OFFSET = CENTERING_PROMPT_ROW * SCREEN_COLUMNS + CENTERING_PROMPT_COLUMN
GAME_OVER_OFFSET        = 8 * SCREEN_COLUMNS + 15

COLOR_BLACK             = 0
COLOR_WHITE             = 1
COLOR_RED               = 2
COLOR_CYAN              = 3
COLOR_PURPLE            = 4
COLOR_GREEN             = 5
COLOR_BLUE              = 6
COLOR_YELLOW            = 7
COLOR_GRAY              = 12
COLOR_LIGHT_BLUE        = 14

; Shared white/gray; local colors provide red bricks, white/green spikes, and red fade
; bricks.
BRICK_CELL_COLOR        = MULTICOLOR_CELL | COLOR_RED
SPIKE_CELL_COLOR        = MULTICOLOR_CELL | COLOR_GREEN
FADE_CELL_COLOR         = MULTICOLOR_CELL | COLOR_RED
CONVEYOR_CELL_COLOR     = MULTICOLOR_CELL | COLOR_BLUE
UI_FRAME_COLOR          = BRICK_CELL_COLOR

; World scrolling uses an 8-bit phase accumulator. Add rate each frame and advance one
; pixel only on unsigned carry. The base rate is 128/256=0.5 pixels/frame; 160/192/224
; gives 1.25/1.5/1.75 times the base speed. The maximum is exactly 1 pixel/frame (2x);
; since 256 cannot fit in a byte, rate=0 is a special marker.
SCROLL_RATE_LEVEL_0     = 128
SCROLL_RATE_LEVEL_1     = 160
SCROLL_RATE_LEVEL_2     = 192
SCROLL_RATE_LEVEL_3     = 224
SCROLL_RATE_LEVEL_4     = 0
SCROLL_LEVEL_MAX        = 4
; Difficulty advances at 20, 40, 60, and 80 first-landed platforms; thresholds store
; tens digits.
SCORE_LEVEL_THRESHOLD_COUNT = SCROLL_LEVEL_MAX
SCORES_PER_HP           = 3
PLATFORM_SCORE_UNCLAIMED = $01
PLATFORM_SCORE_CLAIMED   = $81
PLATFORM_SCORE_CLAIMED_MASK = $80
PLATFORM_GAP_ROWS       = 4
SEED_PLATFORM_COUNT     = 5
PLATFORM_MIN_WIDTH      = 7
PLATFORM_WIDTH_COUNT    = 6

; platform_type is a contiguous array index for generation/rendering. Collision results
; use separate constants because activated fading platforms require one additional
; state.
PLATFORM_TYPE_NORMAL         = 0
PLATFORM_TYPE_SPIKES         = 1
PLATFORM_TYPE_FADE           = 2
PLATFORM_TYPE_SPRING         = 3
PLATFORM_TYPE_CONVEYOR_LEFT  = 4
PLATFORM_TYPE_CONVEYOR_RIGHT = 5
PLATFORM_TYPE_COUNT          = 6
PLATFORM_REPEAT_LIMIT        = 2
PLATFORM_GENERATION_LEVEL_MAX = 4

COLLISION_NONE               = 0
COLLISION_NORMAL             = 1
COLLISION_SPIKES             = 2
COLLISION_FADE_INACTIVE      = 3
COLLISION_FADE_ACTIVE        = 4
COLLISION_SPRING             = 5
COLLISION_CONVEYOR_LEFT      = 6
COLLISION_CONVEYOR_RIGHT     = 7

; Conveyors push two pixels every three frames, matching normal paddle speed. A separate
; counter lets conveyor motion add to player input instead of sharing its throttle
; phase.
CONVEYOR_MOVE_DELAY     = 2
CONVEYOR_PUSH_STEP      = 2
; Animate the surface one multicolor pixel (two display pixels) every three frames, with
; or without a rider.
CONVEYOR_ANIMATION_FRAMES = 3
POTX_DEADZONE_LOW       = 108
POTX_DEADZONE_HIGH      = 148
POTX_FAR_LEFT_END       = 64
POTX_FAR_RIGHT_START    = 192
PLAYER_MOVE_DELAY       = 2
PLAYER_FAST_MOVE_DELAY  = 1
PLAYER_MOVE_STEP        = 2
; Clamp the player inside the brick frame: column 2 starts at sprite X=40, and the
; divider is at X=256, so the rightmost position of the 24-pixel player is X=232.
PLAYER_MIN_X            = 40
PLAYER_MAX_X_LOW        = $e8
PLAYER_MAX_X_HIGH       = 0
PLAYER_HEIGHT           = 21
; The fixed matrix begins at sprite Y=50; 43 is only the software platform-phase base.
; Adding fine_scroll=7 gives the full-character position. Fixed hazards must not use 43
; directly.
SCREEN_CHARACTER_TOP_Y = 50
SCREEN_ROW0_BASE_Y      = SCREEN_CHARACTER_TOP_Y - 7
GRAVITY_MAX             = 8
SPIKE_DAMAGE_BOUNCE_VELOCITY = 16
CEILING_DAMAGE_DROP_PIXELS = 8
PLAYER_HURT_FRAMES      = 16

; Ceiling spikes occupy row 2, ending at Y=73. Head contact consumes 1 HP and moves the
; player clear; zero HP is fatal. Bottom contact at Y=234 remains fatal (player Y=214).
PLAYER_CEILING_CONTACT_Y = SCREEN_CHARACTER_TOP_Y + (CEILING_SPIKE_ROW + 1) * 8 - 1
PLAYER_BOTTOM_GAME_OVER_Y = SCREEN_CHARACTER_TOP_Y + UI_FRAME_BOTTOM_ROW * 8 - (PLAYER_HEIGHT-1)

; Stepping on a fade platform removes some mortar; only bricks remain after 25 frames,
; then vanish at 50 (~1 PAL second).
FADE_PLATFORM_LIFETIME  = 50
FADE_PLATFORM_BROKEN_TIME = FADE_PLATFORM_LIFETIME / 2
FADE_EFFECT_ACTIVATE    = 0
FADE_EFFECT_ADVANCE     = 1
FADE_EFFECT_REMOVE      = 2

; Landing compresses a spring for several frames, then sets an upward velocity in
; 1/8-pixel units. Initial velocity 24, decaying by 1 each frame, gives a theoretical
; rise of about 37 pixels.
SPRING_COMPRESSION_FRAMES = 8
SPRING_LAUNCH_VELOCITY  = 24
SPRING_EFFECT_COMPRESS  = 0
SPRING_EFFECT_RESTORE   = 1

; Shared zero-page temporary pointers. 6502 (pointer),Y addressing requires the pointer
; in zero page, with low byte first and high byte next. The music IRQ does not use these
; pointers, so interrupts cannot corrupt main-loop screen copies.
ROW_PTR                 = $fb
DEST_PTR                = $fd
NEXT_ROW_PTR            = $f9
COLOR_PTR               = $f7
