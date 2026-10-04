; The 2 KB uppercase font from the supplied c64.bin, embedded in the PRG.
; Scene setup copies text glyphs from here. See assets/fonts/README.md for the
; source and extraction details.
;
; Quick reference:
;   line 10: font data
;   line 13: address and size checks

.segment "TEXT_CHARSET"
text_charset:
    .incbin "../assets/fonts/c64-upper.bin"
text_charset_end:
.assert text_charset = TEXT_CHARSET_ADDRESS, error, "Text source moved"
.assert text_charset_end-text_charset = SOURCE_CHARSET_BYTES, error, "Text source must be 2 KB"
