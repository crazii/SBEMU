#ifndef _EMM_H_
#define _EMM_H_ 1
#include <PLATFORM.H>

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint32_t (*QEMM_IOTRAP_HANDLER)(uint32_t port, uint32_t val, uint32_t out);

//user interface, not actual struct
typedef struct QEMM_IODispatchTable
{
    uint16_t    port;
    uint16_t    state; //used internally, no effect
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

//I/O virtualize
BOOL QEMM_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt);

BOOL QEMM_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt);

#ifdef __cplusplus
}
#endif

#endif