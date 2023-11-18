#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#endif
#include <dpmi/dbgutil.h>
#include <untrapio.h>

#include "hdpmipt.h"

#define HDPMIPT_SWITCH_STACK 1 //TODO: debug
#define HDPMIPT_STACKSIZE 16384

typedef struct
{
    uint32_t edi;
    uint16_t es;
}HDPMIPT_ENTRY;

extern uint32_t __djgpp_stack_top;

static const char* VENDOR_HDPMI = "HDPMI";    //vendor string
static HDPMIPT_ENTRY HDPMIPT_Entry;
#if HDPMIPT_SWITCH_STACK
static uint32_t HDPMIPT_OldESP[2];
static uint32_t HDPMIPT_NewStack[2];
#endif

static QEMM_IODT_LINK HDPMIPT_IODT_header;
static QEMM_IODT_LINK* HDPMIPT_IODT_Link = &HDPMIPT_IODT_header;

static uint16_t HDPMIPT_GetDS()
{
    uint16_t ds;
    asm("mov %%ds, %0":"=r"(ds));
    return ds;
}

static uint32_t __attribute__((noinline)) HDPMIPT_TrapHandler()
{
    uint32_t port = 0, out = 0, value = 0;
    asm(
    "mov %%edx, %0 \n\t"
    "mov %%ecx, %1 \n\t"
    "mov %%eax, %2 \n\t"
    :"=m"(port),"=m"(out),"=m"(value)
    :
    :"memory"
    );

    QEMM_TrapFlags |= QEMM_TF_PM;
    //if(port >= 0 && port <= 0xF)
        //_LOG("Trapped PM: %s %x\n", out ? "out" : "in", port);
    QEMM_IODT_LINK* link = HDPMIPT_IODT_header.next;
    while(link)
    {
        for(int i = 0; i < link->count; ++i)
        {
            //if(port>=0&&port<=0xF)
            //    _LOG("port: %s %04x, %04x, %04x\n",out ? "out" : "in",port, link->iodt[i].port, value);
            if(link->iodt[i].port == port)
                return link->iodt[i].handler(port, value, out);
        }
        link = link->next;
    }
    return value;
}

static void __attribute__((naked)) HDPMIPT_TrapHandlerWrapper()
{
    //switch to local stack from trapped client's stack
    #if HDPMIPT_SWITCH_STACK
    asm(
    "mov %%esp, %0 \n\t"
    "mov %%ss, %%sp \n\t"
    "mov %%sp, %1 \n\t"
    "lss %2, %%esp \n\t"
    :"=m"(HDPMIPT_OldESP[0]),"=m"(HDPMIPT_OldESP[1])
    :"m"(HDPMIPT_NewStack[0])
    :"memory"
    );
    #endif

    HDPMIPT_TrapHandler();

    #if HDPMIPT_SWITCH_STACK
    asm("lss %0, %%esp" : :"m"(HDPMIPT_OldESP[0]) ); //restore stack
    #endif

    asm("lret"); //retf
}

static int HDPMIPT_GetVendorEntry(HDPMIPT_ENTRY* entry)
{
    int result = 0;
    asm(
    "push %%es \n\t"
    "push %%esi \n\t"
    "push %%edi \n\t"
    "xor %%eax, %%eax \n\t"
    "xor %%edi, %%edi \n\t"
    "mov %%di, %%es \n\t"
    "mov $0x168A, %%ax \n\t"
    "mov %3, %%esi \n\t"
    "int $0x2F \n\t"
    "mov %%es, %%cx \n\t" //entry->es & entry->edi may use register esi & edi
    "mov %%edi, %%edx \n\t" //save edi to edx and pop first
    "pop %%edi \n\t"
    "pop %%esi \n\t"
    "pop %%es \n\t"
    "mov %%eax, %0 \n\t"
    "mov %%cx, %1 \n\t"
    "mov %%edx, %2 \n\t"
    : "=r"(result),"=m"(entry->es), "=m"(entry->edi)
    : "m"(VENDOR_HDPMI)
    : "eax", "ecx", "edx","memory"
    );
    return (result&0xFF) == 0; //al=0 to succeed
}

static uint32_t HDPMI_Internal_InstallTrap(const HDPMIPT_ENTRY* entry, int start, int end, void(*handler)(void))
{
    uint32_t handle = 0;
    int count = end - start + 1;
    const HDPMIPT_ENTRY ent = *entry; //avoid gcc using ebx
    asm(
    "push %%ebx \n\t"
    "push %%esi \n\t"
    "push %%edi \n\t"
    "mov %1, %%esi \n\t" //ESI: starting port
    "mov %2, %%edi \n\t"    //EDI: port count
    "xor %%ecx, %%ecx \n\t"
    "mov %%cs, %%cx \n\t" //CX: handler code seg
    "xor %%ebx, %%ebx \n\t"
    "mov %%ds, %%bx \n\t" //BX: handler data seg
    "mov %3, %%edx \n\t"  //EDX: handler addr
    "mov $6, %%eax \n\t" //ax=6, install port trap
    "lcall *%4\n\t"
    "jc 1f \n\t"
    "mov %%eax, %0 \n\t"
    "1: pop %%edi \n\t"
    "pop %%esi \n\t"
    "pop %%ebx \n\t"
    :"=m"(handle)
    :"m"(start),"m"(count),"m"(handler),"m"(ent)
    :"eax","ebx","ecx","edx","memory"
    );
    return handle;
}

static BOOL HDPMI_Internal_UninstallTrap(const HDPMIPT_ENTRY* entry, uint32_t handle)
{
    BOOL result = FALSE;
    asm(
    "mov %2, %%edx \n\t"  //EDX=handle
    "mov $7, %%eax \n\t" //ax=7, unistall port trap
    "lcall *%1\n\t"
    "jc 1f \n\t"
    "mov $1, %%eax \n\t"
    "mov %%eax, %0 \n\t"
    "1: nop \n\t"
    :"=m"(result)
    :"m"(*entry),"m"(handle)
    :"eax","ecx","edx","memory"
    );
    return result;
}

BOOL HDPMIPT_Detect()
{
    HDPMIPT_ENTRY entry;
    BOOL result = HDPMIPT_GetVendorEntry(&entry);
    return result && (entry.edi || entry.es);
}

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt)
{
    assert(iopt);
    if(HDPMIPT_IODT_header.next == NULL)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
        {
            HDPMIPT_Entry.es = 0;
            HDPMIPT_Entry.edi = 0;
            puts("Failed to get HDPMI Vendor entry point.\n");
            return FALSE;
        }
        //_LOG("HDPMI vendor entry: %04x:%08x\n", HDPMIPT_Entry.es, HDPMIPT_Entry.edi);
    }

    uint32_t handle = HDPMI_Internal_InstallTrap(&HDPMIPT_Entry, start, end, &HDPMIPT_TrapHandlerWrapper);
    if(!handle)
    {
        _LOG("Failed to install HDPMI io port trap.\n");
        return FALSE;
    }
    
    if(HDPMIPT_IODT_header.next == NULL)
    {
        #if HDPMIPT_SWITCH_STACK
        HDPMIPT_NewStack[0] = (uintptr_t)malloc(HDPMIPT_STACKSIZE) + HDPMIPT_STACKSIZE - 8;
        HDPMIPT_NewStack[1] = HDPMIPT_GetDS();
        #endif
    }
    
    QEMM_IODT* Iodt = (QEMM_IODT*)malloc(sizeof(QEMM_IODT)*count);
    memcpy(Iodt, iodt, sizeof(QEMM_IODT)*count);

    QEMM_IODT_LINK* newlink = (QEMM_IODT_LINK*)malloc(sizeof(QEMM_IODT_LINK));
    newlink->iodt = Iodt;
    newlink->count = count;
    newlink->prev = HDPMIPT_IODT_Link;
    newlink->next = NULL;
    HDPMIPT_IODT_Link->next = newlink;
    HDPMIPT_IODT_Link = newlink;
    iopt->memory = (uintptr_t)newlink;
    iopt->handle = handle;
    return TRUE;
}

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt)
{
    CLIS();
    QEMM_IODT_LINK* link = (QEMM_IODT_LINK*)iopt->memory;
    link->prev->next = link->next;
    if(link->next) link->next->prev = link->prev;
    if(HDPMIPT_IODT_Link == link)
        HDPMIPT_IODT_Link = link->prev;
    STIL();
    BOOL result = HDPMI_Internal_UninstallTrap(&HDPMIPT_Entry, iopt->handle);
    if(!result)
        _LOG("Failed to uninstall HDPMI io port trap.\n");
    free(link->iodt);
    free(link);
    
    if(HDPMIPT_IODT_header.next == NULL)
    {
        #if HDPMIPT_SWITCH_STACK
        free((void*)(HDPMIPT_NewStack[0] - HDPMIPT_STACKSIZE + 8));
        #endif
    }
    return TRUE;
}

void HDPMIPT_UntrappedIO_Write(uint16_t port, uint8_t value)
{
    if(HDPMIPT_Entry.es == 0 || HDPMIPT_Entry.edi == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return;
    }

    asm(
    "mov $0x08, %%eax \n\t" //function no.
    "mov %1, %%dx \n\t"     //dx=port
    "mov %2, %%cl \n\t"     //cl=value
    "mov $1, %%ch \n\t"     //ch=int/out
    "lcall *%0\n\t"
    :
    :"m"(HDPMIPT_Entry),"m"(port),"m"(value)
    :"eax","ecx","edx"
    );
}

uint8_t HDPMIPT_UntrappedIO_Read(uint16_t port)
{
    if(HDPMIPT_Entry.es == 0 || HDPMIPT_Entry.edi == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return FALSE;
    }
    uint8_t result = 0;
    asm(
    "mov $0x08, %%eax \n\t" //function no.
    "mov %2, %%dx \n\t"     //dx=port
    "xor %%ch, %%ch \n\t"   //ch=in/out
    "lcall *%1\n\t"
    "mov %%al, %0 \n\t"
    :"=m"(result)
    :"m"(HDPMIPT_Entry),"m"(port)
    :"eax","ecx","edx"
    );
    return result;
}

BOOL HDPMIPT_InstallIRQACKHandler(uint8_t irq, uint16_t cs, uint32_t offset)
{
    if(HDPMIPT_Entry.es == 0 || HDPMIPT_Entry.edi == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return FALSE;
    }

    asm(
    "push %%esi \n\t"
    "mov $0x0B, %%eax \n\t" //function no.
    "movzx %1, %%esi \n\t" //esi=irq
    "movzx %2, %%ecx \n\t"   //ecx=selector
    "mov %3, %%edx \n\t"    //edx=offset
    "lcall *%0\n\t"
    "pop %%esi"
    :
    :"m"(HDPMIPT_Entry),"m"(irq),"m"(cs),"m"(offset)
    :"eax","ecx","edx"
    );
    return TRUE;
}

BOOL HDPMIPT_GetInterrupContext(INTCONTEXT* context)
{
    if(HDPMIPT_Entry.es == 0 || HDPMIPT_Entry.edi == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return FALSE;
    }
    asm(
    "push %%edi \n\t"
    "mov %%ds, %%dx \n\t"
    "mov %1, %%edi \n\t"
    "mov $0x0C, %%eax \n\t"
    "lcall *%0 \n\t"
    "pop %%edi \n\t"
    :
    :"m"(HDPMIPT_Entry), "m"(context)
    :"eax", "edx"
    );
    //DBG_DumpREG(&context->regs);
    return TRUE;
}
