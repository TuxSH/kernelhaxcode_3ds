#include "types.h"

extern u8 unitinfo;
extern u32 bootenv;

void k9Sync(void);
void p9InitComms(void);
Result p9McShutdown(void);

Result firmlaunch(u64 firmlaunchTid);