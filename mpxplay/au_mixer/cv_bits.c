//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: bit conversions

#include "mpxplay.h"

#define USE_ASM_CV_BITS 1

void cv_float16_to_int16(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int fpuround_chop);

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

void cv_n_bits_to_float(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_scalebits)
{
 PCM_CV_TYPE_F *outptr=((PCM_CV_TYPE_F *)pcm)+samplenum;
 const unsigned int inbits=in_bytespersample<<3;
 const float scalecorr=(out_scalebits>inbits)? ((float)(1UL<<(out_scalebits-inbits))):(1.0f/(float)(1UL<<(inbits-out_scalebits)));

 if(inbits==8){
  PCM_CV_TYPE_UC *inptr=((PCM_CV_TYPE_UC *)pcm)+(samplenum*in_bytespersample);
  do{
   PCM_CV_TYPE_I insamp;
   inptr--;
   outptr--;
   insamp=((PCM_CV_TYPE_I)inptr[0])-128;  // unsigned to signed conversion & scaling
   *outptr=((PCM_CV_TYPE_F)insamp)*scalecorr;
  }while(--samplenum);
 }else{
  PCM_CV_TYPE_C *inptr=((PCM_CV_TYPE_C *)pcm)+(samplenum*in_bytespersample);
  const unsigned int instep=in_bytespersample;
  const unsigned int shift=(PCM_MAX_BITS-inbits);

  inptr-=(shift/8);
  do{
   PCM_CV_TYPE_I insamp;
   inptr-=instep;
   outptr--;
   insamp=*((PCM_CV_TYPE_I *)inptr);
   insamp>>=shift;
   *outptr=((PCM_CV_TYPE_F)insamp)*scalecorr;
  }while(--samplenum);
 }
}

#if defined(USE_ASM_CV_BITS) && defined(__WATCOMC__)
void asm_sc_ob_n(unsigned int samplenum,PCM_CV_TYPE_F *inptr,PCM_CV_TYPE_C *outptr,unsigned int outstep);
#endif

void cv_float_to_n_bits(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_scalebits,unsigned int out_bytespersample,unsigned int fpuround_chop)
{
 PCM_CV_TYPE_F *inptr;
 PCM_CV_TYPE_C *outptr;
 unsigned int outstep,out_bits;
 PCM_CV_TYPE_UI scaleval;
 PCM_CV_TYPE_I  posmax,negmax;
 float scalecorr;

 out_bits=out_bytespersample<<3;

 if((in_scalebits==16) && (out_bits==16)){
  cv_float16_to_int16(pcm,samplenum,fpuround_chop);
  return;
 }

 if(out_bits!=in_scalebits)
  scalecorr=(out_bits>in_scalebits)? ((float)(1UL<<(out_bits-in_scalebits))):(1.0f/(float)(1UL<<(in_scalebits-out_bits)));
 else
  scalecorr=0.0;

 inptr=(PCM_CV_TYPE_F *)pcm;
 outptr=(PCM_CV_TYPE_C *)pcm;
 outstep=out_bytespersample;
 scaleval=(1UL << (out_bits-1));
 posmax=(scaleval-1);
 negmax=-scaleval;

 if(fpuround_chop)
  pds_fpu_setround_chop();

 if(scalecorr){
  if(out_bytespersample==1){
   const float unsignedcorr=128.0f; // signed to unsigned correction at outbits==8
   do{
    PCM_CV_TYPE_F insamp=inptr[0]*scalecorr;
    if(insamp>(PCM_CV_TYPE_F)posmax)
     insamp=(PCM_CV_TYPE_F)posmax;
    else
     if(insamp<(PCM_CV_TYPE_F)negmax)
      insamp=(PCM_CV_TYPE_F)negmax;
    insamp+=unsignedcorr;
    pds_ftoi(insamp,(PCM_CV_TYPE_I *)outptr);
    inptr++;
    outptr+=outstep;
   }while(--samplenum);
  }else{
#if defined(USE_ASM_CV_BITS) && defined(__WATCOMC__)
   #pragma aux asm_sc_ob_n=\
   "fild dword ptr negmax"\
   "fild dword ptr posmax"\
   "fld  dword ptr scalecorr"\
   "back1:"\
    "fld dword ptr [edi]"\
    "fmul st,st(1)"\
    "add edi,4"\
    "fcom st(2)"\
    "fnstsw ax"\
    "sahf"\
    "jbe nogreater2"\
     "fstp st"\
     "mov eax,dword ptr posmax"\
     "mov dword ptr [esi],eax"\
     "jmp done1"\
    "nogreater2:"\
     "fcom st(3)"\
     "fnstsw ax"\
     "sahf"\
     "jae checkok"\
      "fstp st"\
      "mov eax,dword ptr negmax"\
      "mov dword ptr [esi],eax"\
      "jmp done1"\
    "checkok:"\
    "fistp dword ptr [esi]"\
    "done1:"\
    "add esi,edx"\
    "dec ecx"\
   "jnz back1"\
   "fstp st"\
   "fstp st"\
   "fstp st"\
   parm[ecx][edi][esi][edx] modify[eax edi esi];
   asm_sc_ob_n(samplenum,inptr,outptr,outstep);
#else
   do{
    PCM_CV_TYPE_F insamp=inptr[0]*scalecorr;
    if(insamp>(PCM_CV_TYPE_F)posmax)
     *((PCM_CV_TYPE_I *)outptr)=posmax;
    else
     if(insamp<(PCM_CV_TYPE_F)negmax)
      *((PCM_CV_TYPE_I *)outptr)=negmax;
     else
      pds_ftoi(insamp,(PCM_CV_TYPE_I *)outptr);
    inptr++;
    outptr+=outstep;
   }while(--samplenum);
#endif
  }
 }else{
  if(out_bytespersample==1){
   const float unsignedcorr=128.0f; // signed to unsigned correction at outbits==8
   do{
    PCM_CV_TYPE_F insamp=inptr[0];
    if(insamp>(PCM_CV_TYPE_F)posmax)
     insamp=(PCM_CV_TYPE_F)posmax;
    else
     if(insamp<(PCM_CV_TYPE_F)negmax)
      insamp=(PCM_CV_TYPE_F)negmax;
    insamp+=unsignedcorr;
    pds_ftoi(insamp,(PCM_CV_TYPE_I *)outptr);
    inptr++;
    outptr+=outstep;
   }while(--samplenum);
  }else{
   do{
    PCM_CV_TYPE_F insamp=inptr[0];
    if(insamp>(PCM_CV_TYPE_F)posmax)
     *((PCM_CV_TYPE_I *)outptr)=posmax;
    else
     if(insamp<(PCM_CV_TYPE_F)negmax)
      *((PCM_CV_TYPE_I *)outptr)=negmax;
     else
      pds_ftoi(insamp,(PCM_CV_TYPE_I *)outptr);
    inptr++;
    outptr+=outstep;
   }while(--samplenum);
  }
 }

 if(fpuround_chop)
  pds_fpu_setround_near();
}


#if defined(USE_ASM_CV_BITS) && defined(__WATCOMC__)
void asm_float_to_int16(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int fpuround_chop);
static PCM_CV_TYPE_I cv_bits_ftoi16_val;
#endif

//float (with 16-bit scale) to 16-bit integer (with overflow check)
void cv_float16_to_int16(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int fpuround_chop)
{
#if defined(USE_ASM_CV_BITS) && defined(__WATCOMC__)

 #pragma aux asm_float_to_int16=\
 "test ebx,ebx"\
 "jz nochop"\
  "lea ecx,dword ptr cv_bits_ftoi16_val"\
  "fstcw word ptr [ecx]"\
  "or word ptr [ecx],0x0c00"\
  "fldcw word ptr [ecx]"\
 "nochop:"\
 "mov edi,eax"\
 "mov esi,eax"\
 "fti16cycle:"\
  "fld dword ptr [edi]"\
  "add edi,4"\
  "fistp dword ptr [esi]"\
  "movsx eax,word ptr [esi]"\
  "cmp eax,dword ptr [esi]"\
  "je fti16ok"\
   "cmp dword ptr [esi],0"\
   "jge fti16positive"\
    "mov word ptr [esi],-32768"\
    "jmp fti16ok"\
   "fti16positive:"\
    "mov word ptr [esi],32767"\
  "fti16ok:"\
  "add esi,2"\
  "dec edx"\
 "jnz fti16cycle"\
 "test ebx,ebx"\
 "jz no_chop_to_near"\
  "fstcw word ptr [ecx]"\
  "and word ptr [ecx],0xf3ff"\
  "fldcw word ptr [ecx]"\
 "no_chop_to_near:"\
 parm[eax][edx][ebx] modify[eax ecx edx edi esi];

 asm_float_to_int16(pcm,samplenum,fpuround_chop);

#else
 PCM_CV_TYPE_F *fpcm=(PCM_CV_TYPE_F *)pcm;
 PCM_CV_TYPE_I val;

 if(fpuround_chop)
  pds_fpu_setround_chop();

 do{
  pds_ftoi(*fpcm,&val);
  if(val>MIXER_SCALE_MAX)
   val=MIXER_SCALE_MAX;
  else
   if(val<MIXER_SCALE_MIN)
    val=MIXER_SCALE_MIN;
  pcm[0]=(PCM_CV_TYPE_S)val;
  fpcm++;pcm++;
 }while(--samplenum);

 if(fpuround_chop)
  pds_fpu_setround_near();
#endif
}

void cv_scale_float(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_scalebits,unsigned int out_scalebits,unsigned int rangecheck)
{
 PCM_CV_TYPE_F *outptr;
 PCM_CV_TYPE_UI scaleval;
 float posmax,negmax;
 float scalecorr;

 if(rangecheck){

  outptr=((PCM_CV_TYPE_F *)pcm);

  if(out_scalebits<=1){
   posmax=1.0;
   negmax=-1.0;
  }else{
   scaleval=(1UL << (out_scalebits-1));
   posmax=(float)(scaleval-1);
   negmax=-(float)scaleval;
  }

  if(out_scalebits==in_scalebits){
   do{
    PCM_CV_TYPE_F outsamp=outptr[0];
    if(outsamp>posmax)
     outptr[0]=posmax;
    else
     if(outsamp<negmax)
      outptr[0]=negmax;
    outptr++;
   }while(--samplenum);
  }else{
   scalecorr=(out_scalebits>in_scalebits)? ((float)(1UL<<(out_scalebits-in_scalebits))):(1.0f/(float)(1UL<<(in_scalebits-out_scalebits)));
   do{
    PCM_CV_TYPE_F outsamp=outptr[0]*scalecorr;
    if(outsamp>posmax)
     outptr[0]=posmax;
    else
     if(outsamp<negmax)
      outptr[0]=negmax;
     else
      outptr[0]=outsamp;
    outptr++;
   }while(--samplenum);
  }
 }else{
  if(out_scalebits==in_scalebits)
   return;

  outptr=((PCM_CV_TYPE_F *)pcm);
  scalecorr=(out_scalebits>in_scalebits)? ((float)(1UL<<(out_scalebits-in_scalebits))):(1.0f/(float)(1UL<<(in_scalebits-out_scalebits)));

  do{
   *outptr *= scalecorr;
   outptr++;
  }while(--samplenum);
 }
}

void aumixer_cvbits_float64le_to_float32le(PCM_CV_TYPE_S *pcm,unsigned int samplenum)
{
 mpxp_double_t *inptr=(mpxp_double_t *)pcm;
 mpxp_float_t *outptr=(mpxp_float_t *)pcm;
 if(!samplenum)
  return;
 do{
  *outptr++=*inptr++;
 }while(--samplenum);
}
