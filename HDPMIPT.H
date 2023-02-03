#ifndef _HDPMIPT_H_
#define _HDPMIPT_H_
//HDPMI port trap utility

#include "QEMM.H" //QEMM compatible interface

BOOL HDPMIPT_Detect();

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

void HDPMIPT_UntrappedIO_Write(uint16_t port, uint8_t value);
uint8_t HDPMIPT_UntrappedIO_Read(uint16_t port);

#endif //_HDPMIPT_H_