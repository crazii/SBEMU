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
//function: memory handling (with ideas/routines from FFMPEG and MPlayer)

#include "mpxplay.h"

#ifdef NEWFUNC_ASM

//#define NEWFUNC_MEMORY_MMX 1 // use mmx based memcpy

static void pds_memcpy_x86(void *addr_dest,const void *addr_src,unsigned int len);

#ifdef NEWFUNC_MEMORY_MMX
//#define NEWFUNC_MEMORY_SSE 1 // doesn't work properly on my Athlon XP
static void pds_memcpy_mmx_1(void *addr_dest,const void *addr_src,unsigned int len);
static void pds_memcpy_mmx_3dnow(void *addr_dest,const void *addr_src,unsigned int len);
static void pds_memcpy_mmx_ext(void *addr_dest,const void *addr_src,unsigned int len);
static void pds_memcpy_sse(void *addr_dest,const void *addr_src,unsigned int len);
#endif // NEWFUNC_MEMORY_MMX

typedef void (*memcpy_func_t)(void *addr_dest,const void *addr_src,unsigned int len);
static memcpy_func_t selected_memcpy_func=&pds_memcpy_x86;
#endif // NEWFUNC_ASM

void newfunc_memory_init(void)
{
#ifdef NEWFUNC_MEMORY_MMX
 int cap_mm;

 newfunc_cpu_init();

 cap_mm=mpxplay_cpu_capables_mm;

 // speed order (slowest first, fastest last)
 if(cap_mm&CPU_X86_MMCAP_MMX){
  selected_memcpy_func=&pds_memcpy_mmx_1;
 }
 if(cap_mm&CPU_X86_MMCAP_3DNOW){
  selected_memcpy_func=&pds_memcpy_mmx_3dnow;
 }
 if(cap_mm&CPU_X86_MMCAP_MMXEXT){
  selected_memcpy_func=&pds_memcpy_mmx_ext;
 }
#ifdef NEWFUNC_MEMORY_SSE
 if(cap_mm&CPU_X86_MMCAP_SSE){
  selected_memcpy_func=&pds_memcpy_sse;
 }
#endif //NEWFUNC_MEMORY_SSE
#endif //NEWFUNC_MEMORY_MMX
}

#ifdef NEWFUNC_ASM

void pds_memcpy(void *addr_dest,const void *addr_src,unsigned int len)
{
 (selected_memcpy_func)(addr_dest,addr_src,len);
}

// !!! overwrites the original memcpy and memset
/*void *memcpy(void *addr_dest,void *addr_src,unsigned int len)
{
 (selected_memcpy_func)(addr_dest,addr_src,len);
 return addr_dest;
}*/

//------------------------------------------------------------------

void asm_memcpy_x86(void *,const void *,unsigned int);

static void pds_memcpy_x86(void *addr_dest,const void *addr_src,unsigned int len)
{
#pragma aux asm_memcpy_x86=\
 "cld"\
 "mov edi,eax"\
 "mov esi,edx"\
 "mov ecx,ebx"\
 "shr ecx,2"\
 "rep movsd"\
 "mov ecx,ebx"\
 "and ecx,3"\
 "rep movsb"\
 parm[eax][edx][ebx] modify[ecx edi esi];
 asm_memcpy_x86(addr_dest,addr_src,len);
}

#ifdef NEWFUNC_MEMORY_MMX

void asm_memcpy_mmx_1(void *,const void *,unsigned int);

static void pds_memcpy_mmx_1(void *addr_dest,const void *addr_src,unsigned int len)
{
 #pragma aux asm_memcpy_mmx_1=\
 "cld"\
 "mov esi,edx"\
 "mov edi,eax"\
 "cmp ebx,72"\
 "jb x86copy"\
  "and eax,7"\
  "jz nodelta"\
   "mov ecx,8"\
   "sub ecx,eax"\
   "sub ebx,ecx"\
   "rep movsb"\
  "nodelta:"\
  "mov eax,ebx"\
  "shr eax,6"\
  "back1:"\
   "movq mm0,[esi   ]"\
   "movq mm1,[esi+ 8]"\
   "movq mm2,[esi+16]"\
   "movq mm3,[esi+24]"\
   "movq mm4,[esi+32]"\
   "movq mm5,[esi+40]"\
   "movq mm6,[esi+48]"\
   "movq mm7,[esi+56]"\
   "add esi,64"\
   "movq [edi   ],mm0"\
   "movq [edi+ 8],mm1"\
   "movq [edi+16],mm2"\
   "movq [edi+24],mm3"\
   "movq [edi+32],mm4"\
   "movq [edi+40],mm5"\
   "movq [edi+48],mm6"\
   "movq [edi+56],mm7"\
   "add edi,64"\
   "dec eax"\
  "jnz back1"\
  "emms"\
  "and ebx,63"\
  "jz end"\
 "x86copy:"\
  "mov ecx,ebx"\
  "shr ecx,2"\
  "rep movsd"\
  "mov ecx,ebx"\
  "and ecx,3"\
  "rep movsb"\
 "end:"\
 parm[eax][edx][ebx] modify[eax ebx ecx edx edi esi];
 asm_memcpy_mmx_1(addr_dest,addr_src,len);
}

void asm_memcpy_mmx_3dnow(void *,const void *,unsigned int);

static void pds_memcpy_mmx_3dnow(void *addr_dest,const void *addr_src,unsigned int len)
{
 #pragma aux asm_memcpy_mmx_3dnow=\
 "cld"\
 "mov esi,edx"\
 "mov edi,eax"\
 "cmp ebx,72"\
 "jb x86copy"\
  "and eax,7"\
  "jz nodelta"\
   "mov ecx,8"\
   "sub ecx,eax"\
   "sub ebx,ecx"\
   "rep movsb"\
  "nodelta:"\
  "mov eax,ebx"\
  "shr eax,6"\
  "back1:"\
   "prefetch 320[esi]"\
   "movq mm0,[esi   ]"\
   "movq mm1,[esi+ 8]"\
   "movq mm2,[esi+16]"\
   "movq mm3,[esi+24]"\
   "movq mm4,[esi+32]"\
   "movq mm5,[esi+40]"\
   "movq mm6,[esi+48]"\
   "movq mm7,[esi+56]"\
   "add esi,64"\
   "movq [edi   ],mm0"\
   "movq [edi+ 8],mm1"\
   "movq [edi+16],mm2"\
   "movq [edi+24],mm3"\
   "movq [edi+32],mm4"\
   "movq [edi+40],mm5"\
   "movq [edi+48],mm6"\
   "movq [edi+56],mm7"\
   "add edi,64"\
   "dec eax"\
  "jnz back1"\
  "femms"\
  "and ebx,63"\
  "jz end"\
 "x86copy:"\
  "mov ecx,ebx"\
  "shr ecx,2"\
  "rep movsd"\
  "mov ecx,ebx"\
  "and ecx,3"\
  "rep movsb"\
 "end:"\
 parm[eax][edx][ebx] modify[eax ebx ecx edx edi esi];
 asm_memcpy_mmx_3dnow(addr_dest,addr_src,len);
}

void asm_memcpy_mmx_ext(void *,const void *,unsigned int);

static void pds_memcpy_mmx_ext(void *addr_dest,const void *addr_src,unsigned int len)
{
 #pragma aux asm_memcpy_mmx_ext=\
 "cld"\
 "mov esi,edx"\
 "mov edi,eax"\
 "cmp ebx,72"\
 "jb x86copy"\
  "and eax,7"\
  "jz nodelta"\
   "mov ecx,8"\
   "sub ecx,eax"\
   "sub ebx,ecx"\
   "rep movsb"\
  "nodelta:"\
  "mov eax,ebx"\
  "shr eax,6"\
  "back1:"\
   "prefetchnta 320[esi]"\
   "movq mm0,[esi   ]"\
   "movq mm1,[esi+ 8]"\
   "movq mm2,[esi+16]"\
   "movq mm3,[esi+24]"\
   "movq mm4,[esi+32]"\
   "movq mm5,[esi+40]"\
   "movq mm6,[esi+48]"\
   "movq mm7,[esi+56]"\
   "add esi,64"\
   "movntq [edi   ],mm0"\
   "movntq [edi+ 8],mm1"\
   "movntq [edi+16],mm2"\
   "movntq [edi+24],mm3"\
   "movntq [edi+32],mm4"\
   "movntq [edi+40],mm5"\
   "movntq [edi+48],mm6"\
   "movntq [edi+56],mm7"\
   "add edi,64"\
   "dec eax"\
  "jnz back1"\
  "sfence"\
  "emms"\
  "and ebx,63"\
  "jz end"\
 "x86copy:"\
  "mov ecx,ebx"\
  "shr ecx,2"\
  "rep movsd"\
  "mov ecx,ebx"\
  "and ecx,3"\
  "rep movsb"\
 "end:"\
 parm[eax][edx][ebx] modify[eax ebx ecx edx edi esi];
 asm_memcpy_mmx_ext(addr_dest,addr_src,len);
}

#ifdef NEWFUNC_MEMORY_SSE
void asm_memcpy_sse(void *,const void *,unsigned int);

static void pds_memcpy_sse(void *addr_dest,const void *addr_src,unsigned int len)
{
#pragma aux asm_memcpy_sse=\
 "cld"\
 "mov esi,edx"\
 "mov edi,eax"\
 "cmp ebx,72"\
 "jb x86copy"\
  "and eax,15"\
  "jz nodelta"\
   "mov ecx,16"\
   "sub ecx,eax"\
   "sub ebx,ecx"\
   "rep movsb"\
  "nodelta:"\
  "mov eax,ebx"\
  "shr eax,6"\
  "test esi,15"\
  "jz back_ap"\
   "back_up:"\
    "prefetch 320[esi]"\
    "movups xmm0,[esi   ]"\
    "movups xmm1,[esi+16]"\
    "movups xmm2,[esi+32]"\
    "movups xmm3,[esi+48]"\
    "add esi,64"\
    "movntps [edi   ],xmm0"\
    "movntps [edi+16],xmm1"\
    "movntps [edi+32],xmm2"\
    "movntps [edi+48],xmm3"\
    "add edi,64"\
    "dec eax"\
   "jnz back_up"\
  "jmp do_left"\
   "back_ap:"\
    "prefetch 320[esi]"\
    "movaps xmm0,[esi   ]"\
    "movaps xmm1,[esi+16]"\
    "movaps xmm2,[esi+32]"\
    "movaps xmm3,[esi+48]"\
    "add esi,64"\
    "movntps [edi   ],xmm0"\
    "movntps [edi+16],xmm1"\
    "movntps [edi+32],xmm2"\
    "movntps [edi+48],xmm3"\
    "add edi,64"\
    "dec eax"\
   "jnz back_ap"\
  "do_left:"\
  "and ebx,63"\
  "jz end"\
 "x86copy:"\
  "mov ecx,ebx"\
  "shr ecx,2"\
  "rep movsd"\
  "mov ecx,ebx"\
  "and ecx,3"\
  "rep movsb"\
 "end:"\
 parm[eax][edx][ebx] modify[eax ebx ecx edx edi esi];
 asm_memcpy_sse(addr_dest,addr_src,len);
}
#endif //NEWFUNC_MEMORY_SSE

#endif //NEWFUNC_MEMORY_MMX

//------------------------------------------------------------------

void asm_memset(void *,int,unsigned int);

void pds_memset(void *addr,int num,unsigned int len)
{
#pragma aux asm_memset=\
 "cld"\
 "mov edi,eax"\
 "mov dh,dl"\
 "mov eax,edx"\
 "bswap eax"\
 "mov ax,dx"\
 "mov ecx,ebx"\
 "shr ecx,2"\
 "rep stosd"\
 "mov ecx,ebx"\
 "and ecx,3"\
 "rep stosb"\
 parm[eax][edx][ebx] modify[eax ecx edx edi];
 asm_memset(addr,num,len);
}

void asm_qmemreset(void *,unsigned int);

void pds_qmemreset(void *addr,unsigned int len)
{
 #pragma aux asm_qmemreset=\
 "cld"\
 "mov edi,eax"\
 "mov ecx,edx"\
 "xor eax,eax"\
 "rep stosd"\
 parm[eax][edx] modify[eax ecx edi];
 asm_qmemreset(addr,len);
}

void asm_qmemcpy(void *,void *,unsigned int);

void pds_qmemcpy(void *addr_dest,void *addr_src,unsigned int len)
{
 #pragma aux asm_qmemcpy=\
 "cld"\
 "mov edi,eax"\
 "mov esi,edx"\
 "mov ecx,ebx"\
 "rep movsd"\
 parm[eax][edx][ebx] modify[ecx edi esi];
 asm_qmemcpy(addr_dest,addr_src,len);
}

void asm_qmemcpyr(void *,void *,unsigned int);

void pds_qmemcpyr(void *addr_dest,void *addr_src,unsigned int len)
{
 #pragma aux asm_qmemcpyr=\
 "std"\
 "mov edi,eax"\
 "mov esi,edx"\
 "mov ecx,ebx"\
 "dec ebx"\
 "shl ebx,2"\
 "add edi,ebx"\
 "add esi,ebx"\
 "rep movsd"\
 "cld"\
 parm[eax][edx][ebx] modify[ebx ecx edi esi];
 asm_qmemcpyr(addr_dest,addr_src,len);
}

/*void asm_memxch(char *,char *,unsigned int);
// !!! bad (byte copy, first part)
void pds_memxch(char *addr1,char *addr2,unsigned int len)
{
#pragma aux asm_memxch=\
 "mov ecx,ebx"\
 "and ecx,3"\
 "jz jump1"\
 "back1:"\
  "mov edi,dword ptr [eax]"\
  "mov esi,dword ptr [edx]"\
  "mov dword ptr [edx],edi"\
  "mov dword ptr [eax],esi"\
  "inc edx"\
  "inc eax"\
  "dec ecx"\
 "jnz back1"\
 "jump1:"\
 "shr ebx,2"\
 "mov ecx,4"\
 "back2:"\
  "mov edi,dword ptr [eax]"\
  "mov esi,dword ptr [edx]"\
  "mov dword ptr [edx],edi"\
  "mov dword ptr [eax],esi"\
  "add edx,ecx"\
  "add eax,ecx"\
  "dec ebx"\
 "jnz back2"\
 parm[eax][edx][ebx] modify[eax edx ebx ecx edi esi];
 asm_memxch(addr1,addr2,len);
}

#else

void pds_memxch(char *addr1,char *addr2,unsigned int len)
{
 while(len--){
  char tmp1=*addr1,tmp2=*addr2;
  *addr1=tmp2;
  *addr2=tmp1;
  addr1++;addr2++;
 }
}*/

#else  // NEWFUNC_ASM

#define MPXPLAY_MEM_ARCH_MASK   (sizeof(mpxp_ptrsize_t) - 1)
#define MPXPLAY_MEM_ARCH_SHIFT  ((sizeof(mpxp_ptrsize_t) == 8)? 3 : 2) // 2^3=8 or 2^2=4 bytes architecture size

void pds_memcpy(void *dest_ptr, const void *src_ptr, unsigned int len_bytes)
{
 register char *dest = dest_ptr;
 register const char *src = src_ptr;
 register unsigned int len = len_bytes;
 register unsigned int arch_len = len >> MPXPLAY_MEM_ARCH_SHIFT;

 if(!dest || !src)
  return;

 if(arch_len){
  do{
   *((mpxp_ptrsize_t *)dest) = *((mpxp_ptrsize_t *)src);
   dest += sizeof(mpxp_ptrsize_t); src += sizeof(mpxp_ptrsize_t);
  }while(--arch_len);
  len &= MPXPLAY_MEM_ARCH_MASK;
 }

 if(len){
  do{
   *dest ++ = *src++;
  }while(--len);
 }
}

int pds_memcmp(const void *dest_ptr, const void *src_ptr, unsigned int len_bytes)
{
 register const char *dest = dest_ptr;
 register const char *src = src_ptr;
 register unsigned int len = len_bytes;
 register unsigned int arch_len = len >> MPXPLAY_MEM_ARCH_SHIFT;

 if(!dest || !src)
  return -1;

 if(arch_len){
  do{
   if(*((mpxp_ptrsize_t *)dest) != *((mpxp_ptrsize_t *)src))
    return 1;
   dest += sizeof(mpxp_ptrsize_t); src += sizeof(mpxp_ptrsize_t);
  }while(--arch_len);
  len &= MPXPLAY_MEM_ARCH_MASK;
 }

 if(len){
  do{
   if(*dest != *src)
    return 1;
   dest++; src++;
  }while(--len);
 }

 return 0;
}

#endif // NEWFUNC_ASM

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

void pds_mem_reverse(char *addr,unsigned int len)
{
 char *end;
 if(!addr || (len<2))
  return;
 end=addr+len-1;
 len>>=1;
 do{
  char c=*addr;
  *addr=*end;
  *end=c;
  addr++;end--;
 }while(--len);
}

#ifdef MPXPLAY_USE_IXCH_SMP

void pds_smp_memcpy(char *addr_dest,char *addr_src,unsigned int len)
{
 unsigned int i=len>>2;
 LONG *ad=(LONG *)addr_dest,*as=(LONG *)addr_src;
 while(i){
  funcbit_smp_int32_put(ad[0],as[0]);
  ad++;as++;i--;
 }
 if((len>=4) && (len&3)){
  addr_dest+=len-4;
  addr_src+=len-4;
  funcbit_smp_int32_put(addr_dest[0],*((LONG*)addr_src));
 }
}

void pds_smp_memxch(char *addr1,char *addr2,unsigned int len)
{
 unsigned int i=(len+3)>>2; // !!! 4 bytes boundary
 LONG *a1=(LONG *)addr1,*a2=(LONG *)addr2;
 while(i){
  LONG value1=*a1;
  funcbit_smp_int32_put(a1[0],*a2);
  funcbit_smp_int32_put(a2[0],value1);
  a1++;a2++;
  i--;
 }
 /*if((len>=4) && (len&3)){
  LONG value1;
  addr1+=len-4;
  addr2+=len-4;
  value1=*((LONG *)addr1);
  funcbit_smp_int32_put(addr1,(LONG*)addr2);
  funcbit_smp_int32_put(addr2,value1);
 }*/
}

void pds_smp_memset(char *addr,int value,unsigned int len)
{
 unsigned int i=len>>2;
 LONG *ad=(LONG *)addr;
 LONG val=value;
 val|=(val<<8);
 val|=(val<<16);
 while(i){
  funcbit_smp_int32_put(ad[0],val);
  ad++;i--;
 }
 if((len>=4) && (len&3)){
  addr+=len-4;
  funcbit_smp_int32_put(addr[0],val);
 }
}

#endif // MPXPLAY_USE_IXCH_SMP

#include <malloc.h>

//extern unsigned int intsoundconfig,intsoundcontrol;

// can be different (more safe) than the normal malloc
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
