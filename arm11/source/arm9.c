#include <string.h>
#include <stdarg.h>
#include "fmt.h"
#include "PXI.h"

#define CFG11_DSP_CNT           (*(vu8 *)0x10141230)

extern TakeoverParameters g_takeoverParameters;
void print(const char *fmt, ...);

static struct {
    u8 syncStatus;
    u8 unitInfo;
    u8 bootEnv;
    u32 unused;
    u32 brahmaArm11Entrypoint;
    u32 arm11Entrypoint;
} volatile *const mailbox = (void *)0x1FFFFFF0;

u8 g_unitInfo;
u32 g_bootEnv;

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

    g_unitInfo = mailbox->unitInfo;
    g_bootEnv = mailbox->bootEnv;

    if (g_unitInfo == 3) {
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

static void p9SendCmdbufWithServiceId(u32 serviceId, u32 *cmdbuf)
{
    // https://github.com/TuxSH/3ds_pxi/blob/master/source/sender.c#L40
    PXISendWord(serviceId);
    PXITriggerSync9IRQ(); //notify arm9
    PXISendBuffer(cmdbuf, getCmdbufSizeWords(cmdbuf[0]));
    while (!PXIIsSendFIFOEmpty());
}

static Result p9ReceiveCmdbuf(u32 *cmdbuf)
{
    cmdbuf[0] = PXIReceiveWord();
    u32 cmdbufSizeWords = getCmdbufSizeWords(cmdbuf[0]);
    if (cmdbufSizeWords > 0x40) {
        return 0xDEAD3001;
    }
    PXIReceiveBuffer(cmdbuf + 1, cmdbufSizeWords - 1);
    while (!PXIIsReceiveFIFOEmpty());

    return 0;
}

Result p9ReceiveNotification11(u32 *outNotificationId, u32 serviceId)
{
    u32 cmdbuf[0x40];
    Result res = 0;
    TRY(p9ReceiveCmdbuf(cmdbuf));

    switch (cmdbuf[0] >> 16) {
        case 1:
            *outNotificationId = cmdbuf[1];
            cmdbuf[0] = 0x10040;
            res = 0;
            break;
        default:
            *outNotificationId = 0;
            cmdbuf[0] = 0x40;
            res = 0xD900182F;
            break;
    }

    cmdbuf[1] = res;

    p9SendCmdbufWithServiceId(serviceId, cmdbuf);

    return res;
}

static Result p9SendSyncRequest(u32 serviceId, u32 *cmdbuf)
{
    u32 replyServiceId;
    Result res = 0;

    p9SendCmdbufWithServiceId(serviceId, cmdbuf);

    for (;;) {
        // Receive the request or reply cmdbuf from p9 (might be incoming notification)
        // We assume that if we didn't receive serviceId, we received a notification (this is not foolproof)
        // Pre-2.0 (or 3.0? not sure), p9 only has 5 services instead of 8 (no duplicate sessions for FS)
        while (PXIIsReceiveFIFOEmpty());
        replyServiceId = PXIReceiveWord();
        if (replyServiceId == serviceId) {
            TRY(p9ReceiveCmdbuf(cmdbuf));
            return getCmdbufSizeWords(cmdbuf[0]) == 0 ? 0 : cmdbuf[1];
        } else {
            u32 notificationId;
            TRY(p9ReceiveNotification11(&notificationId, replyServiceId));
            print("Received notification ID 0x%lx\n", notificationId);
        }
    }

    return res;
}

Result p9McShutdown(void)
{
    u32 cmdbuf[0x40];
    cmdbuf[0] = 0x10000;

    return p9SendSyncRequest(0, cmdbuf);
}

Result p9LgyPrepareArm9ForTwl(u64 tid)
{
    u32 cmdbuf[0x40];

    cmdbuf[0] = 0x20080;
    cmdbuf[1] = (u32)tid;
    cmdbuf[2] = (u32)(tid >> 32);

    return p9SendSyncRequest(0, cmdbuf);
}

Result p9LgyLog(const char *fmt, ...)
{
    u32 cmdbuf[0x40];

    cmdbuf[0] = 0xC0800;

    char buf[0x80];
    va_list args;
    va_start(args, fmt);
    xvsprintf(buf, fmt, args);
    va_end(args);

    return p9SendSyncRequest(0, cmdbuf);
}

Result p9LgySetParameters(u8 twlCardMode, bool autolaunchMaybe, u64 tid, u8 *bannerHmac)
{
    u32 cmdbuf[0x40];

    cmdbuf[0] = 0xB0240;
    cmdbuf[1] = twlCardMode;
    cmdbuf[2] = autolaunchMaybe ? 1 : 0;
    cmdbuf[3] = (u32)tid;
    cmdbuf[4] = (u32)(tid >> 32);
    memcpy(cmdbuf + 5, bannerHmac, 0x14);

    return p9SendSyncRequest(0, cmdbuf);
}

// ==================================================

Result firmlaunch(u64 firmlaunchTid)
{
    PXIReset();

    // CFG11_DSP_CNT must be zero when doing a firmlaunch
    CFG11_DSP_CNT = 0x00;

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

// ==================================================

static struct {
    DSiHeader hdr;
    u16 overwritten[2];
    u8 code[0x10000 - 4]; // @ section + 8; needs to be 8-byte-aligned
} *const launcher = (void *)0x27C00000;

static inline u32 convertToDsPa(u32 pa)
{
    return 0x02000000 + ((pa - 0x20000000) >> 2);
}

void prepareFakeLauncher(bool isN3ds, u64 twlTid, u32 contentId)
{
    // We only support the lastest versions of AGB_FIRM (no point in supporting more than that)
    u32 pcStackAddress = isN3ds ? 0x0806E634 : 0x0806E1B4;
    u32 gadgetAddress = (isN3ds ? 0x080307BE : 0x08030CB6) | 1;

    DSiHeader *hdr = &launcher->hdr;
    NDSHeader *dsHdr = &hdr->ndshdr;

    u32 dsPa = convertToDsPa(pcStackAddress);

    memset(hdr, 0, sizeof(DSiHeader));

    dsHdr->unitCode = 2; // DSi to avoid whitelist checks.
    dsHdr->arm9destination = (void *)dsPa;
    dsHdr->arm9binarySize = -dsPa;
    dsHdr->arm9romOffset = 0x1000;

    dsHdr->arm7destination = (void *)0x02000000;
    dsHdr->arm7binarySize = 0;
    hdr->arm9idestination = (void *)0x02000000;
    hdr->arm9ibinarySize = 0;
    hdr->arm7idestination = (void *)0x02000000;
    hdr->arm7ibinarySize = 0;

    memcpy(&hdr->tid_low, "1ANH", 4);

    launcher->overwritten[0] = gadgetAddress;       // pc low halfword: pop {r4, pc}
    launcher->overwritten[1] = 4;                   // offset in section, needs to be 8-byte-aligned

    // Copy our arm9 code into the fake launcher
    memcpy(launcher->code, (const void *)0x22100000, 0x8000);

    // Also prepare the TID entries read @ 0x27DF6C00
    *(u32 *)0x27DF6C00 = 1;
    *(u64 *)0x27DF6C50 = twlTid;
    *(u32 *)0x27DF6EC0 = contentId;
}
