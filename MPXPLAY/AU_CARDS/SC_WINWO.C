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
//function: Wibdows WavOut sound output

//#define MPXPLAY_USE_DEBUGF 1
//#define WAVOUT_DEBUG_OUTPUT NULL

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_WINWAVOUT

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <mmsystem.h>
#include "dmairq.h"

//#define SCWAVOUTAP_DEBUGF 1

#define MPXP_WAVEMAPPER_MIN_FREQ       8192 // manual (-of) freq can be lower
#define MPXP_WAVEMAPPER_MAX_FREQ     192000 // or higher too
#define MPXP_WAVEMAPPER_MAX_BUFSIZE 1048576

#define MPXP_WAVEMAPPER_BUFHDR_BLOCKS 32 // inline calc?

#define MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED  1

typedef struct wavemapper_data_s{
 unsigned long flags;
 unsigned int nb_devices;
 unsigned int curr_deviceid;
 WAVEOUTCAPSA *devinfo;
 WAVEOUTCAPSA *curr_dev;
 HWAVEOUT handle;
 WAVEFORMATEX waveform;
 unsigned long pcmout_bufsize;
 char *pcmout_buffer;
 unsigned long freq_config;
 unsigned int nb_bufblock;
 unsigned int curr_bufblock;
 WAVEHDR *bufhdr;
 MMTIME bufinf;
}wavemapper_data_s;

static void WINWAVOUT_close(struct mpxplay_audioout_info_s *aui);
static void WINWAVOUT_clearbuf(struct mpxplay_audioout_info_s *aui);

static void wavout_display_error(unsigned int error,char *header)
{
 unsigned int len=pds_strlen(header);
 char sout[160];
 pds_strcpy(sout,header);
 waveOutGetErrorText(error,&sout[len],sizeof(sout)-len);
 pds_textdisplay_printf(sout);
}

static void wavout_set_waveform(WAVEFORMATEX *wf,struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned long freq;

 if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT){
  aui->bits_card=1;
  wf->wBitsPerSample=sizeof(float)*8;
 }else{
  aui->card_wave_id=WAVE_FORMAT_PCM;
  aui->bits_card=wf->wBitsPerSample=(aui->bits_set)? aui->bits_set:16;
 }

 wf->wFormatTag=aui->card_wave_id;
 aui->chan_card=wf->nChannels=(aui->chan_set)? aui->chan_set:2;

 if(aui->freq_set){
  freq=aui->freq_set;
  card->freq_config=freq;
 }else{
  freq=aui->freq_card;
  if(freq<MPXP_WAVEMAPPER_MIN_FREQ)
   freq=MPXP_WAVEMAPPER_MIN_FREQ;
  else
   if(freq>MPXP_WAVEMAPPER_MAX_FREQ)
    freq=MPXP_WAVEMAPPER_MAX_FREQ;
  card->freq_config=MPXP_WAVEMAPPER_MAX_FREQ;
 }
 aui->freq_card = wf->nSamplesPerSec = freq;

 wf->nBlockAlign = wf->wBitsPerSample/8*wf->nChannels;
 wf->nAvgBytesPerSec = wf->nSamplesPerSec*wf->nBlockAlign;
 wf->cbSize=0;
}

static int WINWAVOUT_detect(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card;
 unsigned int i,error;

 card=pds_calloc(1,sizeof(*card));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->nb_devices=waveOutGetNumDevs();

 card->devinfo=pds_calloc(card->nb_devices+1,sizeof(*card->devinfo));
 if(!card->devinfo)
  goto err_out_detect;

 if(aui->card_select_devicenum && card->nb_devices){
  unsigned int devnum=aui->card_select_devicenum;
  if(devnum>card->nb_devices)
   devnum=card->nb_devices;
  card->curr_deviceid=devnum-1;
  card->curr_dev=&card->devinfo[devnum];
 }else{
  card->curr_deviceid=WAVE_MAPPER;
  card->curr_dev=&card->devinfo[0];
 }

 waveOutGetDevCaps(WAVE_MAPPER,&card->devinfo[0],sizeof(*card->devinfo));
 for(i=0;i<card->nb_devices;i++)
  waveOutGetDevCaps(i,&card->devinfo[i+1],sizeof(*card->devinfo));

 wavout_set_waveform(&card->waveform,aui);

 error=waveOutOpen(&card->handle, card->curr_deviceid, &card->waveform, 0,0, WAVE_FORMAT_QUERY);

 if(error!=MMSYSERR_NOERROR){
  wavout_display_error(error,"WIN : ");
  goto err_out_detect;
 }

 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,MPXP_WAVEMAPPER_MAX_BUFSIZE,card->waveform.nBlockAlign,card->waveform.nBlockAlign/card->waveform.nChannels,card->freq_config);
 card->pcmout_buffer=(char *)pds_malloc(card->pcmout_bufsize);
 if(!card->pcmout_buffer)
  goto err_out_detect;
 aui->card_DMABUFF=card->pcmout_buffer;

 card->nb_bufblock=MPXP_WAVEMAPPER_BUFHDR_BLOCKS;
 card->bufhdr=(WAVEHDR *)pds_calloc(card->nb_bufblock,sizeof(WAVEHDR));
 if(!card->bufhdr)
  goto err_out_detect;

 return 1;

err_out_detect:
 WINWAVOUT_close(aui);
 return 0;
}

static void WINWAVOUT_close(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 if(card){
  if(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED){
   WINWAVOUT_clearbuf(aui);
   waveOutClose(card->handle);
  }
  if(card->devinfo)
   pds_free(card->devinfo);
  if(card->pcmout_buffer)
   pds_free(card->pcmout_buffer);
  if(card->bufhdr)
   pds_free(card->bufhdr);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void WINWAVOUT_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int i;
 char sout[100];

 if(card && card->devinfo){
  for(i=0;i<card->nb_devices+1;i++){
   WAVEOUTCAPSA *di=&card->devinfo[i];
   snprintf(sout,sizeof(sout),"%s dev%d: %s (chans:%d) %s",((i==0)? "WIN :":"     "),
    i,di->szPname,(int)di->wChannels,((di==card->curr_dev)? "(selected)":""));
   /*snprintf(sout,sizeof(sout)," %d. m:%d p:%d n:%s f:%8.8X c:%d r:%d s:%d",i+1,
    (int)di->wMid,(int)di->wPid,di->szPname,di->dwFormats,(int)di->wChannels,
    (int)di->wReserved1,di->dwSupport);*/
   pds_textdisplay_printf(sout);
  }
 }
}

static void WINWAVOUT_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int error=0;
 WAVEFORMATEX *wf;

 if(!card)
  return;
 wf=&card->waveform;
 //if(!(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED) || (aui->card_wave_id!=wf->wFormatTag) || (aui->chan_card!=wf->nChannels) || (aui->bits_card!=wf->wBitsPerSample)){
  if(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED){
   WINWAVOUT_clearbuf(aui);
   error=waveOutClose(card->handle);
   funcbit_disable(card->flags,MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED);
#ifdef SCWAVOUTAP_DEBUGF
   if(error!=MMSYSERR_NOERROR)
    wavout_display_error(error,"close: ");
#endif
  }
  wavout_set_waveform(wf,aui);
  error=waveOutOpen(&card->handle, card->curr_deviceid, wf, 0,0,0);
  if(error==MMSYSERR_NOERROR)
   funcbit_enable(card->flags,MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED);
#ifdef SCWAVOUTAP_DEBUGF
  else
   wavout_display_error(error,"open: ");
#endif
 //}else{
 // wavout_set_waveform(wf,aui);
 // error=waveOutSetPlaybackRate(card->handle,aui->freq_card);
 //}
 MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,card->waveform.nBlockAlign,card->freq_config);

 mpxplay_debugf(WAVOUT_DEBUG_OUTPUT,"bs:%d ds:%d",card->pcmout_bufsize,aui->card_dmasize);
}

static void WINWAVOUT_start(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int error;
 if(!card)
  return;
 if(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED){
  error=waveOutRestart(card->handle);
#ifdef SCWAVOUTAP_DEBUGF
  wavout_display_error(error,"start: ");
#endif
 }
}

static void WINWAVOUT_stop(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int error;
 if(!card)
  return;
 if(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED){
  error=waveOutPause(card->handle);
#ifdef SCWAVOUTAP_DEBUGF
  wavout_display_error(error,"stop: ");
#endif
 }
}

static unsigned int wavout_checknexthdr(struct wavemapper_data_s *card,unsigned int bufblock)
{
 WAVEHDR *curbuf;
 if(bufblock>=card->nb_bufblock)
  bufblock-=card->nb_bufblock;
 if(bufblock>=card->nb_bufblock)
  bufblock=0;
 curbuf=&card->bufhdr[bufblock];
 if((curbuf->dwFlags&WHDR_PREPARED) && !(curbuf->dwFlags&WHDR_DONE))
  return 0;
 return 1;
}

static unsigned int wavout_writehdr(struct wavemapper_data_s *card,char *pcmdata,unsigned int bytes)
{
 unsigned int error;
 WAVEHDR *curbuf=&card->bufhdr[card->curr_bufblock];

 if((curbuf->dwFlags&WHDR_PREPARED) && (curbuf->dwFlags&WHDR_DONE)){
  error=waveOutUnprepareHeader(card->handle,curbuf,sizeof(*curbuf));
#ifdef SCWAVOUTAP_DEBUGF
  if(error!=MMSYSERR_NOERROR)
   wavout_display_error(error,"unprepare: ");
#endif
 }
 if(curbuf->dwFlags&WHDR_PREPARED){ //
  mpxplay_debugf(WAVOUT_DEBUG_OUTPUT,"!!! bb:%d",card->curr_bufblock);
  return 0;                         // !!!
 }
 pds_memset(curbuf,0,sizeof(WAVEHDR));

 curbuf->lpData=pcmdata;
 curbuf->dwBufferLength=bytes;
 curbuf->dwFlags=0;

 error=waveOutPrepareHeader(card->handle,curbuf,sizeof(*curbuf));
#ifdef SCWAVOUTAP_DEBUGF
 if(error!=MMSYSERR_NOERROR)
  wavout_display_error(error,"prepare: ");
#endif
 error=waveOutWrite(card->handle,curbuf,sizeof(*curbuf));
#ifdef SCWAVOUTAP_DEBUGF
 if(error!=MMSYSERR_NOERROR)
  wavout_display_error(error,"write: ");
#endif
 card->curr_bufblock++;
 if(card->curr_bufblock>=card->nb_bufblock)
  card->curr_bufblock=0;
 return 1;
}

static void WINWAVOUT_writedata(struct mpxplay_audioout_info_s *aui,char *pcm_sample,unsigned long outbytenum)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int todo;
 if(!card)
  return;
 if(!(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED))
  return;

 todo=aui->card_dmasize-aui->card_dmalastput;

 if(todo<=outbytenum){
  pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,pcm_sample,todo);
  wavout_writehdr(card,aui->card_DMABUFF+aui->card_dmalastput,todo);
  aui->card_dmalastput=0;
  outbytenum-=todo;
  pcm_sample+=todo;
 }
 if(outbytenum){
  pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,pcm_sample,outbytenum);
  wavout_writehdr(card,aui->card_DMABUFF+aui->card_dmalastput,outbytenum);
  aui->card_dmalastput+=outbytenum;
 }
}

static long WINWAVOUT_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int storedbytes,i;
 WAVEHDR *bufptr;

 if(!card)
  return 0;
 if(!(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED))
  return 0;

 if(!wavout_checknexthdr(card,card->curr_bufblock))  // we need min 2 hdr buffer ahead
  return 0;                                          //
 if(!wavout_checknexthdr(card,card->curr_bufblock+1))//
  return 0;                                          //

 bufptr=&card->bufhdr[0];
 storedbytes=0;
 i=card->nb_bufblock;
 do{
  if((bufptr->dwFlags&WHDR_PREPARED) && !(bufptr->dwFlags&WHDR_DONE))
   storedbytes+=bufptr->dwBufferLength;
  bufptr++;
 }while(--i);

 if(aui->card_dmasize<=storedbytes)
  return 0;
 return (aui->card_dmasize-storedbytes);
}

static void WINWAVOUT_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 unsigned int error,i;
 if(!card)
  return;
 if(!(card->flags&MPXP_WAVEMAPPER_FLAG_WAVEOUTOPENED))
  return;
 error=waveOutReset(card->handle);
#ifdef SCWAVOUTAP_DEBUGF
 wavout_display_error(error,"clearbuf: ");
#endif
 for(i=0;i<card->nb_bufblock;i++){
  WAVEHDR *curbuf=&card->bufhdr[i];
  if(curbuf->dwFlags&WHDR_PREPARED){
   error=waveOutUnprepareHeader(card->handle,curbuf,sizeof(*curbuf));
#ifdef SCWAVOUTAP_DEBUGF
   if(error!=MMSYSERR_NOERROR)
    wavout_display_error(error,"clr unprepare: ");
#endif
  }
  pds_memset(curbuf,0,sizeof(WAVEHDR));
 }
 card->curr_bufblock=0;
}

static void WINWAVOUT_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 if(!card || !card->curr_dev)
  return;
 if(card->curr_dev->dwSupport&WAVECAPS_VOLUME){
  DWORD vol=val&0xffff;
  vol|=vol<<16;
  waveOutSetVolume(card->handle, vol);
 }
}

static unsigned long WINWAVOUT_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct wavemapper_data_s *card=aui->card_private_data;
 if(!card || !card->curr_dev)
  return 0;
 if(card->curr_dev->dwSupport&WAVECAPS_VOLUME){
  DWORD vol=0;
  waveOutGetVolume(card->handle,&vol);
  return (vol&0xffff);
 }
 return 0;
}

static aucards_onemixerchan_s winwavout_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),1,{{0x01,0xffff,0,0}}};

static aucards_allmixerchan_s winwavout_mixerset[]={
 &winwavout_master_vol, // it's rather a pcm volume, just Mpxplay displays the 'master' value on the screen
 NULL
};

one_sndcard_info WINWAVOUT_sndcard_info={
 "WIN",
 SNDCARD_CARDBUF_SPACE|SNDCARD_INT08_ALLOWED,
 NULL,                 // card_config
 NULL,                 // card_init
 &WINWAVOUT_detect,    // card_detect
 &WINWAVOUT_card_info, // card_info
 &WINWAVOUT_start,     // card_start
 &WINWAVOUT_stop,      // card_stop
 &WINWAVOUT_close,     // card_close
 &WINWAVOUT_setrate,   // card_setrate
 &WINWAVOUT_writedata, // cardbuf_writedata
 &WINWAVOUT_getbufpos, // cardbuf_pos
 &WINWAVOUT_clearbuf,  // cardbuf_clear
 NULL,                 // cardbuf_int_monitor
 NULL,                 // irq_routine
 &WINWAVOUT_writeMIXER,// card_writemixer
 &WINWAVOUT_readMIXER, // card_readmixer
 &winwavout_mixerset[0]// card_mixerchans
};

#endif // AU_CARDS_LINK_WINWAVOUT
