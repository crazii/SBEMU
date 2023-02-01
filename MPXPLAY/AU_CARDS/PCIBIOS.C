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

#include "newfunc\newfunc.h"
#include "pcibios.h"

#define PCIDEVNUM(bParam)      (bParam >> 3)
#define PCIFUNCNUM(bParam)     (bParam & 0x07)
#define PCIDEVFUNC(bDev,bFunc) ((((uint32_t)bDev) << 3) | bFunc)

#define pcibios_clear_regs(reg) pds_memset(&reg,0,sizeof(reg))

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
