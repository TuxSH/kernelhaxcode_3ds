.arm
.cpu        mpcore

.section    .text.start, "ax", %progbits
.align      3
.global     _start
.type       _start, %function
.func       _start
.cfi_sections   .debug_frame
.cfi_startproc
_start:
    push    {r4, pc}

    bl      takeoverMain
    cmp     r0, #0
    popne   {r4, lr}

    // DSB, then Flush Prefetch Buffer (equivalent of ISB in later arch. versions). r0 = 0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4

    // Check if Luma kext is there using GetSystemInfo. Always return 0 on vanilla
    ldr     r1, =0x20000
    mov     r2, #0
    svc     0x2A
    movs    r8, r0

    // Invalidate entire instruction cache using UnmapProcessMemory (also clean-invalidates dcache & other barriers...)
    mov     r0, #0
    mov     r1, #0
    mov     r2, #0
    svceq   0x30
    svcne   0x72

    // Firmlaunch
    mov     r0, #0
    ldr     r2, =g_takeoverParameters
    ldrd    r2, [r2]
    svceq   0x31
    svcne   0x7C

    b       .
.cfi_endproc
.endfunc
