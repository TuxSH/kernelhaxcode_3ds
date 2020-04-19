#include "types.h"
#include "PXI.h"
#include "fatfs/ff.h"

static FATFS sdFs;

void arm9main(void)
{
    UINT rd;
    u64 firmlaunchTid = *(vu64 *)0x22200000; // set by pre-firmlaunch payload

    switch (firmlaunchTid & 0xFFFF) {
        case 0x0003:
        case 0x0002:
            PXISendWord(0x0000CAFE);
            break;
        default:
            break;
    }

    while (!PXIIsSendFIFOEmpty());

    // Read the payload and tell the Arm11 stub whether or not we could read it
    FRESULT fsRes = f_mount(&sdFs, "0:", 1);
    FIL f;
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0000 | fsRes);
        for (;;);
    }

    fsRes = f_open(&f, "SafeB9SInstaller.bin", FA_READ);
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0100 | fsRes);
        for (;;);
    }

    if (f_size(&f) > 0xFFFE00) {
        PXISendWord(0xCACA0200 | fsRes);
        for (;;);
    }

    fsRes = f_read(&f, (void *)0x23F00000, 0xFFFE00, &rd);
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0300 | fsRes);
        for (;;);
    }
    f_close(&f);
    f_mount(NULL, "0:", 1);

    PXISendWord(0);
    while (!PXIIsSendFIFOEmpty());

    while (PXIReceiveWord() != 0xC0DE);
    // Return and chainload.
}
