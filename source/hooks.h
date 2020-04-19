#pragma once
#include "types.h"

extern const u32 kernelSvcHandlerHook[];
extern u32 originalKernelSvcHandler;

extern const u32 kernelFirmlaunchHook1[];
extern u32 kernelFirmlaunchHook1PxiRegsOffset;

extern const u32 kernelFirmlaunchHook2[];
extern const u32 kernelFirmlaunchHook2Size;
