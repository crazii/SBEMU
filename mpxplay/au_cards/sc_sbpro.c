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
//function: SB 1.0,1.5,2.0 & SBpro handling

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_SB

#include "dmairq.h"
#include <stdlib.h>  // for getenv

#define SB_MIXER_ADDRESS     (baseport+0x4)
#define SB_MIXER_DATA        (baseport+0x5)
#define SB_RESET_PORT         (baseport+0x6)
#define SB_READ_DATA_PORT    (baseport+0xa)
#define SB_WRITE_DATA_PORT   (baseport+0xc)
#define SB_WRITE_STATUS_PORT (baseport+0xc)
#define SB_DATA_AVAIL_PORT   (baseport+0xe)
#define SB_CARDTYPE_10  1  // it seems this doesn't work
#define SB_CARDTYPE_200 2
#define SB_CARDTYPE_201 3
#define SB_CARDTYPE_PRO 4

#define SB_DSP_DMA8_EXIT   0xda

#define SB_TIMEOUT  32767

one_sndcard_info SB_sndcard_info;

static void SB_select_cardroutines(struct mpxplay_audioout_info_s *);

static int SB_timerconst;

static unsigned int SB_writeDSP(unsigned int baseport,unsigned int data)
{
 unsigned int timeout=SB_TIMEOUT;

 do{
  if(!(--timeout))
   return 0;
 }while((inp(SB_WRITE_STATUS_PORT)&0x80));
 outp(SB_WRITE_DATA_PORT,data);
 return 1;
}

static unsigned int SB_readDSP(unsigned int baseport)
{
 unsigned int timeout=SB_TIMEOUT;

 do{
  if(!(--timeout))
   return 0xffff;
 }while(!(inp(SB_DATA_AVAIL_PORT)&0x80));
 return(inp(SB_READ_DATA_PORT));
}

static void SB_resetDSP(unsigned int baseport)
{
 int t;
 outp(SB_RESET_PORT,1);
 for(t=8;t;t--)
  inp(SB_RESET_PORT);
 outp(SB_RESET_PORT,0);
}

static void SB_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 unsigned int baseport=aui->card_port;
 outp(SB_MIXER_ADDRESS,reg);
 outp(SB_MIXER_DATA,val);
}

static unsigned long SB_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 unsigned int baseport=aui->card_port;
 outp(SB_MIXER_ADDRESS,reg);
 return inp(SB_MIXER_DATA);
}

static void SB1_irq_routine(struct mpxplay_audioout_info_s *aui) // single cycle dma mode
{
 unsigned int baseport=aui->card_port;
 inp(SB_DATA_AVAIL_PORT);
 SB_writeDSP(baseport,0x14);
 SB_writeDSP(baseport,0xff);
 SB_writeDSP(baseport,0xff);
}

static void SB2pro_irq_routine(struct mpxplay_audioout_info_s *aui) // auto init dma mode
{
 unsigned int baseport=aui->card_port;
 inp(SB_DATA_AVAIL_PORT);
 //inp(baseport+0xf); // ???
}

static int SB_testport(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port,hi,lo,ct;

 SB_resetDSP(baseport);
 if(SB_readDSP(baseport)!=0xaa)
  return 0;
 if(!SB_writeDSP(baseport,0xe1))
  return 0;
 hi=SB_readDSP(baseport);
 lo=SB_readDSP(baseport);
 ct=((hi<<8)|lo);
 if(ct<0x200)
  aui->card_type=SB_CARDTYPE_10;
 else
  if(ct==0x200)
   aui->card_type=SB_CARDTYPE_200;
  else
   if(ct<0x300)
    aui->card_type=SB_CARDTYPE_201;
   else
    aui->card_type=SB_CARDTYPE_PRO;

 SB_select_cardroutines(aui);

 return 1;
}

static int SB_init(struct mpxplay_audioout_info_s *aui)
{
 unsigned int port,irq,dma;
 char *envptr=getenv("BLASTER");
 if(!envptr)
  return 0;
 port=irq=dma=0xffff;
 sscanf(envptr,"A%3X I%d D%d",&port,&irq,&dma);
 if((port==0xffff) || (irq==0xffff) || (dma==0xffff))
  return 0;
 aui->card_port=port;
 aui->card_irq=irq;
 aui->card_isa_dma=dma;
 if(!SB_testport(aui))
  return 0;
 return 1;
}

static void SB_card_info(struct mpxplay_audioout_info_s *aui)
{
 char sout[100];
 sprintf(sout,"SBP : SB-pro card found : SET BLASTER=A%3X I%d D%d T%d",
           aui->card_port,aui->card_irq,aui->card_isa_dma,aui->card_type);
 pds_textdisplay_printf(sout);
}

static void SB_setrate(struct mpxplay_audioout_info_s *aui)
{
 unsigned int cf=aui->freq_card;
 if(cf<4000)
  cf=4000;
 if(aui->card_type==SB_CARDTYPE_10)
  if(cf>22222)
   cf=22222;
 if(aui->card_type>=SB_CARDTYPE_200){
  if(cf<8000)
   cf=8000;
  if(cf>43478)
   cf=43478;
 }
 SB_timerconst=256-(1000000/cf);
 cf=1000000/(256-SB_timerconst);
 if(aui->card_type==SB_CARDTYPE_PRO){
  cf>>=1;
  aui->chan_card=2;
 }else
  aui->chan_card=1;
 aui->freq_card=cf;
 aui->bits_card=8;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 MDma_ISA_init(aui);
}

static void SB_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;

 SB_resetDSP(baseport);
 SB_writeDSP(baseport,0xd1);   // speaker on
 //SB_writeDSP(baseport,0xd3);   // speaker off

 SB_writeDSP(baseport,0x40);   // set sample rate
 SB_writeDSP(baseport,SB_timerconst);

 MIrq_Start(aui->card_irq,SB_sndcard_info.irq_routine,&aui->card_infobits);
 MDma_ISA_Start(aui,DMAMODE_AUTOINIT_ON,0,0);

 switch(aui->card_type){
  case SB_CARDTYPE_10:
    SB_writeDSP(baseport,0x14); // SB_DSP_OUTPUT
    SB_writeDSP(baseport,0xFF);
    SB_writeDSP(baseport,0xFF);
    break;
  case SB_CARDTYPE_200:
  case SB_CARDTYPE_201:
    SB_writeDSP(baseport,0x48); // SB_DSP_BLOCK_SIZE
    SB_writeDSP(baseport,0xFF);
    SB_writeDSP(baseport,0xFF);
    SB_writeDSP(baseport,0x90); // SB_DSP_HI_OUTPUT_AUTO
    break;
  case SB_CARDTYPE_PRO:
    SB_writeDSP(baseport,0x48); // SB_DSP_BLOCK_SIZE
    //SB_writeDSP(baseport,aui->card_dmasize&0xff);
    //SB_writeDSP(baseport,aui->card_dmasize>>8);
    SB_writeDSP(baseport,0xFF);
    SB_writeDSP(baseport,0xFF);
    SB_writeDSP(baseport,0x90); // SB_DSP_HI_OUTPUT_AUTO
    SB_writeMIXER(aui,0xE,(SB_readMIXER(aui,0xE)|0x22)); // output filter off, stereo on
    break;
 }

 //MIrq_Start(aui->card_irq,SB_sndcard_info.irq_routine,&aui->card_infobits);
 //SB_writeDSP(baseport,0xd1);   // speaker on
}

static void SB_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;

 SB_resetDSP(baseport);
 SB_resetDSP(baseport);
 SB_resetDSP(baseport);
 MIrq_Stop(aui->card_irq,&aui->card_infobits);
 MDma_ISA_Stop(aui);
}

static unsigned int SB_port_autodetect(struct mpxplay_audioout_info_s *aui)
{
 static unsigned short SB_ports[7]={0x220, 0x240, 0x260, 0x280, 0x210, 0x230, 0x250};
 unsigned int i;
 for(i=0;i<7;i++){
  aui->card_port=SB_ports[i];
  if(SB_testport(aui))
   return 1;
 }
 return 0;
}

static int SB_adetect(struct mpxplay_audioout_info_s *aui)
{
 if(!SB_port_autodetect(aui))
  return 0;
 if(!MDma_ISA_autodetect(aui))
  return 0;
 MIrq_autodetect(aui);
 return 1;
}

//-------------------------------------------------------------------------
//mixer

static aucards_onemixerchan_s sb20_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),1,{{0x02,7,1,0}}};
static aucards_onemixerchan_s sb20_pcm_vol   ={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),   1,{{0x0a,7,1,0}}};

static aucards_allmixerchan_s sb20_mixer_info[]={
 &sb20_master_vol,
 &sb20_pcm_vol,
 NULL
};

static aucards_onemixerchan_s sbpro_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x22,15,4,0},{0x22,15,0,0}}};
static aucards_onemixerchan_s sbpro_pcm_vol   ={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME)   ,2,{{0x04,15,4,0},{0x04,15,0,0}}};

static aucards_allmixerchan_s sbpro_mixer_info[]={
 &sbpro_master_vol,
 &sbpro_pcm_vol,
 NULL
};

one_sndcard_info SB_sndcard_info={
 "SBp",
 SNDCARD_INT08_ALLOWED,

 NULL,
 &SB_init,
 &SB_adetect,
 &SB_card_info,
 &SB_start,
 &SB_stop,
 &MDma_ISA_FreeMem,
 &SB_setrate,

 &MDma_writedata,
 &MDma_ISA_getbufpos,
 &MDma_ISA_Clear,
 &MDma_interrupt_monitor,
 NULL,    // irq_routine: we set it in runtime (depends on the SB type)

 &SB_writeMIXER,
 &SB_readMIXER,
 NULL     // mixer channels info: we set it in runtime
};

static void SB_select_cardroutines(struct mpxplay_audioout_info_s *aui)
{
 switch(aui->card_type){
  case SB_CARDTYPE_10 :SB_sndcard_info.irq_routine=SB1_irq_routine;break;
  case SB_CARDTYPE_200:
  case SB_CARDTYPE_201:
  case SB_CARDTYPE_PRO:SB_sndcard_info.irq_routine=SB2pro_irq_routine;break;
 }
 switch(aui->card_type){
  case SB_CARDTYPE_10 :break; // no mixer on 1.x
  case SB_CARDTYPE_200:
  case SB_CARDTYPE_201:SB_sndcard_info.card_mixerchans=&sb20_mixer_info[0];break;
  case SB_CARDTYPE_PRO:SB_sndcard_info.card_mixerchans=&sbpro_mixer_info[0];break;
 }
}

#endif
