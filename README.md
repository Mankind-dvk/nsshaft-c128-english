# Native Commodore 128 NS-Shaft prototype

This is a native C128/8502 port using the VIC-IIe 40-column output. It starts
from a BASIC 7 program at `$1c01` (`SYS 7424`), runs entirely in C128 mode, and
does not enter C64 compatibility mode. The program forces 1 MHz operation,
maps the 8502 and VIC-IIe to RAM bank 0, and uses the 40-column display.

Short repeated assembly operations live in `src/macros.inc`; gameplay remains
in subsystem subroutines. See `ARCHITECTURE.md` and the source comments for
macro contracts and expansion behavior.

The game implements the core NS-Shaft descent loop. Platforms are custom
character tiles and move upward one pixel at a time. A player standing on a
platform is carried upward with it; walking beyond an edge starts a
gravity-driven fall. The player lands on the next supporting platform below.
The red-brick/white-mortar character frame has a fixed row of white/green downward spikes beneath its
top edge, over the playfield only. Head contact consumes 1 HP and drops the
player clear of the ceiling; contact with zero HP is fatal. Ordinary spike
platforms also consume HP when available; falling through the bottom ends the
game immediately regardless of HP. Disappearing platforms start as red bricks
with gray mortar. Stepping on one removes part of the mortar; after 25 timer
updates only the bricks remain, and after 50 updates the platform disappears
(about one second at PAL's 50 Hz). Purple spring
platforms use a zigzag coil tile: landing compresses it into a solid lower-half block, then
launches the player upward before normal gravity resumes. White-and-blue conveyor
platforms scroll their repeating texture left or right by two pixels every three
frames while the platform itself continues upward. Their horizontal boundaries
stay fixed; a supported player is pushed two pixels every three frames in the
texture's direction.

The program opens on an `NS-SHAFT / PRESS FIRE` character-selection screen.
Three normal sprites are shown side by side: Elien, Ember, and Wasser. POTX
0..84, 85..170, and 171..255 select them respectively; the selected character
keeps its native color while the other two turn gray. Paddle button 1, paddle
button 2, or joystick FIRE on control port 1 locks that choice.

The locked character then moves to the center. It faces left below POTX 108,
faces right above 148, and uses its normal forward frame inside 108..148. This
is accompanied by the instruction `PRESS FIRE WHEN CHARACTER FACES YOU`. A
second FIRE press followed by release starts the round only when POTX is
centered at both checks. Pressing FIRE while it faces left or right shows that
character's hurt frame for at least 12 PAL frames and does not start the round.
After `GAME OVER`, FIRE returns to the selection screen so the next round may
use another character. Scrolling and gameplay are paused throughout these modal
screens.

### Persistent TOP 5

Game over checks the six-digit score before asking for a name. Qualifying
players enter up to eight letters, digits or spaces on the keyboard: DEL erases,
RETURN confirms, and an all-space name is rejected. Tied scores keep older
records first. The text-only leaderboard highlights the new entry; FIRE returns
to character selection. No hurt sprite is drawn on these pages.

Use `run-x128.ps1`: it creates `saves/nsshaft-scores.d64` only on first launch,
mounts it as writable device 8, and injects the PRG without replacing that disk.
Keep this disk to retain scores across program rebuilds and emulator restarts.
Close VICE before copying it for backup. Hardware users need a writable disk in
the loading drive (device 8 by default).

Two small SEQ files, `NSSHAFT.A` and `NSSHAFT.B`, alternate updates. Each has a
version, generation and CRC; saves are read back before reporting success, and
the previous valid slot remains as a fallback. On disk failure, R retries and
FIRE continues with the in-memory table; unsaved scores are explicitly marked.
Leaderboard code/data starts at `$5400`, outside the packed VIC asset regions.

For C128/Pi1541 use, put the PRG in a writable D64 and keep that image mounted
during play. The game creates its two SEQ files inside the mounted image after
a qualifying score; wait for the save to finish before powering off Pi1541.

Platform positions and widths use a PRNG seeded from the KERNAL clock, CIA
timers/TOD and raster timing, mixed with two bytes inside the loaded PRG. Those
bytes are not power-up RAM entropy, and the KERNAL clock stops updating once
the game takes over IRQ service. Timing can vary the seed; uniqueness is not
guaranteed. The VICE launcher also supplies a host-time-derived emulator seed.

Each generated platform also uses the same PRNG stream to select from six
custom-character styles:

- normal platform
- white/green multicolor upward-pointing spike platform
- red-brick/gray-mortar disappearing platform
- purple zigzag spring platform
- white-and-blue conveyor with a left-scrolling texture
- white-and-blue conveyor with a right-scrolling texture

Position, width, and type are reproducible for the same seed, difficulty changes,
and PRNG call order.
Type selection uses score-gated weighted pools rather than exposing every hazard
at the start:

| Score | Unlocked types | Normal share in candidate pool |
| ---: | --- | ---: |
| 0..4 | normal, spring | 75% |
| 5..9 | normal, spring, disappearing | 50% |
| 10..19 | previous types plus both conveyors | 37.5% |
| 20..39 | all types, including spikes | 25% |
| 40+ | all types with extra spike/fade weight | 12.5% |

Normal platforms may repeat freely. Every other individual type is limited to
two consecutive generated platforms; a third identical candidate consumes the
next deterministic PRNG byte and is selected again. The percentages above describe
candidate-pool weights; rejection of repeated types changes the accepted distribution.
The display mixes multicolor and hires characters, with ECM disabled. Normal
platforms use the same red-brick/white-mortar texture as the frame; blank fragment
rows remain black while the pattern moves upward one pixel at a time. Shared
colors are black/white/gray; Color RAM=10 selects multicolor with red bricks.
Disappearing platforms use three independent intact/cracked/broken glyph sets
with Color RAM=10 throughout. Gray mortar progressively becomes black gaps;
neither the red bricks nor the shared palette flash. Conveyors use Color RAM=14
for white-and-blue multicolor textures. Spikes use white/green multicolor;
purple springs and text remain hires. Row transitions clear
only the hidden playfield and draw contiguous spans from platform descriptions,
using one material lookup per span and synchronizing occupied cells' Color RAM.
Conveyor texture animation runs even without a passenger and on frames when the
world does not move upward; its four horizontal phases repeat every 12 frames.

The selected 24x21 hires character starts centered over the lowest initial
platform. Elien is yellow, Ember is red, and Wasser is cyan. A paddle connected
to control port 1 controls horizontal movement:

- POTX 108..148: stop
- POTX 64..107: move left at the normal speed
- POTX 149..191: move right at the normal speed
- POTX 0..63 or 192..255: move in that direction at 1.5x speed

Normal movement is two pixels every three frames. The two extreme paddle ranges
move two pixels every two frames, exactly 1.5 times the normal average speed.
Movement remains clamped inside the playfield.
The hardware border is black. Brick characters draw an inset frame on rows 1/23
and columns 1/38, while column 29 separates the game area from the eight-column
status panel at columns 30..37. `$d011` stays at the natural 25-row phase 3, so
the full 200-pixel character matrix is visible and no border-gap sprite is
required. `POTX:xxx` is rendered directly as eight fixed status-panel
characters. The score uses two fixed rows: `SCORE:` followed by `000000`. The
six digits count distinct platforms reached. Each platform carries a claim flag
that moves upward with its character row. The first airborne landing sets that
flag and awards one point; bouncing or returning to the same platform does not.
First contact with an unclaimed spike platform also awards its point before HP
damage or a fatal result is processed, so spikes remain risky but are not
scoreless.
The next centered row displays `HP:000`. Every three awarded platform points add
one HP, capped at 255, without reducing score. A spike hit consumes one HP and bounces the
player upward; top-frame contact consumes one HP and drops the player back into
the shaft. Either hazard is fatal when HP is already zero. An absorbed hit gives
the selected character's hurt frame priority for 16 player-frame updates. These
hurt sprites appear during gameplay and off-center selection feedback; the
modal game-over screen uses a text-only leaderboard.

World scrolling uses an 8-bit phase accumulator instead of a whole-frame delay.
The initial rate is 128/256 pixel per update, or one pixel every two main-loop
updates. At 20, 40, 60, and 80 points it moves
through rates 160, 192, and 224, then an exact one-pixel-per-frame mode. These
are 1.25x, 1.5x, 1.75x, and 2x the starting speed. Because 256 cannot fit in an
8-bit rate, zero is reserved as the final 256/256 sentinel. Speed follows newly
reached platforms rather than scroll distance and cannot accelerate itself
through a score feedback loop. The SID sequencer uses the same score-level index
and therefore accelerates by the same five multipliers:

| Score | Scroll | Music tempo |
| ---: | ---: | ---: |
| 0..19 | 1.00x | about 129 BPM |
| 20..39 | 1.25x | about 161 BPM |
| 40..59 | 1.50x | about 193 BPM |
| 60..79 | 1.75x | about 226 BPM |
| 80+ | 2.00x | about 258 BPM |

Only event timing changes; SID note frequencies are not transposed. A modulo-512
music phase accumulator represents all five ratios exactly, including 1.25x and
1.75x.

The bottom of the status panel uses sprite 4 to show the selected character's
existing fast-left, left, normal, right, or fast-right frame. It follows the
paddle direction and speed, with 108..148 selecting the forward frame.
No bitmap copy is needed; both screen buffers receive the new sprite pointer.
The divider, POTX display, and direction indicator remain stationary while the left
playfield scrolls. The supplied VICE launcher attaches paddles to control port
1 and maps POTX to the host mouse.

Gameplay includes a three-voice SID arrangement derived from
`Quarter_Slot.mp3`. The MP3 is analysis/reference material only and is not
decoded by the C128. The runtime arrangement uses pulse-wave lead, triangle
bass, and a third voice shared by sawtooth arpeggios and short noise drums. A
raster IRQ at line 240 advances the music independently of expensive scrolling
frames. Its base PAL tempo is approximately 129 BPM and follows the current
score-speed tier up to 2x. Music stops on the game-over screen and restarts from
bar one at base tempo with a new game.

The renderer combines:

- a fixed `$d011` phase of 3 so the status panel never bobs vertically and the
  25-row matrix exactly covers the display window
- runtime-updated fragment glyphs for pixel-smooth rectangular, spike, fade,
  spring, and conveyor platforms
- row-indexed platform descriptions: start, width and canonical FULL code
  (including fade/spring state), totaling 60 bytes; width zero marks an empty row
- a hidden-playfield clear and descriptor-driven redraw at the two character
  layout transitions; the six intermediate pixel steps still only update glyphs

## Memory map (address order)

This table lists the current address ranges in ascending order. Ranges are
inclusive; the two score-state rows are subranges of `CODE`, not extra space.
The architecture-grouped view is in `ARCHITECTURE.md`.

| Range | Use |
| --- | --- |
| `$00f7-$00fe` | Four two-byte zero-page screen/color scratch pointers; outside the PRG segments |
| `$0400-$07ff` | Screen buffer A; sprite pointers at `$07f8-$07ff` |
| `$0c00-$0fff` | Screen buffer B; sprite pointers at `$0ff8-$0fff` |
| `$1c01-$1c0c` | BASIC 7 `SYS 7424` loader |
| `$1d00-$25da` | Core code and writable state: main loop, video, platforms, HUD and scoring (`CODE`) |
| `$257b-$2580` | Current six score digits, within `CODE` |
| `$2587-$259a` | 20 per-row first-landing score flags, within `CODE` |
| `$2800-$2a1f` | Writable 68-slot mixed-mode output charset; slots 22..39 free |
| `$2a20-$2de2` | SID player, frequency tables and 16-bar arrangement |
| `$2e00-$2f2c` | Disappearing-platform effect routines |
| `$3000-$30ff` | First sprite bitmap batch: Elien normal, fall, right and left (four 64-byte frames) |
| `$3100-$313f` | Unused block; HUD sprite 4 reuses the character bitmaps |
| `$3140-$376d` | Player physics, ceiling recovery and spring/conveyor routines |
| `$3800-$39da` | Ceiling drawing, weighted platform generation, HP display and spike recovery |
| `$3a00-$3cbf` | Second sprite bitmap batch: Elien hurt plus five Ember and five Wasser frames (eleven 64-byte frames) |
| `$3cc0-$3e6a` | Character-selection code, prompt text and frame/color lookup tables |
| `$3e80-$3fff` | Fast sprite bitmap batch: fast-left and fast-right for Elien, Ember and Wasser (six 64-byte frames) |
| `$4000-$47ff` | Immutable graphics-only source charset, 2 KB |
| `$4800-$4fff` | Immutable uppercase text source charset, 2 KB |
| `$5000-$5360` | Material/collision tables, descriptor effect updates, conveyor animation and charset composition |
| `$5400-$630a` | Top-five leaderboard, name entry, display and disk-storage code/data; entries at `$5af1-$5b36` and disk buffer at `$62bb-$630a` |
| `$d800-$dbff` | Shared VIC color RAM, not a PRG segment |

The PRG's two-byte load-address header appears at `$1bff-$1c00` in the linker
map but is not runtime RAM. Its fixed output image is zero-filled through
`$7fff`; the last allocated segment ends at `$630a`.

Each logical row holds at most one platform, separated from the next platform by
blank rows. `draw_platform` records its description; coarse scrolling shifts
descriptions with score claims, and fade/spring changes update the stored FULL
code as well as both screen matrices. Collision still samples the visible
matrix. Clearing a scene also clears descriptions, preventing old platforms
from reappearing on the next round. Hidden matrices retain old contents until
cleared; the fixed frame, ceiling, HUD and sprite pointers are never cleared by
the playfield renderer. Glyph RAM and Color RAM remain shared, not double-buffered.

The loop targets one update per PAL frame; polling does not catch up missed
frames. CPU-cycle comparisons are not a substitute for VIC/IRQ timing checks.

Gameplay enables only hardware sprites 0 (selected character) and 4 (direction
indicator using the same character frames). The title selector temporarily
enables sprites 0..2 to preview the three characters; the 21 bitmap frames are
memory blocks, not 21 hardware sprites.

## Source layout

The source follows a subsystem-oriented ca65 layout. `src/main.s` contains the
BASIC loader, C128 bootstrap, new-game setup, and main frame loop. It includes
small modules for display, characters, platform logic, audio, scoring and
leaderboard persistence. The list below groups files by responsibility rather
than by their literal `.include` order:

```text
src/
  # Entry and shared state
  main.s            program entry and frame orchestration
  constants.inc     hardware addresses and shared layout constants
  macros.inc        reusable inline assembly operations
  state.inc         mutable game state bytes

  # Display and charsets
  video.inc         scrolling, double buffering, modal screens, UI characters
  charset_ids.inc   game glyph IDs and separate modal text codes
  charset.inc       writable output charset and resource includes
  graphics_charset.inc source 1: custom graphics only
  text_charset.inc  source 2: imported uppercase font
  charset_loader.inc range copies and scene-entry composition

  # Characters and controls
  assets.inc        sprite bitmap batches, charset include and immutable tables
  character_select.inc title selection, neutral check and character frame tables
  input.inc         paddle/POTX sampling
  player.inc        movement, gravity, landing and spike collision
  hud.inc           character POTX display and sprite direction indicator

  # World and hazards
  platform_materials.inc fragments, colors, transition and collision lookup tables
  platforms.inc     PRNG and platform generation
  platform_rows.inc row descriptions, hidden-playfield clear and span rendering
  fade_platforms.inc disappearing-platform lifecycle and staged crumbling
  spring_platforms.inc spring compression, restoration and upward launch
  conveyor_platforms.inc horizontal texture animation and vertical glyph fragments
  health.inc        HP rewards, decimal HUD and hazard recovery

  # Audio
  music.inc         raster IRQ, SID instruments and 16-bar music patterns

  # Scoring and leaderboard
  score.inc         first-landing score, claim rows and scroll-rate scheduler
  highscores.inc    top-five ranking, name entry and leaderboard screen
  highscore_storage.inc dual-slot disk load/save and validation
```

Gameplay imports only the HUD text it needs; modal screens use the complete
basic uppercase text page. Both source charsets ship inside the PRG; no
additional disk load or raster split is used. See `src/charset_ids.inc` for
slot assignments and [font provenance](assets/fonts/README.md) for the imported font.

This submission contains the assembly sources, linker configuration, build/run
scripts, required font asset, README, and architecture document. Optional
regression tests, beginner guides, and the extended charset-layout document
remain in the full development project. They are not needed to build or run
this copy. Generated builds, score disks, audio assets, sprite-editor projects,
and presentations are excluded from the submission.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the call flow, memory map, sprite
allocation, and cross-module invariants. All source comments are in English.
The comments explain routine inputs, outputs, register clobbers, and important
hardware registers, including the BASIC loader, main loop, double buffering,
smooth scrolling, platform collision, paddle input, sprites, and SID IRQ music.

## Build and run

Install the [cc65 toolchain](https://cc65.github.io/getting-started.html) and
[VICE](https://vice-emu.sourceforge.io/). Put their binaries on PATH, then run:

```powershell
.\build.ps1
.\run-x128.ps1
```

Or supply installation paths without changing the scripts:

```powershell
.\build.ps1 -Cc65Bin 'C:\cc65\bin'
.\run-x128.ps1 -Cc65Bin 'C:\cc65\bin' -VicePath 'C:\VICE\bin\x128.exe'
```

`CC65_BIN` and `VICE_X128` environment variables are also supported. The
output is `build/nsshaft-c128.prg`; the font input is under `assets/fonts/`.

On a real C128, use the 40-column video output, load the PRG normally from
BASIC 7, and run it:

```basic
LOAD"nsshaft-c128.prg",8
RUN
```
