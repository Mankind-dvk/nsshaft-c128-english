; Three-voice SID arrangement of Quarter Slot, with 128 eighth-note steps.
; Voice 1 plays melody, voice 2 bass, and voice 3 arpeggios or noise drums.
; A raster IRQ updates the music once per PAL frame. It uses no zero page and
; preserves A/X/Y. Score tiers increase the tempo together with scrolling speed.
; Pattern entries are note indices; index 0 means rest.
;
; Quick reference:
;   line 17: SID and raster IRQ setup
;   line 83: IRQ entry and frame sequencing
;   line 142: IRQ shutdown and silence
;   line 159: voice triggers
;   line 276: frequency tables
;   line 293: melody, bass, arpeggio and drum patterns

.segment "MUSIC"

initialize_music:
    ; Output: clear SID/player state, configure all three instruments, install/enable
    ; the raster IRQ. Use SEI/CLI around vector/source setup to prevent an IRQ after
    ; only the vector's low byte is written.
    sei
    lda #0
    sta music_step
    sta music_tempo_accumulator_low
    sta music_tempo_accumulator_high
    sta music_gate_timer
    sta music_voice3_delay
    sta SID_FILTER_CUTOFF_LOW
    sta SID_FILTER_CUTOFF_HIGH
    sta SID_FILTER_CONTROL

    ; The melody uses a 50%-duty pulse wave.
    sta SID_VOICE_1_PULSE_LOW
    lda #$08
    sta SID_VOICE_1_PULSE_HIGH
    lda #$24
    sta SID_VOICE_1_ATTACK
    lda #$84
    sta SID_VOICE_1_SUSTAIN

    ; Use a fast-attack triangle bass to avoid competing with the melody's high
    ; frequencies.
    lda #$03
    sta SID_VOICE_2_ATTACK
    lda #$a4
    sta SID_VOICE_2_SUSTAIN

    lda #$02
    sta SID_VOICE_3_ATTACK
    lda #$52
    sta SID_VOICE_3_SUSTAIN

    lda #$0f
    sta SID_VOLUME
    jsr music_trigger_current_step

    ; Disable KERNAL/CIA interrupts, retaining one stable VIC-IIe music raster IRQ.
    lda #0
    sta VIC_IRQ_ENABLE
    lda #$7f
    sta CIA1_INTERRUPT_CONTROL
    sta CIA2_INTERRUPT_CONTROL
    lda CIA1_INTERRUPT_CONTROL
    lda CIA2_INTERRUPT_CONTROL

    ; C128 KERNAL dispatches IRQs through $0314/$0315. Its hardware entry already saves
    ; A/X/Y and MMU configuration. Do not call the original KERNAL IRQ service, which
    ; would treat the VIC-IIe raster IRQ as a system display interrupt and cause sprite
    ; flicker.
    SET_WORD KERNAL_IRQ_VECTOR, music_raster_irq
    ; Do not use wait_for_frame's raster line; IRQ return may miss the synchronization
    ; point and force the main loop to wait another unnecessary frame.
    lda #MUSIC_IRQ_RASTER
    sta VIC_RASTER
    lda VIC_CONTROL_1
    and #%01111111
    sta VIC_CONTROL_1
    ACK_VIC_RASTER_IRQ
    sta VIC_IRQ_ENABLE
    cli
    rts

music_raster_irq:
    ; C128 KERNAL's hardware entry has already stacked registers. Write 1 to $d019 to
    ; acknowledge the raster IRQ, play one frame, then jump to $ff33 for restoration
    ; instead of using RTS.
    ACK_VIC_RASTER_IRQ
    jsr play_music_frame
    jmp KERNAL_IRQ_RETURN

; Each frame handles envelopes and voice-3 drum duration, then advances eighth notes
; with modulo-512 phase.
play_music_frame:
    ; Input: music state and tempo increment in RAM; no register arguments.
    ; Output: may close the previous GATE, restore an arpeggio, or advance music_step.
    ; Carry from low-byte ADC enters high; when high reaches 2, subtract 2 and trigger a
    ; step. This nine-bit phase retains the original BPM and exactly represents
    ; 1.25/1.75 multipliers.
    lda music_voice3_delay
    beq @update_gate
    dec music_voice3_delay
    bne @update_gate
    jsr music_trigger_arp

@update_gate:
    lda music_gate_timer
    beq @update_tempo
    dec music_gate_timer
    bne @update_tempo
    lda #MUSIC_LEAD_WAVE
    sta SID_VOICE_1_CONTROL
    lda #MUSIC_BASS_WAVE
    sta SID_VOICE_2_CONTROL
    lda #MUSIC_ARP_WAVE
    sta SID_VOICE_3_CONTROL

@update_tempo:
    clc
    lda music_tempo_accumulator_low
    adc music_tempo_increment
    sta music_tempo_accumulator_low
    lda music_tempo_accumulator_high
    adc #0
    cmp #MUSIC_TEMPO_WRAP_HIGH
    bcc @store_tempo_high
    sbc #MUSIC_TEMPO_WRAP_HIGH
    sta music_tempo_accumulator_high

    inc music_step
    lda music_step
    and #MUSIC_PATTERN_MASK
    sta music_step
    jsr music_trigger_current_step
@done:
    rts

@store_tempo_high:
    sta music_tempo_accumulator_high
    rts

; Disable IRQs and the raster source, silence SID; keep the installed vector.
stop_music:
    ; Disable IRQ and all three GATEs; set master volume to zero. Leave SEI active; the
    ; next new_game calls initialize_music to reinstall the IRQ and execute CLI.
    sei
    lda #0
    sta VIC_IRQ_ENABLE
    sta SID_VOICE_1_CONTROL
    sta SID_VOICE_2_CONTROL
    sta SID_VOICE_3_CONTROL
    sta SID_VOLUME
    sta music_gate_timer
    sta music_voice3_delay
    ACK_VIC_RASTER_IRQ
    rts

; Each step triggers melody and bass together. Drums use voice 3 for only the first two
; Dispatch lead, bass, arpeggio, and drum events for music_step.
music_trigger_current_step:
    ; Input: music_step=0..127. Output: trigger voices 1/2 and select drums or arpeggio
    ; on voice 3. drum_pattern has eight steps, so AND #7 implements modulo 8.
    ldx music_step
    lda music_lead_pattern,x
    jsr music_set_voice_1

    ldx music_step
    lda music_bass_pattern,x
    jsr music_set_voice_2

    lda #9
    sta music_gate_timer

    lda music_step
    and #7
    tax
    lda music_drum_pattern,x
    beq @play_arp_now
    jsr music_trigger_drum
    lda #2
    sta music_voice3_delay
    rts

@play_arp_now:
    lda #0
    sta music_voice3_delay
    jmp music_trigger_arp

music_set_voice_1:
    ; A is the note index (0=rest). Close GATE before setting the frequency,
    ; then reopen it to retrigger the pulse-wave envelope.
    tax
    lda #MUSIC_LEAD_WAVE
    sta SID_VOICE_1_CONTROL
    cpx #0
    beq @done
    lda music_frequency_low,x
    sta SID_VOICE_1_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_1_FREQ_HIGH
    lda #MUSIC_LEAD_GATE
    sta SID_VOICE_1_CONTROL
@done:
    rts

music_set_voice_2:
    ; As for voice 1, but write SID voice 2 with triangle-wave control.
    tax
    lda #MUSIC_BASS_WAVE
    sta SID_VOICE_2_CONTROL
    cpx #0
    beq @done
    lda music_frequency_low,x
    sta SID_VOICE_2_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_2_FREQ_HIGH
    lda #MUSIC_BASS_GATE
    sta SID_VOICE_2_CONTROL
@done:
    rts

; Trigger voice 3 as a short sawtooth arpeggio note.
music_trigger_arp:
    ; Read the current step's arpeggio note; play_music_frame also calls this after the
    ; drum ends to restore harmony on the shared third voice.
    ldx music_step
    lda music_arp_pattern,x
    tax
    lda #MUSIC_ARP_WAVE
    sta SID_VOICE_3_CONTROL
    cpx #0
    beq @done
    lda #$02
    sta SID_VOICE_3_ATTACK
    lda #$52
    sta SID_VOICE_3_SUSTAIN
    lda music_frequency_low,x
    sta SID_VOICE_3_FREQ_LOW
    lda music_frequency_high,x
    sta SID_VOICE_3_FREQ_HIGH
    lda #MUSIC_ARP_GATE
    sta SID_VOICE_3_CONTROL
@done:
    rts

; A=1 selects a low noise drum; A=2 a brighter snare. Both use very short envelopes.
music_trigger_drum:
    ; Input: A=1 low drum, A=2 snare. X retains the type; both share noise
    ; waveform/short envelope, with different frequency high bytes controlling the
    ; noise's brightness.
    tax
    lda #MUSIC_NOISE_WAVE
    sta SID_VOICE_3_CONTROL
    lda #$04
    sta SID_VOICE_3_ATTACK
    lda #$00
    sta SID_VOICE_3_SUSTAIN
    cpx #1
    bne @snare
    lda #$00
    sta SID_VOICE_3_FREQ_LOW
    lda #$10
    sta SID_VOICE_3_FREQ_HIGH
    jmp @gate
@snare:
    lda #$00
    sta SID_VOICE_3_FREQ_LOW
    lda #$50
    sta SID_VOICE_3_FREQ_HIGH
@gate:
    lda #MUSIC_NOISE_GATE
    sta SID_VOICE_3_CONTROL
    rts

; Note indices 1..61 correspond to C1..C6. Compute frequencies for PAL's 985248 Hz
; clock; reserve index 0 as a rest so arrangement tables can insert silence directly.
music_frequency_low:
    .byte $00
    .byte $2d,$4e,$71,$96,$be,$e7,$14,$42,$74,$a9,$e0,$1b,$5a,$9c,$e2,$2d
    .byte $7b,$cf,$27,$85,$e8,$51,$c1,$37,$b4,$38,$c4,$59,$f7,$9d,$4e,$0a
    .byte $d0,$a2,$81,$6d,$67,$70,$89,$b2,$ed,$3b,$9c,$13,$a0,$45,$02,$da
    .byte $ce,$e0,$11,$64,$da,$76,$39,$26,$40,$89,$04,$b4,$9c

music_frequency_high:
    .byte $00
    .byte $02,$02,$02,$02,$02,$02,$03,$03,$03,$03,$03,$04,$04,$04,$04,$05
    .byte $05,$05,$06,$06,$06,$07,$07,$08,$08,$09,$09,$0a,$0a,$0b,$0c,$0d
    .byte $0d,$0e,$0f,$10,$11,$12,$13,$14,$15,$17,$18,$1a,$1b,$1d,$1f,$20
    .byte $22,$24,$27,$29,$2b,$2e,$31,$34,$37,$3a,$3e,$41,$45

; Each row is one bar of eight eighth notes; the full pattern has 16 bars and 128 steps.
; Decimal entries are frequency-table indices, not MIDI pitches or direct SID register
; values.
music_lead_pattern:
    .byte 44,44,52,51,49,44,47,49       ; 01: G4 G4 Eb5 D5 C5 G4 Bb4 C5
    .byte 52,49,45,49,52,49,47,45       ; 02: Eb5 C5 Ab4 C5 Eb5 C5 Bb4 Ab4
    .byte 44,47,52,56,54,52,47,44       ; 03: G4 Bb4 Eb5 G5 F5 Eb5 Bb4 G4
    .byte 51,54,56,59,56,54,51,48       ; 04: D5 F5 G5 Bb5 G5 F5 D5 B4
    .byte 49,44,49,51,52,56,54,52       ; 05: C5 G4 C5 D5 Eb5 G5 F5 Eb5
    .byte 49,52,57,56,52,49,47,49       ; 06: C5 Eb5 Ab5 G5 Eb5 C5 Bb4 C5
    .byte 45,49,54,57,56,54,49,45       ; 07: Ab4 C5 F5 Ab5 G5 F5 C5 Ab4
    .byte 48,51,56,60,58,56,51,48       ; 08: B4 D5 G5 B5 A5 G5 D5 B4
    .byte 56,52,49,44,47,49,51,52       ; 09: G5 Eb5 C5 G4 Bb4 C5 D5 Eb5
    .byte 54,51,47,42,47,51,54,56       ; 10: F5 D5 Bb4 F4 Bb4 D5 F5 G5
    .byte 57,56,52,49,45,49,52,56       ; 11: Ab5 G5 Eb5 C5 Ab4 C5 Eb5 G5
    .byte 56,54,51,48,51,54,56,51       ; 12: G5 F5 D5 B4 D5 F5 G5 D5
    .byte 49,54,57,61,57,54,52,49       ; 13: C5 F5 Ab5 C6 Ab5 F5 Eb5 C5
    .byte 49,52,57,61,59,57,52,49       ; 14: C5 Eb5 Ab5 C6 Bb5 Ab5 Eb5 C5
    .byte 48,51,56,60,58,56,54,51       ; 15: B4 D5 G5 B5 A5 G5 F5 D5
    .byte 49,44,52,51,49,44,47,49       ; 16: C5 G4 Eb5 D5 C5 G4 Bb4 C5

music_bass_pattern:
    .byte 13,20,25,20,13,20,23,20       ; 01: Cm
    .byte  9,16,21,16, 9,16,20,16       ; 02: Ab
    .byte 16,23,28,23,16,23,27,23       ; 03: Eb
    .byte  8,15,20,15, 8,15,18,15       ; 04: G
    .byte 13,20,25,20,13,20,23,20       ; 05: Cm
    .byte  9,16,21,16, 9,16,20,16       ; 06: Ab
    .byte 18,25,30,25,18,25,28,25       ; 07: Fm
    .byte  8,15,20,15, 8,15,18,15       ; 08: G
    .byte 13,20,25,20,13,20,23,20       ; 09: Cm
    .byte 11,18,23,18,11,18,21,18       ; 10: Bb
    .byte  9,16,21,16, 9,16,20,16       ; 11: Ab
    .byte  8,15,20,15, 8,15,18,15       ; 12: G
    .byte 18,25,30,25,18,25,28,25       ; 13: Fm
    .byte  9,16,21,16, 9,16,20,16       ; 14: Ab
    .byte  8,15,20,15, 8,15,18,15       ; 15: G
    .byte 13,20,25,28,20,28,25,20       ; 16: Cm, returning to the loop's beginning.

music_arp_pattern:
    .byte 37,40,44,40,37,40,44,40       ; 01: Cm
    .byte 33,37,40,37,33,37,40,37       ; 02: Ab
    .byte 40,44,47,44,40,44,47,44       ; 03: Eb
    .byte 32,36,39,36,32,36,39,36       ; 04: G
    .byte 37,40,44,40,37,40,44,40       ; 05: Cm
    .byte 33,37,40,37,33,37,40,37       ; 06: Ab
    .byte 30,33,37,33,30,33,37,33       ; 07: Fm
    .byte 32,36,39,36,32,36,39,36       ; 08: G
    .byte 37,40,44,40,37,40,44,40       ; 09: Cm
    .byte 35,39,42,39,35,39,42,39       ; 10: Bb
    .byte 33,37,40,37,33,37,40,37       ; 11: Ab
    .byte 32,36,39,36,32,36,39,36       ; 12: G
    .byte 30,33,37,33,30,33,37,33       ; 13: Fm
    .byte 33,37,40,37,33,37,40,37       ; 14: Ab
    .byte 32,36,39,36,32,36,39,36       ; 15: G
    .byte 37,40,44,40,37,40,44,40       ; 16: Cm

music_drum_pattern:
    .byte 1,0,2,0,1,0,2,0

; Check arrangement lengths at assembly time to prevent reads into the next table or
; sprite data.
.assert music_frequency_high - music_frequency_low = 62, error, "music low-frequency table must contain 62 bytes"
.assert music_lead_pattern - music_frequency_high = 62, error, "music high-frequency table must contain 62 bytes"
.assert music_bass_pattern - music_lead_pattern = 128, error, "music lead pattern must contain 128 steps"
.assert music_arp_pattern - music_bass_pattern = 128, error, "music bass pattern must contain 128 steps"
.assert music_drum_pattern - music_arp_pattern = 128, error, "music arp pattern must contain 128 steps"
.assert * - music_drum_pattern = 8, error, "music drum pattern must contain 8 steps"
.assert * <= $2e00, error, "music segment overlaps platform effects"
