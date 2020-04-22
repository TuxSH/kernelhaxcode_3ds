#include <string.h>
#include <stdarg.h>
#include "fmt.h"
#include "PXI.h"

#define CFG11_DSP_CNT           (vu8 *)0x10141230

static struct {
    u8 syncStatus;
    u8 unitInfo;
    u8 bootEnv;
    u32 unused;
    u32 brahmaArm11Entrypoint;
    u32 arm11Entrypoint;
} volatile *const mailbox = (void *)0x1FFFFFF0;

u8 unitinfo;
u32 bootenv;

static inline u32 getCmdbufSizeWords(u32 cmdhdr)
{
    return (cmdhdr & 0x3F) + ((cmdhdr & 0xFC0) >> 6) + 1;
}

// Called by k11's entrypoint function
void k9Sync(void)
{
    /*
        So... recent k9 implements _two_ synchronization mechanism, the legacy one and the new one. We'll
        use the legacy one. Newer one is below:

        mailbox->syncStatus = 1;
        while (mailbox->syncStatus != 2);
        mailbox->syncStatus = 3;
     */

    mailbox->syncStatus = 1;
    while (mailbox->syncStatus != 2);
    mailbox->syncStatus = 1;
    while (mailbox->syncStatus != 2);

    unitinfo = mailbox->unitInfo;
    bootenv = mailbox->bootEnv;

    if (unitinfo == 3) {
      for (u32 i = 0; i < 0x800000; i++);
      mailbox->syncStatus = 1;
      while (mailbox->syncStatus != 2);
      mailbox->syncStatus = 3;
    }
}

// Code from 3ds_pxi
// Basically resets and syncs the PXI FIFOs with Process9
void p9InitComms(void)
{
    PXIReset();

    // Ensure that both the Arm11 and Arm9 send FIFOs are full, then agree on the shared byte.
    // Then flush, then agree a last time.
    do {
        while (!PXIIsSendFIFOFull()) {
            PXISendWord(0);
        }
    } while (PXIIsReceiveFIFOEmpty() || !PXIIsSendFIFOFull());

    PXISendByte(1);
    while (PXIReceiveByte() < 1);

    while (!PXIIsReceiveFIFOEmpty()) {
        PXIReceiveWord();
    }

    PXISendByte(2);
    while(PXIReceiveByte() < 2);
}

static Result p9SendSyncRequest(u8 serviceId, u32 *cmdbuf)
{
    // https://github.com/TuxSH/3ds_pxi/blob/master/source/sender.c#L40
    PXISendWord(serviceId);
    PXITriggerSync9IRQ(); //notify arm9
    PXISendBuffer(cmdbuf, getCmdbufSizeWords(cmdbuf[0]));
    while (!PXIIsSendFIFOEmpty());

    while (PXIIsReceiveFIFOEmpty());
    if (PXIReceiveWord() != serviceId) { // service ID
        return 0xDEAD3001;
    }

    cmdbuf[0] = PXIReceiveWord();
    PXIReceiveBuffer(cmdbuf + 1, getCmdbufSizeWords(cmdbuf[0]) - 1);
    while (!PXIIsReceiveFIFOEmpty());

    return getCmdbufSizeWords(cmdbuf[0]) == 0 ? 0 : cmdbuf[1];
}

Result p9McShutdown(void)
{
    u32 cmdbuf[0x40];
    cmdbuf[0] = 0x10000;

    return p9SendSyncRequest(0, cmdbuf);
}

Result firmlaunch(u64 firmlaunchTid)
{
    *CFG11_DSP_CNT = 0x00; // CFG11_DSP_CNT must be null when doing a firmlaunch
    PXISendWord(0x44836);
    if (PXIReceiveWord() != 0x964536) {
        return 0xDEAD4001;
    }

    PXISendWord(0x44837);
    while (!PXIIsSendFIFOEmpty());

    PXISendWord((u32)(firmlaunchTid >> 32));
    PXISendWord((u32)firmlaunchTid);
    while (!PXIIsSendFIFOEmpty());

    mailbox->arm11Entrypoint = 0;
    PXISendWord(0x44846);
    while (!PXIIsSendFIFOEmpty());

    return 0;
}
