/**
 * DolHook Entry Stub (PowerPC Assembly)
 * 
 * This becomes the new DOL entrypoint. Responsibilities:
 * 1. Save volatile registers we'll clobber
 * 2. Call dh_init() to install hooks
 * 3. Tail-jump to original game entry (no stack frame)
 */

    .section .text.entry,"ax",@progbits
    .global __dolhook_entry
    .type __dolhook_entry, @function

__dolhook_entry:
    /* Save LR and volatile regs we'll use */
    mflr    r0
    stwu    r1, -0x20(r1)       /* Create stack frame */
    stw     r0, 0x24(r1)        /* Save LR */
    stw     r3, 0x08(r1)        /* Save r3-r10 (volatile, might be args) */
    stw     r4, 0x0C(r1)
    stw     r5, 0x10(r1)
    stw     r6, 0x14(r1)
    stw     r7, 0x18(r1)
    stw     r8, 0x1C(r1)

    /* Call dh_init() - initializes hooks and prints banner */
    bl      dh_init

    /* Restore saved registers */
    lwz     r8, 0x1C(r1)
    lwz     r7, 0x18(r1)
    lwz     r6, 0x14(r1)
    lwz     r5, 0x10(r1)
    lwz     r4, 0x0C(r1)
    lwz     r3, 0x08(r1)
    lwz     r0, 0x24(r1)
    mtlr    r0
    addi    r1, r1, 0x20        /* Destroy stack frame */

    /* Load original entry address and tail-jump to it */
    lis     r12, __dolhook_original_entry@ha
    lwz     r12, __dolhook_original_entry@l(r12)
    mtctr   r12
    bctr                         /* Jump to original entry (no link) */

    .size __dolhook_entry, . - __dolhook_entry

/* ========================================================================== */

/**
 * Storage for original game entry address.
 * Filled by patcher before injection.
 */
    .section .data.entry,"aw",@progbits
    .global __dolhook_original_entry
    .align 2

__dolhook_original_entry:
    .long   0x80003100          /* Placeholder, overwritten by patcher */

    .size __dolhook_original_entry, 4