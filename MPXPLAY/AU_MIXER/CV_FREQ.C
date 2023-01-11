//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2014 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: speed/freq control

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include "mpxplay.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>

#ifndef SBEMU

#define CVFREQ_USE_ASM 1

static int MIXER_var_speed;
one_mixerfunc_info MIXER_FUNCINFO_speed;

typedef struct cvfreq_info_s{
 PCM_CV_TYPE_F *cv_freq_buffer;
 unsigned int cv_freq_bufsize, mx_sp_begin;
 float inpos;
}cvfreq_info_s;

//--------------------------------------------------------------------------
#if defined(CVFREQ_USE_ASM) && defined(__WATCOMC__)
void asm_cv_freq_floor(void);
void asm_cv_freq_hq(void);
#endif

static void mixer_speed_process(struct mpxp_aumixer_passinfo_s *mpi)
{
 struct cvfreq_info_s *cfs=(struct cvfreq_info_s *)mpi->private_data;
 unsigned long samplenum=mpi->samplenum,channels=mpi->chan_card;
 const float instep=(float)MIXER_var_speed/(float)MIXER_FUNCINFO_speed.var_center*(float)mpi->freq_song/(float)mpi->freq_card;
 const float inend=samplenum/channels;
 PCM_CV_TYPE_F *pcm=(PCM_CV_TYPE_F *)mpi->pcm_sample,*intmp;
 unsigned long savesamplenum=channels,ipi,fpucontrolword_save;
 float inpos;

 if(!cfs || !samplenum)
  return;
 intmp = cfs->cv_freq_buffer;
 if(!intmp || (samplenum>cfs->cv_freq_bufsize))
  return;
 if((MIXER_var_speed==MIXER_FUNCINFO_speed.var_center) && (mpi->freq_song==mpi->freq_card))
  return;

 if(cfs->mx_sp_begin){ // to avoid a click at start
  pds_qmemcpy(intmp,pcm,savesamplenum);
  cfs->mx_sp_begin=0;
  if(instep<1)
   inpos=instep/2;
  else
   inpos=0;
 }else
  inpos=cfs->inpos;

 pds_qmemcpy((intmp+savesamplenum),pcm,samplenum);

#if defined(CVFREQ_USE_ASM) && defined(__WATCOMC__)
 #pragma aux asm_cv_freq_hq=\
 "fstcw word ptr fpucontrolword_save"\
 "mov ax,word ptr fpucontrolword_save"\
 "or ax,0x0c00"\
 "mov word ptr ipi,ax"\
 "fldcw word ptr ipi"\
 "fld dword ptr inpos"\
 "mov ecx,dword ptr channels"\
 "mov esi,dword ptr pcm"\
 "back1:"\
  "fld st"\
  "frndint"\
  "fist dword ptr ipi"\
  "fsubr st,st(1)"\
  "mov eax,dword ptr ipi"\
  "imul ecx"\
  "fld1"\
  "fsub st,st(1)"\
  "shl eax,2"\
  "add eax,dword ptr intmp"\
  "lea ebx,dword ptr [eax+ecx*4]"\
  "mov edx,ecx"\
  "back2:"\
   "fld dword ptr [eax]"\
   "fmul st,st(1)"\
   "add eax,4"\
   "fld dword ptr [ebx]"\
   "fmul st,st(3)"\
   "add ebx,4"\
   "fadd"\
   "fstp dword ptr [esi]"\
   "add esi,4"\
   "dec edx"\
  "jnz back2"\
  "fstp st"\
  "fstp st"\
  "fadd dword ptr instep"\
  "fcom dword ptr inend"\
  "fnstsw ax"\
  "sahf"\
 "jb back1"\
 "fsub dword ptr inend"\
 "mov dword ptr pcm,esi"\
 "fstp dword ptr inpos"\
 "fldcw word ptr fpucontrolword_save"\
 modify[eax ebx ecx edx esi];
 asm_cv_freq_hq();

#else // !CVFREQ_USE_ASM

#ifdef __WATCOMC__
 pds_fpu_setround_chop(); // to asm_cv_freq_floor() !
#endif
 do{
  float m1,m2;
  unsigned int ipi,ch;
  PCM_CV_TYPE_F *intmp1,*intmp2;
#ifdef __WATCOMC__
 #pragma aux asm_cv_freq_floor=\
  "fld dword ptr inpos"\
  "fistp dword ptr ipi"\
  modify[];
  asm_cv_freq_floor();
#else
  ipi=(long)floor(inpos);
#endif
  m2=inpos-(float)ipi;
  m1=1.0f-m2;
  ch=channels;
  ipi*=ch;
  intmp1=intmp+ipi;
  intmp2=intmp1+ch;
  do{
   *pcm++=(*intmp1++)*m1+(*intmp2++)*m2;
  }while(--ch);
  inpos+=instep;
 }while(inpos<inend);
 inpos-=inend;
#ifdef __WATCOMC__
 pds_fpu_setround_near(); // restore default
#endif

#endif //CVFREQ_USE_ASM

 pds_qmemcpy(cfs->cv_freq_buffer,(cfs->cv_freq_buffer+mpi->samplenum),savesamplenum);
 mpi->samplenum=pcm-((PCM_CV_TYPE_F *)mpi->pcm_sample);
 cfs->inpos=inpos;
}

static unsigned int mixer_speed_alloc(struct mpxp_aumixer_passinfo_s *mpi,unsigned int samplenum)
{
 struct cvfreq_info_s *cfs=(struct cvfreq_info_s *)mpi->private_data;
 if(!cfs){
  cfs=(struct cvfreq_info_s *)pds_calloc(1,sizeof(struct cvfreq_info_s));
  if(!cfs)
   return 0;
  mpi->private_data=cfs;
 }
 if(cfs->cv_freq_bufsize<samplenum){
  cfs->cv_freq_bufsize=samplenum;
  if(cfs->cv_freq_buffer)
   pds_free(cfs->cv_freq_buffer);
  cfs->cv_freq_buffer=(PCM_CV_TYPE_F *)pds_malloc(cfs->cv_freq_bufsize*sizeof(PCM_CV_TYPE_F));
  if(!cfs->cv_freq_buffer){
   cfs->cv_freq_bufsize=0;
   return 0;
  }
  cfs->mx_sp_begin=1;
 }
 return 1;
}

static void mixer_speed_dealloc(struct mpxp_aumixer_passinfo_s *mpi)
{
 struct cvfreq_info_s *cfs=(struct cvfreq_info_s *)mpi->private_data;
 if(!cfs)
  return;
 if(cfs->cv_freq_buffer)
  pds_free(cfs->cv_freq_buffer);
 pds_memset(cfs,0,sizeof(*cfs));
 pds_free(cfs);
 mpi->private_data=NULL;
}

static void mixer_speed_chk_var_speed(struct mpxp_aumixer_passinfo_s *mpi)
{
 static unsigned int mx_sp_100sw1000_tested,mx_sp_1000_laststatus;
 mpxp_uint32_t infobits=0;
 mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_GET_INFOBITS,&infobits,NULL);
 if(mx_sp_1000_laststatus!=(infobits&AUINFOS_MIXERCTRLBIT_SPEED1000)){
  mx_sp_1000_laststatus=infobits&AUINFOS_MIXERCTRLBIT_SPEED1000;
  mx_sp_100sw1000_tested=0;
 }
 if(infobits&AUINFOS_MIXERCTRLBIT_SPEED1000){
  MIXER_FUNCINFO_speed.var_min=500;
  MIXER_FUNCINFO_speed.var_max=9999;
  MIXER_FUNCINFO_speed.var_center=1000;
  if(MIXER_var_speed<MIXER_FUNCINFO_speed.var_min)
   if(!mx_sp_100sw1000_tested)
    MIXER_var_speed*=10;
   else
    MIXER_var_speed=MIXER_FUNCINFO_speed.var_min;
  if(MIXER_var_speed>MIXER_FUNCINFO_speed.var_max)
   MIXER_var_speed=MIXER_FUNCINFO_speed.var_max;
  funcbit_enable(infobits,AUINFOS_MIXERINFOBIT_SPEED1000);
 }else{
  MIXER_FUNCINFO_speed.var_min=50;
  MIXER_FUNCINFO_speed.var_max=999;
  MIXER_FUNCINFO_speed.var_center=100;
  if(MIXER_var_speed>MIXER_FUNCINFO_speed.var_max)
   if(!mx_sp_100sw1000_tested)
    MIXER_var_speed/=10;
   else
    MIXER_var_speed=MIXER_FUNCINFO_speed.var_max;
  if(MIXER_var_speed<MIXER_FUNCINFO_speed.var_min)
   MIXER_var_speed=MIXER_FUNCINFO_speed.var_min;
  funcbit_disable(infobits,AUINFOS_MIXERINFOBIT_SPEED1000);
 }
 mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_SET_INFOBITS,&infobits,NULL);
 mx_sp_100sw1000_tested=1;
}

#ifdef MPXPLAY_GUI_QT
void mixer_speed_send_cfg_to_ext(struct mpxp_aumixer_passinfo_s *mpi)
{
 if(mpi->frp && mpi->frp->infile_infos && mpi->frp->infile_infos->control_cb){
  mpxp_uint32_t infobits=0, speed=MIXER_var_speed;
  mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_GET_INFOBITS,&infobits,NULL);
  if(!(infobits & AUINFOS_MIXERINFOBIT_SPEED1000))
   speed *= 10;
  mpi->frp->infile_infos->control_cb(mpi->frp->infile_infos->ccb_data, MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_PLAYSPEED_SET, (void *)&speed, NULL);
 }
 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mixer_speed_send_cfg_to_ext");
}
#endif

static int mixer_speed_init(struct mpxp_aumixer_passinfo_s *mpi,int inittype)
{
 struct mpxplay_audioout_info_s *aui;
 struct cvfreq_info_s *cfs;

 if(!mpi){
  mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "mixer_speed_init fail");
  return 0;
 }

 aui=mpi->aui;
 switch(inittype){
  case MIXER_INITTYPE_INIT:
   if(!mixer_speed_alloc(mpi,PCM_BUFFER_SIZE/(PCM_MAX_BITS/8)+PCM_MAX_CHANNELS))
    return 0;
   break;
  case MIXER_INITTYPE_REINIT:
#ifdef MPXPLAY_GUI_QT
   mixer_speed_send_cfg_to_ext(mpi);
#endif
  case MIXER_INITTYPE_START:
   if(mpi->freq_song>=PCM_MIN_FREQ){
    one_mixerfunc_info *infop_speed=&MIXER_FUNCINFO_speed;
    float speed_expansion=0.0f;
    long samplenum;

    if(infop_speed)
     speed_expansion=(float)infop_speed->var_center/(float)infop_speed->var_min;
    if(speed_expansion<2.0)
     speed_expansion=2.0;

    samplenum=0;
    mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_GET_PCMOUTBLOCKSIZE,&samplenum,NULL);
    samplenum/=mpi->chan_song;
    if(samplenum<PCM_OUTSAMPLES)
     samplenum=mpxplay_infile_get_samplenum_per_frame(mpi->freq_song);
    if(samplenum<PCM_OUTSAMPLES)
     samplenum=PCM_OUTSAMPLES;
    samplenum=speed_expansion*(float)((samplenum+128)*max(mpi->chan_song,mpi->chan_card))
             *(float)(max(mpi->freq_card,mpi->freq_song))/(float)mpi->freq_song;
    if(!mixer_speed_alloc(mpi,samplenum))
     return 0;
   }
   if(inittype==MIXER_INITTYPE_REINIT)
    break;
  case MIXER_INITTYPE_RESET:
   cfs=(struct cvfreq_info_s *)mpi->private_data;
   if(!cfs)
    return 0;
   cfs->mx_sp_begin=1;
   break;
  case MIXER_INITTYPE_CLOSE:
   mixer_speed_dealloc(mpi);
   break;
 }
 return 1;
}

static int mixer_speed_checkvar(struct mpxp_aumixer_passinfo_s *mpi)
{
 float freq=(mpi->freq_song<22050)? 22050.0:(float)mpi->freq_song;
 long  speed_center,speed_cur;
 mpxp_int32_t decoder_cycles;

 speed_center=MIXER_FUNCINFO_speed.var_center;
 speed_cur=MIXER_var_speed;
 if(speed_cur<speed_center)
  speed_cur=speed_center;

 decoder_cycles=(mpxp_int32_t)(((float)speed_cur+(float)(speed_center/2))/(float)speed_center*freq/(float)PCM_OUTSAMPLES)*(float)INT08_DIVISOR_NEW/(float)(INT08_CYCLES_DEFAULT*INT08_DIVISOR_DEFAULT)+1;
 mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_INT08DECODERCYCLES,&decoder_cycles,NULL);

 if((MIXER_var_speed!=speed_center) || (mpi->freq_song!=mpi->freq_card))
  return 1;
 return 0;
}

static void mixer_speed_setvar(struct mpxp_aumixer_passinfo_s *mpi,unsigned int setmode,int value)
{
 switch(setmode){
  case MIXER_SETMODE_ABSOLUTE:MIXER_var_speed=value;break;
  case MIXER_SETMODE_RELATIVE:MIXER_var_speed+=value;break;
  case MIXER_SETMODE_RESET:MIXER_var_speed=MIXER_FUNCINFO_speed.var_center;break;
 }
 mixer_speed_chk_var_speed(mpi);
#ifdef MPXPLAY_GUI_QT
 mixer_speed_send_cfg_to_ext(mpi);
#endif
}

one_mixerfunc_info MIXER_FUNCINFO_speed={
 "MIX_SPEED",
 "mxsp",
 &MIXER_var_speed,
 MIXER_INFOBIT_EXTERNAL_DEPENDENCY, // mpi->freq_song
 50,999,100,1,
 &mixer_speed_init,
 NULL,
 &mixer_speed_process,
 &mixer_speed_checkvar,
 &mixer_speed_setvar
};

//---------------------------------------------------------------

one_mixerfunc_info MIXER_FUNCINFO_seekspeed;
static int seekspeed_base,seekspeed_extra,seekspeed_counter;
extern unsigned int refdisp;
#ifndef SBEMU
#include "display\display.h"
#endif

static void mixer_speed_seekspeed_process(struct mpxp_aumixer_passinfo_s *mpi)
{
 if(seekspeed_counter)
  seekspeed_counter--;
 else{
  mpxp_int32_t val = -25;
  mpi->control_cb(mpi->ccb_data, MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_REL, "MIX_SPEEDSEEK",((void *)&val));
  #ifndef SBEMU
  refdisp|=RDT_OPTIONS;
  #endif
 }
}

static void mixer_speed_seekspeed_setvar(struct mpxp_aumixer_passinfo_s *mpi,unsigned int setmode,int value)
{
 mpxp_int32_t infobits, spde;

 if(!mpi || !mpi->control_cb)
  return;

 infobits=0;
 mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_INFOBITS,&infobits,NULL);

 if(!seekspeed_extra){
  seekspeed_base=MIXER_getvalue("MIX_SPEED");
  seekspeed_counter=35;
 }else if(value>0){
  if(!funcbit_test(infobits,AUINFOS_CARDINFOBIT_DMAFULL))
   value=0;
  seekspeed_counter=20;
 }else
  seekspeed_counter=2;
 // if(value<=0 || funcbit_test(infobits,AUINFOS_CARDINFOBIT_DMAFULL))
 {
  spde=seekspeed_extra;
  switch(setmode){
   case MIXER_SETMODE_ABSOLUTE:spde=value;break;
   case MIXER_SETMODE_RELATIVE:spde+=value;break;
   case MIXER_SETMODE_RESET:spde=0;break;
  }
  if(spde<0)
   spde=0;
  if(spde>MIXER_FUNCINFO_seekspeed.var_max)
   spde=MIXER_FUNCINFO_seekspeed.var_max;
  seekspeed_extra=spde;
  infobits=0;
  mpi->control_cb(mpi->ccb_data,MPXPLAY_CFGFUNCNUM_AUMIXER_GET_INFOBITS,&infobits,NULL);
  if(infobits&AUINFOS_MIXERINFOBIT_SPEED1000)
   spde*=10;
  spde+=seekspeed_base;
  mpi->control_cb(mpi->ccb_data, MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_ABS, "MIX_SPEED",((void *)&spde));
 }

 spde=(seekspeed_extra)? 48 : 0;
 mpi->control_cb(mpi->ccb_data, MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_ABS, "MIX_MUTE",((void *)&spde));
}

one_mixerfunc_info MIXER_FUNCINFO_seekspeed={
 "MIX_SPEEDSEEK",
 NULL,
 &seekspeed_extra,
 0,
 0,890,0,10,
 NULL,
 NULL,
 &mixer_speed_seekspeed_process,
 NULL,
 &mixer_speed_seekspeed_setvar
};

#else

unsigned int mixer_speed_lq(PCM_CV_TYPE_S *pcm16,unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate)
{
 const unsigned int instep=((samplerate/newrate)<<8) | ((256*(samplerate%newrate)/newrate)&0xFF);
 const unsigned int inend=(samplenum/channels) << 8;
 PCM_CV_TYPE_S *pcm,*intmp;
 unsigned long ipi;
 unsigned int inpos = 0;
 if(!samplenum)
  return 0;
 assert(((samplenum/channels)&0xF0000000) == 0); //too many samples, need other approches.
 unsigned int buffcount = max(((float)samplenum*newrate+samplerate-1)/samplerate,samplenum);
 PCM_CV_TYPE_S* buff = (PCM_CV_TYPE_S*)malloc(buffcount*sizeof(PCM_CV_TYPE_S));

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "step: %08x, end: %08x\n", instep, inend);

 pcm = buff;
 intmp = pcm16;

 do{
  unsigned int m1,m2;
  unsigned int ipi,ch;
  PCM_CV_TYPE_S *intmp1,*intmp2;
  ipi = inpos >> 8;
  m2=inpos&0xFF;
  m1=255-m2;
  ch=channels;
  ipi*=ch;
  intmp1=intmp+ipi;
  intmp2=intmp1+ch;
  do{
   *pcm++= ((*intmp1++)*m1+(*intmp2++)*m2) / 255;
  }while(--ch);
  inpos+=instep;
 }while(inpos<inend);

 mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "sample count: %d\n", pcm-buff);
 assert(pcm-buff <= buffcount);
 memcpy(pcm16, buff, (pcm-buff)*sizeof(PCM_CV_TYPE_S));
 free(buff);
 return pcm - buff;
}

#endif