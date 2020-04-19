.arm
.cpu        mpcore

.section    .text.start, "ax", %progbits
.align      2
.global     _start
.type       _start, %function
_start:
    push    {r4, pc}

    bl      exploitMain
    cmp     r0, #0
    popne   {r4, lr}

    // Repeat it multiple times in case an interrupt happens
    bl      evictCaches
    bl      evictCaches
    bl      evictCaches

    // Flush caches for all cores
    mov     r0, #0
    mov     r1, #0
    mov     r2, #0
    svc     0x72

    // Firmlaunch
    mov     r0, #0
    ldr     r2, =firmlaunchTid
    ldrd    r2, [r2]
    svc     0x7C

    b       .

.section    .text.evictCaches, "ax", %progbits
.align      12
.global     evictCaches
.type       evictCaches, %function
evictCaches:
    // Try to evict caches for the core
    .rept   0x1000 - 1
    orr     r0, #1
    .endr
    bx      lr
