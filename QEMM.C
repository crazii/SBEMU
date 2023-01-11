#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <fcntl.h>
#include <assert.h>
#ifdef DJGPP
#include <sys/ioctl.h>
#endif

#include "QEMM.h"

#define EMM_IOTRAP_DIRECT_INT 0

uint16_t QEMM_GetVersion(void)
{
    return 0; 
}


BOOL QEMM_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt)
{
    return TRUE;
}

BOOL QEMM_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt)
{
    return FALSE;
}