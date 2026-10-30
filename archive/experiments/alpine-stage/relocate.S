/* SPDX-License-Identifier: GPL-2.0-only
 * Derived from stock arch/arm/kernel/relocate_kernel.S.
 * Host candidate only: no module or launcher invokes this blob.
 * Entry is the physical identity-mapped address, IRQ/FIQ and caches already
 * quiesced. No stack access, calls, absolute literals, or return path.
 */
    .syntax unified
    .arch armv7-a
    .arm
    .section .text.gw_relocate,"ax",%progbits
    .balign 32
    .global gw_relocate_start
gw_relocate_start:
    /* Amazon's cpu_v7_reset does not clear SCTLR.M. Clear it here while
     * instruction VA=PA, before loading physical relocation addresses. */
    dsb sy
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #1
    bic r0, r0, #4
    bic r0, r0, #0x1000
    mcr p15, 0, r0, c1, c0, 0
    isb sy
    mov r0, #0
    mcr p15, 0, r0, c8, c7, 0
    mcr p15, 0, r0, c7, c5, 0
    mcr p15, 0, r0, c7, c5, 6
    dsb sy
    isb sy
    ldr r0, gw_relocate_list
    ldr r1, gw_relocate_entry
0:
    ldr r3, [r0], #4
    tst r3, #1
    beq 1f
    bic r4, r3, #1
    b 0b
1:
    tst r3, #2
    beq 2f
    bic r0, r3, #2
    b 0b
2:
    tst r3, #4
    bne 4f
    tst r3, #8
    beq 0b
    bic r3, r3, #8
    mov r6, #1024
3:
    ldr r5, [r3], #4
    str r5, [r4], #4
    subs r6, r6, #1
    bne 3b
    b 0b
4:
    dsb sy
    isb sy
    mov lr, r1
    mov r0, #0
    mvn r1, #0
    ldr r2, gw_relocate_dtb
    mov pc, lr
    .balign 4
    .global gw_relocate_list
gw_relocate_list:
    .word 0
    .global gw_relocate_entry
gw_relocate_entry:
    .word 0
    .global gw_relocate_dtb
gw_relocate_dtb:
    .word 0
    .global gw_relocate_end
gw_relocate_end:
