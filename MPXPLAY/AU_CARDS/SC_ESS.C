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
//function: ESS card handling

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_ESS

#include "dmairq.h"
#include <stdlib.h>  // for getenv

#define ESS_MIXER_ADDRESS     (baseport+0x4)
#define ESS_MIXER_DATA        (baseport+0x5)
#define ESS_RESET_PORT        (baseport+0x6)
#define ESS_READ_DATA_PORT    (baseport+0xa)
#define ESS_WRITE_DATA_PORT   (baseport+0xc)
#define ESS_WRITE_STATUS_PORT (baseport+0xc)
#define ESS_DATA_AVAIL_PORT   (baseport+0xe)

#define ESS_MIXERTYPE1     0x0001
#define ESS_MIXERTYPE2     0x0002
#define ESS_MIXERTYPE_MASK (ESS_MIXERTYPE1|ESS_MIXERTYPE2)

#define ES18XX_PCM2       0x0010 /* Has two useable PCM */
#define ES18XX_NEW_RATE    0x0020 /* More precise rate setting at 1869,1879*/
#define ES18XX_HWV       0x0080 /* Has seperate hardware volume mixer controls*/
#define ES18XX_CONTROL       0x0800 /* Has control ports */

typedef struct{
 unsigned int card_id;
 unsigned int mask;
 unsigned int hwinfo;
}ess_hwinfo_s;

static ess_hwinfo_s ess_hw_info[]={
 {0x1688,0xffff,ESS_MIXERTYPE1 },
 {0x6880,0xfff0,ESS_MIXERTYPE1 },
 {0x1868,0xffff,ES18XX_CONTROL|ESS_MIXERTYPE2                               },
 {0x8680,0xfff0,ES18XX_CONTROL|ESS_MIXERTYPE2                               },
 {0x1869,0xffff,ES18XX_PCM2   |ES18XX_NEW_RATE|ES18XX_CONTROL|ESS_MIXERTYPE2|ES18XX_HWV},
 {0x8690,0xfff0,ES18XX_PCM2   |ES18XX_NEW_RATE|ES18XX_CONTROL|ESS_MIXERTYPE2|ES18XX_HWV},
 {0x1878,0xffff,ES18XX_CONTROL|ESS_MIXERTYPE2                    },
 {0x8780,0xfff0,ES18XX_CONTROL|ESS_MIXERTYPE2                    },
 {0x1879,0xffff,ES18XX_PCM2   |ES18XX_NEW_RATE|ES18XX_CONTROL|ESS_MIXERTYPE2|ES18XX_HWV},
 {0x8790,0xfff0,ES18XX_PCM2   |ES18XX_NEW_RATE|ES18XX_CONTROL|ESS_MIXERTYPE2|ES18XX_HWV},
 {0x1887,0xffff,ES18XX_PCM2   |ESS_MIXERTYPE2                    },
 {0x1888,0xffff,ES18XX_PCM2   |ESS_MIXERTYPE2                    },
 {0x8800,0xff00,ES18XX_PCM2   |ESS_MIXERTYPE2                    }
};

static unsigned int ESS_regB1,ESS_regB2;
static unsigned int ESS_hardware;
static long ESS_bits,ESS_div0;

static void ess_select_mixerinfo(unsigned int hardware);

static int ESS_irq_routine(struct mpxplay_audioout_info_s *aui)
{
 // I don't know, what's here... but ess works without irq too
 return 0;
}

static int ESS_command(unsigned int baseport,unsigned int data)
{
 unsigned int timeout=32767;
 do{
  if(!(--timeout))
   return 0;
 }while((inp(ESS_WRITE_STATUS_PORT)&0x80));
 outp(ESS_WRITE_DATA_PORT,data);
 return 1;
}

static unsigned int ESS_getbyte(unsigned int baseport)
{
 unsigned int timeout=32767;
 do{
  if(!(--timeout))
   return 0xff;
 }while(!(inp(ESS_WRITE_STATUS_PORT)&0x40));
 return(inp(ESS_READ_DATA_PORT));
}

static void ESS_writeDSP(unsigned int baseport,unsigned int Register,unsigned int Value)
{
 ESS_command(baseport,Register);
 ESS_command(baseport,Value);
}

static unsigned int ESS_readDSP(unsigned int baseport,unsigned int Register)
{
 ESS_command(baseport,0xC0);
 ESS_command(baseport,Register);
 return ESS_getbyte(baseport);
}

static int ESS_reset(unsigned int baseport)
{
 int t;
 outp(ESS_RESET_PORT,3);
 for(t=8;t;t--)
  inp(ESS_RESET_PORT);
 outp(ESS_RESET_PORT,0);
 return(ESS_getbyte(baseport)==0xAA);
}

static void ESS_get_hwinfo(unsigned int cardtype)
{
 unsigned int i;
 for(i=0;i<(sizeof(ess_hw_info)/sizeof(ess_hwinfo_s));i++)
  if((ess_hw_info[i].card_id&ess_hw_info[i].mask)==cardtype){
   ESS_hardware=ess_hw_info[i].hwinfo;
   break;
  }
 ess_select_mixerinfo(ESS_hardware);
}

static int ESS_testport(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port,hi,lo,cardtype;
 if(!ESS_reset(baseport))
  return 0;
 if(!ESS_command(baseport,0xe7))
  return 0;
 hi=ESS_getbyte(baseport);
 lo=ESS_getbyte(baseport);
 cardtype=(hi<<8)|lo;
 if(cardtype==0xffff || cardtype==0xaaaa)  // 0xaaaa is a Sound Blaster
  return 0;
 aui->card_type=cardtype;
 ESS_get_hwinfo(cardtype);
 return cardtype;
}

static int ESS_init(struct mpxplay_audioout_info_s *aui)
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
 aui->card_isa_hidma=0;//hidma;
 if(!ESS_testport(aui))
  return 0;
 return 1;
}

static void ESS_card_info(struct mpxplay_audioout_info_s *aui)
{
 char sout[100];
 sprintf(sout,"ESS : soundcard found (type=%4.4X) : SET BLASTER=A%3X I%d D%d H%d",
           aui->card_type,aui->card_port,aui->card_irq,aui->card_isa_dma,aui->card_isa_hidma);
 pds_textdisplay_printf(sout);
}

static unsigned int ESS_port_autodetect(struct mpxplay_audioout_info_s *aui)
{
 static unsigned short ESS_ports[8]={0x220,0x240,0x260,0x280,0x210,0x230,0x250,0x270};
 unsigned int i;
 for(i=0;i<8;i++){
  aui->card_port=ESS_ports[i];
  if(ESS_testport(aui))
   return 1;
 }
 return 0;
}

static unsigned int ESS_get_dma(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port,dmachan;

 ESS_command(baseport,0xC6);
 dmachan=ESS_readDSP(baseport,0xB2);
 switch(dmachan&0xC) {
  case 0xC: dmachan=3; break;
  case 0x8: dmachan=1; break;
  case 0x4: dmachan=0; break;
  case 0x0: return 0;
 }
 aui->card_isa_dma=dmachan;
 aui->card_isa_hidma=0;
 return 1;
}

static void ESS_get_irq(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;

 ESS_command(baseport,0xC6);
 aui->card_irq=ESS_readDSP(baseport,0xB1);

 switch(aui->card_irq&0xC) {
  case 0xC: aui->card_irq=10; break;
  case 0x0: aui->card_irq=9;  break;
  case 0x8: aui->card_irq=7;  break;
  case 0x4: aui->card_irq=5;
 }
}

static int ESS_adetect(struct mpxplay_audioout_info_s *aui)
{
 if(!ESS_port_autodetect(aui))
  return 0;
 if(!ESS_get_dma(aui))
  if(!MDma_ISA_autodetect(aui))
   return 0;
 ESS_get_irq(aui);
 //if(!ESS_get_irq(aui))
 // MIrq_autodetect(aui);
 return 1;
}

static void ESS_setrate(struct mpxplay_audioout_info_s *aui)
{
 long rate=aui->freq_card;

 if(rate<4000)
  rate=4000;
 else
  if(rate>48000)
   rate=48000;
 aui->freq_card=rate;
 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 if(ESS_hardware&ES18XX_NEW_RATE){ // 1869 1879
  int div1, diff0, diff1, rate0, rate1;
  ESS_div0 = (793800 + (rate / 2)) / rate;
  rate0 = 793800 / ESS_div0;
  diff0 = (rate0 > rate) ? (rate0 - rate) : (rate - rate0);

  div1 = (768000 + (rate / 2)) / rate;
  rate1 = 768000 / div1;
  diff1 = (rate1 > rate) ? (rate1 - rate) : (rate - rate1);

  if (diff0 < diff1) {
   ESS_bits = 128 - ESS_div0;
   rate = rate0;
  }else {
   ESS_bits = 256 - div1;
   rate = rate1;
  }
  ESS_div0=256-((7160000*20)/(8*82*rate));
 }else{
  if(rate>22000){        // for 18xx
   ESS_div0 = (795444 + (rate / 2)) / rate;
   rate = 795444 / ESS_div0;
   ESS_bits = 256 - ESS_div0;
  }else{
   ESS_div0 = (397722 + (rate / 2)) / rate;
   rate = 397722 / ESS_div0;
   ESS_bits = 128 - ESS_div0;
  }
  ESS_div0 = 256 - 7160000*20/(8*82*rate);
  /*if(rate>22000) {       // for 1688
   ESS_div0 = 256 - (795444 + (rate >> 1)) / rate;
   ESS_bits = 0x80;
  }else{
   ESS_div0 = 128 - (397722 + (rate >> 1)) / rate;
   ESS_bits = 0x00;
  }
  ESS_bits|=ESS_div0;
  rate=(rate*9)/20;
  ESS_div0=256-(7160000/(rate*82));*/
 }
 MDma_ISA_init(aui);
}

static void ESS_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 unsigned long t;

 ESS_reset(baseport);
 ESS_command(baseport,0xC6);          // Enable Extended Mode commands

 ESS_regB1=ESS_readDSP(baseport,0xB1);
 ESS_regB2=ESS_readDSP(baseport,0xB2);

 ESS_writeDSP(baseport,0xB8,4);       // Set auto-initialize DAC transfer

 t=ESS_readDSP(baseport,0xA8)&0xFC;
 t|=1;                       // set stereo
 ESS_writeDSP(baseport,0xA8,t);

 ESS_writeDSP(baseport,0xa1,ESS_bits);  // Set sample rate clock divider
 ESS_writeDSP(baseport,0xa2,ESS_div0);  // Set filter clock divider

 ESS_writeDSP(baseport,0xB6,0x0);
 ESS_writeDSP(baseport,0xB7,0x71);
 ESS_writeDSP(baseport,0xB7,0xbc);    // stereo | signedout | 16bit
 ESS_writeDSP(baseport,0xB1,0x50);    // Set IRQ configuration register

 t=aui->card_isa_dma+(aui->card_isa_dma<3);
 t=(5*t) | 0x50;
 ESS_writeDSP(baseport,0xB2,t);       // Set DRQ configuration register

 MIrq_Start(aui->card_irq,ESS_irq_routine,&aui->card_infobits);
 MDma_ISA_Start(aui,DMAMODE_AUTOINIT_ON,0,0);

 ESS_command(baseport,0xD1);          // Enable voice to mixer
 ESS_writeDSP(baseport,0xB8, ESS_readDSP(baseport,0xB8) | 1);  // Start DMA
}

static void ESS_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;

 ESS_reset(baseport);
 ESS_reset(baseport);
 ESS_command(baseport,0xC6);
 ESS_writeDSP(baseport,0xB1,ESS_regB1);
 ESS_writeDSP(baseport,0xB2,ESS_regB2);
 outp(0x0C,0);
 MIrq_Stop(aui->card_irq,&aui->card_infobits);
 MDma_ISA_Stop(aui);
}

//-------------------------------------------------------------------------
//mixer

static void ESS_writemixer(struct mpxplay_audioout_info_s *aui,unsigned long reg,unsigned long data)
{
 unsigned int baseport=aui->card_port;
 outp(ESS_MIXER_ADDRESS,reg);
 outp(ESS_MIXER_DATA,data);
}

static unsigned long ESS_readmixer(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 unsigned int baseport=aui->card_port;
 outp(ESS_MIXER_ADDRESS,reg);
 return inp(ESS_MIXER_DATA);
}

static aucards_onemixerchan_s ess16xx_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x32,15,4,0},{0x32,15,0,0}}};
static aucards_onemixerchan_s ess_pcm_vol       ={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),2,{{0x14,15,4,0},{0x14,15,0,0}}};

static aucards_allmixerchan_s ess16xx_mixer_info[]={
 &ess16xx_master_vol,
 &ess_pcm_vol,
 NULL
};

static aucards_onemixerchan_s ess18xx_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x60,63,0,0},{0x62,63,0,0}}};
static aucards_onemixerchan_s ess18xx_master_switch={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_MUTE),2,{{0x60,1,6,SUBMIXCH_INFOBIT_REVERSEDVALUE},{0x62,1,6,SUBMIXCH_INFOBIT_REVERSEDVALUE}}};

static aucards_allmixerchan_s ess18xx_mixer_info[]={
 &ess18xx_master_vol,
 &ess18xx_master_switch,
 &ess_pcm_vol,
 NULL
};

static aucards_onemixerchan_s ess18xx_hwv_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x61,63,0,0},{0x63,63,0,0}}};
static aucards_onemixerchan_s ess18xx_hwv_master_switch={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_MUTE),2,{{0x61,1,6,SUBMIXCH_INFOBIT_REVERSEDVALUE},{0x63,1,6,SUBMIXCH_INFOBIT_REVERSEDVALUE}}};

static aucards_allmixerchan_s ess18xx_hwv_mixer_info[]={
 &ess18xx_hwv_master_vol,
 &ess18xx_hwv_master_switch,
 &ess_pcm_vol,
 NULL
};

one_sndcard_info ESS_sndcard_info={
 "ESS",
 SNDCARD_INT08_ALLOWED,

 NULL,
 &ESS_init,
 &ESS_adetect,
 &ESS_card_info,
 &ESS_start,
 &ESS_stop,
 &MDma_ISA_FreeMem,
 &ESS_setrate,

 &MDma_writedata,
 &MDma_ISA_getbufpos,
 &MDma_ISA_Clear,
 &MDma_interrupt_monitor,
 &ESS_irq_routine,

 &ESS_writemixer,
 &ESS_readmixer,
 &ess16xx_mixer_info[0]
};

static void ess_select_mixerinfo(unsigned int hardware)
{
 if(hardware&ESS_MIXERTYPE2){
  if(hardware&ES18XX_HWV)
   ESS_sndcard_info.card_mixerchans=&ess18xx_hwv_mixer_info[0];
  else
   ESS_sndcard_info.card_mixerchans=&ess18xx_mixer_info[0];
 }else
  ESS_sndcard_info.card_mixerchans=&ess16xx_mixer_info[0];
}

#endif
