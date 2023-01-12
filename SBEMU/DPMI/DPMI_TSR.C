#include "DPMI.H"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

//the purpose of this file is to move the program to a known physical addr so
//that external (i.e.EMM386 ring0 callback) code can access it.
//if TSR is only for INT handler or from real mode call, use DPMI callback or DPMI interrupt vector will work without this.

#if defined(__DJ2__)
#include <sys/stat.h>
#include <sys/segments.h>
#include <sys/farptr.h>
#include <sys/exceptn.h>
#include <dpmi.h>
#include <fcntl.h>
#include <unistd.h>
#include <crt0.h>
#include <file.h>
#include <go32.h>
#include <stubinfo.h>
#include "coff.h"
#include "DBGUTIL.H"

#pragma GCC diagnostic ignored "-Wpedantic"
#define NEW_BASE_ALIGNMENT (64*1024UL)
//#define NEW_BASE_ALIGNMENT (4*1024)

static uint32_t ProgramOffset = 0xFFFFFFFF;//total offset
static uint32_t SectionOffset[3];   //text,data,bss
static uint32_t SectionSize[3];     //text,data,bss
static uint32_t ProgramSize = 0;
static uint16_t ProgramOldCS;
static uint16_t ProgramOldDS;
static uint16_t ProgramNewCS;
static uint16_t ProgramNewDS;

extern uint16_t __djgpp_ds_alias;
extern uint32_t __djgpp_base_address;

static uint16_t __djgpp_ds_alias_old;
static uint32_t __djgpp_base_address_old;

inline uint16_t _my_es()
{
    uint16_t es;
    asm("mov %%es, %0" :"=r"(es));
    return es;
}

//copy self to new linear base (relloc)
//poffset: start offset of code/data with current segment base addr
//psize: total size starting at current segment base addr
BOOL DPMI_InitTSR(uint32_t base, uint32_t newbase, uint32_t* outputp poffset, uint32_t* outputp psize)
{
    if(poffset == NULL || psize == NULL)
        return 0;
    if(*poffset == 0 || *psize == 0)
    {
        int fd = open(__dos_argv0, O_RDONLY|O_BINARY, S_IRUSR);
        BOOL ok = fd >= 0;
        uint16_t MZ6[3];    //0:
        if(ok)
            ok = read(fd, MZ6, 6) == 6;
        if(ok)
        {
            if(MZ6[0] == 0x5A4D)    //MZ header, stubbed
                ok = lseek(fd, MZ6[2]*512+MZ6[1],SEEK_SET) != -1;
            else if(MZ6[0] == I386MAGIC)
                ok = lseek(fd, 0, SEEK_SET) != -1;
        }
        struct external_filehdr hdr;
        if(ok)
            ok = read(fd, &hdr, FILHSZ) == FILHSZ && hdr.f_magic == I386MAGIC;
        if(ok)
            ok = lseek(fd, hdr.f_opthdr, SEEK_CUR) != -1;
        if(ok)
        {
            for(int i = 0; i < hdr.f_nscns; ++i)
            {
                struct external_scnhdr sechdr;
                ok = read(fd, &sechdr, sizeof(sechdr)) == sizeof(sechdr);
                if(!ok)
                    break;
                //printf("%5s: %08lx, %08lx\n", sechdr.s_name, sechdr.s_vaddr, sechdr.s_size);
                BOOL text = memcmp(sechdr.s_name, _TEXT,6) == 0;
                BOOL data = memcmp(sechdr.s_name, _DATA,6) == 0;
                BOOL bss = memcmp(sechdr.s_name, _BSS,5) == 0;
                if( text || data || bss) //should be consecutive by COFF spec
                {
                    ProgramOffset = min(ProgramOffset, sechdr.s_vaddr);
                    ProgramSize += sechdr.s_size;
                    int index = text ? 0 : (data ? 1 : 2);
                    SectionOffset[index] = sechdr.s_vaddr;
                    SectionSize[index] = sechdr.s_size;
                }                
            }
        }
        if(!ok || ProgramOffset == 0xFFFFFFFF || ProgramSize == 0)
        {
            printf("failed reading file.\n");
            return FALSE;
        }

        void (*volatile pfree)(void*) = free; //prevent optimization
        void*(*volatile pmalloc)(size_t) = malloc;
        // |CODE|DATA|BSS|STACK|C HEAP|XMS_HEAP| (all fixed size)
        pfree(pmalloc(NEW_BASE_ALIGNMENT));   //preserve space for C runtime heap
        /*uintptr_t end0 = (uintptr_t)sbrk(0);
        //printf("%08lx\n",_go32_info_block.size_of_transfer_buffer);
        if(!stdout->_base)  //printf hack
        {
            stdout->_bufsiz = _go32_info_block.size_of_transfer_buffer;
            stdout->_base = malloc(stdout->_bufsiz);
            stdout->_flag |= _IOLBF|0x40;//_IOMYBUF;
            stdout->_cnt = 0;
            stdout->_ptr = stdout->_base;
            //printf("pre alloc stdout buffer: %08lx, %08lx\n", end0, sbrk(0));
        }*/
        uintptr_t end = (uintptr_t)sbrk(0);
        ProgramSize = (uintptr_t)max(end - ProgramOffset, ProgramSize);

        *poffset = ProgramOffset;
        *psize = align(ProgramOffset+ProgramSize, NEW_BASE_ALIGNMENT);
        //sbrk(*psize - end);
        //assert((uintptr_t)sbrk(0) == *psize);
        if(*psize > (uintptr_t)sbrk(0) + 0x100)
            pfree(pmalloc(*psize - (uintptr_t)sbrk(0) - 0x100));
        return FALSE;
    }
    *psize = align(*psize, NEW_BASE_ALIGNMENT);
    
    uint32_t offset = *poffset;
    uint32_t size = *psize;
    assert(offset == ProgramOffset);
    
    //TODO: shrink stack as we may not need too much space?
    //TODO: custom keep of code & data like DOS TSR. we can do the TSR init on exit (we still need physical addr for driver before TSR)
    //printf("min stack: %08lx, min keep: %08lx\n", _stubinfo->minstack, _stubinfo->minkeep);
    //printf("%08lx, %08lx: %08lx\n", base, ProgramOffset, ProgramSize);
    
    ProgramNewCS = (uint16_t)__dpmi_create_alias_descriptor(_my_cs());
    //int16_t ProgramNewCS = __dpmi_allocate_ldt_descriptors(1);
    int32_t ar = __dpmi_get_descriptor_access_rights(ProgramNewCS);
    ar |= 0x08;  //code: 0x08
    __dpmi_set_descriptor_access_rights(ProgramNewCS,ar);
    __dpmi_set_segment_limit(ProgramNewCS, size-1);
    __dpmi_set_segment_base_address(ProgramNewCS, newbase);
    
    ProgramNewDS = (uint16_t)__dpmi_create_alias_descriptor(_my_ds());
    __dpmi_set_segment_limit(ProgramNewDS, size-1);
    __dpmi_set_segment_base_address(ProgramNewDS, newbase);

    volatile uint32_t PreventOptimize;//prevent optimization out code after ljmp
    asm __volatile__("xor %0, %0":"=r"(PreventOptimize) : :"memory");
    if(PreventOptimize)
        goto switch_space;

    assert(ProgramOffset+ProgramSize <= size);

    asm __volatile__(   //save data before switch.
    "pushw %%ds\n\t popw %0\n\t"
    "pushw %%cs\n\t popw %1\n\t"
    :"=m"(ProgramOldDS),"=m"(ProgramOldCS)
    ::"memory");

    //jump buffer
    uint32_t fjmp[2] = {(uintptr_t)&&switch_space, ProgramNewCS};
    //int eip;
    //asm("movl $., %0" :: "r"(eip) :"memory");
    //printf("%04x:%08lx, %08lx, %08lx, %08lx\n", fjmp[1], fjmp[0], newbase, size-1,eip);
    //printf("src: %08lx, dest : %08lx, size: %08lx\n", base + ProgramOffset, newbase + ProgramOffset, ProgramSize);
    DPMI_CopyLinear(newbase + ProgramOffset, base + ProgramOffset, ProgramSize);    //after local vars inited
    //TODO: we could probably use physical remap instead of jump.
    asm("ljmp *%0" :: "m"(fjmp));
switch_space:
    asm __volatile__(
    "pushw %0\n\t popw %%ds\n\t"
    "pushw %0\n\t popw %%es\n\t"
    "pushw %0\n\t popw %%ss\n\t"
    ::"m"(ProgramNewDS)
    :"memory");
    //*(int*)0=0;
    __djgpp_ds_alias_old = __djgpp_ds_alias;
    __djgpp_base_address_old = __djgpp_base_address;
    __djgpp_ds_alias = ProgramNewDS;
    __djgpp_base_address = newbase;
    //make address persistent
    _crt0_startup_flags = (_crt0_startup_flags&~_CRT0_FLAG_UNIX_SBRK) | _CRT0_FLAG_NONMOVE_SBRK;
    return TRUE;
}

//enable normal exit, if fails out before TSR
BOOL DPMI_ShutdownTSR(void)
{
    volatile uint32_t PreventOptimize;//prevent optimization out code after ljmp
    asm __volatile__("xor %0, %0":"=r"(PreventOptimize) : :"memory");
    if(PreventOptimize)
        goto switch_back;
    uint32_t fjmp[2] = {(uintptr_t)&&switch_back, ProgramOldCS};
    
    uintptr_t oldend = ProgramSize + ProgramOffset;
    uintptr_t end = (uintptr_t)sbrk(0);
    //assert(oldend <= end);
    if(end > oldend)
    {
        /*asm __volatile__("pushw %0\n\t popw %%ds\n\t" ::"m"(ProgramOldDS));
        sbrk(end-oldend);
        asm __volatile__("pushw %0\n\t popw %%ds\n\t": :"m"(ProgramNewDS));*/
        //ProgramSize = end - ProgramOffset;
    }

    __djgpp_exception_toggle();
    int x = __dpmi_get_and_disable_virtual_interrupt_state();
    //DPMI_CopyLinear(__djgpp_base_address_old + ProgramOffset, __djgpp_base_address + ProgramOffset, ProgramSize);
    uint32_t stack = SectionOffset[2] + SectionSize[2];
    DPMI_CopyLinear(__djgpp_base_address_old + stack, __djgpp_base_address + stack, ProgramSize-stack);
    __dpmi_get_and_set_virtual_interrupt_state(x);
    __djgpp_exception_toggle();

    asm("ljmp *%0" :: "m"(fjmp));
switch_back:
    asm __volatile__(   //if we don't copy data back, we must restore data segs after jump(cs seg)
    "pushw %0\n\t popw %%es\n\t"
    "pushw %0\n\t popw %%ss\n\t"
    "pushw %0\n\t popw %%ds\n\t"
    :
    :"m"(ProgramOldDS)
    :"memory");
    /*
    __djgpp_ds_alias = __djgpp_ds_alias_old;
    __djgpp_base_address = __djgpp_base_address_old;
    __djgpp_ds_alias_old = 0;
    __djgpp_base_address_old = 0;
    */
    __dpmi_free_ldt_descriptor(ProgramNewCS);
    __dpmi_free_ldt_descriptor(ProgramNewDS);
    ProgramNewCS = ProgramNewDS = 0;
    //*(int*)0=0;
    return TRUE;
}

BOOL DPMI_TSR()
{
#if DPMI_USE_XMS_HEAP
    __djgpp_exception_toggle();
    //free all old app memory
    for(int i = 1; __djgpp_memory_handle_list[i].address; ++i)
    {
        __djgpp_sbrk_handle* h = &__djgpp_memory_handle_list[i];
        _LOG("memory: %08lx, %08lx\n", h->address, __djgpp_base_address_old);
        //if large block malloced after InitTSR (_CRT0_FLAG_NONMOVE_SBRK), it will apear here. don't free them
        if(h->address == __djgpp_base_address_old)
        {
            _LOG("Free memory: %d\n", h->address, __djgpp_memory_handle_size[i]);
            __dpmi_free_memory((unsigned)h->handle);
            h->handle = 0;
            h->address = 0;
        }
    }
    _LOG("Free stub memory %08lx, addr: %08lx size: %08lx\n", __djgpp_memory_handle_list[0].handle, __djgpp_memory_handle_list[0].address, __djgpp_memory_handle_size[0]);
    __dpmi_free_memory((unsigned)__djgpp_memory_handle_list[0].handle); //free stub memory
    __djgpp_memory_handle_list[0].handle = 0;
    __djgpp_memory_handle_list[0].address = 0;

    __dpmi_free_ldt_descriptor(ProgramOldCS);
    __dpmi_free_ldt_descriptor(ProgramOldDS);
    __dpmi_free_ldt_descriptor(__djgpp_ds_alias_old);

    ProgramOldCS = 0;
    ProgramOldDS = 0;
    __djgpp_ds_alias_old = 0;

    __dpmi_meminfo mem;
    mem.address = __djgpp_base_address;
    mem.size = ProgramSize;
    if( __dpmi_lock_linear_region(&mem) != 0)
        return FALSE;
#endif

    DPMI_REG r = {0};

    _LOG("Transfer buffer: %08lx, PSP: %08lx, transfer buffer size: %08lx\n", _go32_info_block.linear_address_of_transfer_buffer,
        _go32_info_block.linear_address_of_original_psp, _go32_info_block.size_of_transfer_buffer);
        
    r.w.dx = (uint16_t)((_go32_info_block.linear_address_of_transfer_buffer
        - _go32_info_block.linear_address_of_original_psp
        + _go32_info_block.size_of_transfer_buffer) >> 4);

    r.w.dx= 256>>4; //only psp
    _LOG("TSR size: %d\n", r.w.dx<<4);
    r.w.ax = 0x3100;
    return DPMI_CallRealModeINT(0x21, &r) == 0; //won't return on success
}

#endif//__DJ2__



#if defined(__BC__)


#endif//__BC__