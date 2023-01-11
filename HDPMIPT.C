#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#endif
#include <DPMI/DBGUTIL.H>

#include "HDPMIPT.H"

#define HDPMIPT_SWITCH_STACK 1

typedef struct
{
    uint32_t edi;
    uint16_t es;
}HDPMIPT_ENTRY;

extern uint32_t __djgpp_stack_top;

static const char* VENDOR_HDPMI = "HDPMI";    //vendor string
static HDPMIPT_ENTRY HDPMIPT_Entry;
static QEMM_IODT* HDPMIPT_Iodt;
static int HDPMIPT_PortCount;
#if HDPMIPT_SWITCH_STACK
static int32_t HDPMIPT_OldESP[2];
#endif

uint32_t HDPMIPT_TrapHandler()
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

    for(int i = 0; i < HDPMIPT_PortCount; ++i)
    {
        if(HDPMIPT_Iodt[i].port == port)
            return HDPMIPT_Iodt[i].handler(port, value, out);
    }
    return value;
}

void __attribute__((naked)) HDPMIPT_TrapHandlerWrapper()
{
    //switch to local stack from trapped client's stack
    #if HDPMIPT_SWITCH_STACK
    asm(
    "cli \n\t"
    "mov %%esp, %0 \n\t"
    "push %%ss \n\t"
    "pop %1 \n\t"
    "push %%ds \n\t"
    "push %2 \n\t"
    "lss 0(%%esp), %%esp \n\t"
    :"=m"(HDPMIPT_OldESP[0]),"=m"(HDPMIPT_OldESP[1])
    :"m"(__djgpp_stack_top)
    :"memory"
    );
    #endif

    HDPMIPT_TrapHandler();

    #if HDPMIPT_SWITCH_STACK
    asm("lss %0, %%esp" : :"m"(HDPMIPT_OldESP[0]) ); //restore stack
    #endif
    asm("lret"); //retf
}

int DPMIPT_GetVendorEntry(HDPMIPT_ENTRY* entry)
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

uint32_t HDPMI_Internal_InstallTrap(const HDPMIPT_ENTRY* entry, int start, int end, void(*handler)(void))
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

BOOL HDPMI_Internal_UninstallTrap(const HDPMIPT_ENTRY* entry, uint32_t handle)
{
    BOOL result = FALSE;
    asm(
    "mov %2, %%edx \n\t"  //EDX=handle
    "mov $6, %%eax \n\t" //ax=7, unistall port trap
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

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt)
{
    assert(iopt);
    if(!DPMIPT_GetVendorEntry(&HDPMIPT_Entry))
    {
        HDPMIPT_Entry.es = 0;
        HDPMIPT_Entry.edi = 0;
        puts("Failed to get HDPMI Vendor entry point.\n");
        return FALSE;
    }
    _LOG("HDPMI vendor entry: %04x:%08x\n", HDPMIPT_Entry.es, HDPMIPT_Entry.edi);

    uint32_t handle = HDPMI_Internal_InstallTrap(&HDPMIPT_Entry, start, end, &HDPMIPT_TrapHandlerWrapper);
    if(!handle)
    {
        puts("Failed to intall HDPMI io port trap.\n");
        return FALSE;
    }

    assert(!HDPMIPT_Iodt); //unique trap ports for now. TODO: use linked list to support multiple installlation calls
    HDPMIPT_Iodt = malloc(sizeof(QEMM_IODT)*count);
    memcpy(HDPMIPT_Iodt, iodt, sizeof(QEMM_IODT)*count);
    HDPMIPT_PortCount = count;
    //printf("Trap count:%d\n", HDPMIPT_PortCount);

    iopt->memory = (uintptr_t)HDPMIPT_Iodt;
    iopt->handle = handle;
    iopt->func = 0;
    return TRUE;
}

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt)
{
    if(HDPMIPT_Entry.es == 0 || HDPMIPT_Entry.edi == 0)
    {
        //assert(FALSE);
        return FALSE;
    }
    assert(iopt != NULL && iopt->memory == (uintptr_t)HDPMIPT_Iodt);

    free(HDPMIPT_Iodt);
    HDPMIPT_Iodt = NULL;
    HDPMIPT_PortCount = 0;
    iopt->memory = 0;

    uint32_t handle = iopt->handle;
    iopt->handle = 0;
    return HDPMI_Internal_UninstallTrap(&HDPMIPT_Entry, handle);
}
