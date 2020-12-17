#include "types.h"

extern u8 g_unitInfo;
extern u32 g_bootEnv;

void k9Sync(void);
void p9InitComms(void);

Result p9ReceiveNotification11(u32 *outNotificationId, u32 serviceId);
Result p9McShutdown(void);
Result p9LgyPrepareArm9ForTwl(u64 tid);
Result p9LgyLog(const char *fmt, ...);
Result p9LgySetParameters(u8 tidFlag, bool autolaunchMaybe, u64 tid, u8 *bannerHmac);

Result firmlaunch(u64 firmlaunchTid);
void prepareFakeLauncher(bool isN3ds, u64 twlTid, u32 contentId);
