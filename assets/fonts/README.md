# Font provenance

`c64-upper.bin` is the unchanged first 2048 bytes of the 4096-byte `c64.bin`
provided with the teacher's charset example. It selects the first uppercase
256-glyph set, not the second lowercase set.

- Source: teacher-provided `c64-demos-main/charsets/c64.bin`
- Method reference: `c64-demos-main/charsets/main.asm`
- Byte slice: `[0, 2048)`; raw binary, no PRG load-address header
- SHA-256: `3cf89732b10b1d51a267f74df35f10a154108b444a3a0ec9e51ef7ddefb668a1`

The project-local binary is embedded by `src/text_charset.inc`; builds do not
depend on the external path. Runtime copies selected glyphs to the output.
Some digits and punctuation differ from the previous hand-coded font by design.
These differences are expected and are not caused by smooth scrolling overwriting glyphs.
