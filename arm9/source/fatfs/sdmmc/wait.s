.arm

.section    .text.wait, "ax", %progbits
.align      2
.global     wait
.type       wait, %function
wait:
    subs    r0, #2
    nop
    bgt     wait
    bx      lr
