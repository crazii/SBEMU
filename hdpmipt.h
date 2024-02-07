#ifndef _HDPMIPT_H_
#define _HDPMIPT_H_
//HDPMI port trap utility

#include <assert.h>
#include "qemm.h" //QEMM compatible interface

BOOL HDPMIPT_Detect();

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

void HDPMIPT_UntrappedIO_Write(uint16_t port, uint8_t value);
uint8_t HDPMIPT_UntrappedIO_Read(uint16_t port);

//irq hack
//rmcs,rmoffset are optional (-1 to skip, 0 to clear)
BOOL HDPMIPT_InstallIRQRoutedHandler(uint8_t irq, uint16_t cs, uint32_t offset, uint16_t rmcs, uint16_t rmoffset);
BOOL HDPMIPT_GetIRQRoutedHandler(uint8_t irq, uint16_t* cs, uint32_t* offset, uint16_t* rmcs, uint16_t* rmoffset);
BOOL HDPMIPT_DisableIRQRouting(uint8_t irq);
BOOL HDPMIPT_EnableIRQRouting(uint8_t irq);
BOOL HDPMIPT_LockIRQRouting(BOOL locked);

//helpers
typedef struct
{
    uint32_t offset;
    uint16_t cs;
    uint16_t rmoffset;
    uint16_t rmcs;
    uint16_t valid;
}HDPMIPT_IRQRoutedHandle;
#define HDPMIPT_IRQRoutedHandle_Default {-1, -1, -1, -1, 0}

static inline BOOL HDPMIPT_InstallIRQRoutedHandlerH(uint8_t irq, const HDPMIPT_IRQRoutedHandle* h)
{
    if(h == NULL || !h->valid)
    {
        assert(FALSE);
        return FALSE;
    }
    return HDPMIPT_InstallIRQRoutedHandler(irq, h->cs, h->offset, h->rmcs, h->rmoffset);
}

static inline BOOL HDPMIPT_GetIRQRoutedHandlerH(uint8_t irq, HDPMIPT_IRQRoutedHandle* h)
{
    if(h == NULL)
    {
        assert(FALSE);
        return FALSE;
    }
    h->valid = HDPMIPT_GetIRQRoutedHandler(irq, &h->cs, &h->offset, &h->rmcs, &h->rmoffset);
    assert(h->valid);
    return h->valid;
}

typedef struct IntContext //must be same as in HDPMI
{
    uint32_t EIP;
    uint32_t ESP;
    uint32_t EFLAGS;
    DPMI_REG regs;
}INTCONTEXT;

BOOL HDPMIPT_GetInterrupContext(INTCONTEXT* context);

#endif //_HDPMIPT_H_
