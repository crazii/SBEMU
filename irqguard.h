#ifndef _IRQGUARD_H_
#define _IRQGUARD_H_
#include "sbemu/platform.h"

//guard the virtual irq to prevent it pass to default IVT handler (bios)
//I've never seen any problem without the guard for my laptop, but it will get more compatibility

#ifdef __cplusplus
extern "C" {
#endif

//install/reinstall
BOOL IRQGUARD_Install(uint8_t irq);

//uninstall
BOOL IRQGUARD_Uninstall();

//enable irq guard
void IRQGUARD_Enable();

//disable irq guard
void IRQGUARD_Disable();

#ifdef __cplusplus
}
#endif

#endif