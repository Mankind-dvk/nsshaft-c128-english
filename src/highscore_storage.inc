; Saves the leaderboard to NSSHAFT.A/B using C128 KERNAL file calls.
; Writes go to the other slot; the old slot stays valid until read-back succeeds.
; Loading checks both files and uses the newer valid copy.
;
; Quick reference:
;   line 51: startup workspace snapshot
;   line 75: A/B loading and generation selection
;   line 156: alternating save and read-back verification
;   line 226: KERNAL/game environment switching
;   line 331: channels and DOS status
;   line 549: file packing
;   line 615: header and record checks
;   line 717: CRC-16 calculation
;
; Bootstrap runs once before the game uses zero page. Load/save run with music
; and CIA IRQs stopped. A/X/Y may change; C=0 means success, C=1 means failure.
; If neither slot is valid, keep the RAM table. Missing files are normal on first run.
;
; File layout: 80-byte SEQ, without a PRG load address.
; 0..3: NSHS signature; 4: format version (1); 5: valid record count.
; 6..7: generation, low byte first; 8..77: five name/score records.
; 78..79: CRC, low byte first. The CRC covers bytes 0..77.

.segment "HIGHSCORES"

; === 1. File-format constants and KERNAL entry addresses ===
HS_DISK_BYTES = 80
HS_DISK_CRC_OFFSET = 78
HS_DISK_RECORD_OFFSET = 8
; 14/15 are program logical file numbers, not disk-drive device numbers.
HS_DISK_FILE = 14
HS_DISK_COMMAND = 15
HS_DISK_ZP_FIRST = $02
HS_DISK_ZP_BYTES = $fe
HS_K_SETBNK = $ff68
HS_K_SETMSG = $ff90
HS_K_READST = $ffb7
HS_K_SETLFS = $ffba
HS_K_SETNAM = $ffbd
HS_K_OPEN = $ffc0
HS_K_CLOSE = $ffc3
HS_K_CHKIN = $ffc6
HS_K_CHKOUT = $ffc9
HS_K_CLRCHN = $ffcc
HS_K_CHRIN = $ffcf
HS_K_CHROUT = $ffd2

; === 2. Startup snapshot: preserve the system zero-page workspace ===
; Save the system workspace before the game uses zero page. This segment must be
; mapped in first. Use the loading drive from $ba when it is 8..30; otherwise use 8.
highscore_storage_bootstrap:
    ldx #0
@copy:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_kernal_zp,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @copy
    lda $00
    sta hs_disk_boot_cpu_ddr
    lda $01
    sta hs_disk_boot_cpu_port
    lda $ba
    cmp #8
    bcc @default_device
    cmp #31
    bcs @default_device
    sta highscore_storage_device
@default_device:
    rts

; === 3. Load/save workflow ===
; Try both slots and keep the newer valid copy. A bad or missing alternate is harmless
; if the other slot is valid. Leave the RAM table alone if neither passes validation.
highscore_storage_load:
    php
    sei
    cld
    jsr hs_disk_enter
    lda #$ff
    sta hs_disk_active_slot
    lda #0
    sta hs_disk_generation
    sta hs_disk_generation+1
    sta hs_disk_load_error
    sta hs_disk_slot
    jsr hs_disk_open_command
    bcs @open_failed
@slot:
    jsr hs_disk_read_slot
    bcs @failed_slot
    lda hs_disk_active_slot
    cmp #$ff
    beq @accept
    ; Subtract generations so $0000 follows $ffff. The difference must stay below
    ; $8000; ordinary A/B saves leave the two slots only one generation apart.
    lda hs_disk_buffer+6
    sec
    sbc hs_disk_generation
    sta hs_disk_difference
    lda hs_disk_buffer+7
    sbc hs_disk_generation+1
    bmi @next
    ora hs_disk_difference
    beq @next
@accept:
    lda hs_disk_slot
    sta hs_disk_active_slot
    lda hs_disk_buffer+6
    sta hs_disk_generation
    lda hs_disk_buffer+7
    sta hs_disk_generation+1
    lda hs_disk_buffer+5
    sta highscore_count
    ldx #69
@copy:
    lda hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    sta highscore_entries,x
    dex
    bpl @copy
    jmp @next
@failed_slot:
    ; DOS 62 is expected on the first run. Other failures still need an error message.
    lda hs_disk_error
    cmp #62
    beq @next
    lda #1
    sta hs_disk_load_error
@next:
    inc hs_disk_slot
    lda hs_disk_slot
    cmp #2
    bcc @slot
    jsr hs_disk_close_command
    lda hs_disk_active_slot
    cmp #$ff
    beq @no_valid_slot
    lda #0
    sta highscore_storage_status
    jmp hs_disk_success
@no_valid_slot:
    lda hs_disk_load_error
    bne @open_failed
    lda #1
    sta highscore_storage_status
    jmp hs_disk_success
@open_failed:
    jsr hs_disk_close_command
    lda #2
    sta highscore_storage_status
    jmp hs_disk_failure

; Write to the inactive slot, starting with A on the first save. Keep the last valid
; slot until the new file has been closed and read back successfully.
; The caller clears highscore_dirty on success; a failed save leaves it set.
highscore_storage_save:
    php
    sei
    cld
    jsr hs_disk_enter
    lda hs_disk_active_slot
    cmp #$ff
    bne @alternate
    lda #1
@alternate:
    eor #1
    sta hs_disk_slot
    jsr hs_disk_open_command
    bcs @failed
    jsr hs_disk_scratch_slot
    bcs @failed
    jsr hs_disk_build_buffer
    jsr hs_disk_write_slot
    bcs @failed
    jsr hs_disk_read_slot
    bcs @failed
    ; Make sure this is the file just written. An older file can have a valid CRC too.
    lda hs_disk_expected_generation
    cmp hs_disk_buffer+6
    bne @failed
    lda hs_disk_expected_generation+1
    cmp hs_disk_buffer+7
    bne @failed
    lda highscore_count
    cmp hs_disk_buffer+5
    bne @failed
    ldx #69
@verify:
    lda highscore_entries,x
    cmp hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    bne @failed
    dex
    bpl @verify
    lda hs_disk_slot
    sta hs_disk_active_slot
    lda hs_disk_buffer+6
    sta hs_disk_generation
    lda hs_disk_buffer+7
    sta hs_disk_generation+1
    jsr hs_disk_close_command
    lda #0
    sta highscore_storage_status
    jmp hs_disk_success
@failed:
    jsr hs_disk_close_data
    jsr hs_disk_close_command
    lda #3
    sta highscore_storage_status
    jmp hs_disk_failure

; Both exits restore the game environment before returning the result in C.
hs_disk_success:
    jsr hs_disk_leave
    plp
    clc
    rts
hs_disk_failure:
    jsr hs_disk_leave
    plp
    sec
    rts

; === 4. C128 environment preservation ===
; Swap in KERNAL's zero page and use 1 MHz for disk calls. Save the game mapping and
; CIA settings first; its pointers and the system's file variables share zero page.
hs_disk_enter:
    lda MMU_CONFIG
    sta hs_disk_saved_mmu
    lda #MMU_CONFIG_BANK0_IO
    sta MMU_CONFIG
    lda $00
    sta hs_disk_saved_cpu_ddr
    lda $01
    sta hs_disk_saved_cpu_port
    lda hs_disk_boot_cpu_ddr
    sta $00
    lda hs_disk_boot_cpu_port
    sta $01
    lda VIC_CPU_SPEED
    sta hs_disk_saved_speed
    and #$fe
    sta VIC_CPU_SPEED
    lda $d505
    sta hs_disk_saved_mcr
    lda $dc0e
    sta hs_disk_saved_cra
    lda $dc0f
    sta hs_disk_saved_crb
    lda $dd00
    sta hs_disk_saved_cia2_port
    lda $dd02
    sta hs_disk_saved_cia2_ddr
    ora #$3f
    sta $dd02
    ldx #7
@cia:
    lda $dc00,x
    sta hs_disk_saved_cia1,x
    dex
    bpl @cia
    ldx #0
@zp:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_game_zp,x
    lda hs_disk_kernal_zp,x
    sta HS_DISK_ZP_FIRST,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @zp
    ; SETBNK takes a RAM-bank number, not a raw MMU configuration byte.
    lda #0
    tax
    jsr HS_K_SETBNK
    lda #0
    jsr HS_K_SETMSG
    lda #0
    sta hs_disk_command_open
    sta hs_disk_data_open
    rts

; Keep KERNAL's updated workspace for next time, then restore the game workspace.
; Disk I/O runs between rounds, so timer phase continuity is not required.
hs_disk_leave:
    ; Music is stopped. Keep CIA IRQs disabled and clear pending interrupts.
    lda #$7f
    sta $dc0d
    sta $dd0d
    lda $dc0d
    lda $dd0d
    lda #0
    sta $dc0e
    sta $dc0f
    ldx #7
@cia:
    lda hs_disk_saved_cia1,x
    sta $dc00,x
    dex
    bpl @cia
    lda hs_disk_saved_cra
    sta $dc0e
    lda hs_disk_saved_crb
    sta $dc0f
    lda hs_disk_saved_cia2_port
    sta $dd00
    lda hs_disk_saved_cia2_ddr
    sta $dd02
    lda hs_disk_saved_mcr
    sta $d505
    lda hs_disk_saved_speed
    sta VIC_CPU_SPEED
    ldx #0
@zp:
    lda HS_DISK_ZP_FIRST,x
    sta hs_disk_kernal_zp,x
    lda hs_disk_game_zp,x
    sta HS_DISK_ZP_FIRST,x
    inx
    cpx #HS_DISK_ZP_BYTES
    bne @zp
    lda hs_disk_saved_cpu_port
    sta $01
    lda hs_disk_saved_cpu_ddr
    sta $00
    lda hs_disk_saved_mmu
    sta MMU_CONFIG
    rts

; === 5. KERNAL channels and drive status ===
; Secondary address 15 is for DOS commands/status, 2 for file data; neither is the
; physical device number.
hs_disk_open_command:
    lda #0
    jsr HS_K_SETNAM
    lda #HS_DISK_COMMAND
    ldx highscore_storage_device
    ldy #15
    jsr HS_K_SETLFS
    jsr HS_K_OPEN
    bcs @failed
    lda #1
    sta hs_disk_command_open
    ; Drain the old status so the next check belongs to the current operation.
    jsr hs_disk_dos_status
    lda hs_disk_error
    cmp #$ff
    beq @failed
    clc
    rts
@failed:
    sec
    rts

hs_disk_close_command:
    lda hs_disk_command_open
    beq @done
    jsr HS_K_CLRCHN
    lda #HS_DISK_COMMAND
    jsr HS_K_CLOSE
    lda #0
    sta hs_disk_command_open
@done:
    rts

hs_disk_close_data:
    lda hs_disk_data_open
    beq @done
    jsr HS_K_CLRCHN
    lda #HS_DISK_FILE
    jsr HS_K_CLOSE
    lda #0
    sta hs_disk_data_open
@done:
    rts

; Read the two-digit DOS status into hs_disk_error and drain the message through RETURN.
; Stop after 64 more characters if RETURN never arrives. $ff marks a bad response or
; IEC error. Status 00 succeeds; the scratch caller also accepts 01 (FILES SCRATCHED).
hs_disk_dos_status:
    lda #$ff
    sta hs_disk_error
    ldx #HS_DISK_COMMAND
    jsr HS_K_CHKIN
    bcs @bad
    jsr HS_K_CHRIN
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
    sec
    sbc #'0'
    asl
    sta hs_disk_status_tens
    asl
    asl
    clc
    adc hs_disk_status_tens
    sta hs_disk_status_tens
    jsr HS_K_CHRIN
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
    sec
    sbc #'0'
    clc
    adc hs_disk_status_tens
    sta hs_disk_status_code
    lda #64
    sta hs_disk_index
@drain:
    jsr HS_K_CHRIN
    cmp #13
    beq @received
    jsr HS_K_READST
    bne @bad
    dec hs_disk_index
    bne @drain
    beq @bad
@received:
    jsr HS_K_READST
    and #$bf
    bne @bad
    jsr HS_K_CLRCHN
    lda hs_disk_status_code
    sta hs_disk_error
    beq @ok
    sec
    rts
@bad:
    jsr HS_K_CLRCHN
    sec
    rts
@ok:
    clc
    rts

; === 6. File operations and buffer packing ===
; Update the A/B letter in both the filename and scratch command for the target slot.
hs_disk_set_slot_names:
    lda hs_disk_slot
    clc
    adc #'A'
    sta hs_disk_filename_slot
    sta hs_disk_scratch_slot_char
    rts

; S in the filename means SEQ; the caller sets the mode character to R or W.
hs_disk_open_data:
    jsr hs_disk_set_slot_names
    lda #hs_disk_filename_end-hs_disk_filename
    ldx #<hs_disk_filename
    ldy #>hs_disk_filename
    jsr HS_K_SETNAM
    lda #HS_DISK_FILE
    ldx highscore_storage_device
    ldy #2
    jsr HS_K_SETLFS
    jsr HS_K_OPEN
    bcs @failed
    lda #1
    sta hs_disk_data_open
    jmp hs_disk_dos_status
@failed:
    lda #$ff
    sta hs_disk_error
    sec
    rts

; Expect exactly 80 bytes, with EOI ($40) on the last byte. A short or longer file is
; invalid; other READST bits report a communication error.
hs_disk_read_slot:
    lda #'R'
    sta hs_disk_filename_mode
    jsr hs_disk_open_data
    bcs @failed
    ldx #HS_DISK_FILE
    jsr HS_K_CHKIN
    bcs @invalid
    lda #0
    sta hs_disk_index
@read:
    jsr HS_K_CHRIN
    ldx hs_disk_index
    sta hs_disk_buffer,x
    jsr HS_K_READST
    sta hs_disk_read_status
    and #$bf
    bne @invalid
    inc hs_disk_index
    lda hs_disk_index
    cmp #HS_DISK_BYTES
    beq @last
    lda hs_disk_read_status
    bne @invalid
    beq @read
@last:
    lda hs_disk_read_status
    cmp #$40
    bne @invalid
    jsr hs_disk_close_data
    jsr hs_disk_validate_buffer
    bcs @invalid
    clc
    rts
@invalid:
    lda #$fe
    sta hs_disk_error
@failed:
    jsr hs_disk_close_data
    sec
    rts

; Scratch only the inactive slot so the last valid save remains available.
; DOS 01 (FILES SCRATCHED) is a successful result here.
hs_disk_scratch_slot:
    jsr hs_disk_set_slot_names
    ldx #HS_DISK_COMMAND
    jsr HS_K_CHKOUT
    bcs @failed
    lda #0
    sta hs_disk_index
@write:
    ldx hs_disk_index
    lda hs_disk_scratch,x
    jsr HS_K_CHROUT
    jsr HS_K_READST
    bne @failed
    inc hs_disk_index
    lda hs_disk_index
    cmp #hs_disk_scratch_end-hs_disk_scratch
    bcc @write
    jsr HS_K_CLRCHN
    jsr hs_disk_dos_status
    bcc @done
    lda hs_disk_error
    cmp #1
    bne @failed
    clc
@done:
    rts
@failed:
    jsr HS_K_CLRCHN
    sec
    rts

; --- Save-file packing ---
; Build the next generation in the buffer; do not commit it until verification succeeds.
; Copy all five records, use count to mark the valid ones, and append the CRC.
hs_disk_build_buffer:
    ldx #3
@magic:
    lda hs_disk_magic,x
    sta hs_disk_buffer,x
    dex
    bpl @magic
    lda #1
    sta hs_disk_buffer+4
    lda highscore_count
    sta hs_disk_buffer+5
    lda hs_disk_generation
    clc
    adc #1
    sta hs_disk_buffer+6
    sta hs_disk_expected_generation
    lda hs_disk_generation+1
    adc #0
    sta hs_disk_buffer+7
    sta hs_disk_expected_generation+1
    ldx #69
@records:
    lda highscore_entries,x
    sta hs_disk_buffer+HS_DISK_RECORD_OFFSET,x
    dex
    bpl @records
    jsr hs_disk_crc16
    lda hs_disk_crc
    sta hs_disk_buffer+HS_DISK_CRC_OFFSET
    lda hs_disk_crc+1
    sta hs_disk_buffer+HS_DISK_CRC_OFFSET+1
    rts

; Check DOS status after closing, since CHROUT alone cannot confirm the file was saved.
; highscore_storage_save will reopen the file and verify its contents.
hs_disk_write_slot:
    lda #'W'
    sta hs_disk_filename_mode
    jsr hs_disk_open_data
    bcs @failed
    ldx #HS_DISK_FILE
    jsr HS_K_CHKOUT
    bcs @failed
    lda #0
    sta hs_disk_index
@write:
    ldx hs_disk_index
    lda hs_disk_buffer,x
    jsr HS_K_CHROUT
    jsr HS_K_READST
    bne @failed
    inc hs_disk_index
    lda hs_disk_index
    cmp #HS_DISK_BYTES
    bcc @write
    jsr hs_disk_close_data
    jmp hs_disk_dos_status
@failed:
    jsr hs_disk_close_data
    sec
    rts

; === 7. File validation and CRC ===
; Check the CRC and header before trusting count or any records. Names allow letters
; (screen codes 1..26), digits (48..57), and spaces (32), but cannot be all spaces.
; Score digits must be 0..9. Unused records may stay zero; valid scores must be sorted.
hs_disk_validate_buffer:
    jsr hs_disk_crc16
    lda hs_disk_crc
    cmp hs_disk_buffer+HS_DISK_CRC_OFFSET
    bne @bad
    lda hs_disk_crc+1
    cmp hs_disk_buffer+HS_DISK_CRC_OFFSET+1
    bne @bad
    ldx #3
@magic:
    lda hs_disk_buffer,x
    cmp hs_disk_magic,x
    bne @bad
    dex
    bpl @magic
    lda hs_disk_buffer+4
    cmp #1
    bne @bad
    lda hs_disk_buffer+5
    cmp #6
    bcs @bad
    sta hs_disk_rows_left
    beq @ok
    ldx #HS_DISK_RECORD_OFFSET
@record:
    lda #0
    sta hs_disk_nonspace
    ldy #8
@name:
    lda hs_disk_buffer,x
    cmp #32
    beq @next_char
    inc hs_disk_nonspace
    cmp #1
    bcc @bad
    cmp #27
    bcc @next_char
    cmp #'0'
    bcc @bad
    cmp #'9'+1
    bcs @bad
@next_char:
    inx
    dey
    bne @name
    lda hs_disk_nonspace
    beq @bad
    ldy #6
@digit:
    lda hs_disk_buffer,x
    cmp #10
    bcs @bad
    inx
    dey
    bne @digit
    dec hs_disk_rows_left
    bne @record
    jmp hs_disk_validate_order
@ok:
    clc
    rts
@bad:
    sec
    rts

; Ranking assumes descending scores. Reject an out-of-order pair; ties are allowed.
hs_disk_validate_order:
    lda hs_disk_buffer+5
    cmp #2
    bcc @ok
    sec
    sbc #1
    sta hs_disk_rows_left
    lda #HS_DISK_RECORD_OFFSET+8
    sta hs_disk_index
@pair:
    ldx hs_disk_index
    ldy #6
@digit:
    lda hs_disk_buffer,x
    cmp hs_disk_buffer+14,x
    bcc @bad
    bne @next_pair
    inx
    dey
    bne @digit
@next_pair:
    lda hs_disk_index
    clc
    adc #14
    sta hs_disk_index
    dec hs_disk_rows_left
    bne @pair
@ok:
    clc
    rts
@bad:
    sec
    rts

; CRC-16/CCITT-FALSE over bytes 0..77: initial $ffff, polynomial $1021, no final XOR.
; The two stored CRC bytes are excluded from the calculation.
hs_disk_crc16:
    lda #$ff
    sta hs_disk_crc
    sta hs_disk_crc+1
    ldx #0
@byte:
    lda hs_disk_buffer,x
    eor hs_disk_crc+1
    sta hs_disk_crc+1
    ldy #8
@bit:
    asl hs_disk_crc
    rol hs_disk_crc+1
    bcc @next_bit
    lda hs_disk_crc
    eor #$21
    sta hs_disk_crc
    lda hs_disk_crc+1
    eor #$10
    sta hs_disk_crc+1
@next_bit:
    dey
    bne @bit
    inx
    cpx #HS_DISK_CRC_OFFSET
    bcc @byte
    rts

; === 8. Filenames, status, and working buffers ===
; Status: 0=success, 1=no previous save, 2=load failure, 3=save failure.
highscore_storage_status: .byte 1
highscore_storage_device: .byte 8
hs_disk_magic: .byte "NSHS"
hs_disk_filename: .byte "0:NSSHAFT."
hs_disk_filename_slot: .byte "A"
    .byte ",S,"
hs_disk_filename_mode: .byte "R"
hs_disk_filename_end:
hs_disk_scratch: .byte "S0:NSSHAFT."
hs_disk_scratch_slot_char: .byte "A",13
hs_disk_scratch_end:
; Slots: 0=A, 1=B, $ff=no valid save. active_slot changes only after verification;
; slot is the current read/write target.
hs_disk_active_slot: .byte $ff
hs_disk_slot: .byte 0
hs_disk_generation: .word 0
hs_disk_expected_generation: .word 0
hs_disk_difference: .byte 0
hs_disk_load_error: .byte 0
hs_disk_error: .byte 0
hs_disk_command_open: .byte 0
hs_disk_data_open: .byte 0
hs_disk_status_tens: .byte 0
hs_disk_status_code: .byte 0
hs_disk_index: .byte 0
hs_disk_read_status: .byte 0
hs_disk_rows_left: .byte 0
hs_disk_nonspace: .byte 0
hs_disk_crc: .word 0
hs_disk_boot_cpu_ddr: .byte 0
hs_disk_boot_cpu_port: .byte 0
hs_disk_saved_mmu: .byte 0
hs_disk_saved_cpu_ddr: .byte 0
hs_disk_saved_cpu_port: .byte 0
hs_disk_saved_speed: .byte 0
hs_disk_saved_mcr: .byte 0
hs_disk_saved_cra: .byte 0
hs_disk_saved_crb: .byte 0
hs_disk_saved_cia2_port: .byte 0
hs_disk_saved_cia2_ddr: .byte 0
hs_disk_saved_cia1: .res 8,0
; Separate 254-byte workspaces for KERNAL and the game ($02..$ff).
; Save the CPU port registers $00/$01 separately.
hs_disk_kernal_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_game_zp: .res HS_DISK_ZP_BYTES,0
hs_disk_buffer: .res HS_DISK_BYTES,0
