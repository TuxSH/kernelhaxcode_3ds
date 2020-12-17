.arm
.cpu        mpcore

#include "asm_macros.s.h"

FUNCTION _start, .crt0
    // Disable interrupts and switch to supervisor mode
    cpsid   aif, #0x13

    // Set the control register to reset default: everything disabled
    ldr     r0, =0x54078
    mcr     p15, 0, r0, c1, c0, 0

    // Set the auxiliary control register to reset default.
    // Enables instruction folding, static branch prediction,
    // dynamic branch prediction, and return stack.
    mov     r0, #0xF
    mcr     p15, 0, r0, c1, c0, 1

    // Invalidate both caches, flush the prefetch buffer then DSB
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 4
    mcr     p15, 0, r0, c7, c7, 0
    mcr     p15, 0, r0, c7, c10, 4

    // Clear BSS
    ldr     r0, =__bss_start
    mov     r1, #0
    ldr     r2, =__bss_end
    sub     r2, r0
    bl      memset

    ldr     sp, =__stack_top__
    b       arm11main
END_FUNCTION

.global prepareForFirmlaunchStub
.type   prepareForFirmlaunchStub, %function
prepareForFirmlaunchStub:
    str     r0, [r1]            // tell ARM9 we're done
    mov     r0, #0x20000000

    _waitForCore0EntrypointLoop:
        ldr     r1, [r0, #-8]   // check if core0's entrypoint is 0; 0x1FFFFFF8 because of stupid Brahma payload standards
        cmp     r1, #0
        beq     _waitForCore0EntrypointLoop

    bx      r1                  // jump to core0's entrypoint
.hidden prepareForFirmlaunchStubEnd
prepareForFirmlaunchStubEnd:

.global prepareForFirmlaunchStubSize
prepareForFirmlaunchStubSize: .word prepareForFirmlaunchStubEnd - prepareForFirmlaunchStub
