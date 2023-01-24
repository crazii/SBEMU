#ifndef _HDPMIPT_H_
#define _HDPMIPT_H_
//HDPMI port trap utility

#include "QEMM.H" //QEMM compatible interface

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

// Get interrupt context. it's not a port trap function but convient to put here.
typedef struct _HDPMIPT_IntContext
{
    uint32_t IsV86;
    uint16_t DS;
    uint16_t ES;
    uint16_t FS;
    uint16_t GS;
}HDPMIPT_INTCONTEXT;

BOOL HDPMIPT_GetInterruptContext(HDPMIPT_INTCONTEXT* context);

#endif //_HDPMIPT_H_