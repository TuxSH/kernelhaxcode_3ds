#include <string.h>
#include "types.h"
#include "PXI.h"

#define CFG11_SHAREDWRAM_32K_DATA(i)    (*(vu8 *)(0x10140000 + i))
#define CFG11_SHAREDWRAM_32K_CODE(i)    (*(vu8 *)(0x10140008 + i))
#define CFG11_DSP_CNT                   (*(vu8 *)0x10141230)

static void resetDsp(void)
{
    CFG11_DSP_CNT = 2; // PDN_DSP_CNT
    for(vu32 i = 0; i < 10; i++);

    CFG11_DSP_CNT = 3;
    for(vu32 i = 0; i < 10; i++);

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = i << 2; // disabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = i << 2; // disabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = 0x80 | (i << 2); // enabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = 0x80 | (i << 2); // enabled, master = arm9
}

static void doFirmlaunch(void)
{
    // Fake firmlaunch

    while(PXIReceiveWord() != 0x44836);
    PXISendWord(0x964536);
    while(PXIReceiveWord() != 0x44837);
    PXIReceiveWord(); // High FIRM titleId
    PXIReceiveWord(); // Low FIRM titleId
    resetDsp();

    while(PXIReceiveWord() != 0x44846);

    *(vu32 *)0x1FFFFFFC = 0x22000000;

    ((void (*)(void))0x22100000)();
    __builtin_unreachable();
}

// Must be position-indepedendent:

void p9TakeoverMain(u32 l2TablePa, u32 numCores)
{
    *(vu32 *)0x1FFF8000 = l2TablePa | 1;
    *(vu32 *)0x1FFFC000 = l2TablePa | 1;

    if (numCores == 4) {
        *(vu32 *)0x1F3F8000 = l2TablePa | 1;
        *(vu32 *)0x1F3FC000 = l2TablePa | 1;
    }

    doFirmlaunch();
}