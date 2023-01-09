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
//function: MIDAS library handling

//#define AU_CARDS_LINK_MIDAS 1

#if defined(AU_CARDS_LINK_MIDAS) && defined(__DOS__)

#include <midasdll.h>
#include "mpxplay.h"
#include "control\control.h"

static void MIDAS_setvol(void);

static aucards_onemixerchan_s midas_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),1,{{0x01,127,0,0}}
};

static aucards_allmixerchan_s midas_mixer_info[]={
 &midas_master_vol,
 NULL
};

static void *midashandle;
static unsigned int MIDAS_volume;

static int MIDAS_detect(struct mpxplay_audioout_info_s *aui)
{
 unsigned int iscard,len;
 char midascnffile[MAX_PATHNAMELEN];
 mpxplay_control_get_ini_path(midascnffile);
 pds_filename_assemble_fullname(midascnffile, midascnffile, "MPXMIDAS.INI");
 MIDASstartup();
 MIDASsetOption(MIDAS_OPTION_MIXING_MODE,1);
 if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_MIDASMANUALCFG){
  iscard=MIDASconfig();
  MIDASsaveConfig(midascnffile);
 }else{
  iscard=MIDASloadConfig(midascnffile);
  if(!iscard)
   iscard=MIDASdetectSoundCard();
 }
 iscard+=MIDASinit();
 iscard+=MIDASopenChannels(2);
 if(iscard==3){
  midashandle=MIDASplayStreamPolling(4,aui->freq_card,1000);
  if(midashandle){
   MIDASpauseStream(midashandle);
   iscard++;
  }
 }
 if(iscard<4){
  pds_textdisplay_printf("MIDAS error: ");
  pds_textdisplay_printf(MIDASgetErrorMessage(MIDASgetLastError()));
  MIDASclose();
  return 0;
 }
 return 1;
}

static void MIDAS_start(struct mpxplay_audioout_info_s *aui)
{
 if(midashandle){
  MIDASresumeStream(midashandle);
 }else{
  midashandle=MIDASplayStreamPolling(4,aui->freq_card,1000);
  if(midashandle==NULL){
   pds_textdisplay_printf("MIDAS error: ");
   pds_textdisplay_printf(MIDASgetErrorMessage(MIDASgetLastError()));
   MIDASclose();
   exit(0);
  }
  MIDAS_setvol();
 }
 aui->card_infobits|=AUINFOS_CARDINFOBIT_PLAYING;
}

static void MIDAS_stop(struct mpxplay_audioout_info_s *aui)
{
 if(midashandle)
  MIDASpauseStream(midashandle);
}

static void MIDAS_close(struct mpxplay_audioout_info_s *aui)
{
 MIDASclose();
}

static void MIDAS_setrate(struct mpxplay_audioout_info_s *aui)
{
 aui->freq_card=44100;
 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 aui->card_dmasize=aui->freq_card/25;
 if(aui->chan_card>1)
  aui->card_dmasize*=2;
 if(aui->bits_card>8)
  aui->card_dmasize*=2;
 aui->card_dmasize=(aui->card_dmasize+31)&(~31);
}

static void MIDAS_setvol(void)
{
 if(midashandle && MIDAS_volume)
  MIDASsetStreamVolume(midashandle,MIDAS_volume);
}

static void MIDAS_setmixervol(struct mpxplay_audioout_info_s *aui,unsigned long notused,unsigned long value)
{
 MIDAS_volume=value;
 if(midashandle)
  MIDASsetStreamVolume(midashandle,value);
}

static unsigned long MIDAS_getmixervol(struct mpxplay_audioout_info_s *aui,unsigned long notused)
{
 if(!MIDAS_volume)
  MIDAS_volume=127;
 return MIDAS_volume;
}

static long MIDAS_bufspace(struct mpxplay_audioout_info_s *aui)
{
 if(!midashandle)
  MIDAS_start(aui);
 if(midashandle){
  unsigned int bufbytes=MIDASgetStreamBytesBuffered(midashandle);
  if(aui->card_dmasize>=bufbytes)
   return (aui->card_dmasize-bufbytes);
 }
 return 0;
}

static void MIDAS_writedata(struct mpxplay_audioout_info_s *aui,char *sample,unsigned long outbytenum)
{
 if(!midashandle)
  MIDAS_start(aui);

 MIDASfeedStreamData(midashandle,sample,outbytenum,0);
}

static void MIDAS_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 if(midashandle){
  MIDASstopStream(midashandle);
  midashandle=NULL;
 }
}

one_sndcard_info MIDAS_sndcard_info={
 "MID",
 SNDCARD_CARDBUF_SPACE,//infobits (int08 is not allowed, because MIDAS uses it)

 NULL,
 NULL,                //card_init (no init function, MIDAS detection runs last on this way)
 &MIDAS_detect,       //card_detect
 NULL,                //card_info
 &MIDAS_start,        //card_start
 &MIDAS_stop,         //card_stop
 &MIDAS_close,        //card_close
 &MIDAS_setrate,      //card_setrate

 &MIDAS_writedata,    //cardbuf_writedata
 &MIDAS_bufspace,     //cardbuf_pos (space)
 &MIDAS_clearbuf,     //cardbuf_clear
 NULL,                //cardbuf_int_monitor (MIDAS has its own)
 NULL,                //irq_routine (MIDAS has its own)
 &MIDAS_setmixervol,
 &MIDAS_getmixervol,
 &midas_mixer_info[0]
};

#endif // AU_CARDS_LINK_MIDAS
