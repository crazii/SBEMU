#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#define __asm__
#define __volatile__(...)
typedef struct {
  unsigned long size;
  unsigned long pm_offset;
  unsigned short pm_selector;
  unsigned short rm_offset;
  unsigned short rm_segment;
} _go32_dpmi_seginfo;
#else
#include <sys/segments.h>
#include <dpmi.h>
#endif
#include <dpmi/dbgutil.h>

#include "vdpmi.h"

#define VDPMI_STACKSIZE     8192    //port trap stack size
#define VDPMI_STACKCOUNT    4       //number of re-entrance.
                                    //reentrance happens when trap hanlder raises irq and the other clients performs IO in the int handler


#define VDPMI_IOPL 1 //host support IOPL for client driver, this boost IO/CLI/STI instructions

typedef struct
{
    uint32_t offset32;
    uint16_t sel;
    uint16_t padding;
}SBEMU_FARPTR;

//vendor string
static const char* DPMI_VENDOR = "VDPMI";
static SBEMU_FARPTR VDPMI_Entry;
static SBEMU_FARPTR VDPMI_Stack[VDPMI_STACKCOUNT];
static volatile uint32_t VDPMIDR_InTrapHandler;

static SBEMU_IODT_LINK VDPMI_IODT_header;
static SBEMU_IODT_LINK* VDPMI_IODT_Link = &VDPMI_IODT_header;
static SBEMU_FARPTR VDPMI_OldHandler;

static SBEMU_IOTRAP_HANDLER VDPMI_IOTrapTable[0x10000];

static uint16_t VDPMI_GetDS()
{
    uint16_t ds;
    __asm__ __volatile__("mov %%ds, %0":"=a"(ds));
    return ds;
}

static uint32_t VDPMI_GetESP()
{
    uint32_t esp;
    __asm__ __volatile__("mov %%esp, %0":"=a"(esp));
    return esp;
}

static int VDPMI_GetVendorEntry()
{
    int result = 0;
    __asm__ __volatile__(
    "pushl %%es \n\t"
    "pushl %%esi \n\t"
    "pushl %%edi \n\t"
    "xor %%eax, %%eax \n\t"
    "xor %%edi, %%edi \n\t"
    "mov %%di, %%es \n\t"
    "movw $0x168A, %%ax \n\t"
    "movl %3, %%esi \n\t"
    "int $0x2F \n\t"
    "mov %%es, %%cx \n\t"
    "mov %%edi, %%edx \n\t"
    "popl %%edi \n\t"
    "popl %%esi \n\t"
    "popl %%es \n\t"
    "movl %%eax, %0 \n\t"
    "movw %%cx, %1 \n\t"
    "movl %%edx, %2 \n\t"
    : "=a"(result),"=m"(VDPMI_Entry.sel),"=m"(VDPMI_Entry.offset32)
    : "m"(DPMI_VENDOR)
    : "ecx", "edx","memory"
    );
    return (result&0xFF) == 0; //al=0 to succeed
}

#define DPMI_TRAPF_OUT  0x01
#define VDPMI_UNHANDLED 0x80000000
static uint32_t __attribute__((noinline)) VDPMI_TrapHandler()
{
    uint32_t port, trapflags, outval;
    __asm__ __volatile__(
    "nop"
    :"=d"(port),"=c"(trapflags),"=a"(outval)
    );

    if(port <= 0xFFFF)
    {
        if(VDPMI_IOTrapTable[port])
        {
            //_LOG("porttrap: %x, %d, %x\n", port, trapflags&DPMI_TRAPF_OUT, outval&0xFF);
            return VDPMI_IOTrapTable[port](port, outval&0xFF, trapflags&DPMI_TRAPF_OUT);
        }
    }
    return VDPMI_UNHANDLED;
}

static void __attribute__((naked)) VDPMI_TrapHandlerWrapper()
{
    //change ds,es
    __asm__ __volatile__(
        "movl $0x4321ABCD, %eax\n\t" //will be patched by VDPMI_Detect
        "mov %eax, %ds \n\t"
        "mov %eax, %es \n\t"
        "movl %ss:12(%esp), %edx \n\t" //port
        "movl %ss:16(%esp), %ecx \n\t" //trapflags
        "movl %ss:20(%esp), %eax \n\t" //outval
    );

    //switch to local stack from locked pm stack
    __asm__ __volatile__(
    "movl %0, %%ebx \n\t"
    "incl %0 \n\t"
    "shl $0x3, %%ebx \n\t" //sizeof(SBEMU_FARPTR)
    "movl %%esp, %%esi \n\t"
    "mov %%ss, %%edi \n\t"
    "lss %1(%%ebx), %%esp \n\t"
    "pushl %%edi \n\t"
    "pushl %%esi \n\t"
    :"+m"(VDPMIDR_InTrapHandler)
    :"m"(VDPMI_Stack)
    :"memory"
    );

    //NOTE: don't use extra C code here because
    //it will overwrite edx, ecx, eax
    uint32_t result = VDPMI_TrapHandler();

    __asm__ __volatile__(
        "decl %0 \n\t"
        "lss %%ss:(%%esp), %%esp \n\t" 
        :"+m"(VDPMIDR_InTrapHandler)
    ); //restore stack

    __asm__ __volatile__(
        "testl $0x80000000, %0 \n\t"
        "jnz 1f \n\t"
        "andl $0x7FFFFFFF, %0 \n\t"
        "movl %0, %%ss:8(%%esp) \n\t" //inval
        "lret \n\t" //retf

        "1: \n\t"
        :
        :"a"(result)
        :
    );
    __asm__ __volatile__(
        "ljmp *%0"
        :
        :"m"(VDPMI_OldHandler)
    );
}

static BOOL VDPMI_Internal_InstallTrap(uint32_t cs, void(*handler)(void))
{
    BOOL result = 0;
    SBEMU_FARPTR ptr32;
    __asm__ __volatile__(
    "movl $0x01, %%eax \n\t"    //function no 1. get
    "lcall *%3 \n\t"
    "jc ERROR \n\t"
    "movl %%edx, %1 \n\t"
    "movw %%cx, %2 \n\t"
    "movl $0x02, %%eax \n\t"    //function no 2. set
    "movl %4, %%ecx \n\t"
    "movl %5, %%edx \n\t"
    "lcall *%3 \n\t"
    "jc ERROR \n\t"
    "movl $1, %%eax \n\t"
    "jmp DONE \n\t"
    "ERROR: xor %%eax, %%eax \n\t"
    "DONE: \n\t"
    :"=a"(result),"=m"(ptr32.offset32),"=m"(ptr32.sel)
    :"m"(VDPMI_Entry),"m"(cs),"m"(handler)
    :"ecx", "edx"
    );

    if(cs == _my_cs() && ptr32.sel != _my_cs())
        VDPMI_OldHandler = ptr32;
    return result;
}

static BOOL VDPMI_GetPortTrap(uint16_t port, BOOL* trapped)
{
    BOOL result = 0;
    __asm__ __volatile__(
    "movl $0x03, %%eax \n\t" //function no.
    "movzxw %3, %%edx \n\t"   //edx=port
    "lcall *%2 \n\t"
    "jnc 1f \n\t"
    "xorl %%eax, %%eax \n\t"
    "jmp 2f \n\t"
    "1: movl %%eax, %1 \n\t" //eax=enabled
    "movl $0x1, %%eax \n\t"
    "2: \n\t"
    :"=a"(result),"=m"(*trapped)
    :"m"(VDPMI_Entry),"d"(port)
    :"ecx"
    );
    return result;
}

static BOOL VDPMI_SetPortTrap(uint16_t port, BOOL trapped)
{
    BOOL result = 0;
    __asm__ __volatile__(
    "movl $0x04, %%eax \n\t" //function no.
    "movzxw %2, %%edx \n\t" //edx=port
                            //ecx=enable trap
    "lcall *%1\n\t"
    "jnc 1f \n\t"
    "xor %%eax, %%eax\n\t"
    "jmp 2f\n\t"
    "1: movl $0x1, %%eax\n\t"
    "2: \n\t"
    :"=a"(result)
    :"m"(VDPMI_Entry),"d"(port),"c"(trapped)
    :
    );
    return result;
}

static BOOL VDPMI_InstallDriver(uint8_t irq, void(*irqhandler)())
{
    BOOL result = 0;
    __asm__ __volatile__(
    "movl $0x06, %%eax \n\t"    //function no.
                                //cs
    "movzxb %2, %%ecx \n\t"     //ecx=irq
                                //cs:edx=handler
    "lcall *%1 \n\t"
    "jnc 1f \n\t"
    "xor %0, %0 \n\t"
    "jmp 2f \n\t"
    "1: movl $0x1, %0 \n\t"
    "2: \n\t"
    :"=a"(result)
    :"m"(VDPMI_Entry),"c"(irq),"d"(irqhandler)
    :
    );
    return result;
}

BOOL VDPMI_Detect()
{
    BOOL result = VDPMI_GetVendorEntry();
    result =  result && (VDPMI_Entry.offset32 || VDPMI_Entry.sel);

    if(result)
    {
        //patch VDPMI_TrapHandlerWrapper
        uint8_t* opcodes = (uint8_t*)&VDPMI_TrapHandlerWrapper;
        //mov eax, 0x4321ABCD
        assert(opcodes[0] == 0xB8);
        assert( *(uint32_t*)&opcodes[1] == 0x4321ABCD );
        //mov eax, imm of DS
        *(uint32_t*)&opcodes[1] = VDPMI_GetDS();
    }

    return result;
}

BOOL VDPMI_Install_IOPortTrap(SBEMU_IODT* inputp iodt, uint16_t count, SBEMU_IOPT* outputp iopt)
{
    if(!iodt || !iopt)
    {
        assert(FALSE);
        return FALSE;
    }

    if(VDPMI_IODT_header.next == NULL)
    {
        assert(VDPMI_Entry.sel);
        //_LOG("VDPMI vendor entry: %04x:%08x\n", VDPMI_Entry.sel, VDPMI_Entry.offset32);
        if(!VDPMI_Internal_InstallTrap(_my_cs(), &VDPMI_TrapHandlerWrapper))
        {
            _LOG("Failed to install VDPMI io port trap.\n");
            return FALSE;
        }

        VDPMI_Stack[0].offset32 = (uintptr_t)malloc(VDPMI_STACKSIZE*VDPMI_STACKCOUNT) + VDPMI_STACKSIZE;
        VDPMI_Stack[0].sel = VDPMI_GetDS();
        for(int i = 1; i < VDPMI_STACKCOUNT; ++i)
        {
            VDPMI_Stack[i].offset32 = VDPMI_Stack[i-1].offset32 + VDPMI_STACKSIZE;
            VDPMI_Stack[i].sel = VDPMI_GetDS();
        }
    }
   
    SBEMU_IODT* Iodt = (SBEMU_IODT*)malloc(sizeof(SBEMU_IODT)*count);
    memcpy(Iodt, iodt, sizeof(SBEMU_IODT)*count);
    BOOL* states = malloc(sizeof(BOOL)*count);

    SBEMU_IODT_LINK* newlink = (SBEMU_IODT_LINK*)malloc(sizeof(SBEMU_IODT_LINK));
    newlink->iodt = Iodt;
    newlink->states = states;
    newlink->count = count;
    newlink->prev = VDPMI_IODT_Link;
    newlink->next = NULL;
    VDPMI_IODT_Link->next = newlink;
    VDPMI_IODT_Link = newlink;
    iopt->memory = (uintptr_t)newlink;

    for(uint16_t i = 0; i < count; ++i)
    {
        _LOG("port: %x, h=%x\n", iodt[i].port, iodt[i].handler);
        VDPMI_IOTrapTable[iodt[i].port] = iodt[i].handler;
        BOOL result = VDPMI_GetPortTrap(iodt[i].port, states++);
        assert(result);
        result = VDPMI_SetPortTrap(iodt[i].port, TRUE);
        assert(result);
    }
    return TRUE;
}

BOOL VDPMI_Uninstall_IOPortTrap(SBEMU_IOPT* inputp iopt)
{
    CLIS();
    SBEMU_IODT_LINK* link = (SBEMU_IODT_LINK*)iopt->memory;
    link->prev->next = link->next;
    if(link->next) link->next->prev = link->prev;
    if(VDPMI_IODT_Link == link)
        VDPMI_IODT_Link = link->prev;
    STIL();
    for(int i = 0; i < link->count; ++i)
    {
        VDPMI_IOTrapTable[link->iodt[i].port] = NULL;
        VDPMI_SetPortTrap(link->iodt[i].port, link->states[i]);
    }
    free(link->iodt);
    free(link->states);
    free(link);
    
    if(VDPMI_IODT_header.next == NULL)
    {
        //uninstall trap handler: restore to old
        BOOL result = VDPMI_Internal_InstallTrap(VDPMI_OldHandler.sel, (void(*)(void))VDPMI_OldHandler.offset32);
        if(!result)
            _LOG("Failed to uninstall VDPMI io port trap.\n");
        free((void*)(VDPMI_Stack[0].offset32-VDPMI_STACKSIZE));
    }
    return TRUE;
}

void VDPMI_UntrappedIO_Write(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
    "movl $0x05, %%eax \n\t" //function no.
    "movzxw %1, %%edx \n\t"     //edx=port
    "movl $0x1, %%ecx \n\t"     //ecx=out
    "movzxb %2, %%ebx \n\t"     //ebx=outval
    "lcall *%0\n\t"
    :
    :"m"(VDPMI_Entry),"d"(port),"b"(value)
    :"eax", "ecx"
    );
}

uint8_t VDPMI_UntrappedIO_Read(uint16_t port)
{
    uint32_t result = 0;
    __asm__ __volatile__(
    "movl $0x05, %%eax \n\t" //function no.
    "movzxw %2, %%edx \n\t"     //edx=port
    "xor %%ecx, %%ecx \n\t"   //ecx=out
    "lcall *%1\n\t"
    //"mov %%eax, %0 \n\t"
    :"=a"(result)
    :"m"(VDPMI_Entry),"d"(port)
    :"ecx"
    );
    return result;
}

#if !VDPMI_IOPL
void UntrappedIO_OUT(uint16_t port, uint8_t value) __attribute__((alias("VDPMI_UntrappedIO_Write")));
uint8_t UntrappedIO_IN(uint16_t port) __attribute__((alias("VDPMI_UntrappedIO_Read")));
#endif

BOOL VDPMI_InstallISR(uint8_t irq, void(*irqhandler)(), DPMI_ISR_HANDLE* handle)
{
    _go32_dpmi_seginfo go32pa;
    go32pa.pm_selector = (uint16_t)_my_cs();
    go32pa.pm_offset = (uintptr_t)irqhandler;
    if( _go32_dpmi_allocate_iret_wrapper(&go32pa) != 0)
        return FALSE;
    BOOL result = VDPMI_InstallDriver(irq, (void(*)())go32pa.pm_offset);
    if(!result)
        _go32_dpmi_free_iret_wrapper(&go32pa);
    
    handle->n = irq;
    handle->wrapper_cs = go32pa.pm_selector;
    handle->wrapper_offset = go32pa.pm_offset;
    return result;
}

BOOL VDPMI_UninstallISR(DPMI_ISR_HANDLE* handle)
{
    BOOL result = VDPMI_InstallDriver(handle->n, NULL);
    if(result)
    {
        _go32_dpmi_seginfo go32pa;
        go32pa.pm_selector = handle->wrapper_cs;
        go32pa.pm_offset = handle->wrapper_offset;
        _go32_dpmi_free_iret_wrapper(&go32pa);
    }
    return result;
}

BOOL VDPMI_RaiseIRQ(uint8_t irq)
{
    BOOL result = 0;
    __asm__ __volatile__(
    "movl $0x07, %%eax \n\t" //function no.
    "movzxb %2, %%ecx \n\t"   //ecx=irq
    "lcall *%1 \n\t"
    "jnc 1f \n\t"
    "movl $0x0, %0 \n\t"
    "jmp 2f \n\t"
    "1: movl $0x1, %0 \n\t"
    "2: \n\t"
    :"=a"(result)
    :"m"(VDPMI_Entry),"c"(irq)
    :
    );
    return result;
}
