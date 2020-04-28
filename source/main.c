#include <string.h>
#include "hooks.h"
#include "arm9_bin.h"
#include "arm11_bin.h"

#define MAP_ADDR                0x40000000

#define MAKE_BRANCH(src,dst)    (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define KERNVA2PA(a)            ((a) + (*(vu32 *)0x1FF80060 < SYSTEM_VERSION(2, 44, 6) ? 0xD0000000 : 0xC0000000))
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT

u64 firmTid = 0;
static u32 versionInfo = 0;
static u32 *const axiwramStart = (u32 *)MAP_ADDR;
static u32 *const axiwramEnd = (u32 *)(MAP_ADDR + 0x80000);

static inline void lcdDebug(bool topScreen, u32 r, u32 g, u32 b)
{
    u32 base = topScreen ? MAP_ADDR + 0xA0200 : MAP_ADDR + 0xA0A00;
    *(vu32 *)(base + 4) = BIT(24) | b << 16 | g << 8 | r;
}

static inline void *fixAddr(u32 addr)
{
    return (u8 *)MAP_ADDR + (addr - 0x1FF80000);
}

static Result modifySvcTable(void)
{
    // Locate svc handler first: 00 6F 4D E9 STMFD           SP, {R8-R11,SP,LR}^ (2nd instruction)
    u32 *svcTableMirrorVa;
    for (svcTableMirrorVa = axiwramStart; svcTableMirrorVa < axiwramEnd && *svcTableMirrorVa != 0xE94D6F00; svcTableMirrorVa++);

    // Locate the table
    while (*svcTableMirrorVa != 0) svcTableMirrorVa++;

    if (svcTableMirrorVa >= axiwramEnd) {
        return 0xDEAD2101;
    }

    // Everything has access to SendSyncRequest1/2/3/4 (id 0x2E to 0x31). Replace these entries with UnmapProcessMemory and KernelSetState
    svcTableMirrorVa[0x30] = svcTableMirrorVa[0x72];
    svcTableMirrorVa[0x31] = svcTableMirrorVa[0x7C];

    return 0;
}

static Result installFirmlaunchHook(void)
{
    // Find 0x44836, then go back to a known branch, then get the ldr r1, [r5]
    u32 *hook1Loc;
    for (hook1Loc = axiwramStart; hook1Loc < axiwramEnd && (hook1Loc[0] != 0x44836 || hook1Loc[1] != 0x964536); hook1Loc++);
    if (hook1Loc >= axiwramEnd) {
        return 0xDEAD2001;
    }
    for (; *hook1Loc != 0xE3A00080; hook1Loc--);
    for (; (*hook1Loc & 0xFFF0) != 0x1010; hook1Loc--);

    kernelFirmlaunchHook1PxiRegsOffset = (hook1Loc[0] & 0xFF) - 8;
    hook1Loc[0] = 0xE59FC000; // ldr r12, =kernelFirmlaunchHook1
    hook1Loc[1] = 0xE12FFF3C; // blx r12
    hook1Loc[2] = (u32)kernelFirmlaunchHook1;

    // Find 14 FF 2F E1                 BX              R4      ; __core0_stub
    // This should be the first result
    u32 *hook2Loc;
    for (hook2Loc = axiwramStart; hook2Loc < axiwramEnd && *hook2Loc != 0xE12FFF14; hook2Loc++);
    if (hook2Loc >= axiwramEnd) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2002;
    }

    u8 *branchDst = (u8 *)fixAddr(0x1FFF4F00); // should be OK, let's check
    if (*(u32 *)fixAddr(0x1FFF4F00) != 0xFFFFFFFF) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2003;
    }

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
        ret = modifySvcTable();
        if (ret == 0) {
            lcdDebug(true, 0, 255, 0);
        }
    }

    return ret;
}
