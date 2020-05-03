.arm
.cpu        mpcore

.section    .data.hooks, "aw", %progbits

.align      3
.type       kernelFirmlaunchHook1, %function
.global     kernelFirmlaunchHook1
kernelFirmlaunchHook1:
    add     lr, lr, #4
    push    {r0-r12, lr}
    bl      kernDoPrepareForFirmlaunchHook
    pop     {r0-r12, pc}
.pool

.type       kernelFirmlaunchHook2, %function
.global     kernelFirmlaunchHook2

kernelFirmlaunchHook2:
    // Copy hook to 0x1FFFFC00 which is the normal location, to avoid getting overwritten
    ldr     r4, =0x1FFFFC00
    mov     r0, r4
    adr     r1, _kernelFirmlaunchHook2Start
    adr     r2, kernelFirmlaunchHook2End
    sub     r2, r1
    bl      _memcpy32
    bx      r4
.pool

_kernelFirmlaunchHook2Start:
    ldr     r2, =0x1FFFFFFC
    mov     r1, #0
    str     r1, [r2]

    ldr     r0, =0x10163008 // PXI_SEND
    ldr     r1, =0x44846
    str     r1, [r0]        // Tell P9 we're ready

    // Wait for P9 to finish its job & chainload
    _waitForEpLoop:
        ldr     r0, [r2]
        cmp     r0, #0
        beq     _waitForEpLoop

    // Jump
    ldr     pc, =0x22000000

_memcpy32:
    add     r2, r0, r2
    _memcpy32_loop:
        ldr     r3, [r1], #4
        str     r3, [r0], #4
        cmp     r0, r2
        blo     _memcpy32_loop
    bx      lr
.pool
kernelFirmlaunchHook2End:

.global     kernelFirmlaunchHook2Size
kernelFirmlaunchHook2Size:
    .word kernelFirmlaunchHook2End - kernelFirmlaunchHook2
