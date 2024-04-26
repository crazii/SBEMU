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
//function: PCI-BIOS handling
//based on a code of Taichi Sugiyama (YAMAHA)

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <dos.h>
#include <assert.h>

#include "au_base.h"
#include "pcibios.h"
#include "dmairq.h"

#define PCIDEVNUM(bParam)      (bParam >> 3)
#define PCIFUNCNUM(bParam)     (bParam & 0x07)
#define PCIDEVFUNC(bDev,bFunc) ((((uint32_t)bDev) << 3) | bFunc)

#define pcibios_clear_regs(reg) pds_memset(&reg,0,sizeof(reg))

#ifndef _MSC_VER //just to make vscode happy

static uint8_t pcibios_GetBus(void)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_BIOS_PRESENT;
 reg.w.cflag=1;

 int386(PCI_SERVICE, &reg, &reg);

 if(reg.w.cflag)
  return 0;

 return 1;
}

uint8_t    pcibios_FindDevice(uint16_t wVendor, uint16_t wDevice, pci_config_s *ppkey)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_FIND_DEVICE;
 reg.w.cx = wDevice;
 reg.w.dx = wVendor;
 reg.w.si = 0;  //bIndex;

 int386(PCI_SERVICE, &reg, &reg);

 if(ppkey && (reg.h.ah==PCI_SUCCESSFUL)){
  ppkey->bBus  = reg.h.bh;
  ppkey->bDev  = PCIDEVNUM(reg.h.bl);
  ppkey->bFunc = PCIFUNCNUM(reg.h.bl);
  ppkey->vendor_id=wVendor;
  ppkey->device_id=wDevice;
 }

 return reg.h.ah;
}

#endif//_MSC_VER

uint8_t pcibios_search_devices(pci_device_s devices[],pci_config_s *ppkey)
{
 if(pcibios_GetBus()){
  unsigned int i=0;
  while(devices[i].vendor_id){
   if(pcibios_FindDevice(devices[i].vendor_id,devices[i].device_id,ppkey)==PCI_SUCCESSFUL){
    if(ppkey){
     ppkey->device_name=devices[i].device_name;
     ppkey->device_type=devices[i].device_type;
    }
    return PCI_SUCCESSFUL;
   }
   i++;
  }
 }
 return PCI_DEVICE_NOTFOUND;
}

#ifndef SBEMU //BIOS INT service may freeze on some PC (tested a 845M laptop), use pure IOs.
uint8_t    pcibios_ReadConfig_Byte(pci_config_s * ppkey, uint16_t wAdr)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_READ_BYTE;
 reg.h.bh = ppkey->bBus;
 reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
 reg.w.di = wAdr;
 int386(PCI_SERVICE, &reg, &reg);

 return reg.h.cl;
}

uint16_t pcibios_ReadConfig_Word(pci_config_s * ppkey, uint16_t wAdr)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_READ_WORD;
 reg.h.bh = ppkey->bBus;
 reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
 reg.w.di = wAdr;

 int386(PCI_SERVICE, &reg, &reg);

 return reg.w.cx;
}

uint32_t pcibios_ReadConfig_Dword(pci_config_s * ppkey, uint16_t wAdr)
{
 uint32_t dwData;

 dwData  = (uint32_t)pcibios_ReadConfig_Word(ppkey, wAdr + 2) << 16;
 dwData |= (uint32_t)pcibios_ReadConfig_Word(ppkey, wAdr);

 return dwData;
}

void pcibios_WriteConfig_Byte(pci_config_s * ppkey, uint16_t wAdr, uint8_t bData)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_WRITE_BYTE;
 reg.h.bh = ppkey->bBus;
 reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
 reg.h.cl = bData;
 reg.w.di = wAdr;

 int386(PCI_SERVICE, &reg, &reg);
}

void pcibios_WriteConfig_Word(pci_config_s * ppkey, uint16_t wAdr, uint16_t wData)
{
 union REGS reg;

 pcibios_clear_regs(reg);

 reg.h.ah = PCI_FUNCTION_ID;
 reg.h.al = PCI_WRITE_WORD;
 reg.h.bh = ppkey->bBus;
 reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
 reg.w.cx = wData;
 reg.w.di = wAdr;

 int386(PCI_SERVICE, &reg, &reg);
}

void pcibios_WriteConfig_Dword(pci_config_s * ppkey, uint16_t wAdr, uint32_t dwData)
{
 pcibios_WriteConfig_Word(ppkey, wAdr, LoW(dwData ));
 pcibios_WriteConfig_Word(ppkey, wAdr + 2, HiW(dwData));
}
#else
#define PCI_ADDR  0x0CF8
#define PCI_DATA  0x0CFC
#define ENABLE_BIT 0x80000000
#define inpd inportl
#define outpd outportl

uint8_t PCI_ReadByte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
{
    int shift = ((reg & 3) * 8);
    uint32_t val = ENABLE_BIT |
        ((uint32_t)bus << 16) |
        ((uint32_t)dev << 11) |
        ((uint32_t)func << 8) |
        ((uint32_t)reg & 0xFC);
    outpd(PCI_ADDR, val);
    return (inpd(PCI_DATA) >> shift) & 0xFF;
}

uint16_t PCI_ReadWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
{
    if ((reg & 3) <= 2)
    {
        const int shift = ((reg & 3) * 8);
        const uint32_t val = ENABLE_BIT |
            ((uint32_t)bus << 16) |
            ((uint32_t)dev << 11) |
            ((uint32_t)func << 8) |
            ((uint32_t)reg & 0xFC);
        outpd(PCI_ADDR, val);
        return (inpd(PCI_DATA) >> shift) & 0xFFFF;
    }
    else
        return (uint16_t)((PCI_ReadByte(bus, dev, func, (uint8_t)(reg + 1)) << 8) | PCI_ReadByte(bus, dev, func, reg));
}

uint32_t PCI_ReadDWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
{
    if ((reg & 3) == 0)
    {
        uint32_t val = ENABLE_BIT |
            ((uint32_t)bus << 16) |
            ((uint32_t)dev << 11) |
            ((uint32_t)func << 8) |
            ((uint32_t)reg & 0xFC);
        outpd(PCI_ADDR, val);
        return inpd(PCI_DATA);
    }
    else
        return ((uint32_t)PCI_ReadWord(bus, dev, func, (uint8_t)(reg + 2)) << 16L) | PCI_ReadWord(bus, dev, func, reg);
}

void PCI_WriteByte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint8_t value)
 {
    int shift = ((reg & 3) * 8);
    uint32_t val = ENABLE_BIT |
        ((uint32_t)bus << 16) |
        ((uint32_t)dev << 11) |
        ((uint32_t)func << 8) |
        ((uint32_t)reg & 0xFC);
      outpd(PCI_ADDR, val);
      outpd(PCI_DATA, (uint32_t)(inpd(PCI_DATA) & ~(0xFFU << shift)) | ((uint32_t)value << shift));
}


void PCI_WriteWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint16_t value)
{
    if ((reg & 3) <= 2)
    {
        int shift = ((reg & 3) * 8);
        uint32_t val = ENABLE_BIT |
            ((uint32_t)bus << 16) |
            ((uint32_t)dev << 11) |
            ((uint32_t)func << 8) |
            ((uint32_t)reg & 0xFC);
        outpd(PCI_ADDR, val);
        outpd(PCI_DATA, (inpd(PCI_DATA) & ~(0xFFFFU << shift)) | ((uint32_t)value << shift));
    }
    else
    {
        PCI_WriteByte(bus, dev, func, (uint8_t)(reg + 0), (uint8_t)(value & 0xFF));
        PCI_WriteByte(bus, dev, func, (uint8_t)(reg + 1), (uint8_t)(value >> 8));
    }
}


void PCI_WriteDWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t value)
{
    if ((reg & 3) == 0)
    {
        uint32_t val = ENABLE_BIT |
            ((uint32_t)bus << 16) |
            ((uint32_t)dev << 11) |
            ((uint32_t)func << 8) |
            ((uint32_t)reg & 0xFC);
        outpd(PCI_ADDR, val);
        outpd(PCI_DATA, value);
    }
    else
    {
        PCI_WriteWord(bus, dev, func, (uint8_t)(reg + 0), (uint16_t)(value & 0xFFFF));
        PCI_WriteWord(bus, dev, func, (uint8_t)(reg + 2), (uint16_t)(value >> 16));
    }
}

uint8_t    pcibios_ReadConfig_Byte(pci_config_s * ppkey, uint16_t wAdr)
{
    return PCI_ReadByte(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

uint16_t pcibios_ReadConfig_Word(pci_config_s * ppkey, uint16_t wAdr)
{
    return PCI_ReadWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

uint32_t pcibios_ReadConfig_Dword(pci_config_s * ppkey, uint16_t wAdr)
{
    return PCI_ReadDWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

void pcibios_WriteConfig_Byte(pci_config_s * ppkey, uint16_t wAdr, uint8_t bData)
{
    PCI_WriteByte(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, bData);
}

void pcibios_WriteConfig_Word(pci_config_s * ppkey, uint16_t wAdr, uint16_t wData)
{
    PCI_WriteWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, wData);
}

void pcibios_WriteConfig_Dword(pci_config_s * ppkey, uint16_t wAdr, uint32_t dwData)
{
    PCI_WriteDWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, dwData);
}
#endif

void pcibios_set_master(pci_config_s *ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Word(ppkey, PCIR_PCICMD);
 cmd|=0x01|0x04;
 pcibios_WriteConfig_Word(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_memmap_set_master(pci_config_s *ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Word(ppkey, PCIR_PCICMD);
 cmd&=~0x01;     // disable io-port mapping
 cmd|=0x02|0x04; // enable memory mapping and set master
 pcibios_WriteConfig_Word(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_memmap_set_master_all(pci_config_s *ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Word(ppkey, PCIR_PCICMD);
 cmd|=0x01|0x02|0x04; // enable io-port mapping and memory mapping and set master
 pcibios_WriteConfig_Word(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_interrupt(pci_config_s* ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Word(ppkey, PCIR_PCICMD);
 cmd &= ~(1<<10);
 pcibios_WriteConfig_Word(ppkey, PCIR_PCICMD, cmd);
}

#define USE_P32_CALL 0 //32bit protected mode call. if this doesn't work for some PCs, then the final solution should be programming the interrupt router.

#pragma pack(1)

typedef struct
{
    uint16_t size;
    #if USE_P32_CALL
    uint32_t off;
    #else
    uint16_t off;
    #endif
    uint16_t seg;
    #if !USE_P32_CALL
    uint16_t padding;
    #endif
}IRQRoutingOptionBuffer;
_Static_assert(sizeof(IRQRoutingOptionBuffer) == 8, "size error");

typedef struct
{
    uint8_t bus;
    uint8_t dev; //high 5 bit, low 3 bit unspecified
    struct 
    {
        uint8_t link;
        uint16_t map;
    }intpins[4];
    uint8_t slot;
    uint8_t reserved;
}IRQRoutingTable; //table entry
#pragma pack()
_Static_assert(sizeof(IRQRoutingTable) == 16, "size error");

#include "../../sbemu/dpmi/dbgutil.h"
#include "../../sbemu/pic.h"
#ifdef DJGPP
#include <dpmi.h>
#include <go32.h>
#endif
//copied & modified from usbddos
//https://people.freebsd.org/~jhb/papers/bsdcan/2007/article/node4.html
//https://people.freebsd.org/~jhb/papers/bsdcan/2007/article/node5.html
//PCI BIOS SPECIFICATION Revision 2.1
static int pcibios32cs = -1;
static int pcibios32ds = -1;
static uint32_t pcibios32entry = -1;
static int pcibios_inited = 0;

static void pcibios_FreeLDT(void)
{
    if(pcibios32cs != -1)
        __dpmi_free_ldt_descriptor(pcibios32cs);
    if(pcibios32ds != -1)
        __dpmi_free_ldt_descriptor(pcibios32ds);
}

void pcibios_CallFunction32(rminfo* r)
{
    if(!pcibios_inited)
    {
        pcibios_inited = TRUE;

        typedef struct 
        {
            char signature[4];
            uint32_t entry; //physical addr
            uint8_t reivision;
            uint8_t len; //in paragraph
            uint8_t checksum;
            uint8_t zero[5];
        }BIOS32SD;
        _Static_assert(sizeof(BIOS32SD) == 16, "size error");

        char* scanarea = (char*)malloc(0xFFFFF-0xE0000);
        dosget(scanarea, 0xE0000, 0xFFFFF-0xE0000);
        const BIOS32SD* sd = NULL;
        int count = (0xFFFFF-0xE0000)/sizeof(BIOS32SD);
        for(int i = 0; i < count; ++i)
        {
            const BIOS32SD* test = (const BIOS32SD*)(scanarea + i * sizeof(BIOS32SD));
            if(memcmp(test->signature,"_32_", 4) == 0 && memcmp(test->zero,"\0\0\0\0\0", 5) == 0 && test->len == 1)
            {
                const char* bytes = (const char*)test;
                uint8_t checksum = 0;
                for(int i = 0; i < 16; i++)
                    checksum += bytes[i];
                if(checksum == 0)
                {
                    sd = test;
                    break;
                }
            }
        }

        if(sd == NULL) //not found
        {
            free(scanarea);
            r->EAX = PCI_NOT_SUPPORTED<<8;
            return;
        }
        _LOG("Found BIOS32 Service Directory at %x, entry point %x\n", (uintptr_t)sd-(uintptr_t)scanarea + 0xE0000, sd->entry);
        
        int bios32cs = __dpmi_create_alias_descriptor(_my_cs());
        int bios32ds = __dpmi_create_alias_descriptor(_my_ds());
        __dpmi_set_descriptor_access_rights(bios32cs, __dpmi_get_descriptor_access_rights(bios32cs)|0x08); //code/exec

        uint32_t base = sd->entry&~0xFFF; //although it is physical addr, should be under 1M, phyiscal mapping not needed
        __dpmi_set_segment_base_address(bios32cs, base);
        __dpmi_set_segment_base_address(bios32ds, base);
        __dpmi_set_segment_limit(bios32cs, 8192-1); //spec required 2 pages
        __dpmi_set_segment_limit(bios32ds, 8192-1);
        uint32_t pci32entry = 0;
        uint32_t size = 0;
        uint32_t eip = sd->entry - base;
        free(scanarea);

        _ASM_BEGINP
            _ASM(pushad)
            _ASM(push ds)

            _ASM_MOVP(eax, %4) //bios32cs
            _ASM(push eax)
            _ASM_MOVP(eax, %5) //eip
            _ASM(push eax)

            _ASM(mov eax, 0x049435024) //$PCI
            _ASM(xor ebx, ebx)

            _ASM_MOVP(edx, %3) //bios32ds
            _ASM(mov ds, edx)
            _ASM(mov edx, esp)
            _ASM(call far ptr ss:[edx])
            _ASM(add esp, 8)
            _ASM(pop ds)

            _ASM(test al, al)
            _ASM(jnz failed)
            _ASM_MOVP(%0, ebx)
            _ASM_MOVP(%1, ecx)
            _ASM_MOVP(%2, edx)
        _ASMLBL(failed:)
            _ASM(popad)
            
            _ASM_PLIST(base, size, pci32entry, bios32ds, bios32cs, eip)
        _ASM_ENDP

        if(base == 0 || size == 0)
        {
            _LOG("32bit PCI BIOS not found.\n");
            r->EAX = PCI_NOT_SUPPORTED<<8;
            return;
        }
        
        _LOG("Found 32bit PCI BIOS, base: %x, entry %x, size: %x\n", base, pci32entry, size);
        //const int CS_BASE = 0xF000; //spec required CS base //I think it's a typo in the spec
        const int CS_BASE = 0xF0000;
        assert(base >= CS_BASE);
        if(base < CS_BASE) //spec require DS=CS=0xF0000
        {
            r->EAX = PCI_NOT_SUPPORTED<<8;        
            return;
        }
        size += max(0, (int)(base - CS_BASE));
        size = (size+0xFFF)&~0xFFF;
        pci32entry += base;
        base = CS_BASE;
        pci32entry -= base;
        __dpmi_set_segment_base_address(bios32cs, base);
        __dpmi_set_segment_base_address(bios32ds, 0xF0000); //spec required DS base
        __dpmi_set_segment_limit(bios32cs, size-1);
        __dpmi_set_segment_limit(bios32ds, 64*1024-1); //spec required DS size

        pcibios32cs = bios32cs;
        pcibios32ds = bios32ds;
        pcibios32entry = pci32entry;
        atexit(&pcibios_FreeLDT);
    }

    if(pcibios32cs == -1 || pcibios32ds == -1 || pcibios32entry == -1)
    {
        r->EAX = PCI_NOT_SUPPORTED<<8;
        return;
    }

    rminfo reg = *r; //access using EBP
    _LOG("EAX: %08lx, ECX: %08lx, EBX: %08lx EDX: %08lx\n", reg.EAX, reg.ECX, reg.EBX, reg.EDX);
    _LOG("ES: %04lx, EDI: %08lx\n", reg.ES, reg.EDI);
    _ASM_BEGINP
        _ASM(pushad)
        _ASM(push ds)
        _ASM(push es)

        _ASM_MOVP(eax, %7) //pcibios32cs
        _ASM(push eax)
        _ASM_MOVP(eax, %8) //pcibios32entry
        _ASM(push eax)

        _ASM_MOVP(eax, %0)
        _ASM_MOVP(ebx, %1)
        _ASM_MOVP(ecx, %2)
        _ASM_MOVP(edx, %3)
        _ASM_MOVP(edi, %4)
        _ASM_MOVP(si, %5)
        _ASM_MOVP(es, si)

        _ASM_MOVP(esi, %6) //pcibios32ds
        _ASM(mov ds, esi)
        _ASM(mov esi, esp)
        _ASM(call far ptr ss:[esi])
        _ASM(add esp, 8)

        _ASM_MOVP(%0, eax)
        _ASM_MOVP(%1, ebx)
        _ASM_MOVP(%2, ecx)
        _ASM_MOVP(%3, edx)
        
        _ASM(pop es)
        _ASM(pop ds)
        _ASM(popad)

        _ASM_PLIST(reg.EAX, reg.EBX, reg.ECX, reg.EDX, reg.EDI, reg.ES,
            pcibios32ds, pcibios32cs, pcibios32entry)
    _ASM_ENDP
    *r = reg;
}

uint8_t  pcibios_AssignIRQ(pci_config_s* ppkey)
{
    if(!pcibios_GetBus())
        return 0xFF;

    uint8_t INTPIN = pcibios_ReadConfig_Byte(ppkey, PCIR_INTR_PIN);
    _LOG("INTPIN: INT%c#, bdf: %d %d %d\n", 'A'+INTPIN-1, ppkey->bBus, ppkey->bDev, ppkey->bFunc);
    if(INTPIN > 4 || INTPIN < 1) //[1,4]
    {
        assert(FALSE);
        return FALSE;
    }

    const int STACK_SIZE = 1024;
    dosmem_t rmstack = {0};
    if(!pds_dpmi_dos_allocmem(&rmstack, STACK_SIZE)) //PCI BIOS require a 1K stack but DPMI host may not reserve enough space if ss:sp=0
        return 0xFF;
    
    dosmem_t dosmem = {0};
    if(!pds_dpmi_dos_allocmem(&dosmem, sizeof(IRQRoutingOptionBuffer)))
    {
        pds_dpmi_dos_freemem(&rmstack);
        return 0xFF;
    }

    IRQRoutingOptionBuffer buf = {0};
    buf.size = 0; //get actually size with size=0
    dosput(dosmem.linearptr, &buf, sizeof(buf));    

    _LOG("PCI_GET_ROUTING: get size\n");
    rminfo r = {0};
    r.EAX = (PCI_FUNCTION_ID<<8)|PCI_GET_ROUTING;
    r.EBX = 0;
#if USE_P32_CALL
    #if 0 //32 bit offset not working, some BIOS used DI not EDI
    r.ES = _my_ds();
    r.EDI = (uintptr_t)&buf;
    #else
    r.ES = dosmem.selector;
    r.EDI = 0;
    #endif
    pcibios_CallFunction32(&r);
#else
    r.DS = 0xF000;
    r.SS = rmstack.segment;
    r.SP = STACK_SIZE;
    r.ES = dosmem.segment;
    r.EDI = 0;
    pds_dpmi_realmodeint_call(PCI_SERVICE, &r); //get required size
#endif
    dosget(&buf, dosmem.linearptr, sizeof(buf));

    IRQRoutingTable* table = NULL;
    if(((r.EAX>>8)&0xFF) == PCI_BUFFER_SMALL) //intended, should return this
    {
        _LOG("PCI_GET_ROUTING: get data %d\n", buf.size);
        if(buf.size == 0 || !pds_dpmi_dos_allocmem(&dosmem, sizeof(buf) + buf.size)) //buf.size==0 means no PCI devices in the system
        {
            pds_dpmi_dos_freemem(&rmstack);
            return 0xFF;
        }

        table = (IRQRoutingTable*)malloc(buf.size);
        if(!table)
        {
            pds_dpmi_dos_freemem(&rmstack);
            pds_dpmi_dos_freemem(&dosmem);
            return 0xFF;
        }

        r.EAX = (PCI_FUNCTION_ID<<8)|PCI_GET_ROUTING;
        r.EBX = 0;
#if USE_P32_CALL
        #if 0 //32 bit offset not working, some BIOS used DI not EDI
        buf.seg = _my_ds();
        buf.off = (uintptr_t)table;
        #else
        buf.seg = dosmem.selector;
        buf.off = sizeof(buf);
        dosput(dosmem.linearptr, &buf, sizeof(buf));        
        r.ES = dosmem.selector;
        #endif
        pcibios_CallFunction32(&r);
#else
        buf.off = sizeof(buf);
        buf.seg = dosmem.segment;
        dosput(dosmem.linearptr, &buf, sizeof(buf));
        r.ES = dosmem.segment;
        pds_dpmi_realmodeint_call(PCI_SERVICE, &r); //get data
#endif
        dosget(table, dosmem.linearptr+sizeof(buf), buf.size);
    }
    else
        r.EAX = 0xFFFFFFFF; //make sure not successful
    
    if(((r.EAX>>8)&0xFF) != PCI_SUCCESSFUL) 
    {
        free(table);
        pds_dpmi_dos_freemem(&rmstack);
        pds_dpmi_dos_freemem(&dosmem);
        return 0xFF;
    }

    int count = buf.size/sizeof(IRQRoutingTable);
    uint16_t map = 0;
    uint8_t link = 0; //not connected
    for(int i = 0; i < count; ++i)
    {
        if(table[i].bus == ppkey->bBus && (table[i].dev>>3) == ppkey->bDev)
        {
            for(int j = 0; j < 4; ++j) _LOG("link: %d map:%x\n", table[i].intpins[j].link, table[i].intpins[j].map);
            link = table[i].intpins[INTPIN-1].link;
            map = table[i].intpins[INTPIN-1].map;
            break;
        }
    }
    assert(map != 0 && link != 0); //should be in the table
   
    uint8_t linkedIRQ = 0xFF;
    uint16_t originalmap = map;
    pci_config_s cfg;
    if(map && link)
    {
        //iterate all devices to find devices with the same link (wire-ORed)
        for(int i = 0; (i < count) && (map != 0); ++i)
        {
            cfg.bBus = table[i].bus;
            cfg.bDev = table[i].dev>>3;
            for(int j = 0; (j < 4) && (linkedIRQ == 0xFF) && (map != 0); ++j)
            {
                if( table[i].intpins[j].link != link)
                    continue;
                map &= table[i].intpins[j].map;
                if(map == 0) //IRQ routing options conflict: same link value but routing options have no intersection
                    break;
                for(cfg.bFunc = 0; (cfg.bFunc < 8) && (linkedIRQ == 0xFF); ++cfg.bFunc)
                {
                    uint8_t intpin = pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_PIN);
                    if(intpin-1 == j)
                    {
                        linkedIRQ = pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_LN);
                        assert(linkedIRQ == 0xFF || ((1<<linkedIRQ)&table[i].intpins[j].map) != 0);
                        if(linkedIRQ != 0xFF)
                        {
                            _LOG("Found shared IRQ: %d, pin: INT%c# on bdf %d %d %d\n", linkedIRQ, 'A'+j, cfg.bBus, cfg.bDev, cfg.bFunc);
                            #if 0
                            map &= table[i].intpins[j].map;
                            if(((1<<linkedIRQ)&map) == 0) //not in target map
                                linkedIRQ = 0xFF;
                            if(map == 0) //IRQ routing options conflict: same link value but routing options have no intersection
                                break;
                            #endif
                        }
                    }
                }
            }
            if(linkedIRQ != 0xFF)
                break;
        }
    }
    const int mouseIRQ = (1<<12);
    const int ATAIRQ = (1<<15) | (1<<14);

    uint8_t irq = linkedIRQ;
    if(irq != 0xFF)
        pcibios_WriteConfig_Byte(ppkey, PCIR_INTR_LN, irq); //found a shared IRQ, it's gonna work.
    else if(map != 0) //no conflict
    {
        _LOG("PCI dedicated IRQ map: %x\n", (uint16_t)r.EBX);
        if(map&(uint16_t)r.EBX)
            map &= (uint16_t)r.EBX; //prefer PCI dedicated IRQ

        if(map&~ATAIRQ) //mask out ATA
            map &= ~ATAIRQ;
        if(map&~mouseIRQ) //mask out mouse
            map &= ~mouseIRQ;
        _LOG("IRQ map: %x\n", map);

        //find the highset available
        while(map)
        {
            map>>=1;
            ++irq;
        }
        assert(irq > 2 && irq <= 15);

        //ENTER_CRITICAL;
        //mask out the IRQ incase it's not handled by BIOS by deafult (not the default IRQ used by BIOS setup), and system will be overwhelmed by the IRQ
        //code outside is responsible to unmask it.
        uint16_t irqmask = PIC_GetIRQMask();
        PIC_MaskIRQ(irq);

        pcibios_clear_regs(r);
        r.EAX = (PCI_FUNCTION_ID<<8)|PCI_SET_INTERRUPT;
        r.ECX = (((uint32_t)irq)<<8) | (0xA + INTPIN - 1); //cl=INTPIN (0xA~0xD), ch=IRQ
        r.EBX = (((uint32_t)ppkey->bBus)<<8) | ((ppkey->bDev<<3)&0xFF) | (ppkey->bFunc&0x7); //bh=bus, bl=dev|func
        _LOG("PCI_SET_INTERRUPT INT%c#->%d\n", 'A'+INTPIN-1, irq);

#if USE_P32_CALL
        pcibios_CallFunction32(&r);
#else
        r.DS = 0xF000;
        r.SS = rmstack.segment;
        r.SP = STACK_SIZE;
        pds_dpmi_realmodeint_call(PCI_SERVICE, &r);
#endif

        if(((r.EAX>>8)&0xFF) == PCI_SUCCESSFUL)
        {
            //set PCI_SET_INTERRUPT done, it's gonna work too, but the pci bios 2.1 spec require us to update the IRQ_LINE
            //for all linked devices (shared INTPIN)
            //assign to all wire-ORed pin, including input (ppkey)
            int irqmask = (1<<irq);
            
            _LOG("set INTLINE for all\n");
            for(int i = 0; i < count; ++i)
            {
                cfg.bBus = table[i].bus;
                cfg.bDev = table[i].dev>>3;
                for(int j = 0; j < 4; ++j)
                {
                    if(table[i].intpins[j].link != link || (table[i].intpins[j].map&irqmask) == 0)
                        continue;
                    for(cfg.bFunc = 0; cfg.bFunc < 8; ++cfg.bFunc)
                    {
                        uint8_t intpin = pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_PIN);
                        if(intpin-1 == j)
                        {
                            assert(pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_LN) == 0xFF);
                            pcibios_WriteConfig_Byte(&cfg, PCIR_INTR_LN, irq);
                        }
                    }
                }
            }
        }
        else
        {
            _LOG("PCI_SET_INTERRUPT failed %x\n", ((r.EAX>>8)&0xFF));
            #if 1 //try pci config space register only
            printf("warning: set IRQ failed, set pci config space INTLINE only\n");
            pcibios_WriteConfig_Byte(ppkey, PCIR_INTR_LN, irq);
            #else
            irq = 0xFF;
            #endif
            PIC_SetIRQMask(irqmask);
        }

        //LEAVE_CRITICAL;
    }
    else if(originalmap != 0) //there's conflict, cannot set irq via pci bios.
    {
        map = originalmap;
        if(map&(uint16_t)r.EBX)
            map &= (uint16_t)r.EBX; //prefer PCI dedicated IRQ
        _LOG("PCI dedicated IRQ map: %x\n", map);

        if(map&~ATAIRQ) //mask out ATA
            map &= ~ATAIRQ;
        if(map&~mouseIRQ) //mask out mouse
            map &= ~mouseIRQ;

        //find the highset available
        while(map)
        {
            map>>=1;
            ++irq;
        }
        assert(irq > 2 && irq <= 15);
        printf("warning: IRQ conflict, set pci config space INTLINE only\n");
        pcibios_WriteConfig_Byte(ppkey, PCIR_INTR_LN, irq); //only set pci config space irq, tested work in cases of usbddos, but may not work on all PCs
    }

    pds_dpmi_dos_freemem(&rmstack);
    pds_dpmi_dos_freemem(&dosmem);
    free(table);
    return irq;
}
