#ifndef _EMM_H_
#define _EMM_H_ 1
#include <platform.h>
#include <dpmi/dpmi.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define QEMM_TF_PM 0x01 //set if in pm, otherwise in rm(v86)
extern int QEMM_TrapFlags;

typedef uint32_t (*QEMM_IOTRAP_HANDLER)(uint32_t port, uint32_t val, uint32_t out);

//user interface, not actual struct
typedef struct QEMM_IODispatchTable
{
    uint32_t    port;
    QEMM_IOTRAP_HANDLER handler;
}QEMM_IODT;

typedef struct EMM_IOPorTrap
{
    uint32_t memory;
    uint32_t handle;
    uint16_t func;
}QEMM_IOPT;

//used internally
typedef struct QEMM_IODT_LINK
{
    QEMM_IODT* iodt; //owner
    int count;
    struct QEMM_IODT_LINK* prev; //observer
    struct QEMM_IODT_LINK* next; //observer
}QEMM_IODT_LINK;

//get QEMM version
uint16_t QEMM_GetVersion(void);

BOOL QEMM_GetIOPrtTrap_Context(DPMI_REG* regs); //get trap context, must be in port trap handler to be called, and QEMM_TF_PM must be cleared in QEMM_TrapFlags

//I/O virtualize
BOOL QEMM_Install_IOPortTrap(QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL QEMM_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

void QEMM_UntrappedIO_Write(uint16_t port, uint8_t value);
uint8_t QEMM_UntrappedIO_Read(uint16_t port);

#ifdef __cplusplus
}
#endif

#endif
