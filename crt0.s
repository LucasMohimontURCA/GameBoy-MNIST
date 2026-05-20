;; Minimal Game Boy crt0 for SDCC sm83
;; Provides: ROM header at 0x100, vector table, startup code, simple int handler
;;
;; Memory map (DMG):
;;   0x0000-0x3FFF  ROM bank 0
;;   0x4000-0x7FFF  ROM bank 1+
;;   0x8000-0x9FFF  VRAM
;;   0xC000-0xDFFF  WRAM (8KB)
;;   0xFE00-0xFE9F  OAM
;;   0xFF00-0xFF7F  IO regs
;;   0xFF80-0xFFFE  HRAM (127B, stack here)
;;   0xFFFF         Interrupt enable
;;
.module crt0
.globl _main

;; ---- Reset / interrupt vectors ----
.area _HEADER (ABS)

.org 0x00
    ret
.org 0x08
    ret
.org 0x10
    ret
.org 0x18
    ret
.org 0x20
    ret
.org 0x28
    ret
.org 0x30
    ret
.org 0x38
    ret

;; VBlank interrupt — just jump to the real handler (vector slot is only 8 bytes)
.org 0x40
    jp _vbl_isr
.org 0x48
    reti
.org 0x50
    reti
.org 0x58
    reti
.org 0x60
    reti

;; The real VBlank handler — placed past the vector table.
.org 0x68
_vbl_isr:
    push af
    push hl
    ld a, #1
    ld (_vbl_flag), a
    ;; Increment 16-bit _frame_counter (little-endian in WRAM)
    ld hl, #_frame_counter
    inc (hl)
    jr nz, vbl_no_carry
    inc hl
    inc (hl)
vbl_no_carry:
    pop hl
    pop af
    reti

;; ---- Cartridge entry point ----
.org 0x100
    nop
    jp 0x150          ; jump to start

;; ---- Nintendo logo (required, must match exactly) ----
.org 0x104
    .byte 0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83
    .byte 0x00,0x0C,0x00,0x0D,0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E
    .byte 0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,0xBB,0xBB,0x67,0x63
    .byte 0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E

;; Title (15 bytes), padded with zeros
.org 0x134
    .ascii "GB-MNIST"
    .byte 0,0,0,0,0,0,0
;; CGB flag
.org 0x143
    .byte 0x00
;; New licensee code
.org 0x144
    .byte 0x00,0x00
;; SGB flag
.org 0x146
    .byte 0x00
;; Cartridge type: ROM only
.org 0x147
    .byte 0x00
;; ROM size: 32KB (no banking)
.org 0x148
    .byte 0x00
;; RAM size
.org 0x149
    .byte 0x00
;; Destination
.org 0x14A
    .byte 0x01
;; Old licensee code
.org 0x14B
    .byte 0x33
;; Mask ROM version
.org 0x14C
    .byte 0x00
;; Header checksum (filled by makebin -Z)
.org 0x14D
    .byte 0x00
;; Global checksum (filled by makebin -Z)
.org 0x14E
    .byte 0x00,0x00

;; ---- Startup code ----
.org 0x150
    di                          ; disable interrupts during setup

    ; Wait for VBlank, then turn off LCD before touching VRAM
1$:
    ldh a, (#0x44)              ; LY register
    cp #144
    jr c, 1$
    xor a
    ldh (#0x40), a              ; LCDC = 0 (LCD off)

    ; Zero out WRAM 0xC000-0xDDFF (skip top 512 bytes, reserved for stack).
    ld hl, #0xC000
    ld bc, #0x1E00              ; 0xDE00 - 0xC000 = 0x1E00 bytes
2$:
    xor a
    ld (hl+), a
    dec bc
    ld a, b
    or c
    jr nz, 2$

    ; Set stack at top of WRAM (0xDFFF). Gives ~512 bytes of stack.
    ld sp, #0xDFFF

    ; Set palettes: BGP = 0xE4 (3,2,1,0 -> dark to light)
    ld a, #0xE4
    ldh (#0x47), a              ; BGP
    ld a, #0xE4
    ldh (#0x48), a              ; OBP0
    ldh (#0x49), a              ; OBP1

    ; Initialize SDCC C globals — call the start of _GSINIT area.
    ; SDCC fills _GSINIT with initializers from all C files; _GSFINAL has a RET.
    call s__GSINIT

    ; Clear LY-compare etc.
    xor a
    ldh (#0x42), a              ; SCY
    ldh (#0x43), a              ; SCX
    ldh (#0x4A), a              ; WY
    ldh (#0x4B), a              ; WX

    ; Enable VBlank interrupt only
    ld a, #0x01
    ldh (#0xFF), a              ; IE
    xor a
    ldh (#0x0F), a              ; IF clear
    ei

    ; Jump to user main
    call _main
3$:
    halt
    nop
    jr 3$

;; ---- VBlank flag and frame counter in WRAM ----
.area _DATA
_vbl_flag::
    .ds 1
_frame_counter::
    .ds 2                       ; 16-bit, little-endian

;; ---- SDCC C runtime areas — declared here for ordering. SDCC links init
;; code from each .c file's _GSINIT area; _GSFINAL has the RET. ----
.area _HOME
.area _CODE
.area _INITIALIZER
.area _GSINIT
.area _GSFINAL
    ret

;; Initialized-data copy region
.area _INITIALIZED
