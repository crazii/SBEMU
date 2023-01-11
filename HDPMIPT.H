#ifndef _HDPMIPT_H_
#define _HDPMIPT_H_
//HDPMI port trap utility

#include "QEMM.H" //QEMM compatible interface

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

#endif //_HDPMIPT_H_