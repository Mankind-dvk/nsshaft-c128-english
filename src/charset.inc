; Reserves the live charset read by VIC-IIe. Scene setup copies selected source
; glyphs here; scrolling updates the dynamic fragments in this buffer.
;
; Quick reference:
;   line 9: live buffer and size checks
;   line 15: source fonts and composition routines

.segment "CHARSET"
charset_begin:
    .res CHARSET_GLYPH_COUNT*CHARSET_GLYPH_BYTES, 0
charset_end:
.assert charset_begin = CHARSET_ADDRESS, error, "Live charset moved"
.assert charset_end = $2a20, error, "Live charset overlaps music"

.include "graphics_charset.inc"
.include "text_charset.inc"
.include "charset_loader.inc"
