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
//function: SB16 low level routines

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_SB16

#include "dmairq.h"
#include <stdlib.h>  // for getenv

#define SB16_TIMEOUT 65535

#define SB_MIXER_ADDRESS     (baseport+0x4)
#define SB_MIXER_DATA        (baseport+0x5)
#define SB_RESET_PORT         (baseport+0x6)
#define SB_READ_DATA_PORT    (baseport+0xa)
#define SB_WRITE_DATA_PORT   (baseport+0xc)
#define SB_WRITE_STATUS_PORT (baseport+0xc)
#define SB_DATA_AVAIL_PORT   (baseport+0xe)
#define SB16_DATA_AVAIL_PORT (baseport+0xf)

#define SB_DSP_DMA16_OFF    0xd5
#define SB_DSP_DMA16_EXIT    0xd9

#define SB_MIXERREG_VOL_MASTER_L  0x30
#define SB_MIXERREG_VOL_MASTER_R  0x31
#define SB_MIXERREG_VOL_WAVE_L    0x32
#define SB_MIXERREG_VOL_WAVE_R    0x33
#define SB_MIXERREG_TONE_TREBLE_L 0x44
#define SB_MIXERREG_TONE_TREBLE_R 0x45
#define SB_MIXERREG_TONE_BASS_L   0x46
#define SB_MIXERREG_TONE_BASS_R   0x47
#define SB_MIXERREG_IRQINFO       0x80
#define SB_MIXERREG_DMAINFO       0x81

static void SB16_select_mixerinfo(struct mpxplay_audioout_info_s *);

static void SB16_irq_routine(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 inp(SB16_DATA_AVAIL_PORT);
}

static unsigned int SB16_writeDSP(unsigned int baseport,unsigned int data)
{
 unsigned int timeout=SB16_TIMEOUT;

 do{
  if(!(--timeout))
   return 0;
 }while((inp(SB_WRITE_STATUS_PORT)&0x80));
 outp(SB_WRITE_DATA_PORT,data);
 return 1;
}

static unsigned int SB16_readDSP(unsigned int baseport)
{
 unsigned int timeout=SB16_TIMEOUT;

 do{
  if(!(--timeout))
   return 0xffff;
 }while(!(inp(SB_DATA_AVAIL_PORT)&0x80));
 return(inp(SB_READ_DATA_PORT));
}

static void SB16_resetDSP(unsigned int baseport)
{
 int t;
 outp(SB_RESET_PORT,1);
 for(t=8;t;t--)
  inp(SB_RESET_PORT);
 outp(SB_RESET_PORT,0);
}

static void SB16_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 unsigned int baseport=aui->card_port;
 outp(SB_MIXER_ADDRESS,reg);
 outp(SB_MIXER_DATA,val);
}

static unsigned long SB16_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 unsigned int baseport=aui->card_port;
 outp(SB_MIXER_ADDRESS,reg);
 return inp(SB_MIXER_DATA);
}

static unsigned int SB16_testport(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port,hi,lo,ct;

 SB16_resetDSP(baseport);
 if(SB16_readDSP(baseport)!=0xaa)
  return 0;
 if(!SB16_writeDSP(baseport,0xe1)) // get version
  return 0;
 hi=SB16_readDSP(baseport);
 lo=SB16_readDSP(baseport);
 ct=((hi<<8)|lo);
 if(ct<0x0400 || ct>=0x0500)
  return 0;
 aui->card_type=6;
 SB16_select_mixerinfo(aui);
 return 1;
}

static int SB16_init(struct mpxplay_audioout_info_s *aui)
{
 unsigned int port,irq,dma,hidma;
 char *envptr=getenv("BLASTER");
 if(!envptr)
  return 0;
 port=irq=dma=hidma=0xffff;
 sscanf(envptr,"A%3X I%d D%d H%d",&port,&irq,&dma,&hidma);
 if((port==0xffff) || (irq==0xffff) || (dma==0xffff) || (hidma==0xffff))
  return 0;
 aui->card_port=port;
 aui->card_irq=irq;
 aui->card_isa_dma=dma;
 aui->card_isa_hidma=hidma;
 if(!SB16_testport(aui))
  return 0;
 return 1;
}

static void SB16_card_info(struct mpxplay_audioout_info_s *aui)
{
 char sout[100];
 sprintf(sout,"S16 : SB16 soundcard found : SET BLASTER=A%3X I%d D%d H%d T%d",aui->card_port,aui->card_irq,aui->card_isa_dma,aui->card_isa_hidma,aui->card_type);
 pds_textdisplay_printf(sout);
}

static unsigned int SB16_port_autodetect(struct mpxplay_audioout_info_s *aui)
{
 static unsigned short SB16_ports[8]={0x220,0x240,0x260,0x280,0x210,0x230,0x250,0x270};
 unsigned int i;
 for(i=0;i<8;i++){
  aui->card_port=SB16_ports[i];
  if(SB16_testport(aui))
   return 1;
 }
 return 0;
}

static unsigned int SB16_get_irq(struct mpxplay_audioout_info_s *aui)
{
 unsigned int a;
 a=SB16_readMIXER(aui,SB_MIXERREG_IRQINFO);
 if(!(a&0x0F))
  return 0;
 aui->card_irq=(a&2)?5:(a&4)?7:(a&1)?2:10;
 return 1;
}

static unsigned int SB16_get_dma(struct mpxplay_audioout_info_s *aui)
{
 unsigned int a;
 a=SB16_readMIXER(aui,SB_MIXERREG_DMAINFO);
 if(!(a&0x0B))
  return 0;
 aui->card_isa_dma=(a&2)? 1:(a&1)? 0:3;
 aui->card_isa_hidma=(a&0x20)? 5:(a&0x40)? 6:(a&0x80)? 7:aui->card_isa_dma;
 return 1;
}

static int SB16_adetect(struct mpxplay_audioout_info_s *aui)
{
 if(!SB16_port_autodetect(aui))
  return 0;
 if(!SB16_get_irq(aui))
  return 0;
 if(!SB16_get_dma(aui))
  return 0;
 return 1;
}

static void SB16_setrate(struct mpxplay_audioout_info_s *aui)
{
 if(aui->freq_card<4000)
  aui->freq_card=4000;
 else
  if(aui->freq_card>48000)
   aui->freq_card=48000;
  //if(rate>44100)  // disabled for SB Live (use -of 44100 at original SB16)
   //rate=44100;

 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 MDma_ISA_init(aui);
}

static void SB16_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;

 SB16_writeDSP(baseport,0x41); // set output sample rate
 SB16_writeDSP(baseport,aui->freq_card>>8);
 SB16_writeDSP(baseport,aui->freq_card&0xff);

 MIrq_Start(aui->card_irq,SB16_irq_routine,&aui->card_infobits);
 MDma_ISA_Start(aui,DMAMODE_AUTOINIT_ON,0,0);

 SB16_writeDSP(baseport,0xb6);  // SB_DSP4_OUT16_AI
 SB16_writeDSP(baseport,0x30);  // signed stereo format

 SB16_writeDSP(baseport,0xff);
 SB16_writeDSP(baseport,0xef);

 //SB16_writeDSP(baseport,aui->card_dmasize&0xff);
 //SB16_writeDSP(baseport,aui->card_dmasize>>8);

 //SB16_writeDSP(baseport,0xfc);
 //SB16_writeDSP(baseport,0xff);
}

static void SB16_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 SB16_writeDSP(baseport,SB_DSP_DMA16_OFF);
 SB16_writeDSP(baseport,SB_DSP_DMA16_EXIT);
 SB16_writeDSP(baseport,SB_DSP_DMA16_OFF);
 SB16_resetDSP(baseport);
 SB16_resetDSP(baseport);
 SB16_resetDSP(baseport);
 MDma_ISA_Stop(aui);
 MIrq_Stop(aui->card_irq,&aui->card_infobits);
 //inp(SB16_DATA_AVAIL_PORT);
}

//------------------------------------------------------------------------
//mixer

// only OPL based SB16 and AWE 32/64 have tone control, we test this
static unsigned int SB16_testtone(struct mpxplay_audioout_info_s *aui)
{
 int savetreble,savebass;

 savetreble=SB16_readMIXER(aui,SB_MIXERREG_TONE_TREBLE_L);
 savebass  =SB16_readMIXER(aui,SB_MIXERREG_TONE_BASS_L);

 SB16_writeMIXER(aui,SB_MIXERREG_TONE_TREBLE_L,10<<4);
 SB16_writeMIXER(aui,SB_MIXERREG_TONE_BASS_L  ,12<<4);
 if((SB16_readMIXER(aui,SB_MIXERREG_TONE_TREBLE_L)>>4)==10 && (SB16_readMIXER(aui,SB_MIXERREG_TONE_BASS_L)>>4)==12)
  aui->card_infobits|=AUINFOS_CARDINFOBIT_HWTONE;

 SB16_writeMIXER(aui,SB_MIXERREG_TONE_TREBLE_L,savetreble);
 SB16_writeMIXER(aui,SB_MIXERREG_TONE_BASS_L,savebass);

 return (aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE);
}

static aucards_onemixerchan_s sb16_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x30,31,3,0},{0x31,31,3,0}}};
static aucards_onemixerchan_s sb16_pcm_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),      2,{{0x32,31,3,0},{0x33,31,3,0}}};
static aucards_onemixerchan_s sb16_synth_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_SYNTH,AU_MIXCHANFUNC_VOLUME),  2,{{0x34,31,3,0},{0x35,31,3,0}}};
static aucards_onemixerchan_s sb16_cdin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME),    2,{{0x36,31,3,0},{0x37,31,3,0}}};
static aucards_onemixerchan_s sb16_linein_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME),2,{{0x38,31,3,0},{0x39,31,3,0}}};
static aucards_onemixerchan_s sb16_micin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MICIN,AU_MIXCHANFUNC_VOLUME),  1,{{0x3A,31,3,0}}};
static aucards_onemixerchan_s sb16_tone_bass={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_BASS,AU_MIXCHANFUNC_VOLUME),   2,{{0x46,15,4,0},{0x47,15,4,0}}};
static aucards_onemixerchan_s sb16_tone_treble={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_TREBLE,AU_MIXCHANFUNC_VOLUME),2,{{0x44,15,4,0},{0x45,15,4,0}}};

static aucards_allmixerchan_s sb16_mixer_no_tone[]={
 &sb16_master_vol,
 &sb16_pcm_vol,
 &sb16_synth_vol,
 &sb16_cdin_vol,
 &sb16_linein_vol,
 &sb16_micin_vol,
 NULL
};

static aucards_allmixerchan_s sb16_mixer_with_tone[]={
 &sb16_master_vol,
 &sb16_pcm_vol,
 &sb16_synth_vol,
 &sb16_cdin_vol,
 &sb16_linein_vol,
 &sb16_micin_vol,
 &sb16_tone_bass,
 &sb16_tone_treble,
 NULL
};

one_sndcard_info SB16_sndcard_info={
 "S16",
 SNDCARD_INT08_ALLOWED,

 NULL,
 &SB16_init,
 &SB16_adetect,
 &SB16_card_info,
 &SB16_start,
 &SB16_stop,
 &MDma_ISA_FreeMem,
 &SB16_setrate,

 &MDma_writedata,
 &MDma_ISA_getbufpos,
 &MDma_ISA_Clear,
 &MDma_interrupt_monitor,
 &SB16_irq_routine,

 &SB16_writeMIXER,
 &SB16_readMIXER,
 NULL
};

static void SB16_select_mixerinfo(struct mpxplay_audioout_info_s *aui)
{
 if(SB16_testtone(aui))
  SB16_sndcard_info.card_mixerchans=&sb16_mixer_with_tone[0];
 else
  SB16_sndcard_info.card_mixerchans=&sb16_mixer_no_tone[0];
}

#endif
