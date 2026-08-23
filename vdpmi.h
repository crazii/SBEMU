#ifndef _VDPMI_H_
#define _VDPMI_H_
#include "iotrap.h"
#include "sbemu/dpmi/dpmi.h"

//client mode drvier for VDPMI

//return result
#define DPMI_DRVF_SKIPVM  0x80000000L //skip passing to client

BOOL VDPMI_Detect();

BOOL VDPMI_Install_IOPortTrap(SBEMU_IODT* inputp iodt, uint16_t count, SBEMU_IOPT* outputp iopt);

BOOL VDPMI_Uninstall_IOPortTrap(SBEMU_IOPT* inputp iopt);

void VDPMI_UntrappedIO_Write(uint16_t port, uint8_t value);
uint8_t VDPMI_UntrappedIO_Read(uint16_t port);

//install client mode driver
BOOL VDPMI_InstallISR(uint8_t irq, void(*irqhandler)(), DPMI_ISR_HANDLE* handle);
BOOL VDPMI_UninstallISR(DPMI_ISR_HANDLE* handle);

//raise virq
BOOL VDPMI_RaiseIRQ(uint8_t irq);

//get physical addr from linear addr
uint32_t VDPMI_GetPhysicalAddr(uint32_t laddr);

#endif //_VDPMI_H_
