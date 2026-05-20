;; fc_asm.s — Optimized FC inner product for SDCC sm83 (Game Boy CPU).

.module fc_asm

.area _DATA
_fc_w_ptr::            .ds 2
_fc_f_ptr::            .ds 2
_fc_acc_val::          .ds 4
_fc_count_val::        .ds 1
fc_prod_lo:        .ds 1
fc_prod_mid:       .ds 1
fc_prod_hi:        .ds 1
fc_neg:            .ds 1

.area _CODE
.globl _fc_run
_fc_run::
    push bc
    push de
    push hl

main_loop:
    ld   a, (_fc_count_val)
    or   a
    jp   z, fc_done
    dec  a
    ld   (_fc_count_val), a

    ld   a, (_fc_w_ptr+0)
    ld   l, a
    ld   a, (_fc_w_ptr+1)
    ld   h, a
    ld   b, (hl)
    inc  hl
    ld   a, l
    ld   (_fc_w_ptr+0), a
    ld   a, h
    ld   (_fc_w_ptr+1), a
    xor  a
    bit  7, b
    jr   z, w_positive
    ld   a, b
    cpl
    inc  a
    ld   b, a
    ld   a, #0xFF
w_positive:
    ld   (fc_neg), a

    ld   a, (_fc_f_ptr+0)
    ld   l, a
    ld   a, (_fc_f_ptr+1)
    ld   h, a
    ld   e, (hl)
    inc  hl
    ld   d, (hl)
    inc  hl
    ld   a, l
    ld   (_fc_f_ptr+0), a
    ld   a, h
    ld   (_fc_f_ptr+1), a

    xor  a
    ld   l, a
    ld   h, a
    ld   c, #8
mul_bit:
    sla  a
    rl   l
    rl   h
    sla  b
    jr   nc, mul_no_add
    add  a, e
    push af
    ld   a, l
    adc  a, d
    ld   l, a
    ld   a, h
    adc  a, #0
    ld   h, a
    pop  af
mul_no_add:
    dec  c
    jr   nz, mul_bit

    ld   (fc_prod_lo), a
    ld   a, l
    ld   (fc_prod_mid), a
    ld   a, h
    ld   (fc_prod_hi), a

    ld   a, (fc_neg)
    or   a
    jr   nz, do_subtract

    ld   hl, #_fc_acc_val
    ld   a, (fc_prod_lo)
    add  a, (hl)
    ld   (hl), a
    inc  hl
    ld   a, (fc_prod_mid)
    adc  a, (hl)
    ld   (hl), a
    inc  hl
    ld   a, (fc_prod_hi)
    adc  a, (hl)
    ld   (hl), a
    inc  hl
    ld   a, #0
    adc  a, (hl)
    ld   (hl), a
    jp   main_loop

do_subtract:
    ld   hl, #_fc_acc_val
    ld   a, (hl)
    ld   d, a
    ld   a, (fc_prod_lo)
    ld   e, a
    ld   a, d
    sub  a, e
    ld   (hl), a
    inc  hl
    ld   a, (hl)
    ld   d, a
    ld   a, (fc_prod_mid)
    ld   e, a
    ld   a, d
    sbc  a, e
    ld   (hl), a
    inc  hl
    ld   a, (hl)
    ld   d, a
    ld   a, (fc_prod_hi)
    ld   e, a
    ld   a, d
    sbc  a, e
    ld   (hl), a
    inc  hl
    ld   a, (hl)
    sbc  a, #0
    ld   (hl), a
    jp   main_loop

fc_done:
    pop  hl
    pop  de
    pop  bc
    ret
