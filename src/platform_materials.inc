; Builds brick/fade fragments and maps screen codes to colors and collision types.
; Clipped areas stay black. Springs and text use hires glyphs; brick and conveyor
; textures use multicolor. This code shares the charset-loader memory segment.
;
; Quick reference:
;   line 14: fragment bitmaps
;   line 67: per-character colors
;   line 84: collision lookup
;   line 129: platform colors
;   line 136: lower/upper fragment code tables

.segment "CHARSETCODE"

update_brick_and_fade_fragments:
    ldy #7
    lda #0
@clear:
    sta normal_upper_glyph, y
    sta normal_lower_glyph, y
    sta fade_upper_glyph, y
    sta fade_lower_glyph, y
    sta fade_cracked_upper_glyph, y
    sta fade_cracked_lower_glyph, y
    sta fade_broken_upper_glyph, y
    sta fade_broken_lower_glyph, y
    dey
    bpl @clear

    ; Split source rows: place the first part at the preceding cell's bottom and the
    ; rest at the current cell's top.
    ldx #0
    ldy fragment_split
@upper:
    lda brick_source_glyph, x
    sta normal_upper_glyph, y
    lda fade_source_glyph, x
    sta fade_upper_glyph, y
    lda fade_cracked_source_glyph, x
    sta fade_cracked_upper_glyph, y
    lda fade_broken_source_glyph, x
    sta fade_broken_upper_glyph, y
    inx
    iny
    cpx fragment_count
    bne @upper

    ldx fragment_count
    ldy #0
@lower:
    lda brick_source_glyph, x
    sta normal_lower_glyph, y
    lda fade_source_glyph, x
    sta fade_lower_glyph, y
    lda fade_cracked_source_glyph, x
    sta fade_cracked_lower_glyph, y
    lda fade_broken_source_glyph, x
    sta fade_broken_lower_glyph, y
    inx
    iny
    cpx #8
    bne @lower
    rts

; Color RAM bit 3=1 selects multicolor, 0 selects hires; the low three bits select cell
; foreground color. Shared multicolor slots: 00 black, 01 white, 10 gray, 11 local
; foreground.
game_character_colors:
    .byte COLOR_BLACK, BRICK_CELL_COLOR, SPIKE_CELL_COLOR ; 0..2
    .byte COLOR_PURPLE, COLOR_PURPLE, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR, FADE_CELL_COLOR ; 3..7
    .byte BRICK_CELL_COLOR, BRICK_CELL_COLOR, SPIKE_CELL_COLOR, SPIKE_CELL_COLOR ; 8..11
    .res 4, COLOR_PURPLE        ; 12..15 spring fragments
    .byte FADE_CELL_COLOR, FADE_CELL_COLOR, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR ; 16..19
    .byte UI_FRAME_COLOR, SPIKE_CELL_COLOR ; 20..21
    .res GLYPH_FREE_COUNT, COLOR_BLACK ; 22..39 available for future glyphs
    .res 16, COLOR_WHITE        ; 40..55 HUD text
    .res 6, FADE_CELL_COLOR     ; 56..61 intact/broken fade variants
    .res 4, COLOR_WHITE         ; 62..65 SCORE letters
    .res 2, CONVEYOR_CELL_COLOR ; 66..67 conveyor LOWER fragments
game_character_colors_end:
.assert game_character_colors_end-game_character_colors = CHARSET_GLYPH_COUNT, error, "Color table size mismatch"

; Screen-code -> logical support type. UPPER fragments stay non-solid so the collision
; anchor remains on FULL/LOWER instead of moving up one character row.
collision_type_by_char:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte COLLISION_NORMAL
    .elseif code = CHAR_NORMAL_LOWER
        .byte COLLISION_NORMAL
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte COLLISION_SPIKES
    .elseif code = CHAR_SPIKE_LOWER
        .byte COLLISION_SPIKES
    .elseif code = CHAR_FADE_FULL
        .byte COLLISION_FADE_INACTIVE
    .elseif code = CHAR_FADE_LOWER
        .byte COLLISION_FADE_INACTIVE
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_CRACKED_LOWER
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_FADE_BROKEN_LOWER
        .byte COLLISION_FADE_ACTIVE
    .elseif code = CHAR_SPRING_FULL
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_LOWER
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte COLLISION_SPRING
    .elseif code = CHAR_SPRING_COMPRESSED_LOWER
        .byte COLLISION_SPRING
    .elseif code = CHAR_CONVEYOR_LEFT_FULL
        .byte COLLISION_CONVEYOR_LEFT
    .elseif code = CHAR_CONVEYOR_LEFT_LOWER
        .byte COLLISION_CONVEYOR_LEFT
    .elseif code = CHAR_CONVEYOR_RIGHT_FULL
        .byte COLLISION_CONVEYOR_RIGHT
    .elseif code = CHAR_CONVEYOR_RIGHT_LOWER
        .byte COLLISION_CONVEYOR_RIGHT
    .else
        .byte COLLISION_NONE
    .endif
.endrepeat
collision_type_by_char_end:
.assert collision_type_by_char_end-collision_type_by_char = CHARSET_GLYPH_COUNT, error, "Collision table size mismatch"

platform_colors:
    .byte BRICK_CELL_COLOR, SPIKE_CELL_COLOR, FADE_CELL_COLOR
    .byte COLOR_PURPLE, CONVEYOR_CELL_COLOR, CONVEYOR_CELL_COLOR

; Generate CHARSET_GLYPH_COUNT-entry conversion tables at assembly time; look up FULL ->
; LOWER/UPPER once per platform. Map FULL codes to LOWER fragments; non-platform codes
; map to themselves.
fragment_lower_codes:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte CHAR_NORMAL_LOWER
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte CHAR_SPIKE_LOWER
    .elseif code = CHAR_SPRING_FULL
        .byte CHAR_SPRING_LOWER
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte CHAR_SPRING_COMPRESSED_LOWER
    .elseif code = CHAR_FADE_FULL
        .byte CHAR_FADE_LOWER
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte CHAR_FADE_CRACKED_LOWER
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte CHAR_FADE_BROKEN_LOWER
    .elseif code = CHAR_CONVEYOR_LEFT_FULL
        .byte CHAR_CONVEYOR_LEFT_LOWER
    .elseif code = CHAR_CONVEYOR_RIGHT_FULL
        .byte CHAR_CONVEYOR_RIGHT_LOWER
    .else
        .byte code
    .endif
.endrepeat

; Map each platform's FULL code to its UPPER fragment, drawn one row above.
fragment_upper_codes:
.repeat CHARSET_GLYPH_COUNT, code
    .if code = CHAR_PLATFORM_FULL
        .byte CHAR_NORMAL_UPPER
    .elseif code = CHAR_SPIKE_TRIANGLE
        .byte CHAR_SPIKE_UPPER
    .elseif code = CHAR_SPRING_FULL
        .byte CHAR_SPRING_UPPER
    .elseif code = CHAR_SPRING_COMPRESSED_FULL
        .byte CHAR_SPRING_COMPRESSED_UPPER
    .elseif code = CHAR_CONVEYOR_LEFT_FULL
        .byte CHAR_CONVEYOR_LEFT_UPPER
    .elseif code = CHAR_CONVEYOR_RIGHT_FULL
        .byte CHAR_CONVEYOR_RIGHT_UPPER
    .elseif code = CHAR_FADE_FULL
        .byte CHAR_FADE_UPPER
    .elseif code = CHAR_FADE_CRACKED_FULL
        .byte CHAR_FADE_CRACKED_UPPER
    .elseif code = CHAR_FADE_BROKEN_FULL
        .byte CHAR_FADE_BROKEN_UPPER
    .else
        .byte CHAR_BLANK
    .endif
.endrepeat

.assert * <= $5400, error, "Material renderer exceeds charset-code memory"
