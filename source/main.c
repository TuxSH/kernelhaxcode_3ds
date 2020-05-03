#include <string.h>
#include "hooks.h"
#include "PXI.h"
#include "arm9_bin.h"
#include "arm11_bin.h"

#define MAKE_BRANCH(src,dst)    (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

// PXI regs at +0xA1000
#define LCD_REGS_BASE           (MAP_ADDR + 0xA0000)
#define CFG11_REGS_BASE         (MAP_ADDR + 0xA2000)

//#define CFG11_SOCINFO           REG16(CFG11_REGS_BASE + 0x0FFC)
#define CFG11_DSP_CNT           REG8(CFG11_REGS_BASE + 0x1230)

#define KERNEL_VERSION_MAJOR    REG8(0x1FF80003)
#define KERNEL_VERSION_MINOR    REG8(0x1FF80002)
#define KERNPA2VA(a)            ((a) + (KERNEL_VERSION_MINOR < 44 ? 0xD0000000 : 0xC0000000))
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT. Handles O3DS fw running on N3DS


TakeoverParameters g_takeoverParameters = {};

static u32 *const axiwramStart = (u32 *)MAP_ADDR;
static u32 *const axiwramEnd = (u32 *)(MAP_ADDR + 0x80000);

static inline void lcdDebug(bool topScreen, u32 r, u32 g, u32 b)
{
    u32 base = topScreen ? LCD_REGS_BASE + 0x200 : LCD_REGS_BASE + 0xA00;
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

void kernDoPrepareForFirmlaunchHook(void)
{
    // Only core0 executes this code

    // Copy our sections:
    memmove((void *)KERNPA2VA(0x22000000), arm11_bin, arm11_bin_size);
    memmove((void *)KERNPA2VA(0x22100000), arm9_bin, arm9_bin_size);
    memmove((void *)KERNPA2VA(0x22200000), &g_takeoverParameters, sizeof(g_takeoverParameters));

    // Send PXIMC (service id 0) command 0, telling p9 to stop doing stuff and to wait for command 0x44836
    // (this is what the pxi sysmodule normally does when terminating)
    PXISendWord(0);
    PXITriggerSync9IRQ();
    PXISendWord(0x10000);
    while (!PXIIsSendFIFOEmpty());

    // Wait for the reply, and ignore it.
    PXIReceiveWord();
    PXIReceiveWord();
    PXIReceiveWord();

    // Reset the PXI FIFOs like the pxi sysmodule does. This also drains potential P9 notifications, if any
    PXIReset();

    // CFG11_DSP_CNT must be zero when doing a firmlaunch
    CFG11_DSP_CNT = 0x00;

    // We hooked that:
    PXISendWord(0x44836);
}

Result takeoverMain(u64 firmTid, const char *payloadFileName, size_t payloadFileOffset)
{
    Result res = 0;

    g_takeoverParameters.firmTid = firmTid;
    g_takeoverParameters.kernelVersionMajor = KERNEL_VERSION_MAJOR;
    g_takeoverParameters.kernelVersionMinor = KERNEL_VERSION_MINOR;
    g_takeoverParameters.isN3ds = IS_N3DS;
    g_takeoverParameters.payloadFileOffset = payloadFileOffset;
    strncpy(g_takeoverParameters.payloadFileName, payloadFileName, 255);

    TRY(installFirmlaunchHook());
    TRY(modifySvcTable());
    lcdDebug(true, 0, 255, 0);

    return res;
}
