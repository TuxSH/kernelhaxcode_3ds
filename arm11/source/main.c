#include <string.h>
#include <stdarg.h>
#include "arm9.h"
#include "screen.h"
#include "draw.h"
#include "PXI.h"
#include "i2c.h"

#define HID_PAD                 (*(vu32 *)0x10146000 ^ 0xFFF)
#define FIRM_MAGIC              0x4D524946 // 'FIRM' in little-endian

#define DS_INTERNET_TID         0x0004800542383841ULL
#define DS_INTERNET_TID_TWL     0x0003000542383841ULL
#define DS_INTERNET_CONTENT_ID  2

static u32 posY = 10;
#define PRINT_FUNC(name, color, hang)\
void name(const char *fmt, ...)\
{\
    va_list args;\
    va_start(args, fmt);\
    posY = drawFormattedStringV(true, 10, posY, color, fmt, args);\
    va_end(args);\
    while (hang) { \
        while (HID_PAD == 0); \
        I2C_writeReg(I2C_DEV_MCU, 0x20, 1); /* shutdown */ \
        for (;;);\
    };\
}

PRINT_FUNC(print, COLOR_WHITE, false)
PRINT_FUNC(title, COLOR_TITLE, false)
PRINT_FUNC(success, COLOR_GREEN, false)
PRINT_FUNC(error, COLOR_RED, true)

extern const u32 prepareForFirmlaunchStub[];
extern u32 prepareForFirmlaunchStubSize;

TakeoverParameters g_takeoverParameters = {};

static inline const char *getFirmName(u64 tid)
{
    switch (tid & 0x0FFFFFFF) {
        case 2: return "NATIVE_FIRM";
        case 3: return "SAFE_FIRM";
        default: return "Arm9 exploit";
    }
}

static void launchFirm(u64 firmTid)
{
    Result res = p9McShutdown();
    if (res & 0x80000000) {
        error("Shutdown returned error 0x%08lx!\n", res);
    }

    res = firmlaunch(firmTid);
    if (res & 0x80000000) {
        error("Firmlaunch returned error 0x%08lx!\n", res);
    }
}

static void doFirmlaunchHax(u64 firmTid)
{
    static vu32 *const firmMagic = (vu32 *)0x24000000;
    static vu32 *const arm9Entrypoint = (vu32 *)0x2400000C; // old firmwares only

    launchFirm(firmTid);
    while(*firmMagic != FIRM_MAGIC); // Wait for the firm header to be written

    for (u32 i = 0; i < 0x10000; i++) {
        *arm9Entrypoint = 0x22100000;
    }

    while (PXIReceiveWord() != 0x0000CAFE);

    success("Got Arm9 arbitrary code execution!\n");
}

static void doLgyFirmHax(bool isN3ds)
{
    prepareFakeLauncher(isN3ds, DS_INTERNET_TID_TWL, DS_INTERNET_CONTENT_ID);

    // Apparently the following 2 commands are necessary to make it work on all consoles (matches TwlBg behavior).
    // In particular p9LgyLog seems to be needed... even though it tampers lgy.log.
    // Hmm looks like not anymore...?
    Result res = 0; //p9LgyLog("Hello world from agbhax!\n");
    if (res & 0x80000000) {
        error("Log returned error %08lx!\n", res);
    }

    u8 bannerHmac[0x14] = {0};
    res = p9LgySetParameters(3, false, DS_INTERNET_TID, bannerHmac);
    if (res & 0x80000000) {
        error("SetParameters returned error %08lx!\n", res);
    }

    res = p9LgyPrepareArm9ForTwl(DS_INTERNET_TID);
    if (res & 0x80000000) {
        error("PrepareArm9ForTwl returned error %08lx!\n", res);
    } else if (res == 0x0000CAFE) {
        success("Got Arm9 arbitrary code execution!\n");
    }
}

static void initFirm(void)
{
    vu32 *entrypointAddr = (vu32 *)0x1FFFFFFC;

    // Wait for Process9 to write the new entrypoint
    // (launchFirm doesn't do that because of firmlaunchhax)
    u32 entrypoint;
    do {
        entrypoint = *entrypointAddr;
    }
    while (entrypoint == 0);

    if (entrypoint == 0x1FF80000) {
        // This may crash before the user can read the message, with Luma running in the background
        error("Please recompile Luma3DS without firmlaunch\npatches.\n");
    }
    *entrypointAddr = 0;

    print("Arm11 entrypoint is 0x%08lx.\n", entrypoint);

    k9Sync();
    print("Synchronization with Kernel9 done.\n");

    p9InitComms();
    print("Synchronization with Process9 done.\n");
}

static void doSafeHax11(u64 firmTidMask)
{
    u64 firmTid = firmTidMask | 3;

    print("Launching %s...\n", getFirmName(firmTid));
    launchFirm(firmTid);
    initFirm();
    print("Doing firmlaunchhax...\n");
    doFirmlaunchHax(firmTid);
}

void arm11main(void)
{
    // Fill the screens with black while Luma (if present) may be loading sysmodules into VRAM.
    // prepareScreens will unset the regs.
    LCD_TOP_FILL_REG = LCD_FILL(0, 0, 0);
    LCD_BOTTOM_FILL_REG = LCD_FILL(0, 0, 0);

    memcpy(&g_takeoverParameters, (const void *)0x22200000, sizeof(g_takeoverParameters));

    u64 firmTid = g_takeoverParameters.firmTid;
    u64 firmTidMask = firmTid & ~0x0FFFFFFFull;
    bool isN3ds = g_takeoverParameters.isN3ds;

    // I2C_init(); <-- this fucks up
    prepareScreens();
    title("Post-firmlaunch Arm11 stub (%s)\n\n", getFirmName(firmTid));

    if (firmTid != 0xFFFFFFFF) {
        initFirm();
        switch (firmTid & 0x0FFFFFFF) {
            case 2:
                print("Doing safehax v1.1...\n");
                doSafeHax11(firmTidMask);
                break;
            case 3:
                print("Doing firmlaunchhax...\n");
                doFirmlaunchHax(firmTid);
                break;
            case 0x202:
                print("Doing agbhax...\n");
                doLgyFirmHax(isN3ds);
                break;
            default:
                error("FIRM TID not supported!\n");
                break;
        }
    } else {
        while (PXIReceiveWord() != 0x0000CAFE);
    }

    const char *fileName = g_takeoverParameters.payloadFileName;
    Result res = PXIReceiveWord();
    if ((res & 0x80000000) == 0) {
        success("%s read successfully!\n", fileName);
    } else {
        switch ((res >> 8) & 0xFF) {
            case 0:
                error("Failed to mount the SD card! (%u)\n", res & 0xFF);
                break;
            case 1:
                error("Failed to open %s! (%u)\n", fileName, res & 0xFF);
                break;
            case 2:
                error("%s is too big!\n", fileName);
                break;
            case 3:
                error("Failed to read %s! (%u)\n", fileName, res & 0xFF);
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
