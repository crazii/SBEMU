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
 cmd=pcibios_ReadConfig_Byte(ppkey, PCIR_PCICMD);
 cmd|=0x01|0x04;
 pcibios_WriteConfig_Byte(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_memmap_set_master(pci_config_s *ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Byte(ppkey, PCIR_PCICMD);
 cmd&=~0x01;     // disable io-port mapping
 cmd|=0x02|0x04; // enable memory mapping and set master
 pcibios_WriteConfig_Byte(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_interrupt(pci_config_s* ppkey)
{
 unsigned int cmd;
 cmd=pcibios_ReadConfig_Byte(ppkey, PCIR_PCICMD);
 cmd &= ~(1<<10);
 pcibios_WriteConfig_Byte(ppkey, PCIR_PCICMD, cmd);
}

typedef struct
{
    uint16_t size;
    uint16_t off;
    uint16_t seg;
    uint16_t padding;
}IRQRoutingOptionBuffer;

#define BUFSIZE (16*1024)
//copied & modified for usbddos
uint8_t  pcibios_AssignIRQ(pci_config_s* ppkey)
{
    uint8_t INTPIN = pcibios_ReadConfig_Byte(ppkey, PCIR_INTR_PIN);
    
    dosmem_t dosmem = {0};
    pds_dpmi_dos_allocmem(&dosmem, BUFSIZE);

    IRQRoutingOptionBuffer buf;
    buf.size = BUFSIZE-sizeof(buf);
    buf.off = sizeof(buf);
    buf.seg = dosmem.segment;
    dosput(dosmem.linearptr, &buf, sizeof(buf));

    rminfo r = {0};
    r.EAX = (PCI_FUNCTION_ID<<8)|PCI_GET_ROUTING;
    r.DS = 0xF000;
    r.ES = dosmem.segment;
    r.EDI = 0;
    pds_dpmi_realmodeint_call(PCI_SERVICE, &r);

    uint16_t map = 0;
    uint8_t link = 0; //not connected
    if(((r.EAX>>8)&0xFF) == PCI_SUCCESSFUL) //ah=PCI_SUCCESSFUL)
    {
        dosget(&buf, dosmem.linearptr, sizeof(buf));

        for(uint16_t start = 0; start < buf.size; start+=16)
        {
            char* addr = dosmem.linearptr+sizeof(buf)+start;
            uint8_t b,d;
            dosget(&b, addr, sizeof(b));
            dosget(&d, addr+1, sizeof(d));
            d>>=3;
            if(b == ppkey->bBus && d == ppkey->bDev)
            {
                dosget(&map, addr+INTPIN*3, sizeof(map));
                dosget(&link,addr+INTPIN*3-1, sizeof(link));
                break;
            }
        }
    }
   
    uint8_t linkedIRQ = 0xFF;
    pci_config_s cfg;
    if(map)
    {
        assert(link != 0);
        //iterate all devices to find devices with the same link (wire-ORed)
        for(uint16_t start = 0; start < buf.size; start+=16)
        {
            char* addr = dosmem.linearptr+sizeof(buf)+start;
            uint8_t l = 0;
            dosget(&l,addr+INTPIN*3-1, sizeof(l));
            if(l != link /*|| (b == ppkey->bBus && d == ppkey->bDev && f == ppkey->bFunc)*/)
                continue;
            dosget(&cfg.bBus, addr, sizeof(cfg.bBus));
            dosget(&cfg.bDev, addr+1, sizeof(cfg.bDev));
            cfg.bFunc = cfg.bDev&0x7;
            cfg.bDev>>=3;
            linkedIRQ = pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_LN);
            if(linkedIRQ != 0xFF)
                break;
        }
    }

    uint8_t irq = linkedIRQ;
    if(irq != 0xFF)
        pcibios_WriteConfig_Byte(ppkey, PCIR_INTR_LN, irq); //found a shared IRQ, it's gonna work.
    else
    {
        if(map&(uint16_t)r.EBX)
            map &= (uint16_t)r.EBX; //PCI dedicated IRQ
        //find the highset available
        while(map)
        {
            map>>=1;
            ++irq;
        }

        if(irq != 0xFF)
        {
            assert(irq > 2);

            pcibios_clear_regs(r);
            r.EAX = (PCI_FUNCTION_ID<<8)|PCI_SET_INTERRUPT;
            r.ECX = ((uint32_t)irq<<8) | (0xA + INTPIN);
            r.EBX = ((uint32_t)ppkey->bBus<<8) | (ppkey->bDev<<3) | (ppkey->bFunc&0x7);
            r.DS = 0xF000;
            pds_dpmi_realmodeint_call(PCI_SERVICE, &r);

            if(((r.EAX>>8)&0xFF) == PCI_SUCCESSFUL)
            {
                //set PCI_SET_INTERRUPT done, it's gonna work too, but the pci bios 2.1 spec require us to update the IRQ_LINE
                //for all linked devices (shared INTPIN)
                //assign to all wire-ORed pin, including input (ppkey)
                for(uint16_t start = 0; start < buf.size; start+=16)
                {
                    char* addr = dosmem.linearptr+sizeof(buf)+start;
                    uint8_t l = 0;
                    dosget(&l,addr+INTPIN*3-1, sizeof(l));
                    if(l != link)
                        continue;
                    dosget(&cfg.bBus, addr, sizeof(cfg.bBus));
                    dosget(&cfg.bDev, addr+1, sizeof(cfg.bDev));
                    cfg.bFunc = cfg.bDev&0x7;
                    cfg.bDev>>=3;
                    assert(pcibios_ReadConfig_Byte(&cfg, PCIR_INTR_LN) == 0xFF);
                    pcibios_WriteConfig_Byte(&cfg, PCIR_INTR_LN, irq);
                }
            }
            else //TODO: if PCI_SET_INTERRUPT failed, just return the one in routing options, it might work?
                irq = 0xFF;
        }
    }

    pds_dpmi_dos_freemem(&dosmem);
    return irq;
}
#undef BUFSIZE