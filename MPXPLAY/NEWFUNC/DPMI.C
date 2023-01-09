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

#endif // __DOS__
