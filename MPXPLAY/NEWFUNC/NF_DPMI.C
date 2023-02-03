//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: DPMI callings

#ifdef __DOS__

#if !defined(DJGPP)
#include <i86.h>
//#include <dos.h>
#include "newfunc.h"

long pds_dpmi_segment_to_selector(unsigned int segment)
{
 union REGS regs;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0002;
 regs.w.bx=segment;
 int386(0x31,&regs,&regs);
 if(regs.x.cflag)
  return -1;
 return regs.w.ax;
}

void far *pds_dpmi_getrmvect(unsigned int intno) // real mode vector
{
 union REGS regs;
 long selector;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0200;
 regs.h.bl=intno;
 int386(0x31,&regs,&regs);
 if(regs.x.cflag)
  return NULL;
 selector=pds_dpmi_segment_to_selector(regs.w.cx);
 if(selector<0)
  return NULL;
 return MK_FP(selector,regs.x.edx);
}

void pds_dpmi_setrmvect(unsigned int intno, unsigned int segment,unsigned int offset)
{
 union REGS regs;
 if(!segment && !offset)
  return;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0201;
 regs.h.bl=intno;
 regs.w.cx=segment;
 regs.x.edx=offset;
 int386(0x31,&regs,&regs);
}

void far *pds_dpmi_getexcvect(unsigned int intno) // cpu exception vector
{
 union REGS regs;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x202;
 regs.h.bl=intno;
 int386(0x31,&regs,&regs);
 if(regs.x.cflag)
  return NULL;
 return MK_FP(regs.w.cx, regs.x.edx);
}

void pds_dpmi_setexcvect(unsigned int intno, void far *vect)
{
 union REGS regs;
 if(!vect)
  return;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x203;
 regs.h.bl=intno;
 regs.w.cx=FP_SEG(vect);
 regs.x.edx=FP_OFF(vect);
 int386(0x31,&regs,&regs);
}

void far *pds_dos_getvect(unsigned int intno)       // protected mode vector
{
 union REGPACK regp;
 pds_memset((void *)&regp,0,sizeof(union REGPACK));
 regp.x.eax=0x3500|(intno&0xff);
 intr(0x21,&regp);
 return MK_FP(regp.x.es,regp.x.ebx);
 //return _dos_getvect(intno);
}

void pds_dos_setvect(unsigned int intno, void far *vect)
{
 union REGPACK regp;
 if(!vect)
  return;
 pds_memset((void *)&regp,0,sizeof(union REGPACK));
 regp.x.eax=0x2500|(intno&0xff);
 regp.x.edx=FP_OFF(vect);
 regp.x.ds =FP_SEG(vect);
 intr(0x21,&regp);
 //_dos_setvect(intno,vect);
}

int pds_dpmi_dos_allocmem(dosmem_t *dm,unsigned int size)
{
 union REGS regs;
 if(dm->selector){
  pds_dpmi_dos_freemem(dm);
  dm->selector=0;
 }
 pds_newfunc_regs_clear(&regs);
 regs.x.eax = 0x0100;
 regs.x.ebx = (size+15)>>4;
 regs.x.cflag=1;
 int386(0x31,&regs,&regs);
 if(regs.x.cflag)
  return 0;
 dm->selector=regs.x.edx;
 dm->segment=(unsigned short)regs.x.eax;
 dm->linearptr=(void *)(regs.x.eax<<4);
 return 1;
}

void pds_dpmi_dos_freemem(dosmem_t *dm)
{
 union REGS regs;
 if(dm->selector){
  pds_newfunc_regs_clear(&regs);
  regs.x.eax = 0x0101;
  regs.x.edx = dm->selector;
  int386(0x31, &regs, &regs);
  dm->selector=dm->segment=0;
  dm->linearptr=NULL;
 }
}

void pds_dpmi_realmodeint_call(unsigned int intnum,struct rminfo *rmi)
{
 union REGS regs;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0300;
 regs.w.bx=intnum;
 regs.x.edi=(unsigned long)(rmi);
 int386(0x31,&regs,&regs);
}

unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize)
{
 union REGS regs;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0800;
 regs.w.bx=(phys_addr>>16);
 regs.w.cx=(phys_addr&0xffff);
 regs.w.di=(memsize&0xffff);
 regs.w.si=(memsize>>16);
 regs.w.cflag=1;
 int386(0x31,&regs,&regs);
 if(regs.w.cflag)
  return 0;
 return ((regs.x.ebx<<16)|(regs.x.ecx&0xffff));
}

void pds_dpmi_unmap_physycal_memory(unsigned long linear_addr)
{
 union REGS regs;
 if(!linear_addr)
  return;
 pds_newfunc_regs_clear(&regs);
 regs.w.ax=0x0801;
 regs.w.bx=linear_addr>>16;
 regs.w.cx=linear_addr&0xffff;
 int386(0x31,&regs,&regs);
}
#else//DJGPP

#include <dpmi.h>
#include <sys/exceptn.h>
#include "newfunc.h"

long pds_dpmi_segment_to_selector(unsigned int segment)
{
    return __dpmi_segment_to_descriptor(segment);
}

void far *pds_dpmi_getrmvect(unsigned int intno) // real mode vector
{
    __dpmi_raddr addr;
    if( __dpmi_get_real_mode_interrupt_vector(intno, &addr) == 0)
        return (void*)(long)(addr.offset16 | (addr.segment<<16));
    return NULL;
}

void pds_dpmi_setrmvect(unsigned int intno, unsigned int segment,unsigned int offset)
{
    __dpmi_raddr addr = {segment, offset};
    __dpmi_set_real_mode_interrupt_vector(intno, &addr);
}

farptr pds_dpmi_getexcvect(unsigned int intno)
{
 __dpmi_paddr addr;
 __dpmi_get_processor_exception_handler_vector(intno, &addr);
 farptr ptr = {addr.offset32, addr.selector};
 return ptr;
}

void pds_dpmi_setexcvect(unsigned int intno, farptr vect)
{
    __dpmi_paddr addr = {vect.off, vect.sel};
    __dpmi_set_processor_exception_handler_vector(intno, &addr);
}

farptr pds_dos_getvect(unsigned int intno)
{
    __dpmi_paddr addr;
    __dpmi_get_protected_mode_interrupt_vector(intno, &addr);
    farptr ptr = {addr.offset32, addr.selector};
    return ptr;
}

static _go32_dpmi_seginfo intaddr_go32[256];

void pds_dos_setvect(unsigned int intno, farptr vect)
{
 if(intaddr_go32[intno].pm_offset == vect.off && intaddr_go32[intno].pm_selector == vect.sel) //already set
    return;

_go32_dpmi_seginfo old_addr = intaddr_go32[intno];

 if(vect.sel == _my_cs())
 {
    intaddr_go32[intno].pm_selector = vect.sel;
    intaddr_go32[intno].pm_offset = vect.off;
    _go32_interrupt_stack_size = 4096; //512 minimal
    if(_go32_dpmi_allocate_iret_wrapper(&intaddr_go32[intno]) != 0)
        return;
    _go32_dpmi_set_protected_mode_interrupt_vector(intno, &intaddr_go32[intno]);
 }
 else
 {
    __dpmi_paddr addr = {vect.off, vect.sel};
    __dpmi_set_protected_mode_interrupt_vector(intno, &addr);
 }

 if(old_addr.pm_selector != 0 || old_addr.pm_offset != 0) //release old wrapper after new wrapper is set
 {
    _go32_dpmi_free_iret_wrapper(&old_addr);
    if(vect.sel != _my_cs())
    {
        intaddr_go32[intno].pm_selector = 0;
        intaddr_go32[intno].pm_offset = 0;
    }
 }
}

int pds_dpmi_dos_allocmem(dosmem_t *dm,unsigned int size)
{
 if(dm->selector){
  pds_dpmi_dos_freemem(dm);
  dm->selector=0;
 }
 int sel = 0;
 int seg = __dpmi_allocate_dos_memory((size+15)>>4, &sel);
 if(seg != -1)
 {
    dm->segment = seg;
    dm->selector = sel;
    dm->linearptr=(void *)(dm->segment<<4);
    return 1;
 }
 dm->selector = 0;
 return 0;
}

void pds_dpmi_dos_freemem(dosmem_t *dm)
{
 if(dm->selector){
  __dpmi_free_dos_memory(dm->selector);
  dm->selector=dm->segment=0;
  dm->linearptr=NULL;
 }
}

void pds_dpmi_realmodeint_call(unsigned int intnum,struct rminfo *rmi)
{
 __dpmi_simulate_real_mode_interrupt(intnum, (__dpmi_regs*)rmi);
}

#define PHYSICAL_MAP_COUNT 64
static __dpmi_meminfo physicalmaps[PHYSICAL_MAP_COUNT];

unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize)
{
 memsize = (memsize+4095)/4096*4096; //__dpmi_set_segment_limit need page aligned
 __dpmi_meminfo info = {0, memsize, phys_addr};

 int i = 0;
 for(; i < PHYSICAL_MAP_COUNT; ++i)
 {
    if(physicalmaps[i].handle == 0)
        break;
 }

 unsigned long base = 0;
 __dpmi_get_segment_base_address(_my_ds(), &base);
 unsigned long limit = __dpmi_get_segment_limit(_my_ds());

 if(i < PHYSICAL_MAP_COUNT && __dpmi_physical_address_mapping(&info) == 0)
 {
    if(info.address < base)
    {
        __dpmi_free_physical_address_mapping(&info);
        info.address = base + limit + 1;
        if(__dpmi_allocate_linear_memory(&info, 0) != 0)
        {
            printf("DPMI map physical memory failed.\n");
            return 0;
        }
        __dpmi_meminfo remap = info;
        remap.address = 0;
        remap.size = (memsize+4095)/4096;
        if(__dpmi_map_device_in_memory_block(&remap, phys_addr) != 0)
        {
            __dpmi_free_memory(info.handle);
            return 0;
        }
        info.size = 0;
    }
    info.address -= base;
    physicalmaps[i] = info;
    unsigned long newlimit = info.address + memsize - 1;
    __dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
    return info.address;
 }
 return 0;
}

void pds_dpmi_unmap_physycal_memory(unsigned long linear_addr)
{
 int i = 0;
 for(; i < PHYSICAL_MAP_COUNT; ++i)
 {
    if(physicalmaps[i].handle != 0 && physicalmaps[i].address == linear_addr)
        break;
 }
 if(i >= PHYSICAL_MAP_COUNT)
    return;
if(physicalmaps[i].size != 0)
    __dpmi_free_physical_address_mapping(&physicalmaps[i]);
else
    __dpmi_free_memory(physicalmaps[i].handle);
 physicalmaps[i].handle = 0;
}

//copied from USBDDOS
static __dpmi_regs pds_xms_regs;
#define pds_xms_inited() (pds_xms_regs.x.cs != 0 || pds_xms_regs.x.ip != 0)

static int pds_xms_init(void)
{
    if(pds_xms_inited())
        return 1;   
    memset(&pds_xms_regs, 0, sizeof(pds_xms_regs));
    pds_xms_regs.x.ax = 0x4300;
    __dpmi_simulate_real_mode_interrupt(0x2F, &pds_xms_regs);
    if(pds_xms_regs.h.al != 0x80)
        return  0;
    pds_xms_regs.x.ax = 0x4310;
    __dpmi_simulate_real_mode_interrupt(0x2F, &pds_xms_regs);    //control function in es:bx
    pds_xms_regs.x.cs = pds_xms_regs.x.es;
    pds_xms_regs.x.ip = pds_xms_regs.x.bx;
    pds_xms_regs.x.ss = pds_xms_regs.x.sp = 0;
    return 1;
}

unsigned short pds_xms_alloc(unsigned short sizeKB, unsigned long* addr)
{
    __dpmi_regs r;
    unsigned short handle = 0;
    *addr = 0;
   
    if(sizeKB == 0 || !pds_xms_init())
        return handle;
    r = pds_xms_regs;
    r.h.ah = 0x09;      //alloc XMS
    r.x.dx = sizeKB;    //size in kb
    __dpmi_simulate_real_mode_procedure_retf(&r);
    if (r.x.ax != 0x1)
        return handle;
    handle = r.x.dx;

    r = pds_xms_regs;
    r.x.dx = handle;
    r.h.ah = 0x0C;    //lock XMS
    __dpmi_simulate_real_mode_procedure_retf(&r);
    if(r.x.ax != 0x1)
    {
        r = pds_xms_regs;
        r.h.ah = 0x0A; //free XMS
        __dpmi_simulate_real_mode_procedure_retf(&r);
        return 0;
    }
    *addr = ((unsigned long)r.x.dx << 16L) | (unsigned long)r.x.bx;
    return handle;
}

int pds_xms_free(unsigned short handle)
{
    __dpmi_regs r = pds_xms_regs;

    if(!pds_xms_inited())
        return 0;
    r.h.ah = 0x0D;
    r.x.dx = handle;
    __dpmi_simulate_real_mode_procedure_retf(&r);
    if(r.x.ax != 1)
        return 0;
    r = pds_xms_regs;
    r.h.ah = 0x0A;
    r.x.dx = handle;
    __dpmi_simulate_real_mode_procedure_retf(&r);
    return r.x.ax == 1;
}


int pds_dpmi_xms_allocmem(xmsmem_t * mem,unsigned int size)
{
    unsigned long addr;
    size = (size+4095)/1024*1024;
    if( (mem->xms=pds_xms_alloc(size/1024, &addr)) )
    {
        unsigned long base = 0;
        unsigned long limit = __dpmi_get_segment_limit(_my_ds());
        __dpmi_get_segment_base_address(_my_ds(), &base);
        
        __dpmi_meminfo info = {0, size, addr};
        mem->remap = 0;
        do {
            if( __dpmi_physical_address_mapping(&info) == 0)
            {
                if(info.address < base)
                {
                    __dpmi_free_physical_address_mapping(&info);
                    info.address = base + limit + 1;
                    if(__dpmi_allocate_linear_memory(&info, 0) != 0)//TODO: handle error
                    {
                        printf("DPMI Failed allocate linear memory.\n");
                        break;
                    }
                    __dpmi_meminfo remap = info;
                    remap.address = 0;
                    remap.size = size/4096;
                    if(__dpmi_map_device_in_memory_block(&remap, addr) != 0)
                        break;
                    mem->remap = 1;
                }
                mem->handle = info.handle;
                mem->physicalptr = (char*)addr;
                mem->linearptr = (char*)(info.address - base);
                unsigned long newlimit = info.address + size - base - 1;
                newlimit = ((newlimit+1+0xFFF)&~0xFFF) - 1;//__dpmi_set_segment_limit must be page aligned
                //printf("addr: %08x, limit: %08x\n",mem->linearptr, newlimit);
                __dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
                __dpmi_set_segment_limit(__djgpp_ds_alias, max(limit, newlimit));
                return 1;
            }
        }while(0);
        pds_xms_free(mem->xms);
        mem->xms = 0;
    }
    printf("Failed allocatee XMS.\n");
    return 0;
}

void pds_dpmi_xms_freemem(xmsmem_t * mem)
{
    if(mem->remap)
        __dpmi_free_memory(mem->handle);
    else
    {
        __dpmi_meminfo info = {mem->handle, 0, 0};
        __dpmi_free_physical_address_mapping(&info);
    }
    pds_xms_free(mem->xms);
}

#endif

#endif // __DOS__
