#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "XMS.H"
#include "DPMI.H"

static DPMI_REG XMSReg = {0};

//http://www.phatcode.net/res/219/files/xms30.txt

#define XMS_IsInited() (XMSReg.w.cs != 0 || XMSReg.w.ip != 0)

static BOOL XMS_Init(void)
{
    if(XMS_IsInited())
        return TRUE;
    
    memset(&XMSReg, 0, sizeof(XMSReg));

    XMSReg.w.ax = 0x4300;
    DPMI_CallRealModeINT(0x2F, &XMSReg);
    if(XMSReg.h.al != 0x80)
    {
        printf("Error: No XMS found.\n");
        return  FALSE;
    }

    /*
    XMSReg.w.ax = 0x4310;
    DPMI_CallRealModeINT(0x2f, &XMSReg); 
    XMSReg.h.ah = 0x00; //get version. need 3.0 to allocate block larger than 64M
    XMSReg.w.cs =XMSReg.w.es;
    XMSReg.w.ip =XMSReg.w.bx;
    DPMI_CallRealModeRETF(&XMSReg);
    if (XMSReg.w.ax != 0x0300)
    {
        printf("Error: No XMS 3.0 found.\n");
        return handle;
    }
    */

    XMSReg.w.ax = 0x4310;
    DPMI_CallRealModeINT(0x2F, &XMSReg);    //control function in es:bx
    XMSReg.w.cs = XMSReg.w.es;
    XMSReg.w.ip = XMSReg.w.bx;
    XMSReg.w.ss = XMSReg.w.sp = 0;
    return TRUE;
}

uint16_t XMS_Alloc(uint16_t sizeKB, uint32_t* outputp addr)
{
    DPMI_REG r;
    uint16_t handle = 0;
    *addr = 0;
   
    if(sizeKB == 0)
    {
        assert(FALSE);
        return handle;
    }

    if(!XMS_Init())
    {
        assert(FALSE);
        return handle;
    }

    r = XMSReg;
    r.h.ah = 0x09;      //alloc XMS
    r.w.dx = sizeKB;    //size in kb
    DPMI_CallRealModeRETF(&r);
    if (r.w.ax != 0x1)
    {
        return handle;
    }
    handle = r.w.dx;

    r = XMSReg;
    r.w.dx = handle;
    r.h.ah = 0x0C;    //lock XMS
    DPMI_CallRealModeRETF(&r);
    if(r.w.ax != 0x1)
    {
        printf("Error: Failed to lock XMS memory (%02x).", r.h.bl);
        r = XMSReg;
        r.h.ah = 0x0A; //free XMS
        DPMI_CallRealModeRETF(&r);
        return handle;
    }
    *addr = ((uint32_t)r.w.dx << 16L) | (uint32_t)r.w.bx;
    return handle;
}

BOOL XMS_Realloc(uint16_t handle, uint16_t newSizeKB, uint32_t* outputp addr)
{
    BOOL result = FALSE;
    DPMI_REG r = XMSReg;

    if(!XMS_IsInited())
    {
        assert(FALSE);
        return result;
    }

    r.h.ah = 0x0D;    //unlock first
    r.w.dx = handle;
    DPMI_CallRealModeRETF(&r);
    if(r.w.ax != 1)
    {
        assert(FALSE);
        return result;
    }

    r = XMSReg;
    r.h.ah = 0x0F;
    r.w.dx = handle;
    r.w.bx = newSizeKB;
    DPMI_CallRealModeRETF(&r);
    result = (r.w.ax == 1);

    r = XMSReg;//relock
    r.w.dx = handle;
    r.h.ah = 0x0C;
    DPMI_CallRealModeRETF(&r);
    if(r.w.ax != 0x1)
        *addr = 0;
    else
        *addr = ((uint32_t)r.w.dx << 16L) | (uint32_t)r.w.bx;
    return result;
}

BOOL XMS_Free(uint16_t handle)
{
    DPMI_REG r = XMSReg;

    if(!XMS_IsInited())
    {
        assert(FALSE);
        return FALSE;
    }

    //printf("unlocking XMS...\n");
    r.h.ah = 0x0D;
    r.w.dx = handle;
    DPMI_CallRealModeRETF(&r);
    if(r.w.ax != 1)
    {
        assert(FALSE);
        return FALSE;
    }

    //printf("freeing XMS...\n");
    r = XMSReg;
    r.h.ah = 0x0A;
    r.w.dx = handle;
    DPMI_CallRealModeRETF(&r);
    assert(r.w.ax == 1);
    return r.w.ax == 1;
}

uint16_t XMS_AllocUMB(uint16_t size16B)
{
    DPMI_REG r;
    uint16_t segment = 0;
    if(size16B == 0 || size16B > 0xFFF)
    {
        assert(FALSE);
        return segment;
    }
    if(!XMS_Init())
    {
        assert(FALSE);
        return segment;
    }
    r = XMSReg;
    r.h.ah = 0x10;      //alloc UMB
    r.w.dx = size16B;    //size in paragrah
    DPMI_CallRealModeRETF(&r);
    if(r.w.ax == 1) //succeess
        segment = r.w.bx;
    return segment;
}

BOOL XMS_FreeUMB(uint16_t segment)
{
    BOOL result = FALSE;
    DPMI_REG r = XMSReg;
    if(!XMS_IsInited())
    {
        assert(FALSE);
        return result;
    }
    if(segment == 0)
        return result;
    r.h.ah = 0x11;
    r.w.dx = segment;
    DPMI_CallRealModeRETF(&r);
    result = (r.w.ax == 1);
    if(!result)
        printf("Error: Failed to free UMB memory (%02x).", r.h.bl);
    return result;
}
