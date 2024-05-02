//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2015 by PDSoft (Attila Padar)                *
//*                 http://mpxplay.sourceforge.net                         *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//Collection of depedencies used by AU_CARDS

#include "au_base.h"
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#if defined(__GNUC__)
#include <sys/time.h>
#endif
#ifdef DJGPP
#include <dpmi.h>
#include <sys/exceptn.h>
extern unsigned long __djgpp_selector_limit;

//the djgpp disable/enable uses CLI/STI, change it to dpmi std method.
#define disable __dpmi_get_and_disable_virtual_interrupt_state
#define enable __dpmi_get_and_enable_virtual_interrupt_state

#endif

//dummy symbol to keep original code unmodified
#ifdef __DOS__
#include <dos.h>
//void (__far *oldint08_handler)();
#define oldint08_handler 0
volatile unsigned long int08counter;
unsigned long mpxplay_signal_events;
#endif

//DPMI
#ifdef DJGPP
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
    //_go32_interrupt_stack_size = 4096; //512 minimal
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
 memsize = (memsize+0xFFF)&~0xFFF;
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
    if(info.address < base && 0) //not needed, limit will be expanded for the overflow
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
        remap.size = memsize/4096;
        if(__dpmi_map_device_in_memory_block(&remap, phys_addr) != 0)
        {
            __dpmi_free_memory(info.handle);
            return 0;
        }
        info.size = 0;
    }
    else
        info.size = info.address;

    info.address -= base;
    physicalmaps[i] = info;
    unsigned long newlimit = info.address + memsize - 1;
    newlimit = ((newlimit+1+0xFFF)&~0xFFF)-1;//__dpmi_set_segment_limit need page aligned
    int intr = disable();
    __dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
    __dpmi_set_segment_limit(_my_cs(), max(limit, newlimit));
    __dpmi_set_segment_limit(__djgpp_ds_alias, max(limit, newlimit));
    __djgpp_selector_limit = max(limit, newlimit);
    if (intr) enable();
    return info.address;
 }
 return 0;
}

void pds_dpmi_unmap_physycal_memory(unsigned long offset32)
{
 int i = 0;
 for(; i < PHYSICAL_MAP_COUNT; ++i)
 {
    if(physicalmaps[i].address == offset32)
        break;
 }
 if(i >= PHYSICAL_MAP_COUNT)
    return;
if(physicalmaps[i].size != 0)
{
    physicalmaps[i].address = physicalmaps[i].size;
    __dpmi_free_physical_address_mapping(&physicalmaps[i]);
}
else
    __dpmi_free_memory(physicalmaps[i].handle);
 physicalmaps[i].handle = 0;
 physicalmaps[i].address = 0;
 physicalmaps[i].size = 0;
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
    pds_xms_regs.x.ax = pds_xms_regs.x.dx = 0;
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
        r.x.dx = handle;
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
    size = ((size+0xFFF)&~0xFFF) + 4096; //align to 4K
    if( (mem->xms=pds_xms_alloc(size/1024, &addr)) )
    {
        addr = ((addr+0xFFF)&~0xFFF);

        unsigned long base = 0;
        unsigned long limit = __dpmi_get_segment_limit(_my_ds());
        __dpmi_get_segment_base_address(_my_ds(), &base);
        __dpmi_meminfo info = {0, size, addr};
        mem->remap = 0;
        do {
            if( __dpmi_physical_address_mapping(&info) == 0 )
            {
                if(info.address < base && 0) //not needed, limit will be expanded for the overflow
                {
                    printf("DPMI remap base address %x\n", addr);
                    __dpmi_free_physical_address_mapping(&info);
                    //info.address = base + limit + 1;
                    info.address = 0;
                    info.size = size;
                    if(__dpmi_allocate_linear_memory(&info, 0) != 0)
                    {
                        printf("DPMI Failed allocate linear memory.\n");
                        break;
                    }
                    __dpmi_meminfo remap = info;
                    remap.address = 0;
                    remap.size /= 4096;
                    if(__dpmi_map_device_in_memory_block(&remap, addr) != 0)
                        break;
                    mem->remap = 1;
                    mem->handle = info.handle;
                }
                else
                {
                    mem->remap = 0;
                    mem->handle = info.address;
                }
                mem->physicalptr = (char*)addr;
                mem->linearptr = (char*)(info.address - base);
                unsigned long newlimit = info.address + size - base - 1;
                newlimit = ((newlimit+1+0xFFF)&~0xFFF) - 1;//__dpmi_set_segment_limit must be page aligned
                //printf("addr: %08x, limit: %08x\n",mem->linearptr, newlimit);
                int intr = disable();
                __dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
                __dpmi_set_segment_limit(_my_cs(), max(limit, newlimit));
                __dpmi_set_segment_limit(__djgpp_ds_alias, max(limit, newlimit));
                __djgpp_selector_limit = max(limit, newlimit);
                if (intr) enable();
                return 1;
            }
        }while(0);
        pds_xms_free(mem->xms);
        mem->xms = 0;
    }
    printf("Failed allocating XMS.\n");
    return 0;
}

void pds_dpmi_xms_freemem(xmsmem_t * mem)
{
    if(mem->remap)
        __dpmi_free_memory(mem->handle);
    else
    {
        __dpmi_meminfo info = {0, 0, mem->handle};
        __dpmi_free_physical_address_mapping(&info);
    }
    pds_xms_free(mem->xms);
}
#endif

//strings
unsigned int pds_strcpy(char *dest,char *src)
{
 char *begin;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(1);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strmove(char *dest,char *src)
{
 unsigned int len,count;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 if(dest<src)
  return pds_strcpy(dest,src);
 count=len=pds_strlen(src)+1;
 src+=len;
 dest+=len;
 do{
  src--;dest--;
  *dest=*src;
 }while(--count);
 return len; // returns the lenght of string
}

unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen)
{
 char *begin;
 if(!dest || !maxlen)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(--maxlen);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strcat(char *strp1,char *strp2)
{
 if(!strp1 || !strp2)
  return 0;
 return pds_strcpy(&strp1[pds_strlen(strp1)],strp2);
}

static int pds_strchknull(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;

 if(!s1 || !s1[0])
 {
  if(s2 && s2[0])
   return -1;
  else
   return 0;
 }

 if(!s2 || !s2[0])
 {
  if(s1 && s1[0])
   return 1;
  else
   return 0;
 }
 return 2;
}

int pds_strcmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

 do{
   c1 = (unsigned char) *s1++;
   c2 = (unsigned char) *s2++;
   if(!c1)
    break;
 }while (c1 == c2);

 return c1 - c2;
}

int pds_stricmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

 do{
  c1 = (unsigned char) *s1++;
  c2 = (unsigned char) *s2++;
  if(!c1)
   break;
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;                // c1-='a'-'A'
  if(c2>='a' && c2<='z')
   c2-=32;
 }while(c1 == c2);
 return (c1 - c2);
}

//faster (no pointer check), returns 1 if equal
unsigned int pds_stri_compare(char *strp1,char *strp2)
{
 char c1,c2;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;                // c1-='a'-'A'
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2)
    return 0;
  }
  strp1++;strp2++;
 }while(c1 && c2);
 return 1;
}

int pds_strricmp(char *str1,char *str2)
{
 char *pstr1=str1,*pstr2=str2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 while(pstr1[0]!=0)
  pstr1++;
 while(pstr1[0]==0 || pstr1[0]==32)
  pstr1--;
 if(pstr1<=str1)
  return 1;
 while(pstr2[0]!=0)
  pstr2++;
 while(pstr2[0]==0 || pstr2[0]==32)
  pstr2--;
 if(pstr2<=str2)
  return -1;
 while(pstr1>=str1 && pstr2>=str2){
  char c1=pstr1[0];
  char c2=pstr2[0];
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;
  if(c2>='a' && c2<='z')
   c2-=32;
  if(c1!=c2){
   if(c1<c2)
    return -1;
   else
    return 1;
  }
  pstr1--;pstr2--;
 }
 return 0;
}

int pds_strlicmp(char *str1,char *str2)
{
 char c1,c2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 do{
  c1=*str1;
  c2=*str2;
  if(!c1 || !c2)
   break;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  str1++;str2++;
 }while(1);
 return 0;
}

int pds_strncmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2)
  {
   if(c1<c2)
    return -1;
   else
    return 1;
  }
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

int pds_strnicmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

unsigned int pds_strlen(char *strp)
{
 char *beginp;
 if(!strp || !strp[0])
  return 0;
 beginp=strp;
 do{
  strp++;
 }while(*strp);
 return (unsigned int)(strp-beginp);
}

unsigned int pds_strlenc(char *strp,char seek)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0])
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}

char *pds_strchr(char *strp,char seek)
{
 if(!strp)
  return NULL;
 do{
  char c=strp[0];
  if(c==seek)
   return strp;
  if(!c)
   break;
  strp++;
 }while(1);
 return NULL;
}

char *pds_strrchr(char *strp,char seek)
{
 char *foundp=NULL,curr;

 if(!strp)
  return foundp;

 curr=*strp;
 if(!curr)
  return foundp;
 do{
  if(curr==seek)
   foundp=strp;
  strp++;
  curr=*strp;
 }while(curr);
 return foundp;
}

char *pds_strnchr(char *strp,char seek,unsigned int len)
{
 if(!strp || !strp[0] || !len)
  return NULL;
 do{
  if(*strp==seek)
   return strp;
  strp++;
 }while(*strp && --len);
 return NULL;
}

char *pds_strstr(char *s1,char *s2)
{
 if(s1 && s2 && s2[0]){
  char c20=*s2;
  do{
   char c1=*s1;
   if(!c1)
    break;
   if(c1==c20){        // search the first occurence
    char *s1p=s1,*s2p=s2;
    do{                 // compare the strings (part of s1 with s2)
     char c2=*(++s2p);
     if(!c2)
      return s1;
     c1=*(++s1p);
     if(!c1)
      return NULL;
     if(c1!=c2)
      break;
    }while(1);
   }
   s1++;
  }while(1);
 }
 return NULL;
}

char *pds_strstri(char *s1,char *s2)
{
 if(s1 && s2 && s2[0]){
  char c20=*s2;
  if(c20>='a' && c20<='z')  // convert to uppercase (first character of s2)
   c20-=32;
  do{
   char c1=*s1;
   if(!c1)
    break;
   if(c1>='a' && c1<='z')  // convert to uppercase (current char of s1)
    c1-=32;
   if(c1==c20){        // search the first occurence
    char *s1p=s1,*s2p=s2;
    do{                 // compare the strings (part of s1 with s2)
     char c2;
     s2p++;
     c2=*s2p;
     if(!c2)
      return s1;
     s1p++;
     c1=*s1p;
     if(!c1)
      return NULL;
     if(c1>='a' && c1<='z')  // convert to uppercase
      c1-=32;
     if(c2>='a' && c2<='z')  // convert to uppercase
      c2-=32;
     if(c1!=c2)
      break;
    }while(1);
   }
   s1++;
  }while(1);
 }
 return NULL;
}

unsigned int pds_strcutspc(char *src)
{
 char *dest,*dp;

 if(!src)
  return 0;

 dest=src;

 while(src[0] && (src[0]==32))
  src++;

 if(!src[0]){
  dest[0]=0;
  return 0;
 }
 if(src>dest){
  char c;
  dp=dest;
  do{
   c=*src++; // move
   *dp++=c;  //
  }while(c);
  dp-=2;
 }else{
  while(src[1])
   src++;
  dp=src;
 }
 while((*dp==32) && (dp>=dest))
  *dp--=0;

 if(dp<dest)
  return 0;

 return (dp-dest+1);
}

// some filename/path routines (string handling only, no DOS calls)
#define PDS_GETFILENAME_MODE_VIRTNAME (1 << 0) // get short virtual name (without http:// pretag) if no filename

static char *pds_getfilename_from_fullname_modeselect(char *fullname, unsigned int getfilename_mode)
{
 char *filenamep, *beginp, *virtual_drive = NULL;

 if(!fullname)
  return fullname;

 beginp = pds_strchr(fullname,':');
 if(beginp)
  beginp++;
 else
  beginp=fullname;
 if( ((beginp[0]==PDS_DIRECTORY_SEPARATOR_CHAR) && (beginp[1]==PDS_DIRECTORY_SEPARATOR_CHAR)) // virtual drive (like http://)
  || ((beginp[0]==PDS_DIRECTORY_SEPARATOR_CHRO) && (beginp[1]==PDS_DIRECTORY_SEPARATOR_CHRO)))
 {
  while((*beginp == PDS_DIRECTORY_SEPARATOR_CHAR) || (*beginp == PDS_DIRECTORY_SEPARATOR_CHRO))
   beginp++;
  virtual_drive = beginp;
 }
 if(!*beginp)
  return NULL;
 filenamep = pds_strrchr(beginp, PDS_DIRECTORY_SEPARATOR_CHAR);
 if(!filenamep)
  filenamep = pds_strrchr(beginp, PDS_DIRECTORY_SEPARATOR_CHRO);  // for http
 if(filenamep && filenamep[1])  // normal path/filename
  filenamep++;
 else if(virtual_drive && (getfilename_mode & PDS_GETFILENAME_MODE_VIRTNAME))  // no filename, use http address
  filenamep = virtual_drive;
 else if(virtual_drive)
  filenamep = NULL;
 else if(filenamep)
  filenamep++;
 else{
  filenamep = fullname;
  if((fullname[1]==':') && fullname[2]) // cut drive
   filenamep += 2;
 }

 return filenamep;
}

// returns NULL if no filename found
char *pds_getfilename_from_fullname(char *fullname)
{
 return pds_getfilename_from_fullname_modeselect(fullname, 0);
}

//memory
void pds_memxch(char *addr1,char *addr2,unsigned int len)
{
 if(!addr1 || !addr2 || !len)
  return;
 do{
  char tmp1=*addr1,tmp2=*addr2;
  *addr1=tmp2;
  *addr2=tmp1;
  addr1++;addr2++;
 }while(--len);
}

void *pds_malloc(unsigned int bufsize)
{
 //unsigned int intsoundcntrl_save;
 void *bufptr;
 if(!bufsize)
  return NULL;
 //MPXPLAY_INTSOUNDDECODER_DISALLOW;
 //_disable();
 bufptr=malloc(bufsize + 8);
 //_enable();
 //MPXPLAY_INTSOUNDDECODER_ALLOW;
 return bufptr;
}

void *pds_zalloc(unsigned int bufsize)
{
 //unsigned int intsoundcntrl_save;
 void *bufptr;
 if(!bufsize)
  return NULL;
 //MPXPLAY_INTSOUNDDECODER_DISALLOW;
 //_disable();
 bufptr=malloc(bufsize + 8);
 memset(bufptr, 0, bufsize+8);
 //_enable();
 //MPXPLAY_INTSOUNDDECODER_ALLOW;
 return bufptr;
}

void *pds_calloc(unsigned int nitems,unsigned int itemsize)
{
 //unsigned int intsoundcntrl_save;
 void *bufptr;
 if(!nitems || !itemsize)
  return NULL;
 //MPXPLAY_INTSOUNDDECODER_DISALLOW;
 //_disable();
 bufptr=calloc(nitems,(itemsize + 8));
 //_enable();
 //MPXPLAY_INTSOUNDDECODER_ALLOW;
 return bufptr;
}

void *pds_realloc(void *bufptr,unsigned int bufsize)
{
 //unsigned int intsoundcntrl_save;
 //MPXPLAY_INTSOUNDDECODER_DISALLOW;
 //_disable();
 bufptr=realloc(bufptr,(bufsize + 8));
 //_enable();
 //MPXPLAY_INTSOUNDDECODER_ALLOW;
 return bufptr;
}

void pds_free(void *bufptr)
{
 //unsigned int intsoundcntrl_save;
 if(bufptr){
  //MPXPLAY_INTSOUNDDECODER_DISALLOW;
  //_disable();
  free(bufptr);
  //_enable();
  //MPXPLAY_INTSOUNDDECODER_ALLOW;
 }
}

//bits
static void cv_8bits_unsigned_to_signed(PCM_CV_TYPE_S *pcm,unsigned int samplenum)
{
 PCM_CV_TYPE_UC *inptr=(PCM_CV_TYPE_UC *)pcm;

 do{
  PCM_CV_TYPE_I insamp=(PCM_CV_TYPE_I)*inptr;
  insamp-=128;
  *((PCM_CV_TYPE_C *)inptr)=(PCM_CV_TYPE_C)insamp;
  inptr++;
 }while(--samplenum);
}

static void cv_8bits_signed_to_unsigned(PCM_CV_TYPE_S *pcm,unsigned int samplenum)
{
 PCM_CV_TYPE_C *inptr=(PCM_CV_TYPE_C *)pcm;

 do{
  PCM_CV_TYPE_I insamp=(PCM_CV_TYPE_I)*inptr;
  insamp+=128;
  *((PCM_CV_TYPE_UC *)inptr)=(PCM_CV_TYPE_UC)insamp;
  inptr++;
 }while(--samplenum);
}

//compress (32->24,32->16,32->8,24->16,24->8,16->8)
static void cv_bits_down(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int instep,unsigned int outstep)
{
 PCM_CV_TYPE_C *inptr=(PCM_CV_TYPE_C *)pcm;
 PCM_CV_TYPE_C *outptr=(PCM_CV_TYPE_C *)pcm;
 unsigned int skip=instep-outstep;

 do{
  unsigned int oi=outstep;
  inptr+=skip;
  do{
   *outptr=*inptr;
   outptr++;inptr++;
  }while(--oi);
 }while(--samplenum);
}

//expand (8->16,8->24,8->32,16->24,16->32,24->32)
static void cv_bits_up(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int instep,unsigned int outstep)
{
 PCM_CV_TYPE_C *inptr=(PCM_CV_TYPE_C *)pcm;
 PCM_CV_TYPE_C *outptr=(PCM_CV_TYPE_C *)pcm;
 inptr+=samplenum*instep;
 outptr+=samplenum*outstep;

 do{
  unsigned int ii=instep;
  unsigned int oi=outstep;
  do{    //copy upper bits (bytes) to the right/correct place
   inptr--;
   outptr--;
   *outptr=*inptr;
   oi--;
  }while(--ii);
  do{    //fill lower bits (bytes) with zeroes
   outptr--;
   *outptr=0;
  }while(--oi);
 }while(--samplenum);
}

void cv_bits_n_to_m(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_bytespersample)
{
 if(out_bytespersample>in_bytespersample){
  if(in_bytespersample==1)
   cv_8bits_unsigned_to_signed(pcm,samplenum);
  cv_bits_up(pcm,samplenum,in_bytespersample,out_bytespersample);
 }else{
  if(out_bytespersample<in_bytespersample){
   cv_bits_down(pcm,samplenum,in_bytespersample,out_bytespersample);
   if(out_bytespersample==1)
    cv_8bits_signed_to_unsigned(pcm,samplenum);
  }
 }
}

//channels
unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int newchannels,unsigned int bytespersample)
{
 register unsigned int i,ch,b;
 PCM_CV_TYPE_C *pcms=((PCM_CV_TYPE_C *)pcm_sample)+(samplenum*bytespersample);
 PCM_CV_TYPE_C *pcmt=((PCM_CV_TYPE_C *)pcm_sample)+(samplenum*bytespersample*newchannels);

 for(i=samplenum;i;i--){
  pcms-=bytespersample;
  for(ch=newchannels;ch;ch--){
   pcmt-=bytespersample;
   for(b=0;b<bytespersample;b++)
    pcmt[b]=pcms[b];
  }
 }
 return (samplenum*newchannels);
}

//sample rates
unsigned int mixer_speed_lq(PCM_CV_TYPE_S* dest, unsigned int destsample, const PCM_CV_TYPE_S* source, unsigned int sourcesample, unsigned int channels, unsigned int samplerate, unsigned int newrate)
{
 const unsigned int instep=((samplerate/newrate)<<12) | (((4096*(samplerate%newrate)-1)/(newrate-1))&0xFFF);
 const unsigned int inend=(sourcesample/channels - 1) << 12; //for n samples, interpolation n-1 steps
 int16_t *pcm; int16_t const* intmp;
 //unsigned long ipi;
 unsigned int inpos = 0;//(samplerate<newrate) ? instep/2 : 0;
 if(!sourcesample)
  return 0;
 assert(((sourcesample/channels)&0xFFF00000) == 0); //too many samples, need other approches.
 unsigned int buffcount = max(((unsigned long long)max(sourcesample,512)*newrate+samplerate-1)/samplerate,max(sourcesample,512))*2+32;
 assert(buffcount <= destsample);
 int16_t* buff = dest;

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "step: %08x, end: %08x\n", instep, inend);

 pcm = buff;
 intmp = source;
 //int total = sourcesample/channels;

 do{
  int m1,m2;
  unsigned int ipi,ch;
  const int16_t *intmp1,*intmp2;
  ipi = inpos >> 12;
  m2=inpos&0xFFF;
  m1=4096-m2;
  ch=channels;
  ipi*=ch;
  intmp1=intmp+ipi;
  intmp2=intmp1+ch;
  do{
   *pcm++= ((*intmp1++)*m1+(*intmp2++)*m2)/4096;// >> 12; //don't use shift, signed right shift impl defined, maybe logical shift
  }while(--ch);
  inpos+=instep;

  if(pcm - buff >= destsample) //check overflow
  {
    assert(FALSE);
    return pcm - buff;
  }
 }while(inpos<inend);

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "sample count: %d\n", pcm-buff);
 //_LOG("MIXER_SPEED_LQ: %d, %d\n", pcm-buff, buffcount);
 assert(pcm-buff <= buffcount);
 return pcm - buff;
}

//times
unsigned long pds_gettimeh(void)
{
 return ((unsigned long)clock()*100/CLOCKS_PER_SEC);
}

mpxp_int64_t pds_gettimem(void)
{
    mpxp_int64_t time_ms;
#ifdef __DOS__
    if(oldint08_handler){
        unsigned long tsc;
        _disable();
        outp(0x43, 0x04);
        tsc = inp(0x40);
        tsc += inp(0x40) << 8;
        _enable();
        if(tsc < INT08_DIVISOR_NEW)
            tsc = INT08_DIVISOR_NEW - tsc;
        else
            tsc = 0;
        time_ms = (mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000.0/(float)INT08_CYCLES_NEW);
        //fprintf(stderr,"time_ms:%d \n",time_ms);
    }else
#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tss;
    if(clock_gettime(CLOCK_MONOTONIC, &tss) == 0)
    {
        time_ms = (mpxp_int64_t)tss.tv_sec * (mpxp_int64_t)1000 + (mpxp_int64_t)(tss.tv_nsec / 1000000);
    }else
#endif
    time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000 / (mpxp_int64_t)CLOCKS_PER_SEC;

    return time_ms;
}

mpxp_int64_t pds_gettimeu(void)
{
    mpxp_int64_t time_ms;
#ifdef __DOS__
    if(oldint08_handler){
        unsigned long tsc;
        _disable();
        outp(0x43,0x04);
        tsc = inp(0x40);
        tsc += inp(0x40)<<8;
        _enable();
        if(tsc < INT08_DIVISOR_NEW)
            tsc = INT08_DIVISOR_NEW - tsc;
        else
            tsc = 0;
        time_ms = (mpxp_int64_t)(((float)int08counter+(float)tsc/(float)INT08_DIVISOR_NEW)*1000000.0/(float)INT08_CYCLES_NEW);
        //fprintf(stderr,"time_ms:%d \n",(long)time_ms);
    }else
#elif defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tss;
    if(clock_gettime(CLOCK_MONOTONIC, &tss) == 0)
    {
        time_ms = (mpxp_int64_t)tss.tv_sec * (mpxp_int64_t)1000000 + (mpxp_int64_t)(tss.tv_nsec / 1000);
    }else
#endif
    time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000000 / (mpxp_int64_t)CLOCKS_PER_SEC;

    return time_ms;
}

unsigned long pds_gettime(void)
{
 unsigned long timeval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 timeval=(t)? (t->tm_sec&63)|((t->tm_min&63)<<8)|((t->tm_hour&31)<<16) : 0;
 return timeval; // 0x00HHMMSS
}

unsigned long pds_getdate(void)
{
 unsigned long dateval;
 time_t timer;
 struct tm *t;
 timer=time(NULL);
 t=localtime(&timer);
 dateval=(t)? (t->tm_mday&31)|(((t->tm_mon+1)&15)<<8)|(((t->tm_year+1900)&65535)<<16) : 0;
 return dateval; // 0xYYYYMMDD
}

void pds_delay_10us(unsigned int ticks) //each tick is 10us
{
#ifdef __DOS__
 unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
 unsigned int i,oldtsc, tsctemp, tscdif;

 for(i=0;i<ticks;i++){
#ifdef DJGPP
  int intr = 
#endif
  _disable();
  outp(0x43,0x04);
  oldtsc=inp(0x40);
  oldtsc+=inp(0x40)<<8;
#ifdef DJGPP
  if(intr)
#endif
  {
   _enable();
  }

  do{
#ifdef DJGPP
  int intr2 = 
#endif
   _disable();
   outp(0x43,0x04);
   tsctemp=inp(0x40);
   tsctemp+=inp(0x40)<<8;
#ifdef DJGPP
  if(intr2)
#endif
  {
    _enable();
  }
   if(tsctemp<=oldtsc)
    tscdif=oldtsc-tsctemp; // handle overflow
   else
    tscdif=divisor+oldtsc-tsctemp;
  }while(tscdif<12); //wait for 10us  (12/(65536*18) sec)
 }
#else
 pds_mdelay((ticks+99)/100);
 //unsigned int oldclock=clock();
 //while(oldclock==clock()){} // 1ms not 0.01ms (10us)
#endif
}

void pds_delay_1695ns (unsigned int ticks) //each tick is approximately 1695ns
{
#ifdef __DOS__
 unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
 unsigned int i,oldtsc, tsctemp, tscdif;

 for(i=0;i<ticks;i++){
  disable();
  outp(0x43,0x04);
  oldtsc=inp(0x40);
  oldtsc+=inp(0x40)<<8;
  enable();

  do{
   disable();
   outp(0x43,0x04);
   tsctemp=inp(0x40);
   tsctemp+=inp(0x40)<<8;
   enable();
   if(tsctemp<=oldtsc)
    tscdif=oldtsc-tsctemp; // handle overflow
   else
    tscdif=divisor+oldtsc-tsctemp;
  }while(tscdif<2); //wait for 1695.421ns  (2/(65536*18) sec) // XXX
 }
#else
 pds_mdelay((ticks+99)/100);
 //unsigned int oldclock=clock();
 //while(oldclock==clock()){} // 1ms not 0.01ms (10us)
#endif
}

void pds_mdelay(unsigned long msec)
{
#ifdef __DOS__
 delay(msec);
#else
 pds_threads_sleep(msec);
#endif
}
