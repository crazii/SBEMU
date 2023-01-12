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
#include <DPMI/DPMI.H>
#include <DPMI/DBGUTIL.H>

static QEMM_IODT_LINK QEMM_IODT_header;
static QEMM_IODT_LINK* QEMM_IODT_Link = &QEMM_IODT_header;
static uint16_t QEMM_EntryIP;
static uint16_t QEMM_EntryCS;

static uint16_t QEMM_OldCallbackIP;
static uint16_t QEMM_OldCallbackCS;

static void __NAKED QEMM_RM_Wrapper()
{//al=data,cl=out,dx=port
    _ASM_BEGIN16
        _ASM(call dword ptr cs:[0])
        _ASM(retf)
    _ASM_END16
}
static void __NAKED QEMM_RM_WrapperEnd() {}

static DPMI_REG QEMM_TrapHandlerREG;
static void QEMM_TrapHandler()
{
    uint16_t port = QEMM_TrapHandlerREG.w.dx;
    uint8_t val = QEMM_TrapHandlerREG.h.al;
    uint8_t out = QEMM_TrapHandlerREG.h.cl;
    QEMM_IODT_LINK* link = QEMM_IODT_header.next;

    while(link)
    {
        for(int i = 0; i < link->count; ++i)
        {
            if(link->iodt[i].port == port)
            {
                QEMM_TrapHandlerREG.w.flags &= ~CPU_CFLAG;
                QEMM_TrapHandlerREG.h.al = link->iodt[i].handler(port, val, out);
                return;
            }
        }
        link = link->next;
    }
    //QEMM_TrapHandlerREG.w.flags |= CPU_CFLAG;
    DPMI_REG r = {0};
    r.w.cs = QEMM_OldCallbackCS;
    r.w.ip = QEMM_OldCallbackIP;
    DPMI_CallRealModeRETF(&r);
}

//https://www.cs.cmu.edu/~ralf/papers/qpi.txt
//https://fd.lod.bz/rbil/interrup/memory/673f_cx5145.html
//http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-7414.htm
uint16_t QEMM_GetVersion(void)
{
    /*DPMI_REG r = {0};
    r.w.cx = 0x5145;
    r.w.dx = 0x4d4d;
    r.h.ah = 0x3f;
    DPMI_CallRealModeINT(0x67, &r); //dead?
    if(r.h.ah != 0)
        return 0;
    r.w.cs = r.w.es;
    r.w.ip = r.w.di;
    r.h.ah = 0x03;
    DPMI_CallRealModeRETF(&r);*/

    //http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-2830.htm
    int fd = 0;
    unsigned int result = _dos_open("QEMM386$", O_RDONLY, &fd);
    if(result != 0)
        return 0;
    uint32_t entryfar = 0;
    //ioctl - read from character device control channel
    int count = ioctl(fd, DOS_RCVDATA, 4, &entryfar);
    _dos_close(fd);
    if(count != 4)
        return 0;
    DPMI_REG r = {0};
    r.w.cs = entryfar>>16;
    r.w.ip = entryfar&0xFFFF;
    r.h.ah = 0x03;
    QEMM_EntryIP = r.w.ip;
    QEMM_EntryCS = r.w.cs;
    if( DPMI_CallRealModeRETF(&r) == 0)
        return r.w.ax;
    return 0;
}


BOOL QEMM_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt)
{
    if(QEMM_IODT_header.next == NULL) //no entries
    {
        DPMI_REG r = {0};
        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A06;
        if(DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags&CPU_CFLAG))
            return FALSE;
        QEMM_OldCallbackIP = r.w.es;
        QEMM_OldCallbackCS = r.w.di;

        uint32_t codesize = (uintptr_t)&QEMM_RM_WrapperEnd - (uintptr_t)&QEMM_RM_Wrapper;
        uint32_t dosmem = DPMI_HighMalloc((codesize + 4 + 15)>>4, TRUE);
        uint32_t rmcb = DPMI_AllocateRMCB_RETF(&QEMM_TrapHandler, &QEMM_TrapHandlerREG);
        if(rmcb == 0)
        {
            DPMI_HighFree(dosmem);
            return FALSE;
        }
        DPMI_CopyLinear(DPMI_SEGOFF2L(dosmem, 0), DPMI_PTR2L(&rmcb), 4);
        void* buf = malloc(codesize);
        memcpy_c2d(buf, &QEMM_RM_Wrapper, codesize); //copy to ds seg in case cs&ds are not same
        DPMI_CopyLinear(DPMI_SEGOFF2L(dosmem, 4), DPMI_PTR2L(buf), codesize);
        free(buf);

        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A07;
        r.w.es = dosmem&0xFFFF;
        r.w.di = 4;
        if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags&CPU_CFLAG))
        {
            DPMI_HighFree(dosmem);
            return FALSE;
        }
    }
    assert(QEMM_IODT_Link->next == NULL);
    QEMM_IODT* mem = (QEMM_IODT*)malloc(sizeof(QEMM_IODT)*count);
    memcpy(mem, iodt, sizeof(QEMM_IODT)*count);

    for(int i = 0; i < count; ++i)
    {
        DPMI_REG r = {0};
        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A08;
        r.w.dx = mem[i].port;
        DPMI_CallRealModeRETF(&r);
        mem[i].state = r.h.bl; //previously trapped state

        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A09;
        r.w.dx = mem[i].port;
        DPMI_CallRealModeRETF(&r); //set port trapped
    }

    QEMM_IODT_LINK* newlink = (QEMM_IODT_LINK*)malloc(sizeof(QEMM_IODT_LINK));
    newlink->iodt = mem;
    newlink->count = count;
    newlink->prev = QEMM_IODT_Link;
    newlink->next = NULL;
    QEMM_IODT_Link->next = newlink;
    QEMM_IODT_Link = newlink;
    iopt->memory = (uintptr_t)newlink;
    return TRUE;
}

BOOL QEMM_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt)
{
    CLIS();
    QEMM_IODT_LINK* link = (QEMM_IODT_LINK*)iopt->memory;
    link->prev->next = link->next;
    if(link->next) link->next->prev = link->prev;
    STIL();

    for(int i = 0; i < link->count; ++i)
    {
        if(!link->iodt[i].state) //previously not trapped
        {
            DPMI_REG r = {0};
            r.w.cs = QEMM_EntryCS;
            r.w.ip = QEMM_EntryIP;
            r.w.ax = 0x1A0A; //clear trapped
            r.w.dx = link->iodt[i].port;
            DPMI_CallRealModeRETF(&r);
        }
    }

    free(link->iodt);
    free(link);
    
    if(QEMM_IODT_header.next == NULL) //empty
    {
        DPMI_REG r = {0};
        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A07;
        r.w.es = QEMM_OldCallbackCS;
        r.w.di = QEMM_OldCallbackIP;
        if( DPMI_CallRealModeRETF(&r) != 0) //restore old handler
            return FALSE;
    }
    return TRUE;
}