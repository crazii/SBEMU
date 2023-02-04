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
//function: audio main functions

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <dos.h>

#include "mpxplay.h"
#include "dmairq.h"
#ifndef SBEMU
#include "newfunc\dll_load.h"
#include "au_mixer\au_mixer.h"
#include "display\display.h"
#endif

typedef int (*aucards_writedata_t)(struct mpxplay_audioout_info_s *aui,unsigned long);

static unsigned int cardinit(struct mpxplay_audioout_info_s *aui);
static unsigned int carddetect(struct mpxplay_audioout_info_s *aui, unsigned int retry);

#ifndef SBEMU
static unsigned int AU_cardbuf_space(struct mpxplay_audioout_info_s *aui);
#endif
static int aucards_writedata_normal(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left);
static int aucards_writedata_intsound(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left);
#ifdef SBEMU
static int aucards_writedata_nowait(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left);
#endif
static void aucards_dma_monitor(void);
#ifndef SBEMU
static void aucards_interrupt_decoder(void);
#endif
static void aucards_get_cpuusage_int08(void);

#ifdef __DOS__
extern one_sndcard_info ES1371_sndcard_info;
extern one_sndcard_info ICH_sndcard_info;
extern one_sndcard_info IHD_sndcard_info;
extern one_sndcard_info VIA82XX_sndcard_info;
#ifndef SBEMU
extern one_sndcard_info ESS_sndcard_info;
extern one_sndcard_info WSS_sndcard_info;
extern one_sndcard_info SB16_sndcard_info;
extern one_sndcard_info SBLIVE_sndcard_info;
extern one_sndcard_info EMU20KX_sndcard_info;
extern one_sndcard_info CMI8X38_sndcard_info;
extern one_sndcard_info GUS_sndcard_info;
extern one_sndcard_info SB_sndcard_info;
extern one_sndcard_info MIDAS_sndcard_info;
#else
#undef AU_CARDS_LINK_ESS
#undef AU_CARDS_LINK_WSS
#undef AU_CARDS_LINK_SBLIVE
#undef AU_CARDS_LINK_SB16
#undef AU_CARDS_LINK_EMU20KX
#undef AU_CARDS_LINK_CMI8X38
#undef AU_CARDS_LINK_GUS
#undef AU_CARDS_LINK_SB
#undef AU_CARDS_LINK_MIDAS
#endif
#endif

#ifdef AU_CARDS_LINK_WIN
extern one_sndcard_info WINDSOUND_sndcard_info;
extern one_sndcard_info WINWAVOUT_sndcard_info;
#endif

#ifndef SBEMU
extern one_sndcard_info WAV_sndcard_info;
extern one_sndcard_info NON_sndcard_info;
extern one_sndcard_info TST_sndcard_info;
extern one_sndcard_info NUL_sndcard_info;
#endif

static one_sndcard_info *all_sndcard_info[]={
#ifdef AU_CARDS_LINK_SB16
 &SB16_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_ESS
 &ESS_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_WSS
 &WSS_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_SBLIVE
 &SBLIVE_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_EMU20KX
 &EMU20KX_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_CMI8X38
 &CMI8X38_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_ES1371
 &ES1371_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_ICH
 &ICH_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_IHD
 &IHD_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_VIA82XX
 &VIA82XX_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_GUS
 &GUS_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_SB
 &SB_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_MIDAS
 &MIDAS_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_WINDSOUND
 &WINDSOUND_sndcard_info,
#endif
#ifdef AU_CARDS_LINK_WINWAVOUT
 &WINWAVOUT_sndcard_info,
#endif
#ifndef SBEMU
 &WAV_sndcard_info,
 &NON_sndcard_info,
 &TST_sndcard_info,
 &NUL_sndcard_info,
#endif
 NULL
};

//control\control.c
extern struct mpxplay_audioout_info_s au_infos;
extern unsigned int playcontrol,outmode;
extern unsigned int intsoundconfig,intsoundcontrol;
extern unsigned long allcpuusage,allcputime;
#ifdef __DOS__
extern unsigned long int08counter, mpxplay_signal_events;
extern unsigned int is_lfn_support,uselfn,iswin9x;
#endif

#ifdef SBEMU
struct mainvars mvps;
struct mpxplay_audioout_info_s au_infos;
unsigned int playcontrol,outmode = OUTMODE_TYPE_AUDIO;
unsigned int intsoundconfig=INTSOUND_NOINT08|INTSOUND_NOBUSYWAIT,intsoundcontrol;
unsigned long allcpuusage,allcputime;
unsigned int is_lfn_support,uselfn,iswin9x;
#endif

static aucards_writedata_t aucards_writedata_func;

void AU_init(struct mpxplay_audioout_info_s *aui)
{
 unsigned int error_code = MPXERROR_SNDCARD;
 one_sndcard_info **asip;
 char cardselectname[32]="";
 char sout[100];

 aui->card_dmasize=aui->card_dma_buffer_size=MDma_get_max_pcmoutbufsize(aui,65535,4608,2,0);

#ifndef MPXPLAY_GUI_CONSOLE
auinit_retry:
#endif

 if(aui->card_selectname){
  pds_strncpy(cardselectname,aui->card_selectname,sizeof(cardselectname)-1);
  cardselectname[sizeof(cardselectname)-1]=0;
  pds_strcutspc(cardselectname);
  if(pds_stricmp(cardselectname,"AUTO")==0)
   cardselectname[0]=0;
 }

 if(cardselectname[0]){
  asip=&all_sndcard_info[0];
  aui->card_handler=*asip;
  do{
   if(pds_stricmp(cardselectname,aui->card_handler->shortname)==0)
    break;
   asip++;
   aui->card_handler=*asip;
  }while(aui->card_handler);

  if(!aui->card_handler){
#ifdef MPXPLAY_LINK_DLLLOAD
   mpxplay_module_entry_s *dll_soundcard=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_AUCARD,0,cardselectname,NULL);
   if(dll_soundcard){
    if(dll_soundcard->module_structure_version==MPXPLAY_DLLMODULEVER_AUCARD){ // !!!
     aui->card_handler=(one_sndcard_info *)dll_soundcard->module_callpoint;
    }else{
     sprintf(sout,"Cannot handle DLL module (old structure) : %s",cardselectname);
     pds_textdisplay_printf(sout);
     goto err_out_auinit;
    }
   }else
#endif
   {
    sprintf(sout,"Unknown soundcard (output module) name : %s",cardselectname);
    pds_textdisplay_printf(sout);
    goto err_out_auinit;
   }
  }
  if(!cardinit(aui) || (aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD)){
   if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD){
    sprintf(sout,"Testing selected output/soundcard (%s), please wait...",cardselectname);
    pds_textdisplay_printf(sout);
   }
   if(!carddetect(aui,1))
    aui->card_handler=NULL;
  }
  if(!aui->card_handler){
   sprintf(sout,"Cannot initialize %s soundcard (not exists or unsupported settings)!",cardselectname);
   pds_textdisplay_printf(sout);
   goto err_out_auinit;
  }
  if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD){
   pds_textdisplay_printf("Testing finished... Press ESC to exit, other key to start Mpxplay...");
   #ifndef SBEMU
   if(pds_extgetch()==KEY_ESC){
    #else
    {
    #endif
    error_code = MPXERROR_UNDEFINED;
    goto err_out_auinit;
   }
  }
 }else{
  // init/search card(s) via environment variable
  if(!(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD)){
   asip=&all_sndcard_info[0];
   aui->card_handler=*asip;
   do{
    if(!(aui->card_handler->infobits&SNDCARD_SELECT_ONLY))
     if(cardinit(aui))
      break;
    asip++;
    aui->card_handler=*asip;
   }while(aui->card_handler);

#ifdef MPXPLAY_LINK_DLLLOAD
   if(!aui->card_handler){
    mpxplay_module_entry_s *dll_soundcard=NULL;
    do{
     dll_soundcard=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_AUCARD,0,NULL,dll_soundcard);
     if(dll_soundcard){
      if(dll_soundcard->module_structure_version==MPXPLAY_DLLMODULEVER_AUCARD){ // !!!
       aui->card_handler=(one_sndcard_info *)dll_soundcard->module_callpoint;
       if(!(aui->card_handler->infobits&SNDCARD_SELECT_ONLY)){
        if(cardinit(aui))
         break;
        if(aui->card_handler->card_close)
         aui->card_handler->card_close(aui);
        aui->card_private_data=NULL;
       }
       aui->card_handler=NULL;
      }
      newfunc_dllload_disablemodule(0,0,NULL,dll_soundcard);
     }
    }while(dll_soundcard);
   }
#endif
  }

  // autodetect sound card(s) (on brutal force way)
  if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD)  // -sct
   pds_textdisplay_printf("Autodetecting/testing available outputs/soundcards, please wait...");

  // test built-in cards
  if(!aui->card_handler || (aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD)){
   asip=&all_sndcard_info[0];
   aui->card_handler=*asip;
   do{
    if(aui->card_handler->card_detect){
     if(!(aui->card_handler->infobits&SNDCARD_SELECT_ONLY)
#ifndef MPXPLAY_WIN32
      && (!(aui->card_handler->infobits&SNDCARD_LOWLEVELHAND) || !iswin9x)
#endif
     ){
      if(carddetect(aui,0)){
       if(!(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD))
        break;
       if(aui->card_handler->card_close)
        aui->card_handler->card_close(aui);
       aui->card_private_data=NULL;
      }
     }
    }
    asip++;
    aui->card_handler=*asip;
   }while(aui->card_handler);
  }

#ifdef MPXPLAY_LINK_DLLLOAD
  // test DLLs
  if(!aui->card_handler || (aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD)){
   mpxplay_module_entry_s *dll_soundcard=NULL;
   do{
    dll_soundcard=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_AUCARD,0,NULL,dll_soundcard);
    if(dll_soundcard){
     if(dll_soundcard->module_structure_version==MPXPLAY_DLLMODULEVER_AUCARD){ // !!!
      aui->card_handler=(one_sndcard_info *)dll_soundcard->module_callpoint;
      if(!(aui->card_handler->infobits&SNDCARD_SELECT_ONLY)
#ifndef MPXPLAY_WIN32
       && (!(aui->card_handler->infobits&SNDCARD_LOWLEVELHAND) || !iswin9x)
#endif
      ){
       if(carddetect(aui,0)){
        if(!(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD))
         break;
        if(aui->card_handler->card_close)
         aui->card_handler->card_close(aui);
        aui->card_private_data=NULL;
       }
      }
      aui->card_handler=NULL;
     }
     newfunc_dllload_disablemodule(0,0,NULL,dll_soundcard);
    }
   }while(dll_soundcard);
  }
#endif

  if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD){
   pds_textdisplay_printf("Autodetecting finished... Exiting...");
   error_code = MPXERROR_UNDEFINED;
   goto err_out_auinit;
  }
  if(!aui->card_handler){
   pds_textdisplay_printf("No soundcard found!");
   goto err_out_auinit;
  }
 }

 printf("Found sound card: %s\n", aui->card_handler->shortname);

 if(intsoundconfig&INTSOUND_NOINT08)
  funcbit_disable(aui->card_handler->infobits,SNDCARD_INT08_ALLOWED);
 if(!(aui->card_handler->infobits&SNDCARD_INT08_ALLOWED))
  funcbit_disable(intsoundconfig,INTSOUND_FUNCTIONS);

 aui->freq_card=aui->chan_card=aui->bits_card=0;
 return;

err_out_auinit:
#ifdef MPXPLAY_GUI_CONSOLE
 mpxplay_close_program(error_code);
#else
 //mpxplay_timer_addfunc(&display_textwin_openwindow_errormsg_ok,"Couldn't initialize audio output!\nSwitched to Null output.", (MPXPLAY_TIMERTYPE_WAKEUP | MPXPLAY_TIMERTYPE_SIGNAL), MPXPLAY_SIGNALTYPE_GUIREADY);
 if(pds_stricmp(aui->card_selectname, "NUL") != 0){
   aui->card_selectname = "NUL";
   goto auinit_retry;
 }
#endif
}

static unsigned int cardinit(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_handler->card_init)
  if(aui->card_handler->card_init(aui))
   return 1;
 return 0;
}

static unsigned int carddetect(struct mpxplay_audioout_info_s *aui, unsigned int do_retry)
{
 unsigned int ok, retry = do_retry;
jump_back:

 if(outmode&OUTMODE_CONTROL_FILE_FLOATOUT){
  aui->card_wave_id=MPXPLAY_WAVEID_PCM_FLOAT; // float pcm
  aui->bits_card=1;
  aui->bytespersample_card=sizeof(float);
 }else{
  aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE; // integer pcm
  aui->bits_card=16;
  aui->bytespersample_card=(aui->bits_card+7)/8;
 }
 aui->card_port=aui->card_type=0xffff;
 aui->card_irq=aui->card_isa_dma=aui->card_isa_hidma=0xff;
 aui->freq_card=44100;
 aui->chan_card=2;

 if(aui->card_handler->card_detect){
  if(aui->card_handler->card_detect(aui))
   ok=1;
  else{
   if(!retry)
    return 0;
   funcbit_disable(outmode,OUTMODE_CONTROL_FILE_FLOATOUT);
   aui->freq_set=0;
   aui->bits_set=0;
   aui->chan_set=0;
   retry=0;
   goto jump_back;
  }
  if(ok){
   if(do_retry && !retry){
    pds_textdisplay_printf("Warning: initial soundcard config failed! Using default values: stereo, 16 bits");
   }
   if(aui->card_handler->card_info && (aui->card_controlbits&AUINFOS_CARDCNTRLBIT_TESTCARD))
    aui->card_handler->card_info(aui);
   return 1;
  }
 }
 return 0;
}

void AU_ini_interrupts(struct mpxplay_audioout_info_s *aui)
{
 aucards_writedata_func=&aucards_writedata_normal;
 if(aui->card_handler->infobits&SNDCARD_INT08_ALLOWED){
  newfunc_newhandler08_init();
  if(aui->card_handler->cardbuf_int_monitor)
   mpxplay_timer_addfunc(&aucards_dma_monitor,NULL,MPXPLAY_TIMERTYPE_INT08|MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_OWNSTACK|MPXPLAY_TIMERFLAG_STI,0);
  if(intsoundconfig&INTSOUND_DECODER){
   #ifndef SBEMU
   mpxplay_timer_addfunc(&aucards_interrupt_decoder,NULL,MPXPLAY_TIMERTYPE_INT08|MPXPLAY_TIMERTYPE_REPEAT|MPXPLAY_TIMERFLAG_OWNSTCK2|MPXPLAY_TIMERFLAG_STI,0);
   #endif
   aucards_writedata_func=&aucards_writedata_intsound;
  }
 }
  #ifdef SBEMU
  if(intsoundconfig&INTSOUND_NOBUSYWAIT){
    aucards_writedata_func = &aucards_writedata_nowait;
  }
  #endif 
}

void AU_del_interrupts(struct mpxplay_audioout_info_s *aui)
{
 AU_close(aui);
#ifndef __DOS__
 mpxplay_timer_deletefunc(&aucards_dma_monitor,NULL);
 #ifndef SBEMU
 mpxplay_timer_deletefunc(&aucards_interrupt_decoder,NULL);
 #endif
#endif
}

void AU_prestart(struct mpxplay_audioout_info_s *aui)
{
 if(intsoundcontrol&INTSOUND_DECODER)
  AU_start(aui);
 else{
  if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DMACLEAR)
   AU_clearbuffs(aui);
  funcbit_smp_enable(playcontrol,PLAYC_RUNNING);
 }
}

void AU_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int intsoundcntrl_save;
 if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING)){
  MPXPLAY_INTSOUNDDECODER_DISALLOW;

  if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DMACLEAR)
   AU_clearbuffs(aui);
  if(aui->card_handler->card_start)
   aui->card_handler->card_start(aui);
  funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_PLAYING);

  MPXPLAY_INTSOUNDDECODER_ALLOW;
 }
 funcbit_smp_enable(playcontrol,PLAYC_RUNNING);
 funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
}

void AU_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int intsoundcntrl_save;
 funcbit_smp_disable(playcontrol,PLAYC_RUNNING);

 if(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING){
  funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_PLAYING);
  MPXPLAY_INTSOUNDDECODER_DISALLOW;

  if(aui->card_handler && aui->card_handler->card_stop)
   aui->card_handler->card_stop(aui);
  aui->card_dmafilled=aui->card_dmalastput;
  aui->card_dmaspace=aui->card_dmasize-aui->card_dmafilled;
  funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);

  MPXPLAY_INTSOUNDDECODER_ALLOW;
 }
}

void AU_wait_and_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int intsoundcntrl_save;
 //mvp->idone=MPXPLAY_ERROR_INFILE_EOF;
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING){
  //this is not finished, not tested fully
  //extra cardbuf_writedata may cause problems
  /*int b,empty=0;
  AU_cardbuf_space(aui);
  b=aui->card_dmasize-aui->card_dmaspace;
  while(b>0){
   int s;
   AU_cardbuf_space(aui);
   s=aui->card_dmaspace&0xfffffffc;
   b-=s;
   while(s>0){
    aui->card_handler->cardbuf_writedata((char *)&empty,4);
    s-=4;
   }
  }*/
  funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_PLAYING);
  MPXPLAY_INTSOUNDDECODER_DISALLOW;
  if(aui->card_handler && aui->card_handler->card_stop)
   aui->card_handler->card_stop(aui);
  //aui->card_dmalastput=aui->card_dmaspace=aui->card_dmafilled=aui->card_dmasize>>1;
  //funcbit_enable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_DMACLEAR);
  MPXPLAY_INTSOUNDDECODER_ALLOW;
 }
 funcbit_smp_disable(playcontrol,PLAYC_RUNNING);
}

void AU_suspend_decoding(struct mpxplay_audioout_info_s *aui)
{
 #ifndef SBEMU
 struct mainvars *mvp=aui->mvp;
 funcbit_smp_int32_put(mvp->idone,MPXPLAY_ERROR_INFILE_EOF);
 #endif
 newfunc_newhandler08_waitfor_threadend();
}

void AU_resume_decoding(struct mpxplay_audioout_info_s *aui)
{
#ifndef SBEMU
 struct mainvars *mvp=aui->mvp;
 funcbit_smp_int32_put(mvp->idone,MPXPLAY_ERROR_INFILE_OK);
#endif
}

void AU_close(struct mpxplay_audioout_info_s *aui)
{
 if(!aui)
  return;
 AU_stop(aui);
 if(aui->card_handler && aui->card_handler->card_close)
  aui->card_handler->card_close(aui);
}

void AU_pause_process(struct mpxplay_audioout_info_s *aui)
{
#ifdef __DOS__
 if(!(aui->card_handler->infobits&SNDCARD_INT08_ALLOWED)){
  if(!funcbit_test(mpxplay_signal_events,MPXPLAY_SIGNALMASK_OTHER))
   pds_delay_10us(1400);
  int08counter+=REFRESH_DELAY_JOYMOUSE;
 }
#endif
}

void AU_clearbuffs(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_handler->cardbuf_clear)
  aui->card_handler->cardbuf_clear(aui);
 funcbit_smp_disable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_DMACLEAR);
}

void AU_setrate(struct mpxplay_audioout_info_s *aui,struct mpxplay_audio_decoder_info_s *adi)
{
 unsigned int intsoundcntrl_save,new_cardcontrolbits;
 #ifndef SBEMU
 aui->pei=aui->mvp->pei0;
 #endif

 aui->chan_song=adi->outchannels;
 aui->bits_song=adi->bits;

 new_cardcontrolbits=aui->card_controlbits;
 if(adi->infobits&ADI_FLAG_BITSTREAMOUT){
  funcbit_enable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_BITSTREAMOUT);
  if(adi->infobits&ADI_FLAG_BITSTREAMHEAD)
   funcbit_enable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD);
  if(adi->infobits&ADI_FLAG_BITSTREAMNOFRH)
   funcbit_enable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_BITSTREAMNOFRH);
 }else{
  funcbit_disable(new_cardcontrolbits,(AUINFOS_CARDCNTRLBIT_BITSTREAMOUT|AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD|AUINFOS_CARDCNTRLBIT_BITSTREAMNOFRH));
 }

#ifdef MPXPLAY_WIN32
 if(outmode&OUTMODE_CONTROL_FILE_TAGLFN)
  funcbit_enable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_AUTOTAGLFN);
#else
 if(is_lfn_support && (uselfn&USELFN_ENABLED)){
  if(outmode&OUTMODE_CONTROL_FILE_TAGLFN)
   funcbit_enable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_AUTOTAGLFN);
 }else
  funcbit_disable(new_cardcontrolbits,AUINFOS_CARDCNTRLBIT_AUTOTAGLFN);
#endif

 if(new_cardcontrolbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
  aui->card_wave_name=adi->shortname;

 // We (stop and) reconfigure the card if the frequency has changed (and crossfade is disabled)
 // The channel and bit differences are allways handled by the AU_MIXER

 if(   (aui->freq_set  && (aui->freq_set!=aui->freq_card))
    || (!aui->freq_set && (aui->freq_song!=adi->freq))
    || (aui->card_controlbits&AUINFOS_CARDCNTRLBIT_UPDATEFREQ)
    || (new_cardcontrolbits!=aui->card_controlbits)
    || ((aui->card_controlbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT) && (aui->card_wave_id!=adi->wave_id))
    || (aui->card_handler->infobits&SNDCARD_SETRATE)
  ){
  if(aui->card_handler->infobits&SNDCARD_SETRATE){ // !!!
   if(aui->card_handler->card_stop)                //
    aui->card_handler->card_stop(aui);             //
  }else{
   if(playcontrol&PLAYC_RUNNING){
    AU_stop(aui);
    funcbit_enable(playcontrol,PLAYC_STARTNEXT);
   }
  }

  aui->freq_song=adi->freq;

  aui->freq_card=(aui->freq_set)? aui->freq_set:adi->freq;
  aui->chan_card=(aui->chan_set)? aui->chan_set:adi->outchannels;
  aui->bits_card=(aui->bits_set)? aui->bits_set:(((adi->bits<=1) && (adi->infobits&ADI_FLAG_FLOATOUT))? MIXER_SCALE_BITS:adi->bits); // for wav out (1-bit float to int16)
  if(new_cardcontrolbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
   aui->card_wave_id=adi->wave_id;
  else{
   if(outmode&OUTMODE_CONTROL_FILE_FLOATOUT)
    aui->card_wave_id=MPXPLAY_WAVEID_PCM_FLOAT; // float pcm
   else
    aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE; // integer pcm
  }
  aui->bytespersample_card=0;
  aui->card_controlbits=new_cardcontrolbits;
  funcbit_disable(aui->card_infobits,(AUINFOS_CARDINFOBIT_BITSTREAMOUT|AUINFOS_CARDINFOBIT_BITSTREAMNOFRH));

  MPXPLAY_INTSOUNDDECODER_DISALLOW;    // ???
  if(aui->card_handler->card_setrate)
   aui->card_handler->card_setrate(aui);
  MPXPLAY_INTSOUNDDECODER_ALLOW;       // ???

  if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT)
   aui->bytespersample_card=4;
  else
   if(!aui->bytespersample_card) // card haven't set it (not implemented in the au_mixer yet!: bits/8 !=bytespersample_card)
    aui->bytespersample_card=(aui->bits_card+7)/8;

  funcbit_enable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_DMACLEAR);
  funcbit_disable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_UPDATEFREQ);

  if(aui->freq_set) aui->freq_set=aui->freq_card;
  if(aui->chan_set) aui->chan_set=aui->chan_card;
  if(aui->bits_set) aui->bits_set=aui->bits_card;

  if(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT){
   if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMNOFRH))
    funcbit_disable(adi->infobits,ADI_CNTRLBIT_BITSTREAMNOFRH);
   aui->bytespersample_card=1;
  }else{
   funcbit_disable(aui->card_controlbits,(AUINFOS_CARDCNTRLBIT_BITSTREAMOUT|AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD));
   funcbit_disable(adi->infobits,(ADI_FLAG_BITSTREAMOUT|ADI_FLAG_BITSTREAMHEAD|ADI_CNTRLBIT_BITSTREAMOUT|ADI_CNTRLBIT_BITSTREAMNOFRH));
  }

  aui->card_bytespersign=aui->chan_card*aui->bytespersample_card;
#ifdef SBEMU
  aui->card_outbytes=aui->card_dmasize;
#else
  aui->card_outbytes=aui->card_dmasize/4; // ??? for interrupt_decoder
#endif
 }
 aui->freq_song=adi->freq;
}

//---------------------------------------------------------------------------

void AU_setmixer_init(struct mpxplay_audioout_info_s *aui)
{
 unsigned int c;

 aui->card_master_volume=-1;

 for(c=0;c<AU_MIXCHANS_NUM;c++)
  aui->card_mixer_values[c]=-1;
}

static aucards_onemixerchan_s *AU_search_mixerchan(aucards_allmixerchan_s *mixeri,unsigned int mixchannum)
{
 unsigned int i=0;
 while(*mixeri){
  if((*mixeri)->mixchan==mixchannum)
   return (*mixeri);
  if(++i >= AU_MIXCHANS_NUM)
   break;
  mixeri++;
 }
 return NULL;
}

void AU_setmixer_one(struct mpxplay_audioout_info_s *aui,unsigned int mixchannum,unsigned int setmode,int newvalue)
{
 one_sndcard_info *cardi;
 aucards_onemixerchan_s *onechi; // one mixer channel infos (master,pcm,etc.)
 unsigned int subchannelnum,sch,channel,function,intsoundcntrl_save;
 long newpercentval, maxpercentval;

 //mixer structure/values verifying
 function=AU_MIXCHANFUNCS_GETFUNC(mixchannum);
 if(function>=AU_MIXCHANFUNCS_NUM)
  return;
 channel=AU_MIXCHANFUNCS_GETCHAN(mixchannum);
 if(channel>AU_MIXCHANS_NUM)
  return;
 cardi=aui->card_handler;
 if(!cardi)
  return;
 if(!cardi->card_writemixer || !cardi->card_readmixer || !cardi->card_mixerchans)
  return;
 onechi=AU_search_mixerchan(cardi->card_mixerchans,mixchannum);
 if(!onechi)
  return;
 subchannelnum=onechi->subchannelnum;
 if(!subchannelnum || (subchannelnum>AU_MIXERCHAN_MAX_SUBCHANNELS))
  return;

 switch(mixchannum){
  case AU_MIXCHAN_BASS:
  case AU_MIXCHAN_TREBLE: maxpercentval = AU_MIXCHAN_MAX_VALUE_TONE; break;
  default: maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME; break;
 }

 //calculate new percent
 switch(setmode){
  case MIXER_SETMODE_ABSOLUTE:newpercentval=newvalue;
                  break;
  case MIXER_SETMODE_RELATIVE:if(function==AU_MIXCHANFUNC_VOLUME)
                               newpercentval=aui->card_mixer_values[channel]+newvalue;
                              else
                               if(newvalue<0)
                                newpercentval=0;
                               else
                                newpercentval=maxpercentval;
                  break;
  default:return;
 }
 if(newpercentval<0)
  newpercentval=0;
 if(newpercentval>maxpercentval)
  newpercentval=maxpercentval;

 MPXPLAY_INTSOUNDDECODER_DISALLOW;
 ENTER_CRITICAL;

 //read current register value, mix it with the new one, write it back
 for(sch=0;sch<subchannelnum;sch++){
  aucards_submixerchan_s *subchi=&(onechi->submixerchans[sch]); // one subchannel infos (left,right,etc.)
  unsigned long currchval,newchval;

  if((subchi->submixch_register>AU_MIXERCHAN_MAX_REGISTER) || !subchi->submixch_max || (subchi->submixch_shift>AU_MIXERCHAN_MAX_BITS)) // invalid subchannel infos
   continue;

  newchval=(long)(((float)newpercentval*(float)subchi->submixch_max+((float)((maxpercentval >> 1) - 1)))/(float)maxpercentval);   // percent to chval (rounding up)
  if(newchval>subchi->submixch_max)
   newchval=subchi->submixch_max;
  if(subchi->submixch_infobits&SUBMIXCH_INFOBIT_REVERSEDVALUE)   // reverse value if required
   newchval=subchi->submixch_max-newchval;

  newchval<<=subchi->submixch_shift;                             // shift to position

  currchval=cardi->card_readmixer(aui,subchi->submixch_register);// read current value
  currchval&=~(subchi->submixch_max<<subchi->submixch_shift);    // unmask
  newchval=(currchval|newchval);                                 // add new value

  cardi->card_writemixer(aui,subchi->submixch_register,newchval);// write it back
 }
 LEAVE_CRITICAL;
 MPXPLAY_INTSOUNDDECODER_ALLOW;
 if(function==AU_MIXCHANFUNC_VOLUME)
  aui->card_mixer_values[channel]=newpercentval;
}

static int AU_getmixer_one(struct mpxplay_audioout_info_s *aui,unsigned int mixchannum)
{
 one_sndcard_info *cardi;
 aucards_onemixerchan_s *onechi; // one mixer channel infos (master,pcm,etc.)
 aucards_submixerchan_s *subchi; // one subchannel infos (left,right,etc.)
 unsigned long channel,function,subchannelnum;
 long value,maxpercentval;

 //mixer structure/values verifying
 function=AU_MIXCHANFUNCS_GETFUNC(mixchannum);
 if(function>=AU_MIXCHANFUNCS_NUM)
  return -1;
 channel=AU_MIXCHANFUNCS_GETCHAN(mixchannum);
 if(channel>AU_MIXCHANS_NUM)
  return -1;
 cardi=aui->card_handler;
 if(!cardi)
  return -1;
 if(!cardi->card_readmixer || !cardi->card_mixerchans)
  return -1;
 onechi=AU_search_mixerchan(cardi->card_mixerchans,mixchannum);
 if(!onechi)
  return -1;
 subchannelnum=onechi->subchannelnum;
 if(!subchannelnum || (subchannelnum>AU_MIXERCHAN_MAX_SUBCHANNELS))
  return -1;

 switch(mixchannum){
  case AU_MIXCHAN_BASS:
  case AU_MIXCHAN_TREBLE: maxpercentval = AU_MIXCHAN_MAX_VALUE_TONE; break;
  default: maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME; break;
 }

 // we read one (the left at stereo) sub-channel only
 subchi=&(onechi->submixerchans[0]);
 if((subchi->submixch_register>AU_MIXERCHAN_MAX_REGISTER) || (subchi->submixch_shift>AU_MIXERCHAN_MAX_BITS)) // invalid subchannel infos
  return -1;

 value=cardi->card_readmixer(aui,subchi->submixch_register); // read
 value>>=subchi->submixch_shift;                             // shift
 value&=subchi->submixch_max;                                // mask

 if(subchi->submixch_infobits&SUBMIXCH_INFOBIT_REVERSEDVALUE)// reverse value if required
  value=subchi->submixch_max-value;

 value=(long)((float)value*(float)maxpercentval/(float)subchi->submixch_max);       // chval to percent
 if(value>maxpercentval)
  value=maxpercentval;
 return value;
}

#define AU_MIXCHANS_OUTS 4

static const unsigned int au_mixchan_outs[AU_MIXCHANS_OUTS]={
 AU_MIXCHAN_MASTER,AU_MIXCHAN_PCM,AU_MIXCHAN_HEADPHONE,AU_MIXCHAN_SPDIFOUT};

void AU_setmixer_outs(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int newvalue)
{
 unsigned int i;

 for(i=0;i<AU_MIXCHANS_OUTS;i++)
  AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK(au_mixchan_outs[i],AU_MIXCHANFUNC_VOLUME),setmode,newvalue);

 aui->card_master_volume=aui->card_mixer_values[AU_MIXCHAN_MASTER]; // ???
}

void AU_setmixer_all(struct mpxplay_audioout_info_s *aui)
{
 unsigned int i;
 int vol=aui->card_master_volume;

 if(vol>=0) // we set all output channels to the master volume
  for(i=0;i<AU_MIXCHANS_OUTS;i++)
   if(aui->card_mixer_values[au_mixchan_outs[i]]<0) // except the separated settings
    aui->card_mixer_values[au_mixchan_outs[i]]=vol;

 for(i=0;i<AU_MIXCHANS_NUM;i++){
  vol=aui->card_mixer_values[i];
  if(vol>=0){
#ifdef AU_AUTO_UNMUTE
   AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_MUTE),MIXER_SETMODE_ABSOLUTE, ((i==AU_MIXCHAN_BASS) || (i==AU_MIXCHAN_TREBLE))? AU_MIXCHAN_MAX_VALUE_TONE:AU_MIXCHAN_MAX_VALUE_VOLUME);
#endif
   AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_VOLUME),MIXER_SETMODE_ABSOLUTE,vol);
  }else{
   vol=AU_getmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_VOLUME));
   if(vol>=0)
    aui->card_mixer_values[i]=vol;
  }
 }
}

//-------------------------------------------------------------------------
#define SOUNDCARD_BUFFER_PROTECTION 32 // in bytes (requried for PCI cards)

#ifndef SBEMU
static
#endif
unsigned int AU_cardbuf_space(struct mpxplay_audioout_info_s *aui)
{
 unsigned long buffer_protection;

 buffer_protection=SOUNDCARD_BUFFER_PROTECTION;     // rounding to bytespersign
 buffer_protection+=aui->card_bytespersign-1;
 buffer_protection-=(buffer_protection%aui->card_bytespersign);

 if(aui->card_dmalastput>=aui->card_dmasize) // checking
  aui->card_dmalastput=0;

 if(aui->card_handler->cardbuf_pos){
  if(aui->card_handler->infobits&SNDCARD_CARDBUF_SPACE){
   if(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING){
    aui->card_dmaspace=aui->card_handler->cardbuf_pos(aui);
    aui->card_dmaspace-=(aui->card_dmaspace%aui->card_bytespersign); // round
   }else
    aui->card_dmaspace=(aui->card_dmaspace>aui->card_outbytes)? (aui->card_dmaspace-aui->card_outbytes):0;
  }else{
   unsigned long bufpos;

   if(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING){
    bufpos=aui->card_handler->cardbuf_pos(aui);
    if(bufpos>=aui->card_dmasize)  // checking
     bufpos=0;
    else
     bufpos-=(bufpos%aui->card_bytespersign); // round

    if(aui->card_infobits&AUINFOS_CARDINFOBIT_DMAUNDERRUN){   // sets a new put-pointer in this case
     if(bufpos>=aui->card_outbytes)
      aui->card_dmalastput=bufpos-aui->card_outbytes;
     else
      aui->card_dmalastput=aui->card_dmasize+bufpos-aui->card_outbytes;
     funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
    }
   }else{
    bufpos=0;
   }

   //if(aui->card_dmalastput>=aui->card_dmasize) // checking
   // aui->card_dmalastput=0;

   if(bufpos>aui->card_dmalastput)
    aui->card_dmaspace=bufpos-aui->card_dmalastput;
   else
    aui->card_dmaspace=aui->card_dmasize-aui->card_dmalastput+bufpos;
  }
 }else{
  aui->card_dmaspace=aui->card_outbytes+buffer_protection;
  funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
 }

 if(aui->card_dmaspace>aui->card_dmasize) // checking
  aui->card_dmaspace=aui->card_dmasize;

 aui->card_dmafilled=aui->card_dmasize-aui->card_dmaspace;

 return (aui->card_dmaspace>buffer_protection)? (aui->card_dmaspace-buffer_protection):0;
}

int AU_writedata(struct mpxplay_audioout_info_s *aui)
{
 unsigned int outbytes_left;

 if(!aui->samplenum)
  return 0;

 if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT)){
  aui->samplenum-=(aui->samplenum%aui->chan_card); // if samplenum is buggy (round to chan_card)
  outbytes_left=aui->samplenum*aui->bytespersample_card;
 }else
  outbytes_left=aui->samplenum;

 #ifdef SBEMU
 aui->card_outbytes =min(outbytes_left,(aui->card_dmasize));
 #else
 aui->card_outbytes =min(outbytes_left,(aui->card_dmasize/4));
 #endif

 if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT))
  aui->card_outbytes-=(aui->card_outbytes%aui->card_bytespersign);

 int left = aucards_writedata_func(aui,outbytes_left); // normal or intsound

 aui->samplenum = 0;

 //slow processor test :)
 //{
 // unsigned int i;
 // for(i=0;i<0x0080ffff;i++);
 //}
 return left/aui->bytespersample_card;
}

static int aucards_writedata_normal(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left)
{
 unsigned long space,first;
 char *pcm_outdata=(char *)aui->pcm_sample;

 funcbit_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
 allcputime+=outbytes_left;
#ifdef __DOS__
 if(!(aui->card_handler->infobits&SNDCARD_INT08_ALLOWED))
  int08counter+=REFRESH_DELAY_JOYMOUSE;
#endif
 first=1;

 do{
  space=AU_cardbuf_space(aui);            // pre-checking (because it's not called before)
  if(first){
   allcpuusage+=space; // CPU usage
   first=0;
  }
  if(space<=aui->card_outbytes){
   AU_start(aui); // start playing (only then) if the DMA buffer is full
   if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DMADONTWAIT){
    funcbit_disable(aui->card_controlbits,AUINFOS_CARDCNTRLBIT_DMADONTWAIT);
    return aui->card_outbytes;
   }
  }
  if(space>=aui->card_bytespersign){
   unsigned int outbytes_putblock=min(space,outbytes_left);

   aui->card_handler->cardbuf_writedata(aui,pcm_outdata,outbytes_putblock);
   pcm_outdata+=outbytes_putblock;
   outbytes_left-=outbytes_putblock;

   aui->card_dmafilled+=outbytes_putblock; // dma monitor needs this
  }
  if(!outbytes_left)
   break;
 }while(1);
 return 0;
}

static int aucards_writedata_intsound(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left)
{
 char *pcm_outdata=(char *)aui->pcm_sample;
 unsigned long buffer_protection,space;

 buffer_protection=SOUNDCARD_BUFFER_PROTECTION;
 buffer_protection+=aui->card_bytespersign-1;
 buffer_protection-=(buffer_protection%aui->card_bytespersign);

 space=(aui->card_dmaspace>buffer_protection)? (aui->card_dmaspace-buffer_protection):0;

 do{
  if(space>=aui->card_bytespersign){
   unsigned int outbytes_putblock=min(space,outbytes_left);
   aui->card_handler->cardbuf_writedata(aui,pcm_outdata,outbytes_putblock);
   pcm_outdata+=outbytes_putblock;
   outbytes_left-=outbytes_putblock;

   aui->card_dmafilled+=outbytes_putblock;
   if(aui->card_dmafilled>aui->card_dmasize)
    aui->card_dmafilled=aui->card_dmasize;
   if(aui->card_dmaspace>outbytes_putblock)
    aui->card_dmaspace-=outbytes_putblock;
   else
    aui->card_dmaspace=0;
  }
  if(!outbytes_left)
   break;
  space=AU_cardbuf_space(aui); // post-checking (because aucards_interrupt_decoder also calls it)
 }while(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING);
 return 0;
}

#ifdef SBEMU
static int aucards_writedata_nowait(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left)
{
 char *pcm_outdata=(char *)aui->pcm_sample;
 unsigned long buffer_protection,space;

 buffer_protection=SOUNDCARD_BUFFER_PROTECTION;
 buffer_protection+=aui->card_bytespersign-1;
 buffer_protection-=(buffer_protection%aui->card_bytespersign);

 space=(aui->card_dmaspace>buffer_protection)? (aui->card_dmaspace-buffer_protection):0;

 do{
  if(space>=aui->card_bytespersign){
   unsigned int outbytes_putblock=min(space,outbytes_left);
   aui->card_handler->cardbuf_writedata(aui,pcm_outdata,outbytes_putblock);
   pcm_outdata+=outbytes_putblock;
   outbytes_left-=outbytes_putblock;
   space-=outbytes_putblock;

   aui->card_dmafilled+=outbytes_putblock;
   if(aui->card_dmafilled>aui->card_dmasize)
    aui->card_dmafilled=aui->card_dmasize;
   if(aui->card_dmaspace>outbytes_putblock)
    aui->card_dmaspace-=outbytes_putblock;
   else
    aui->card_dmaspace=0;
  }
  if(!outbytes_left)
   break;
  //space=AU_cardbuf_space(aui); // post-checking (because aucards_interrupt_decoder also calls it)
 }while(space>=aui->card_bytespersign);
 return outbytes_left;
}
#endif

//---------------------------------------------------------------------------
static void aucards_dma_monitor(void)
{
 struct mpxplay_audioout_info_s *aui=&au_infos;
 if(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING)
  if(aui->card_handler->cardbuf_int_monitor)
   aui->card_handler->cardbuf_int_monitor(aui);
}

//---------------- Timer Interrupt ------------------------------------------
unsigned long intdec_timer_counter;
#ifndef SBEMU
static void aucards_interrupt_decoder(void)
{
 struct mpxplay_audioout_info_s *aui=&au_infos;
 struct mainvars *mvp=aui->mvp;
 unsigned int i;
 if(!funcbit_smp_test(intsoundcontrol,INTSOUND_DECODER))
  return;
 if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_PLAYING) || (mvp->idone==MPXPLAY_ERROR_INFILE_EOF))
  return;
 if(!(aui->card_handler->cardbuf_int_monitor))
  AU_cardbuf_space(aui);
 if((aui->card_dmasize-aui->card_dmafilled)<aui->card_outbytes){
  funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
  goto aid_end;
 }
 if(aui->card_handler->cardbuf_int_monitor)
  if(AU_cardbuf_space(aui)<aui->card_outbytes){
   funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
   goto aid_end;
  }

 funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);

 if(mvp->idone==MPXPLAY_ERROR_INFILE_RESYNC)
  return;

 i=0;
 do{
  funcbit_smp_int32_put(mvp->idone,mpxplay_infile_decode(aui));
  if((mvp->idone==MPXPLAY_ERROR_INFILE_EOF) || (mvp->idone==MPXPLAY_ERROR_INFILE_NODATA) || (mvp->idone==MPXPLAY_ERROR_INFILE_SYNC_IN) || (mvp->idone==MPXPLAY_ERROR_INFILE_RESYNC))
   break;
  display_bufpos_int08(mvp);
  if((aui->card_dmasize-aui->card_dmafilled)<aui->card_outbytes){
   funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAFULL);
   break;
  }
 }while((++i)<aui->int08_decoder_cycles);

aid_end:
 aucards_get_cpuusage_int08();
}
#endif

#if defined(__DOS__)

static void aucards_get_cpuusage_int08(void)
{
 unsigned long t=intdec_timer_counter;
 allcputime+=t;
 outp(0x43,0);
 t-=inp(0x40);
 t-=inp(0x40)<<8;
 allcpuusage+=t;
 intdec_timer_counter=0;
}

#else

static unsigned long intdec_prevtickcount;

static void aucards_get_cpuusage_int08(void)
{
 unsigned long currtickcount=pds_threads_timer_tick_get();
 //char sout[100];

 if(intdec_prevtickcount && (intdec_prevtickcount<=intdec_timer_counter) && (intdec_timer_counter<=currtickcount)){
  unsigned long tlen=currtickcount-intdec_timer_counter;
  if(tlen){
   //sprintf(sout,"%6d %6d",currtickcount-intdec_timer_counter,intdec_timer_counter-intdec_prevtickcount);
   //pds_textdisplay_printf(sout);
   allcpuusage+=tlen;
   allcputime+=intdec_timer_counter-intdec_prevtickcount;
   intdec_prevtickcount=intdec_timer_counter;
   intdec_timer_counter=0;
  }
 }else{
  intdec_prevtickcount=intdec_timer_counter;
  intdec_timer_counter=0;
 }
}

#endif
