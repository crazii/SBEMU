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
//function: WSS (Windows Sound System) compatible card handling
//based on the OpenCP,MIDAS sources and OPL3SA datasheet

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_WSS

#include "dmairq.h"
#include <stdlib.h>  // for getenv

#define WSS_CONFIG_PORT     (baseport)     // read-only!
#define WSS_COMMAND_PORT    (baseport+0x4)
#define WSS_DATA_PORT       (baseport+0x5)
#define WSS_TIMEOUT  65535

#define wssinp(reg) inp(reg)
#define wssoutp(reg,val) outp(reg,val)

static unsigned int WSS_rate;

static void WSS_writeDSP(unsigned int baseport,unsigned int func,unsigned int data)
{
 unsigned int timeout=WSS_TIMEOUT;

 while((inp(WSS_COMMAND_PORT)&0x80) && (--timeout));

 outp(WSS_COMMAND_PORT,func);
 outp(WSS_DATA_PORT,data);
}

/*static unsigned int WSS_readDSP(unsigned int baseport,unsigned int r)
{
  wssoutp(WSS_COMMAND_PORT, r);
  return wssinp(WSS_DATA_PORT);
}

static void WSS_writeDSP(unsigned int baseport,unsigned int r, unsigned int v)
{
  wssoutp(WSS_COMMAND_PORT, r);
  wssoutp(WSS_DATA_PORT, v);
}*/

static void waste_time(unsigned int val)
{
 int i,j;
 for(i=0; i<val; i++)
  for(j=0; j<1000; j++);
}

static int WSS_testport(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 unsigned int ver1,ver2;
 unsigned int i;
 //char sout[100];

 for(i=0;i<1000;i++){
  if(wssinp(WSS_COMMAND_PORT)&0x80){
   waste_time(1);
  }else{
   WSS_writeDSP(baseport,0x0c,0);
   //old
   //ver1=wssinp(WSS_DATA_PORT)&0xf;
   //new
   ver1=wssinp(WSS_COMMAND_PORT)&0xf;
   //fprintf(stderr,"1. %d. try  port:%X ver:%d\n",i,baseport,ver1);
   if((ver1>=1)&&(ver1<15))
    break;
  }
 }
 if(i==1000)
  return 0;

 WSS_writeDSP(baseport,0x0c,0);
 ver1=wssinp(WSS_DATA_PORT);
 wssoutp(WSS_DATA_PORT,0);
 ver2=wssinp(WSS_DATA_PORT);
 if(ver1!=ver2)
  return 0;
 if(wssinp(WSS_COMMAND_PORT)&0x80)
  return 0;
 WSS_writeDSP(baseport,0x49,4);
 wssoutp(WSS_COMMAND_PORT, 0x09);
 WSS_writeDSP(baseport,0x0c,0);
 return 1;
}

static int WSS_init(struct mpxplay_audioout_info_s *aui)
{
 unsigned int port,irq,dma,cardtype;
 char *envptr=getenv("ULTRA16");
 if(!envptr)
  return 0;
 port=irq=dma=cardtype=0xffff;
 sscanf(envptr,"%3X,%d,%d,%d",&port,&dma,&irq,&cardtype);
 if((port==0xffff) || (irq==0xffff) || (dma==0xffff) || (cardtype==0xffff))
  return 0;
 aui->card_port=port;
 aui->card_irq=irq;
 aui->card_isa_dma=dma;

 return WSS_testport(aui);
}

static void WSS_card_info(struct mpxplay_audioout_info_s *aui)
{
 char sout[100];
 sprintf(sout,"WSS : soundcard found : SET ULTRA16=%3X,%d,%d,0",aui->card_port,aui->card_isa_dma,aui->card_irq);
 pds_textdisplay_printf(sout);
}

static unsigned int WSS_port_autodetect(struct mpxplay_audioout_info_s *aui)
{
 static unsigned short WSS_ports[6]={0x530,0xe80,0xf40,0x680};
 unsigned int i;

 for(i=0;i<4;i++){
  aui->card_port=WSS_ports[i];
  if(WSS_testport(aui))
   return 1;
 }
 return 0;
}

static int WSS_adetect(struct mpxplay_audioout_info_s *aui)
{
 if(!WSS_port_autodetect(aui))
  return 0;
 if(!MDma_ISA_autodetect(aui))
  return 0;
 aui->card_irq=0;
 return 1;
}

static void WSS_setrate(struct mpxplay_audioout_info_s *aui)
{
 static unsigned int wssfreqs[16]={8000,5513,16000,11025,27429,18900,32000,
                  22050, 0,37800, 0,44100,48000,33075,9600,6615};
 int i,j,mindiff=65535,wssrate,setfreq=aui->freq_card;
 for(i=0;i<16;i++){
  if(wssfreqs[i]){
   j=wssfreqs[i]-setfreq;
   if(j<0)
    j=-j;
   if(j<mindiff){
    wssrate=i;
    if(j==0)
     break;
    mindiff=j;
   }
  }
 }
 WSS_rate=wssrate;
 aui->freq_card=wssfreqs[wssrate];
 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 if(aui->chan_card==2)
  WSS_rate|=0x10;
 if(aui->bits_card==16)
  WSS_rate|=0x40;
 MDma_ISA_init(aui);
}

static void WSS_extra(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 unsigned int timeout;
 WSS_writeDSP(baseport,0x48,WSS_rate);
 WSS_writeDSP(baseport,0x48,WSS_rate);
 wssinp(WSS_DATA_PORT);
 wssinp(WSS_DATA_PORT);

 timeout=WSS_TIMEOUT;
 while ((wssinp(WSS_COMMAND_PORT)&0x80) && --timeout);

 timeout=WSS_TIMEOUT;
 while((wssinp(WSS_COMMAND_PORT)!=0x08) && --timeout)
  wssoutp(WSS_COMMAND_PORT, 0x08);

 timeout=WSS_TIMEOUT;
 while((wssinp(WSS_COMMAND_PORT)!=0x0B) && --timeout)
  wssoutp(WSS_COMMAND_PORT, 0x0B);

 timeout=WSS_TIMEOUT;
 while((wssinp(WSS_DATA_PORT)&0x20) && --timeout)
  wssoutp(WSS_COMMAND_PORT, 0x0B);

 WSS_writeDSP(baseport,0x0c,0);
}

static void WSS_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 //old
 //WSS_writeDSP(baseport,0x0a,0);
 //WSS_writeDSP(baseport,0x49,4|8);
 //WSS_writeDSP(baseport,0x48,WSS_rate);// (16 bit,stereo|rate)

 //new
 WSS_extra(aui);

 MDma_ISA_Start(aui,DMAMODE_AUTOINIT_ON,0,0);

 WSS_writeDSP(baseport,0x0f,0xf0); // 0xf0 or 0xff ??
 WSS_writeDSP(baseport,0x0e,0xff);

 //old
 //WSS_writeDSP(baseport,0x0a,0);
 //WSS_writeDSP(baseport,0x09,5);
 //new
 WSS_writeDSP(baseport,0x09,5);
}

static void WSS_stop(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 WSS_writeDSP(baseport,0x0a,0);

 //old
 //WSS_writeDSP(baseport,0x09,0);

 //new
 WSS_writeDSP(baseport,0x09,4);

 MDma_ISA_Stop(aui);
}

//-------------------------------------------------------------------------
// mixer

static void WSS_write_reg(struct mpxplay_audioout_info_s *aui,unsigned long reg,unsigned long data)
{
 WSS_writeDSP(aui->card_port,reg,data);
}

static unsigned long WSS_read_reg(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 unsigned int baseport=aui->card_port;
 unsigned int timeout=WSS_TIMEOUT;

 while((inp(WSS_COMMAND_PORT)&0x80) && (--timeout));
 outp(WSS_COMMAND_PORT,reg);
 return inp(WSS_DATA_PORT);
}

static aucards_onemixerchan_s wss_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x06,63,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},{0x07,63,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s wss_master_switch={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_MUTE),2,{{0x06,1,7,SUBMIXCH_INFOBIT_REVERSEDVALUE},{0x07,1,7,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_allmixerchan_s wss_mixer_info[]={
 &wss_master_vol,
 &wss_master_switch,
 NULL
};


one_sndcard_info WSS_sndcard_info={
 "WSS",
 SNDCARD_INT08_ALLOWED,

 NULL,
 &WSS_init,
 &WSS_adetect,
 &WSS_card_info,
 &WSS_start,
 &WSS_stop,
 &MDma_ISA_FreeMem,
 &WSS_setrate,

 &MDma_writedata,
 &MDma_ISA_getbufpos,
 &MDma_ISA_Clear,
 &MDma_interrupt_monitor,
 NULL,

 &WSS_write_reg,
 &WSS_read_reg,
 &wss_mixer_info[0]
};

#endif // AU_CARDS_LINK_WSS
