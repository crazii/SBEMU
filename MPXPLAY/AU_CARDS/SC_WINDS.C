//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Win32 DirectSound output
//based on DSound interface of Mplayer

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_WINDSOUND

#include "dmairq.h"
#include "au_mixer\mix_func.h"

//#define DIRECTSOUND_VERSION 0x0600
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <mmsystem.h>
#include "dsound.h"

#define MPXP_WINDSOUND_MAX_CHANNELS 8
#define MPXP_WINDSOUND_MAX_BUFSIZE 1048576

//#define MPXP_WINDSOUND_PARALELL_WAV 1 // !!! for testing only (write a wav file parallel with playing)

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

static const GUID KSDATAFORMAT_SUBTYPE_PCM = {WAVE_FORMAT_PCM,0x0000,0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ={ WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};
static const GUID KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF ={ WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }};

static const int channel_mask[] = {
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT,
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT ,
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY,
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT,
 SPEAKER_FRONT_LEFT | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT | SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
};

static mpxp_uint8_t winds_channel_matrix[MPXP_WINDSOUND_MAX_CHANNELS-2][MPXP_WINDSOUND_MAX_CHANNELS]={
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_LFE,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,  MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_LFE,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,MPXPLAY_PCMOUTCHAN_DISABLED,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,MPXPLAY_PCMOUTCHAN_SIDE_LEFT,MPXPLAY_PCMOUTCHAN_SIDE_RIGHT,MPXPLAY_PCMOUTCHAN_DISABLED},
{MPXPLAY_PCMOUTCHAN_FRONT_LEFT,MPXPLAY_PCMOUTCHAN_FRONT_RIGHT,MPXPLAY_PCMOUTCHAN_FRONT_CENTER,MPXPLAY_PCMOUTCHAN_LFE,MPXPLAY_PCMOUTCHAN_REAR_LEFT,MPXPLAY_PCMOUTCHAN_REAR_RIGHT,MPXPLAY_PCMOUTCHAN_SIDE_LEFT,MPXPLAY_PCMOUTCHAN_SIDE_RIGHT}
};

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
 WAVEFORMATEX    Format;
 union {
  WORD wValidBitsPerSample;
  WORD wSamplesPerBlock;
  WORD wReserved;
 } Samples;
 DWORD           dwChannelMask;
 GUID            SubFormat;
}WAVEFORMATEXTENSIBLE;
#endif

#define WINDS_FLAG_PRIBUF_INIT_OK 1
#define WINDS_FLAG_PRINT_DEVICES  2

typedef struct winds_data_s{
 LPDIRECTSOUND hds;
 LPDIRECTSOUNDBUFFER hdspribuf;
 LPDIRECTSOUNDBUFFER hdsbuf;
 unsigned long flags;
 unsigned long pcmout_bufsize;
 unsigned int config_select;
 unsigned int devicenum_select;
 unsigned int devicenum_count;
 GUID device;
 DSCAPS dscaps;
 WAVEFORMATEXTENSIBLE wf; // gcc (stack calling conv) doesn't like stack infos
 DSBUFFERDESC dsbpridesc;
 DSBUFFERDESC dsbdesc;
}winds_data_s;

#define AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF 1
#define AUCARDSCONFIG_WDS_LOCHARDWARE       2
#define AUCARDSCONFIG_WDS_DEFAULT           AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF

#ifdef MPXP_WINDSOUND_PARALELL_WAV
extern one_sndcard_info WAV_sndcard_info;
#endif

static void WINDS_setrate(struct mpxplay_audioout_info_s *aui);
static void WINDS_close(struct mpxplay_audioout_info_s *aui);

static HWND scwinds_get_topwindow(void)
{
 HWND window_handler;
#ifdef MPXPLAY_GUI_CONSOLE
 if(newfunc_dllload_kernel32_init())
  window_handler = (HWND)newfunc_dllload_kernel32_call(MPXPLAY_KERNEL32FUNC_GETCONSOLEWINDOW, NULL, 0);
 else
  window_handler = NULL;
#else
 window_handler = GetTopWindow(NULL);
#endif
 if(!window_handler)
  window_handler = GetDesktopWindow();
 return window_handler;
}

static BOOL CALLBACK DirectSoundEnum(LPGUID guid,LPCSTR desc,LPCSTR module,LPVOID context)
{
 struct winds_data_s *card=(winds_data_s *)context;
 char sout[80];
 if(card->flags&WINDS_FLAG_PRINT_DEVICES){
  snprintf(sout,sizeof(sout),"%s dev%d: %s %s",((card->devicenum_count==0)? "WDS :":"     "),
    card->devicenum_count,desc,((card->devicenum_select==card->devicenum_count)? "(selected)":""));
  pds_textdisplay_printf(sout);
 }

 if((card->devicenum_select==card->devicenum_count) && guid)
  memcpy(&card->device,guid,sizeof(GUID));
 card->devicenum_count++;
 return TRUE;
}

static int WINDS_detect(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card;

 card=calloc(1,sizeof(*card));
 if(!card)
  return 0;
 aui->card_private_data=card;

 if(aui->card_select_config>=0)
  card->config_select=aui->card_select_config;
 else
  card->config_select=AUCARDSCONFIG_WDS_DEFAULT;

 if(aui->card_select_devicenum){
  card->devicenum_select=aui->card_select_devicenum;
  DirectSoundEnumerateA(DirectSoundEnum,card);
 }

 if(DirectSoundCreate(((card->devicenum_count)? (&card->device):NULL),&card->hds,NULL)!=0)
  goto err_out_detect;

 if(IDirectSound_SetCooperativeLevel(card->hds, scwinds_get_topwindow(), DSSCL_EXCLUSIVE))
  goto err_out_detect;

 card->dscaps.dwSize = sizeof(DSCAPS);
 if(IDirectSound_GetCaps(card->hds,&card->dscaps)!=DS_OK)
  goto err_out_detect;

 aui->freq_card=(aui->freq_set)? aui->freq_set:44100;
 aui->chan_card=(aui->chan_set)? aui->chan_set:PCM_CHANNELS_DEFAULT;
 if(aui->chan_card>MPXP_WINDSOUND_MAX_CHANNELS)
  aui->chan_card=MPXP_WINDSOUND_MAX_CHANNELS;
 aui->bits_card=(aui->bits_set)? aui->bits_set:16;

 if(!aui->card_wave_id)
  aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 WINDS_setrate(aui);
 if(!card->hdsbuf) // unsupported freq/bit/chan config
  goto err_out_detect;

 if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF) && !funcbit_test(card->flags,WINDS_FLAG_PRIBUF_INIT_OK))
  pds_textdisplay_printf("DirectSound error: Couldn't config primary buffer (lower quality)!");

 if(aui->chan_card>2)
  aui->card_channelmap=&winds_channel_matrix[aui->chan_card-3][0];

 return 1;

err_out_detect:
 WINDS_close(aui);
 return 0;
}

static void WINDS_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(!card)
  return;
 card->devicenum_count=0;
 funcbit_enable(card->flags,WINDS_FLAG_PRINT_DEVICES);
 DirectSoundEnumerateA(DirectSoundEnum,card);
 funcbit_disable(card->flags,WINDS_FLAG_PRINT_DEVICES);
}

static void WINDS_start(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(!card || !card->hdsbuf)
  return;
 IDirectSoundBuffer_Play(card->hdsbuf, 0, 0, DSBPLAY_LOOPING);
}

static void WINDS_stop(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(!card || !card->hdsbuf)
  return;
 IDirectSoundBuffer_Stop(card->hdsbuf);
#ifdef MPXP_WINDSOUND_PARALELL_WAV
 WAV_sndcard_info.card_stop(aui);
#endif
}

static void winds_buffer_release(struct winds_data_s *card)
{
 if(card->hdsbuf){
  IDirectSoundBuffer_Release(card->hdsbuf);
  card->hdsbuf=NULL;
 }
 if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF))
  if(card->hdspribuf){
   IDirectSoundBuffer_Release(card->hdspribuf);
   card->hdspribuf=NULL;
  }
}

static void WINDS_close(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(card){
  winds_buffer_release(card);
  if(card->hds)
   IDirectSound_Release(card->hds);
  free(card);
  aui->card_private_data=NULL;
 }
#ifdef MPXP_WINDSOUND_PARALELL_WAV
 WAV_sndcard_info.card_close(aui);
#endif
}

static void WINDS_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 int result;
 WAVEFORMATEXTENSIBLE *wf;
 DSBUFFERDESC *dsbpridesc;
 DSBUFFERDESC *dsbdesc;

 if(!card || !card->hds)
  return;

 wf=&card->wf;
 dsbpridesc=&card->dsbpridesc;
 dsbdesc=&card->dsbdesc;

 winds_buffer_release(card);

 if(aui->freq_card<44100) // !!! lower freqs don't work properly at me
  aui->freq_card=48000;   //
 if(aui->freq_card<card->dscaps.dwMinSecondarySampleRate)
  aui->freq_card=card->dscaps.dwMinSecondarySampleRate;
 if(aui->freq_card>card->dscaps.dwMaxSecondarySampleRate)
  aui->freq_card=card->dscaps.dwMaxSecondarySampleRate;
 aui->chan_card=(aui->chan_set)? aui->chan_set:2;
 if((aui->chan_card<2) && !aui->chan_set) // mono files converted to stereo in au_mixer
  aui->chan_card=2;
 if(aui->chan_card>MPXP_WINDSOUND_MAX_CHANNELS)
  aui->chan_card=MPXP_WINDSOUND_MAX_CHANNELS;

 pds_memset(wf,0,sizeof(WAVEFORMATEXTENSIBLE));
 wf->Format.nChannels = aui->chan_card;
 wf->Format.nSamplesPerSec  = aui->freq_card;
 switch(aui->card_wave_id){
  case MPXPLAY_WAVEID_AC3:
   wf->Format.wFormatTag     = WAVE_FORMAT_DOLBY_AC3_SPDIF;
   wf->SubFormat             = KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
   wf->dwChannelMask         = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
   wf->Format.wBitsPerSample = 16;
   wf->Format.nChannels      = 2;
   wf->Format.nSamplesPerSec = aui->freq_song;
   if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
    funcbit_enable(aui->card_infobits,(AUINFOS_CARDINFOBIT_BITSTREAMOUT|ADI_CNTRLBIT_BITSTREAMNOFRH)); // ???
   break;
  case MPXPLAY_WAVEID_PCM_FLOAT:
   wf->Format.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
   wf->SubFormat             = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
   wf->Format.wBitsPerSample = 32;
   aui->bits_card = 1;
   break;
  default:
   wf->Format.wFormatTag     = WAVE_FORMAT_PCM;
   wf->SubFormat             = KSDATAFORMAT_SUBTYPE_PCM;
   wf->Format.wBitsPerSample = aui->bits_card;
   aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
   break;
 }
 wf->Format.nBlockAlign = wf->Format.nChannels * (wf->Format.wBitsPerSample >> 3);
 wf->Format.nAvgBytesPerSec = wf->Format.nSamplesPerSec * wf->Format.nBlockAlign;
 wf->Samples.wValidBitsPerSample = wf->Format.wBitsPerSample;

 if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF)){
  pds_memset(dsbpridesc, 0, sizeof(DSBUFFERDESC));
  dsbpridesc->dwSize = sizeof(DSBUFFERDESC);
  dsbpridesc->dwFlags       = DSBCAPS_PRIMARYBUFFER;
  dsbpridesc->dwBufferBytes = 0;
  dsbpridesc->lpwfxFormat   = NULL;
 }

 pds_memset(dsbdesc, 0, sizeof(DSBUFFERDESC));
 dsbdesc->dwSize = sizeof(DSBUFFERDESC);
 dsbdesc->dwFlags = DSBCAPS_GETCURRENTPOSITION2 // Better position accuracy
                 | DSBCAPS_GLOBALFOCUS         // Allows background playing
             | DSBCAPS_CTRLVOLUME;         // volume control enabled
 if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_LOCHARDWARE))
  dsbdesc->dwFlags |= DSBCAPS_LOCHARDWARE;

 if(wf->Format.nChannels>2){
  wf->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wf->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
  wf->dwChannelMask = channel_mask[wf->Format.nChannels - 3];
 }

 //card->pcmout_bufsize = MDma_get_max_pcmoutbufsize(aui,MPXP_WINDSOUND_MAX_BUFSIZE,wf->Format.nBlockAlign,wf->Format.nBlockAlign/wf->Format.nChannels,aui->freq_card);
 card->pcmout_bufsize = MDma_get_max_pcmoutbufsize(aui,MPXP_WINDSOUND_MAX_BUFSIZE,wf->Format.nBlockAlign,wf->Format.nBlockAlign/2,aui->freq_card); // !!!
 dsbdesc->dwBufferBytes = card->pcmout_bufsize;
 dsbdesc->lpwfxFormat = (WAVEFORMATEX *)wf;

 if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF))
  if(IDirectSound_CreateSoundBuffer(card->hds,dsbpridesc,&card->hdspribuf,NULL)==DS_OK)
   if(IDirectSoundBuffer_SetFormat(card->hdspribuf,(WAVEFORMATEX *)wf)==DS_OK)
    funcbit_enable(card->flags,WINDS_FLAG_PRIBUF_INIT_OK);

 result = IDirectSound_CreateSoundBuffer(card->hds, dsbdesc, &card->hdsbuf, NULL);
 if(result!=DS_OK){
  if(dsbdesc->dwFlags & DSBCAPS_LOCHARDWARE){
   dsbdesc->dwFlags &= ~DSBCAPS_LOCHARDWARE;
   result = IDirectSound_CreateSoundBuffer(card->hds, dsbdesc, &card->hdsbuf, NULL);
  }
  if(result!=DS_OK)
   return;
  if(funcbit_test(card->config_select,AUCARDSCONFIG_WDS_CREATE_PRIMARYBUF))
   if(IDirectSoundBuffer_SetFormat(card->hdsbuf,(WAVEFORMATEX *)wf)!=DS_OK)
    return;
 }

 MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,wf->Format.nBlockAlign,0);
 aui->card_dmalastput=0;

 if(aui->chan_card>2)
  aui->card_channelmap=&winds_channel_matrix[aui->chan_card-3][0];
 else
  aui->card_channelmap=NULL;

#ifdef MPXP_WINDSOUND_PARALELL_WAV
 WAV_sndcard_info.card_setrate(aui);
#endif
}

static void WINDS_writedata(struct mpxplay_audioout_info_s *aui,char *pcm_sample,unsigned long outbytenum)
{
 struct winds_data_s *card=aui->card_private_data;
 LPVOID putptr1=NULL,putptr2=NULL;
 DWORD putbytes1=0,putbytes2=0;
 HRESULT result;

 if(!card || !card->hdsbuf)
  return;

 result = IDirectSoundBuffer_Lock(card->hdsbuf,aui->card_dmalastput, outbytenum, &putptr1, &putbytes1, &putptr2, &putbytes2, 0);
 if(result==DSERR_BUFFERLOST){
  IDirectSoundBuffer_Restore(card->hdsbuf);
  putptr1=NULL;putptr2=NULL;
  putbytes1=0;putbytes2=0;
  result = IDirectSoundBuffer_Lock(card->hdsbuf,aui->card_dmalastput, outbytenum, &putptr1, &putbytes1, &putptr2, &putbytes2, 0);
 }
 if((result!=DS_OK) || !putptr1 || !putbytes1)
  return;

 if(pcm_sample){ // write pcm data
  pds_memcpy(putptr1,pcm_sample,putbytes1);
  if(putptr2 && putbytes2)
   pds_memcpy(putptr2,pcm_sample+putbytes1,putbytes2);
 }else{          // clear buffer
  pds_memset(putptr1,0,putbytes1);
  if(putptr2)
   pds_memset(putptr2,0,putbytes2);
 }

 aui->card_dmalastput+=putbytes1+putbytes2;
 if(aui->card_dmalastput>=card->pcmout_bufsize)
  aui->card_dmalastput=putbytes2;

 IDirectSoundBuffer_Unlock(card->hdsbuf,putptr1,putbytes1,putptr2,putbytes2);

#ifdef MPXP_WINDSOUND_PARALELL_WAV
 if(pcm_sample)
  WAV_sndcard_info.cardbuf_writedata(aui,pcm_sample,outbytenum);
#endif
}

static long WINDS_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 DWORD play_offset;
 if(!card || !card->hdsbuf)
  return 0;
 IDirectSoundBuffer_GetCurrentPosition(card->hdsbuf,&play_offset,NULL);
 return play_offset;
}

static void WINDS_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(!card || !card->hdsbuf)
  return;
 IDirectSoundBuffer_SetCurrentPosition(card->hdsbuf, 0);
 WINDS_writedata(aui,NULL,card->pcmout_bufsize);
 aui->card_dmalastput=0;
}

static void WINDS_dma_monitor(struct mpxplay_audioout_info_s *aui)
{
 struct winds_data_s *card=aui->card_private_data;
 if(!card || !card->hdsbuf)
  return;
 if(aui->card_dmafilled<(aui->card_dmaout_under_int08*2)){
  if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_DMAUNDERRUN)){
   WINDS_writedata(aui,NULL,card->pcmout_bufsize);
   funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
  }
 }else{
  aui->card_dmafilled-=aui->card_dmaout_under_int08;
  funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
 }
}

static void WINDS_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct winds_data_s *card=aui->card_private_data;
 long vol;
 if(!card || !card->hdsbuf)
  return;
 vol=-val;
 IDirectSoundBuffer_SetVolume(card->hdsbuf,vol);
}

static unsigned long WINDS_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct winds_data_s *card=aui->card_private_data;
 long vol=0;
 if(!card || !card->hdsbuf)
  return 0;
 IDirectSoundBuffer_GetVolume(card->hdsbuf, &vol);
 if(vol<0)
  vol=-vol;
 return vol;
}

static aucards_onemixerchan_s winds_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),1,{{0x01,8191,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}};

static aucards_allmixerchan_s winds_mixerset[]={
 &winds_master_vol, // it's rather a pcm volume, just Mpxplay displays the 'master' value on the screen
 NULL
};

one_sndcard_info WINDSOUND_sndcard_info={
 "WDS",
 SNDCARD_INT08_ALLOWED,
 NULL,             // card_config
 NULL,             // card_init
 &WINDS_detect,
 &WINDS_card_info,
 &WINDS_start,
 &WINDS_stop,
 &WINDS_close,
 &WINDS_setrate,
 &WINDS_writedata,
 &WINDS_getbufpos,
 &WINDS_clearbuf,
 &WINDS_dma_monitor,
 NULL,             // irq_routine
 &WINDS_writeMIXER,
 &WINDS_readMIXER,
 &winds_mixerset[0]
};

#endif // AU_CARDS_LINK_WINDSOUND
