#include <string.h>
#include "hooks.h"
#include "arm9_bin.h"
#include "arm11_bin.h"

#define MAKE_BRANCH(src,dst)    (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define KERNVA2PA(a)            ((a) + (*(vu32 *)0x1FF80060 < SYSTEM_VERSION(2, 44, 6) ? 0xD0000000 : 0xC0000000))
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT

u64 firmTid = 0;
static u32 versionInfo = 0;

static inline void *fixAddr(u32 addr)
{
    return (u8 *)0x80000000 + (addr - 0x1FF80000);
}

static void installKernelSvcHook(void)
{
    u32 *excepPage = fixAddr(0x1FFF4000); // never changed; VA 0xFFFF0000
    u32 svcOffset = (-((excepPage[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; // Branch offset + 8 for prefetch
    u32 pointedInstructionVa = 0xFFFF0008 - svcOffset;
    u32 baseKernelVa = pointedInstructionVa & ~0xFFFFF;
    u32 pointedInstructionPa = pointedInstructionVa - baseKernelVa + 0x1FF80000;
    u32 *pointedInstructionMirrorVa = fixAddr(pointedInstructionPa);

    // VA FFF00000 -> PA 0x1FF80000 never changed that much either except on super old versions
    originalKernelSvcHandler = pointedInstructionMirrorVa[2];
    pointedInstructionMirrorVa[2]= (u32)kernelSvcHandlerHook;
}

static Result installFirmlaunchHook(void)
{
    // Find 0x44836, then go back to a known branch, then get the ldr r1, [r5]
    u32 *hook1Loc;
    for (hook1Loc = (u32 *)fixAddr(0x1FF80000); hook1Loc < (u32 *)fixAddr(0x1FF80000 + 0x80000) && (hook1Loc[0] != 0x44836 || hook1Loc[1] != 0x964536); hook1Loc++);
    if (hook1Loc >= (u32 *)fixAddr(0x1FF80000 + 0x80000)) {
        return 0xDEAD2002;
    }
    for (; *hook1Loc != 0xE3A00080; hook1Loc--);
    for (; (*hook1Loc & 0xFFF0) != 0x1010; hook1Loc--);

    kernelFirmlaunchHook1PxiRegsOffset = (hook1Loc[0] & 0xFF) - 8;
    hook1Loc[0] = 0xE59FC000; // ldr r12, =kernelFirmlaunchHook1
    hook1Loc[1] = 0xE12FFF3C; // blx r12
    hook1Loc[2] = (u32)kernelFirmlaunchHook1;

    // Find .fini:1FFF49A8 14 FF 2F E1                 BX              R4      ; __core0_stub
    // This should be the first result
    u32 *hook2Loc;
    for (hook2Loc = (u32 *)fixAddr(0x1FFF4000); hook2Loc < (u32 *)fixAddr(0x1FFF5000) && *hook2Loc != 0xE12FFF14; hook2Loc++);
    if (hook2Loc >= (u32 *)fixAddr(0x1FFF5000)) {
        return 0xDEAD2003;
    }

    u8 *branchDst = (u8 *)fixAddr(0x1FFF4F00); // should be OK

    memcpy(branchDst, kernelFirmlaunchHook2, kernelFirmlaunchHook2Size);

    *hook2Loc = MAKE_BRANCH(hook2Loc, branchDst);
    return 0;
}

void kernCopySections(void)
{
    memmove((void *)KERNVA2PA(0x22000000), arm11_bin, arm11_bin_size);
    memmove((void *)KERNVA2PA(0x22100000), arm9_bin, arm9_bin_size);

    *(vu64 *)KERNVA2PA(0x22200000) = firmTid;
    *(vu32 *)KERNVA2PA(0x22200008) = versionInfo;
}

Result exploitMain(u64 tid)
{
    firmTid = tid;
    versionInfo = (IS_N3DS ? 0x10000 : 0) | 0;
    Result ret = installFirmlaunchHook();
    if (ret == 0) {
        installKernelSvcHook();
    }
    return ret;
}
