#include "dpmi.h"
#if defined(__DJ2__)
#include <conio.h>
#include <stdlib.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <sys/segments.h>
#include <sys/exceptn.h>
#include <crt0.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include "xms.h"
#include "dbgutil.h"
#include "../pic.h"

extern DPMI_ADDRESSING DPMI_Addressing;

int _crt0_startup_flags = /*_CRT0_FLAG_FILL_DEADBEEF | _CRT0_FLAG_UNIX_SBRK | */_CRT0_FLAG_LOCK_MEMORY;

static uint32_t DPMI_DSBase = 0;
static uint32_t DPMI_DSLimit = 0;
static BOOL DPMI_TSR_Inited = 0;
static uint16_t DPMI_Selector4G;

typedef struct _AddressMap
{
    uint32_t Handle;
    uint32_t LinearAddr;
    uint32_t PhysicalAddr;
    uint32_t Size;
}AddressMap;

#define ADDRMAP_TABLE_SIZE (1024 / sizeof(AddressMap))

static AddressMap AddresMapTable[ADDRMAP_TABLE_SIZE];

static void AddAddressMap(const __dpmi_meminfo* info, uint32_t PhysicalAddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        //DBG_Logi("aa %d %x %x %x %x\n", i, AddresMapTable[i].LinearAddr, AddresMapTable[i].Size, info->address, info->size);
        if(AddresMapTable[i].Size == 0)
        {
            AddressMap* map = &AddresMapTable[i];
            map->Handle = info->handle;
            map->LinearAddr = info->address;
            map->PhysicalAddr = PhysicalAddr;
            map->Size = info->size;
            break;
        }
    }
}

static int FindAddressMap(uint32_t linearaddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        //DBG_Logi("fa %d %x %x\n", i, AddresMapTable[i].LinearAddr, linearaddr);
        if(AddresMapTable[i].LinearAddr == linearaddr)
            return i;
    }
    return -1;
}

static void DPMI_Shutdown(void);

#define NEW_IMPL 1
#if NEW_IMPL
extern uint32_t DPMI_InitTSR(uint32_t base, uint32_t newbase, uint32_t* poffset, uint32_t* psize);
extern BOOL DPMI_ShutdownTSR();
static uint32_t XMS_Bias;
#else
static __dpmi_meminfo XMS_Info;
#endif
#define ONLY_MSPACES 1
#define NO_MALLOC_STATS 1
#define USE_LOCKS 1
#define LACKS_SCHED_H 1
#define HAVE_MMAP 0
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma push_macro("DEBUG")
#undef DEBUG
#define DEBUG 0
#include "dlmalloc.h"
#pragma pop_macro("DEBUG")

#define XMS_HEAP_SIZE (1024*1024*4)  //maxium around 64M (actual size less than 64M)
_Static_assert((uint16_t)(XMS_HEAP_SIZE/1024) == (uint32_t)(XMS_HEAP_SIZE/1024), "XMS_HEAP_SIZE must be < 64M");
static mspace XMS_Space;
static uint32_t XMS_Physical;
#if DEBUG
static uint32_t XMS_Allocated;
#endif
static uint16_t XMS_Handle; //handle to XMS API

//http://www.delorie.com/djgpp/v2faq/faq18_13.html choice 3
//choice 1 (DOS alloc) is not suitable here since we need near ptr but DJGPP's DS base is larger than that, (we may need __djgpp_nearptr_enable)
//to do that. for most DOS Extenders like DOS/4G is OK to use choice 1 since there DS have a base of 0, thus DOS memory are near ptr.
static void Init_XMS()
{
    #if NEW_IMPL
    //use XMS memory since we need know the physical base for some non-DPMI DOS handlers, or drivers.
    //it is totally physically mapped, and doesn't support realloc, making malloc/brk/sbrk not working.
    //it will work if djgpp support srbk hook. stdout buffer will be pre-alloced for msg output & debug use.
    //doesn't matter, this is a driver anyway.
    uint32_t size = 0;
    uint32_t offset = 0;
    DPMI_InitTSR(0, 0, &offset, &size);

    //notify runtime of brk change
    sbrk(XMS_HEAP_SIZE);
    __dpmi_get_segment_base_address(_my_ds(), &DPMI_DSBase); //_CRT0_FLAG_UNIX_SBRK may have re-allocation

    assert((uint16_t)((size+XMS_HEAP_SIZE)/1024) == (uint32_t)((size+XMS_HEAP_SIZE)/1024)); //check overflow
    XMS_Handle = XMS_Alloc((uint16_t)((size+XMS_HEAP_SIZE)/1024), &XMS_Physical);
    if(XMS_Handle == 0)
        exit(1);

    XMS_Bias = offset >= 4096 ? 4096 : 0; //re-enable null pointer page fault
    uint32_t XMSBase = DPMI_MapMemory(XMS_Physical, size + XMS_HEAP_SIZE);
    DPMI_TSR_Inited = DPMI_InitTSR(DPMI_DSBase, XMSBase - XMS_Bias, &offset, &size);
    _LOG("TSR inited.\n");
    int ds; //reload ds incase this function inlined and ds optimized as previous
    asm __volatile__("mov %%ds, %0":"=r"(ds)::"memory");
    //_LOG("ds: %02x, limit: %08lx, new limit: %08lx\n", ds, __dpmi_get_segment_limit(ds), size-1);
    assert(__dpmi_get_segment_limit(ds) == size-1);
    __dpmi_set_segment_limit(ds, size + XMS_HEAP_SIZE - 1);
    XMS_Space = create_mspace_with_base((void*)size, XMS_HEAP_SIZE, 0);
    _LOG("XMS init done.\n");
    //update mapping
    DPMI_DSBase = XMSBase - XMS_Bias;
    DPMI_DSLimit = size + XMS_HEAP_SIZE;
    _LOG("XMS base %08lx, XMS lbase %08lx XMS heap base %08lx\n", XMS_Physical, XMSBase, XMS_Physical+size);
    #else
    //the idea is to allocate XMS memory in physical addr and mapped it after current ds's limit region,
    //then expand current ds' limit so that the mapped addr are within the current ds segment,
    //and the mapped data can be directly accessed as normal pointer (near ptr)
    //another trick is to use dlmalloc with mapped based ptr to allocate arbitary memory.
    XMS_Handle = XMS_Alloc((XMS_HEAP_SIZE+4096)/1024, &XMS_Physical);
    if(XMS_Handle == 0)
        exit(1);
    XMS_Physical = align(XMS_Physical, 4096);
    __dpmi_meminfo info = {0};
    info.size = XMS_HEAP_SIZE;
    #if 0     //Not supported by CWSDPMI and Windows, but by DPMIONE or HDPMI
    info.address = (DPMI_DSBase + DPMI_DSLimit + 4095) / 4096 * 4096;
    if( __dpmi_allocate_linear_memory(&info, 0) == -1)
    #else
    if(__dpmi_allocate_memory(&info) == -1)
    #endif
    {
        XMS_Free(XMS_Handle);
        printf("Failed to allocate linear memory (%08lx). \n", info.address);
        exit(1);
    }

    __dpmi_meminfo info2 = info;
    info2.address = 0;
    info2.size = XMS_HEAP_SIZE / 4096;
    if( __dpmi_map_device_in_memory_block(&info2, XMS_Physical) == -1) //supported by CWSDPMI and others
    {
        XMS_Free(XMS_Handle);
        printf("Error: Failed to map XMS memory %08lx, %08lx.\n", info.address, info.size);
        exit(1);
    }
    uint32_t XMSBase = info.address;
    XMS_Info = info;
    info.handle = -1;
    AddAddressMap(&info, XMS_Physical);
    __dpmi_set_segment_limit(_my_ds(), XMSBase - DPMI_DSBase + XMS_HEAP_SIZE - 1);
    __dpmi_set_segment_limit(__djgpp_ds_alias, XMSBase - DPMI_DSBase + XMS_HEAP_SIZE - 1); //interrupt used.
    _LOG("XMS base %08lx, XMS lbase %08lx offset %08lx\n", XMS_Physical, XMSBase, XMSBase - DPMI_DSBase);
    XMS_Space = create_mspace_with_base((void*)(XMSBase - DPMI_DSBase), XMS_HEAP_SIZE, 0);
    #endif
}

static void sig_handler(int signal)
{
    _LOG("SIGNAL: %x\n", signal);
    exit(-1);   //perform DPMI clean up on atexit
}

static void DPMI_InitFlat()
{
    DPMI_Selector4G = (uint16_t)__dpmi_allocate_ldt_descriptors(1);
    __dpmi_set_segment_base_address(DPMI_Selector4G, 0);
    __dpmi_set_segment_limit(DPMI_Selector4G, 0xFFFFFFFF);
    DPMI_Addressing.selector = DPMI_Selector4G;
    DPMI_Addressing.physical = FALSE;

    __dpmi_get_segment_base_address(_my_ds(), &DPMI_DSBase);
    DPMI_DSLimit = __dpmi_get_segment_limit(_my_ds());
}

void DPMI_Init(void)
{
    atexit(&DPMI_Shutdown);
    //signal(SIGINT, sig_handler);
    signal(SIGABRT, sig_handler);

    DPMI_InitFlat();
#if DPMI_USE_XMS_HEAP
    Init_XMS();
#endif

    __dpmi_meminfo info;    //1:1 map DOS memory. (0~640K). TODO: get 640K~1M mapping from VCPI
    info.handle = -1;
    info.address = 1024;    //skip IVT and expose NULL ptr
    info.size = 640L*1024L - 1024;
    AddAddressMap(&info, 1024);

    /*
    int32_t* ptr = (int32_t*)DPMI_DMAMalloc(256,16);
    *ptr = 0xDEADBEEF;
    int32_t addr = DPMI_PTR2L(ptr);
    int32_t val = DPMI_LoadD(addr);
    printf("%08lx:%08lx\n",addr, val);

    int32_t addr2 = DPMI_MapMemory(DPMI_L2P(addr), 256);
    printf("%08lx:%08lx", addr2, DPMI_LoadD(addr2));
    exit(0);
    */
}

static void DPMI_Shutdown(void)
{
#if DPMI_USE_XMS_HEAP
    #if NEW_IMPL
    _LOG("Cleanup TSR...\n");
    uint32_t size = mspace_mallinfo(XMS_Space).uordblks;
    unused(size); //make compiler happy. log not always enabled.
    #if DEBUG
    size = XMS_Allocated;
    #endif
    _LOG("XMS heap allocated: %d\n", size);
    if(DPMI_TSR_Inited)
    {
        DPMI_ShutdownTSR();
        asm __volatile__("":::"memory");
        DPMI_TSR_Inited = FALSE;
    }
    #else
    //libc may expand this limit, if we restore it to a smaller value, it may cause crash
    //__dpmi_set_segment_limit(_my_ds(), DPMI_DSLimit);
    _LOG("Free mapped XMS space...\n");
    if(XMS_Info.handle != 0)
    {
        __dpmi_free_memory(XMS_Info.handle);
        XMS_Info.handle = 0;
    }
    #endif

    _LOG("Free XMS memory...\n");
    if(XMS_Handle != 0)
    {
        XMS_Free(XMS_Handle);
        XMS_Handle = 0;
    }
#endif

    _LOG("Free mapped space...\n");
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Size)
            continue;
        if(map->Handle == ~0UL)//XMS mapped
            continue;
        __dpmi_meminfo info;
        info.handle = map->Handle;
        info.address = map->LinearAddr;
        info.size = map->Size;
        __dpmi_free_physical_address_mapping(&info);
    }
    _LOG("DPMI_Shutdown done.\n");
}

uint32_t DPMI_L2P(uint32_t vaddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Size)
            continue;
        if(map->LinearAddr <= vaddr && vaddr <= map->LinearAddr + map->Size)
        {
            int32_t offset = vaddr - map->LinearAddr;
            return map->PhysicalAddr + offset;
        }
    }
    printf("Error mapping linear address to physical: %08lx (%08lx,%08lx).\n", vaddr, DPMI_DSBase, DPMI_DSBase+DPMI_DSLimit);
    printf("Exit\n");
    exit(1);
    return 0; //make compiler happy
}

uint32_t DPMI_P2L(uint32_t paddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Size)
            continue;
        if(map->PhysicalAddr <= paddr && paddr <= map->PhysicalAddr + map->Size)
        {
            int32_t offset = paddr - map->PhysicalAddr;
            return map->LinearAddr + offset;
        }
    }
    printf("Error mapping physical address to linear: %08lx.\n", paddr);
    assert(FALSE);
    exit(1);
    return 0; //make compiler happy
}

uint32_t DPMI_PTR2L(void* ptr)
{
    return ptr ? DPMI_DSBase + (uint32_t)ptr : 0;
}

void* DPMI_L2PTR(uint32_t addr)
{
    return addr > DPMI_DSBase ? (void*)(addr - DPMI_DSBase) : NULL;
}


uint32_t DPMI_MapMemory(uint32_t physicaladdr, uint32_t size)
{
    if(size == 0)
    {
        assert(FALSE);
        return 0;
    }

    __dpmi_meminfo info;
    info.handle = 0;
    info.address = physicaladdr;
    info.size = size;
    if( __dpmi_physical_address_mapping(&info) != -1)
    {
        AddAddressMap(&info, physicaladdr);
        return info.address;
    }
    //assert(FALSE);
    //exit(-1);
    return 0;
}

BOOL DPMI_UnmappMemory(uint32_t mappedaddr)
{
    int index = FindAddressMap(mappedaddr);
    if(index == -1)
        return FALSE;
    AddressMap* map = &AddresMapTable[index];
    __dpmi_meminfo info;
    info.handle = map->Handle;
    info.address = map->LinearAddr;
    info.size = map->Size;
    __dpmi_free_physical_address_mapping(&info);
    memset(map, 0, sizeof(*map));
    return TRUE;
}

void* DPMI_DMAMalloc(unsigned int size, unsigned int alignment/* = 4*/)
{
    #if DEBUG
    CLIS();
    XMS_Allocated += size;
    uint8_t* ptr = (uint8_t*)mspace_malloc(XMS_Space, size+alignment+8) + 8;
    uintptr_t addr = (uintptr_t)ptr;
    uint32_t offset = align(addr, alignment) - addr;
    uint32_t* uptr = (uint32_t*)(ptr + offset);
    uptr[-1] = size;
    uptr[-2] = offset + 8;
    STIL();
    assert(align((uintptr_t)uptr, alignment) == (uintptr_t)uptr);
    return uptr;
    #else
    return mspace_memalign(XMS_Space, alignment, size);
    #endif
}

void DPMI_DMAFree(void* ptr)
{
    #if DEBUG
    uint32_t* uptr = (uint32_t*)ptr;
    XMS_Allocated -= uptr[-1];
    mspace_free(XMS_Space, (uint8_t*)ptr - uptr[-2]);
    #else
    mspace_free(XMS_Space, ptr);
    #endif
}

uint32_t DPMI_DOSMalloc(uint16_t size)
{
    int selector = 0;
    uint16_t segment = (uint16_t)__dpmi_allocate_dos_memory(size, &selector);
    if(segment != -1)
        return (selector << 16) | segment;
    else
        return 0;
}

void DPMI_DOSFree(uint32_t segment)
{
    __dpmi_free_dos_memory((uint16_t)(segment>>16));
}

uint16_t DPMI_CallRealModeRETF(DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_retf((__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeINT(uint8_t i, DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_interrupt(i, (__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeIRET(DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_iret((__dpmi_regs*)reg);
}

#define RAW_HOOK 1
uint16_t DPMI_InstallISR(uint8_t i, void(*ISR)(void), DPMI_ISR_HANDLE* outputp handle, BOOL chained)
{
    if(i < 0 || i > 255 || handle == NULL || ISR == NULL)
        return -1;
        
    _go32_dpmi_seginfo go32pa;
    go32pa.pm_selector = (uint16_t)_my_cs();
    go32pa.pm_offset = (uintptr_t)ISR;
    //_go32_interrupt_stack_size = 2048; //512 minimal, default 16K
    if(!chained)
    {
        if( _go32_dpmi_allocate_iret_wrapper(&go32pa) != 0)
            return -1;
    }

    _go32_dpmi_seginfo go32pa_rm = {0};
    __dpmi_raddr ra;
#if RAW_HOOK
    CLIS();
    ra.offset16 = DPMI_LoadW(i*4);
    ra.segment = DPMI_LoadW(i*4+2);
    STIL();
#else
    __dpmi_get_real_mode_interrupt_vector(i, &ra);
#endif
    __dpmi_paddr pa;
    __dpmi_get_protected_mode_interrupt_vector(i, &pa);

    handle->old_offset = pa.offset32;
    handle->old_cs = pa.selector;
    handle->old_rm_cs = ra.segment;
    handle->old_rm_offset = ra.offset16;
    handle->wrapper_offset = go32pa.pm_offset;
    handle->wrapper_cs = go32pa.pm_selector;
    handle->n = i;
    handle->chained = chained;
    handle->chainedDOSMem = 0;

    memcpy(handle->internal1, &go32pa, sizeof(go32pa));
    memset(handle->internal2, 0, sizeof(handle->internal2));

    int result;
    if(chained)
    {
        result = _go32_dpmi_chain_protected_mode_interrupt_vector(i, &go32pa);
        if(result == 0)
        {
            handle->wrapper_offset = go32pa.pm_offset;
            handle->wrapper_cs = go32pa.pm_selector;
        }
    }
    else
    {
        result = _go32_dpmi_set_protected_mode_interrupt_vector(i, &go32pa);
        if(result != 0)
            _go32_dpmi_free_iret_wrapper(&go32pa);
    }

    if(result != 0)
        memset(handle, 0, sizeof(*handle));

    return result;
}

static void __NAKED DPMI_RMISR_ChainedWrapper()
{
    _ASM_BEGIN16
        _ASM(pushf) //calling iret
        _ASM(call dword ptr cs:[0]) //call target

        _ASM(push ax)

        //now EOI is allowed for the previous called target
        //skip calling next if not the same IRQ pending
        //read pic
        _ASM(xor ah, ah)
        _ASM(mov al, 0x0B) //read isr
        _ASM(out 0x20, al)
        _ASM(in al, 0x20)
        _ASM(test al, 0x04)
        _ASM(jz noslave1)
        _ASM(and al, ~0x04)
        _ASM(mov ah, al)
        _ASM(mov al, 0x0B) //read slave isr
        _ASM(out 0xA0, al)
        _ASM(in al, 0xA0)
        _ASM(xchg ah, al)
    _ASMLBL(noslave1:)
        _ASM(test ax, ax)
        _ASM(jz done) //no pending irq in pic isr, done
        _ASM(bsf ax, ax)
        _ASM(cmp al, byte ptr cs:[10]) //compare with irq
        _ASM(jne done) //different irq pending, done


        _ASM(pushf) //calling iret
        _ASM(call dword ptr cs:[4]) //call chained next


        //unmask IRQ because default entry in IVT may mask the irq out
        _ASM(in al, 0x21)
        _ASM(and al, byte ptr cs:[8])
        _ASM(out 0x21, al)
        _ASM(in al, 0xA1)
        _ASM(and al, byte ptr cs:[9])
        _ASM(out 0xA1, al)

        //read pic
        _ASM(xor ah, ah)
        _ASM(mov al, 0x0B) //read isr
        _ASM(out 0x20, al)
        _ASM(in al, 0x20)
        _ASM(test al, 0x04)
        _ASM(jz noslave)
        _ASM(and al, ~0x04)
        _ASM(mov ah, al)
        _ASM(mov al, 0x0B) //read slave isr
        _ASM(out 0xA0, al)
        _ASM(in al, 0xA0)
        _ASM(xchg ah, al)
    _ASMLBL(noslave:)
        _ASM(test ax, ax)
        _ASM(jz done) //no pending irq in pic isr, done
        _ASM(bsf ax, ax)
        _ASM(cmp al, byte ptr cs:[10]) //compare with irq
        _ASM(jne done) //different irq pending, done

        //same irq pending after the full chain is called, send EOI in case the default entry in IVT doesn't send EOI
        //note if this irq is new coming, send EOI doesn't matter (at least for SBEMU) since it is level triggered.
        _ASM(mov al, 0x20)
        _ASM(cmp byte ptr cs:[10], 8) //irq: 0-7?
        _ASM(jb masterEOI)
        _ASM(out 0xA0, al)
    _ASMLBL(masterEOI:)
        _ASM(out 0x20, al)

    _ASMLBL(done:)
        _ASM(pop ax)
        _ASM(iret)
    _ASM_END16
}
static void __NAKED DPMI_RMISR_ChainedWrapperEnd() {}

//http://www.delorie.com/djgpp/v2faq/faq18_9.html
uint16_t DPMI_InstallRealModeISR(uint8_t i, void(*ISR_RM)(void), DPMI_REG* RMReg, DPMI_ISR_HANDLE* outputp handle, BOOL chained)
{
    if(i < 0 || i > 255 || handle == NULL || ISR_RM == NULL || RMReg == NULL)
        return -1;
        
    _go32_dpmi_seginfo go32pa_rm = {0};
    go32pa_rm.pm_selector = _my_cs();
    go32pa_rm.pm_offset = (uintptr_t)ISR_RM;
    if(_go32_dpmi_allocate_real_mode_callback_iret(&go32pa_rm, (_go32_dpmi_registers*)RMReg) != 0)
        return -1;
    __dpmi_raddr ra;
    #if RAW_HOOK
    {
        CLIS();
        ra.offset16 = DPMI_LoadW(i*4);
        ra.segment = DPMI_LoadW(i*4+2);
        STIL();
    }
    #else
    __dpmi_get_real_mode_interrupt_vector(i, &ra);
    #endif
    __dpmi_paddr pa;
    __dpmi_get_protected_mode_interrupt_vector(i, &pa);

    //there's no go32 chained api for realmode, either we edit gormcb.c
    //or we allocate extra memory and do it on our own
    handle->chainedDOSMem = 0;
    if(chained)
    {
        uint32_t codesize = (uintptr_t)&DPMI_RMISR_ChainedWrapperEnd - (uintptr_t)&DPMI_RMISR_ChainedWrapper;
        const uint32_t extra_size = 11;  //+target + chained next (both real mode far ptr) + irqunmask + irq
        handle->chainedDOSMem = DPMI_HighMalloc((codesize + extra_size + 15)>>4, TRUE);
        if(handle->chainedDOSMem == 0)
        {
            _go32_dpmi_free_real_mode_callback(&go32pa_rm);
            assert(FALSE);
            return -1;
        }
        uint8_t irq = PIC_VEC2IRQ(i);
        uint16_t unmask = 1<<irq;
        if(irq >= 8) unmask |= 0x04; //mark slave cascaded in master
        unmask = ~unmask;

        uint8_t* buf = (uint8_t*)malloc(codesize+extra_size);
        //copy target
        memcpy(buf+0, &go32pa_rm.rm_offset, 2);
        memcpy(buf+2, &go32pa_rm.rm_segment, 2);
        //copy chained next
        memcpy(buf+4, &ra.offset16, 2);
        memcpy(buf+6, &ra.segment, 2);
        //copy irq unmask
        memcpy(buf+8, &unmask, 2);
        //copy irq
        memcpy(buf+10, &irq, 1);
        //copy warpper code
        memcpy(buf+extra_size, &DPMI_RMISR_ChainedWrapper, codesize);
        DPMI_CopyLinear(DPMI_SEGOFF2L(handle->chainedDOSMem, 0), DPMI_PTR2L(buf), codesize+extra_size);
        free(buf);

        go32pa_rm.rm_segment = handle->chainedDOSMem&0xFFFF;
        go32pa_rm.rm_offset = (uint16_t)extra_size;
    }

    int result = -1;
    #if RAW_HOOK
    {
        CLIS();
        DPMI_StoreW(i*4, go32pa_rm.rm_offset);
        DPMI_StoreW(i*4+2, go32pa_rm.rm_segment);
        STIL();
        result = 0;
    }
    #else
    {
        __dpmi_raddr ra2;
        ra2.segment = go32pa_rm.rm_segment;
        ra2.offset16 = go32pa_rm.rm_offset;
        result = __dpmi_set_real_mode_interrupt_vector(i, &ra2);
    }
    #endif

    if(result == 0)
    {
        handle->old_offset = pa.offset32;
        handle->old_cs = pa.selector;
        handle->old_rm_cs = ra.segment;
        handle->old_rm_offset = ra.offset16;
        handle->wrapper_offset = go32pa_rm.rm_offset;
        handle->wrapper_cs = go32pa_rm.rm_segment;
        handle->n = i;
        handle->chained = chained;
        memset(handle->internal1, 0, sizeof(handle->internal1));
        memcpy(handle->internal2, &go32pa_rm, sizeof(go32pa_rm));
    }
    else
    {
        _go32_dpmi_free_real_mode_callback(&go32pa_rm);
        
        if(chained && handle->chainedDOSMem)
            DPMI_HighFree(handle->chainedDOSMem);

        memset(handle, 0, sizeof(*handle));
    }
    return (uint16_t)result;
}

uint16_t DPMI_InstallRealModeISR_Direct(uint8_t i, uint16_t seg, uint16_t off, DPMI_ISR_HANDLE* outputp handle, BOOL rawIVT)
{
    if(i < 0 || i > 255 || handle == NULL)
        return -1;
        
    __dpmi_raddr ra;
    if(rawIVT)
    {
        CLIS();
        ra.offset16 = DPMI_LoadW(i*4);
        ra.segment = DPMI_LoadW(i*4+2);
        STIL();
    }
    else
        __dpmi_get_real_mode_interrupt_vector(i, &ra);
    
    int result = -1;
    if(rawIVT)
    {
        CLIS();
        DPMI_StoreW(i*4, off);
        DPMI_StoreW(i*4+2, seg);
        STIL();
        result = 0;
    }
    else
    {
        __dpmi_raddr ra2;
        ra2.segment = seg;
        ra2.offset16 = off;
        result = __dpmi_set_real_mode_interrupt_vector(i, &ra2);
    }

    if(result == 0)
    {
        handle->old_offset = 0;
        handle->old_cs = 0;
        handle->old_rm_cs = ra.segment;
        handle->old_rm_offset = ra.offset16;
        handle->n = i;
        handle->chained = 0;
        memset(handle->internal1, 0, sizeof(handle->internal1));
        memset(handle->internal2, 0, sizeof(handle->internal2));
        handle->internal1[0] = rawIVT;
    }
    else
        memset(handle, 0, sizeof(*handle));
    return (uint16_t)result;
}

uint16_t DPMI_UninstallRealModeISR_Direct(DPMI_ISR_HANDLE* inputp handle)
{
    BOOL rawIVT = handle->internal1[0];
    if(rawIVT)
    {
        CLIS();
        DPMI_StoreW(handle->n*4, handle->old_rm_offset);
        DPMI_StoreW(handle->n*4+2, handle->old_rm_cs);
        STIL();
        return 0;
    }
    else
    {
        __dpmi_raddr ra2;
        ra2.segment = handle->old_rm_cs;
        ra2.offset16 = handle->old_rm_offset;
        return (uint16_t)__dpmi_set_real_mode_interrupt_vector(handle->n, &ra2);
    }
}

uint16_t DPMI_UninstallISR(DPMI_ISR_HANDLE* inputp handle)
{
     _go32_dpmi_seginfo go32pa;
     go32pa.pm_selector = handle->old_cs;
     go32pa.pm_offset = handle->old_offset;
     int result = _go32_dpmi_set_protected_mode_interrupt_vector(handle->n, &go32pa);

#if RAW_HOOK
    CLIS();
    DPMI_StoreW(handle->n*4, handle->old_rm_offset);
    DPMI_StoreW(handle->n*4+2, handle->old_rm_cs);
    STIL();
#else
    __dpmi_raddr ra;
    ra.segment = handle->old_rm_cs;
    ra.offset16 = handle->old_rm_offset;
    result = __dpmi_set_real_mode_interrupt_vector(handle->n, &ra) | result;
#endif

    memcpy(&go32pa, handle->internal2, sizeof(go32pa));
    if(go32pa.pm_offset)
        result = _go32_dpmi_free_real_mode_callback(&go32pa) | result;

    memcpy(&go32pa, handle->internal1, sizeof(go32pa));

    if(handle->chained) //chained go32 wrapper cannot be freed.
    {
        if(handle->chainedDOSMem)
            DPMI_HighFree(handle->chainedDOSMem);
        return (uint16_t)result;
    }
    else if(go32pa.pm_offset)
        return (uint16_t)(_go32_dpmi_free_iret_wrapper(&go32pa) | result);
}

uint32_t DPMI_CallOldISR(DPMI_ISR_HANDLE* inputp handle)
{
    asm(
        "pushfl \n\t"
        "lcall *%0 \n\t"
        ::"m"(*handle)
    );
    return 0;
}

uint32_t DPMI_CallOldISRWithContext(DPMI_ISR_HANDLE* inputp handle, const DPMI_REG* regs)
{
    DPMI_REG r = *regs; /*make sure regs on the stack, accessed with ebp, not esi/edi/ebx */
    DPMI_ISR_HANDLE h = *handle;
    asm(
    "pushal \n\t pushfl \n\t"
    "push %%ds \n\t push %%es \n\t push %%fs \n\t push %%gs \n\t"
    
    "mov %0, %%eax \n\t mov %1, %%ecx \n\t mov %2, %%edx \n\t mov %3, %%ebx \n\t mov %4, %%esi \n\t mov %5, %%edi \n\t"
    "push %6 \n\t pop %%ds \n\t push %7 \n\t pop %%es \n\t push %8 \n\t pop %%fs \n\t push %9 \n\t pop %%gs \n\t"
    
    "push %12 \n\t"
    "push %11 \n\t"
    //"pushl %10 \n\t andw $0xFCFF, (%%esp) \n\t"
    "pushfl \n\t"
    "mov %13, %%ebp \n\t"
    "lcall *4(%%esp) \n\t"
    "add $8, %%esp \n\t"
    
    "pop %%gs \n\t pop %%fs \n\t pop %%es \n\t pop %%ds \n\t" 
    "popfl \n\t popal \n\t"
    ::"m"(r.d.eax),"m"(r.d.ecx),"m"(r.d.edx),"m"(r.d.ebx),"m"(r.d.esi),"m"(r.d.edi),
    "m"(r.w.ds),"m"(r.w.es),"m"(r.w.fs),"m"(r.w.gs),
    "m"(r.w.flags), "m"(h.old_offset), "m"(h.old_cs), "m"(r.d.ebp)
    );
    return 0;
}

uint32_t DPMI_CallRealModeOldISR(DPMI_ISR_HANDLE* inputp handle, DPMI_REG* regs)
{
    DPMI_REG* r = regs;
    r->w.cs = handle->old_rm_cs;
    r->w.ip = handle->old_rm_offset;
    #if RAW_HOOK
    return DPMI_CallRealModeIRET(r);
    #endif
    return DPMI_CallRealModeINT(handle->n,r);
}

uint32_t DPMI_GetISR(uint8_t i, DPMI_ISR_HANDLE* outputp handle)
{
    memset(handle, 0, sizeof(*handle));
    
    __dpmi_raddr ra;
    #if RAW_HOOK
    CLIS();
    ra.offset16 = DPMI_LoadW(i*4);
    ra.segment = DPMI_LoadW(i*4+2);
    STIL();
    #else
    __dpmi_get_real_mode_interrupt_vector(i, &ra);
    #endif
    __dpmi_paddr pa;
    __dpmi_get_protected_mode_interrupt_vector(i, &pa);

    handle->old_offset = pa.offset32;
    handle->old_cs = pa.selector;
    handle->old_rm_cs = ra.segment;
    handle->old_rm_offset = ra.offset16;
    return 0;
}

uint32_t DPMI_AllocateRMCB_RETF(void(*Fn)(void), DPMI_REG* reg)
{
    _go32_dpmi_seginfo info;
    info.pm_selector = (uint16_t)_my_cs();
    info.pm_offset = (uintptr_t)Fn;
    if(_go32_dpmi_allocate_real_mode_callback_retf(&info, (_go32_dpmi_registers*)reg) == 0)
    {
        return (((uint32_t)info.rm_segment)<<16) | (info.rm_offset);
    }
    else
        return 0;
}

uint32_t DPMI_AllocateRMCB_IRET(void(*Fn)(void), DPMI_REG* reg)
{
    _go32_dpmi_seginfo info;
    info.pm_selector = (uint16_t)_my_cs();
    info.pm_offset = (uintptr_t)Fn;
    if(_go32_dpmi_allocate_real_mode_callback_iret(&info, (_go32_dpmi_registers*)reg) == 0)
    {
        return (((uint32_t)info.rm_segment)<<16) | (info.rm_offset);
    }
    else
        return 0;
}

uint8_t DPMI_DisableInterrupt()
{
    return __dpmi_get_and_disable_virtual_interrupt_state();
}

void DPMI_RestoreInterrupt(uint8_t state)
{
    __dpmi_get_and_set_virtual_interrupt_state(state);
}

void DPMI_GetPhysicalSpace(DPMI_SPACE* outputp spc)
{
    #if NEW_IMPL//doesn't work with old method.
    spc->baseds = XMS_Physical - XMS_Bias;
    spc->limitds = __dpmi_get_segment_limit(_my_ds());
    spc->basecs = spc->baseds;
    spc->limitcs = __dpmi_get_segment_limit(_my_cs());

    extern uint32_t __djgpp_stack_top;
    _LOG("DPMI_GetPhysicalSpace: physical ds base: %08lx, limit: %08lx, esp %08lx\n", spc->baseds, __dpmi_get_segment_limit(_my_ds()), __djgpp_stack_top);
    spc->stackpointer = __djgpp_stack_top;
    #else
    #error not supported. cannot get physical base from DPMI.
    #endif
}

#endif
