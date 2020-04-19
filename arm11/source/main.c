#include <string.h>
#include <stdarg.h>
#include "arm9.h"
#include "screen.h"
#include "draw.h"
#include "PXI.h"
#include "i2c.h"

extern vu32 versionInfo;

extern const u32 prepareForFirmlaunchStub[];
extern u32 prepareForFirmlaunchStubSize;

static u32 posY = 10;

#define HID_PAD                 (*(vu32 *)0x10146000 ^ 0xFFF)
#define CFG11_DSP_CNT           (vu8 *)0x10141230
#define FIRM_MAGIC              0x4D524946 // 'FIRM' in little-endian

#define PRINT_FUNC(name, color, hang)\
static void name(const char *fmt, ...)\
{\
    va_list args;\
    va_start(args, fmt);\
    posY = drawFormattedStringV(true, 10, posY, color, fmt, args);\
    va_end(args);\
    while (hang) { \
        while (HID_PAD == 0); \
        I2C_writeReg(I2C_DEV_MCU, 0x20, 1); /* shutdown */ \
    };\
}

PRINT_FUNC(print, COLOR_WHITE, false)
PRINT_FUNC(title, COLOR_TITLE, false)
PRINT_FUNC(success, COLOR_GREEN, false)
PRINT_FUNC(error, COLOR_RED, true)

static void doFirmlaunchHax(u64 firmlauchTid)
{
    static vu32 *const firmMagic = (vu32 *)0x24000000;
    static vu32 *const arm9Entrypoint = (vu32 *)0x2400000C; // old firmwares only

    Result res = p9McShutdown();
    if (res & 0x80000000) {
        error("Shutdown returned error %08lx!\n", res);
    }

    *CFG11_DSP_CNT = 0x00; // CFG11_DSP_CNT must be null when doing a firmlaunch
    res = firmlaunch(firmlauchTid);
    if (res & 0x80000000) {
        error("Firmlaunch returned error %08lx!\n", res);
    }

    while(*firmMagic != FIRM_MAGIC); // Wait for the firm header to be written

    for (u32 i = 0; i < 0x10000; i++) {
        *arm9Entrypoint = 0x22100000;
    }

    while (PXIReceiveWord() != 0x0000CAFE);

    success("Got Arm9 arbitrary code execution!\n");
}

void arm11main(u32 entrypoint)
{
    // Fill the screens with black while Luma (if present) may be loading sysmodules into VRAM.
    // prepareScreens will unset the regs.
    LCD_TOP_FILL_REG = LCD_FILL(0, 0, 0);
    LCD_BOTTOM_FILL_REG = LCD_FILL(0, 0, 0);

    u64 firmlaunchTid = *(vu64 *)0x22200000; // set by pre-firmlaunch payload

    vu32 *entrypointAddr = (vu32 *)0x1FFFFFFC;
    vu32 *lumaOperation = (vu32 *)0x1FF80004;

    bool isLuma = entrypoint == 0x1FF80000;
    *entrypointAddr = 0;

    if (isLuma) {
        // Handle Luma doing Luma things & wait for patching to finish -- note: can break in the future
        *lumaOperation = 7; // "ARM11_READY"
        while (*lumaOperation == 7);
        *lumaOperation = 7;

        while (*entrypointAddr == 0);
        entrypoint = *entrypointAddr;
    }

    // I2C_init(); <-- this fucks up
    prepareScreens();

    title("Post-firmlaunch Arm11 stub\n\n");

    if (isLuma) {
        print("Luma detected and bypassed.\n");
    }

    print("Arm11 entrypoint is %08lx.\n", entrypoint);

    k9Sync();
    print("Synchronization with Kernel9 done.\n");

    p9InitComms();
    print("Synchronization with Process9 done.\n");

    switch (firmlaunchTid & 0xFFFF) {
        case 0x0003:
            print("Doing firmlaunchhax (SAFE_FIRM).\n");
            doFirmlaunchHax(firmlaunchTid);
            break;
        case 0x0002:
            print("Doing firmlaunchhax (NATIVE_FIRM).\n");
            doFirmlaunchHax(firmlaunchTid);
            break;
        default:
            error("FIRM TID not supported!\n");
            break;
    }

    Result res = PXIReceiveWord();
    if ((res & 0x80000000) == 0) {
        success("SafeB9SInstaller.bin read successfully!\n");
    } else {
        switch ((res >> 8) & 0xFF) {
            case 0:
                error("Failed to mount the SD card! (%u)\n", res & 0xFF);
                break;
            case 1:
                error("Failed to open SafeB9SInstaller.bin! (%d)\n", res & 0xFF);
                break;
            case 2:
                error("SafeB9SInstaller.bin is too big!\n");
                break;
            case 3:
                error("Failed to read SafeB9SInstaller.bin! (%d)\n", res & 0xFF);
                break;
            default:
                error("Unknown filesystem error!\n");
                break;
        }
    }

    print("Preparing to chainload...\n");

    // Set framebuffer structure for Brahma payloads & reset framebuffer status.
    memcpy((void *)0x23FFFE00, defaultFbs, 2 * sizeof(struct fb));
    resetScreens();

    // Copy the firmlaunch stub.
    memcpy((void *)0x1FFFFC00, prepareForFirmlaunchStub, prepareForFirmlaunchStubSize);

    // We're ready.
    PXISendWord(0xC0DE);
    while (!PXIIsSendFIFOEmpty());

    ((void (*)(void))0x1FFFFC00)();
}
