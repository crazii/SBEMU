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
//function: GUS low level routines
//based on the OpenCP player (by Niklas Beisert)

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_GUS

#include <conio.h>
#include <stdlib.h>
#include "dmairq.h"

static unsigned int gusIRQ2;
static unsigned int activevoices,gus_volume;
static int gus_timeconst;
static int buflen2,savepos,startinitok;

#define bit16     1
#define stereo    1
#define signedout 1

static unsigned int GUS_getcfg(struct mpxplay_audioout_info_s *aui)
{
 unsigned int port,irq,dma,hidma;
 char *ptr=getenv("ULTRASND");
 if(!ptr)
  return 0;
 port=irq=dma=hidma=0xffff;
 sscanf(ptr,"%3X,%d,%d,%d,%d",&port,&dma,&hidma,&irq,&gusIRQ2);

 if((port==0xffff) || (irq==0xffff) || (dma==0xffff) || (hidma==0xffff))
  return 0;

 irq=(irq<gusIRQ2)? irq:gusIRQ2;
 aui->card_port=port;
 aui->card_irq=irq;
 aui->card_isa_dma=dma;
 aui->card_isa_hidma=hidma;

 return 1;
}

static void GUS_delay(unsigned int baseport)
{
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
 inp(baseport+0x107);
}

#define inpGUS(baseport,reg) inp(baseport+reg)
#define outpGUS(baseport,reg,val) outp(baseport+reg,val)

#define outpGUS0(baseport,val) outpGUS(baseport,0x00,val)
#define outpGUSB(baseport,val) outpGUS(baseport,0x0B,val)
#define outpGUSF(baseport,val) outpGUS(baseport,0x0F,val)

#define GUS_select_voice(baseport,ch) outpGUS(baseport,0x102,ch)

static void outGUS(unsigned int baseport,unsigned int reg,unsigned int val)
{
  outp(baseport+0x103, reg);
  outp(baseport+0x105, val);
}

static void outdGUS(unsigned int baseport,unsigned int reg,unsigned int val)
{
  outp(baseport+0x103, reg);
  outp(baseport+0x105, val);
  GUS_delay(baseport);
  outp(baseport+0x105, val);
}

static void outwGUS(unsigned int baseport,unsigned int reg,unsigned int val)
{
  outp(baseport+0x103, reg);
  outpw(baseport+0x104, val);
}

static unsigned int inGUS(unsigned int baseport,unsigned int reg)
{
  outp(baseport+0x103, reg);
  return inp(baseport+0x105);
}

static unsigned int inwGUS(unsigned int baseport,unsigned int reg)
{
  outp(baseport+0x103, reg);
  return inpw(baseport+0x104);
}

static unsigned int peekGUS(unsigned int baseport,unsigned long adr)
{
  outwGUS(baseport,0x43, adr);
  outGUS(baseport,0x44, adr>>16);
  return inpGUS(baseport,0x107);
}

static void pokeGUS(unsigned int baseport,unsigned long adr, unsigned int data)
{
  outwGUS(baseport,0x43, adr);
  outGUS(baseport,0x44, adr>>16);
  outpGUS(baseport,0x107, data);
}

static void GUS_setfreq(unsigned int baseport,unsigned int frq)
{
  outwGUS(baseport,0x01, frq&~1);
}

/*static unsigned int getvol(unsigned int baseport)
{
 return (inwGUS(baseport,0x09)>>4);
}*/

static void gussetvol(unsigned int baseport,unsigned int vol)
{
  outwGUS(baseport,0x09, vol<<4);
}

static void GUS_setpan(unsigned int baseport,unsigned int pan)
{
  outGUS(baseport,0x0C, pan);
}

static void GUS_setpoint8(unsigned int baseport,unsigned long p, unsigned int t)
{
  t=(t==1)?0x02:(t==2)?0x04:0x0A;
  outwGUS(baseport,t, (p>>7)&0x1FFF);
  outwGUS(baseport,t+1, p<<9);
}

static unsigned long GUS_getpoint8(unsigned int baseport,unsigned int t)
{
  t=(t==1)?0x82:(t==2)?0x84:0x8A;
  return (((inwGUS(baseport,t)<<16)|inwGUS(baseport,t+1))&0x1FFFFFFF)>>9;
}

static void GUS_setmode(unsigned int baseport,unsigned int m)
{
  outdGUS(baseport,0x00, m);
}

static void GUS_setvmode(unsigned int baseport,unsigned int m)
{
  outdGUS(baseport,0x0D, m);
}

static void GUS_settimer(unsigned int baseport,unsigned int o)
{
  outGUS(baseport,0x45, o);
}

static int GUS_testport(unsigned int baseport)
{
 int v0,v1,gus;

 outGUS(baseport,0x4C, 0);

 GUS_delay(baseport);
 GUS_delay(baseport);

 outGUS(baseport,0x4C, 1);

 GUS_delay(baseport);
 GUS_delay(baseport);

 v0=peekGUS(baseport,0);
 v1=peekGUS(baseport,1);

 pokeGUS(baseport,0,0xAA);
 pokeGUS(baseport,1,0x55);

 gus=peekGUS(baseport,0)==0xAA;

 pokeGUS(baseport,0,v0);
 pokeGUS(baseport,1,v1);

 return gus;
}

static void initgus(struct mpxplay_audioout_info_s *aui,int voices)
{
 int i;
 unsigned char l1="\x00\x00\x01\x03\x00\x02\x00\x04\x00\x00\x00\x05\x06\x00\x00\x07"[aui->card_irq]|((aui->card_irq==gusIRQ2)?0x40:"\x00\x00\x08\x18\x00\x10\x00\x20\x00\x00\x00\x28\x30\x00\x00\x38"[gusIRQ2]);
 unsigned char l2="\x00\x01\x00\x02\x00\x03\x04\x05"[aui->card_isa_dma]|((aui->card_isa_dma==aui->card_isa_hidma)?0x40:"\x00\x08\x00\x10\x00\x18\x20\x28"[aui->card_isa_hidma]);
 unsigned int baseport=aui->card_port;

 if(voices<14)
  voices=14;
 if(voices>32)
  voices=32;

 activevoices=voices;

 outGUS(baseport,0x4C, 0);
 for (i=0; i<10; i++)
  GUS_delay(baseport);

 outGUS(baseport,0x4C, 1);
 for (i=0; i<10; i++)
  GUS_delay(baseport);

 outGUS(baseport,0x41, 0x00);
 outGUS(baseport,0x45, 0x00);
 outGUS(baseport,0x49, 0x00);

 outGUS(baseport,0xE, (voices-1)|0xC0);

 inpGUS(baseport,0x6);
 inGUS(baseport,0x41);
 inGUS(baseport,0x49);
 inGUS(baseport,0x8F);

 for (i=0; i<32; i++){
  GUS_select_voice(baseport,i);
  gussetvol(baseport,0);  // vol=0
  GUS_setmode(baseport,3);  // stop voice
  GUS_setvmode(baseport,3);  // stop volume
  GUS_setpoint8(baseport,0,0);
  GUS_delay(baseport);
 }

 inpGUS(baseport,0x6);
 inGUS(baseport,0x41);
 inGUS(baseport,0x49);
 inGUS(baseport,0x8F);

 outGUS(baseport,0x4C,0x07);

 outpGUSF(baseport,5);
 outpGUS0(baseport,0x0B);
 outpGUSB(baseport,0);
 outpGUSF(baseport,0);

 outpGUS0(baseport,0x0B);
 outpGUSB(baseport,l2|0x80);
 outpGUS0(baseport,0x4B);
 outpGUSB(baseport,l1);
 outpGUS0(baseport,0x0B);
 outpGUSB(baseport,l2);
 outpGUS0(baseport,0x4B);
 outpGUSB(baseport,l1);

 GUS_select_voice(baseport,0);
 outpGUS0(baseport,0x08);
 GUS_select_voice(baseport,0);
}

static void dmaupload(struct mpxplay_audioout_info_s *aui,int dest,long bufpos,long buflen)
{
 unsigned int baseport=aui->card_port;
 inGUS(baseport,0x41);
 dest>>=4;
 if(aui->card_isa_dma&4)
  dest>>=1;
 outGUS(baseport,0x41, 0);

 MDma_ISA_Start(aui,DMAMODE_AUTOINIT_OFF,bufpos,buflen);

 outwGUS(baseport,0x42, dest);
 outGUS(baseport,0x41, (aui->card_isa_dma&4)|(signedout?0x00:0x80)|(bit16?0x40:0x00)|0x1);
}

static void handle_voice(struct mpxplay_audioout_info_s *aui)
{
 unsigned long wave_ignore=0;
 unsigned long volume_ignore=0;

 while(1){
  unsigned int baseport=aui->card_port;
  unsigned int irq_source=inGUS(baseport,0x8F);
  unsigned int voice=irq_source&0x1F;
  unsigned long voice_bit=1<<voice;

  if((irq_source&0xC0)==0xC0)
   break;

  if(!(irq_source&0x80))
   if(!(wave_ignore&voice_bit)){
    GUS_select_voice(baseport,voice);
    if(!((inGUS(baseport,0x80)&0x08)||(inGUS(baseport,0x8D)&0x04)))
     wave_ignore|=voice_bit;
    if((voice!=0)&&(voice!=1))
     continue;
    if(voice){
     unsigned char *bufaddr=(unsigned char *)aui->card_DMABUFF;
#if (bit16)
      pokeGUS(baseport,(buflen2<<1)+0, bufaddr[0]);
      pokeGUS(baseport,(buflen2<<1)+1, bufaddr[1]^(signedout?0x00:0x80));
      pokeGUS(baseport,(buflen2<<1)+2, bufaddr[2]);
      pokeGUS(baseport,(buflen2<<1)+3, bufaddr[3]^(signedout?0x00:0x80));
#else
      pokeGUS(baseport,(buflen2<<1)+0, bufaddr[0]^(signedout?0x00:0x80));
      pokeGUS(baseport,(buflen2<<1)+1, bufaddr[1]^(signedout?0x00:0x80));
#endif
     dmaupload(aui,0, 0, buflen2);
    }else
     dmaupload(aui,buflen2,buflen2, buflen2);
   }

  if(!(irq_source&0x40))
   if(!(volume_ignore&voice_bit)){
    GUS_select_voice(baseport,voice);
     if(!(inGUS(baseport,0x8D)&0x08))
      volume_ignore|=voice_bit;
   }
 }
}

static void GUS_irq_routine(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 while (1){
  unsigned int source=inpGUS(baseport,0x6);
  if(!source)
   break;
  if(source&0x03)
   inpGUS(baseport,0x100);
  if(source&0x80)
   inGUS(baseport,0x41);
  if(source&0x0C)
   GUS_settimer(baseport,0x00);
  if(source&0x60)
   handle_voice(aui);
 }
}

static void GUS_memclear(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port,bufsize=aui->card_dmasize;
 unsigned int i;

 outGUS(baseport,0x44,0);

 for(i=0;i<bufsize;i++){
  outwGUS(baseport,0x43, i);
  outpGUS(baseport,0x107, 0);
  //pokeGUS(baseport,i,0);
 }
}

static void GUS_buffers_clear(struct mpxplay_audioout_info_s *aui)
{
 GUS_memclear(aui);
 MDma_ISA_Clear(aui);
 savepos=-1;
}

static void GUS_setrate(struct mpxplay_audioout_info_s *aui)
{
 long newrate=aui->freq_card;

 //if(newrate<19293)
 // newrate=19293;
 if(newrate<22050)
  newrate=22050;
 if(newrate>44100)
  newrate=44100;

 gus_timeconst=617400/newrate;
 newrate=617400/gus_timeconst;

 if(abs(newrate-aui->freq_card)<100)
  newrate=aui->freq_card;

 aui->freq_card=newrate;
 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 MDma_ISA_init(aui);
}

static long GUS_getpos(struct mpxplay_audioout_info_s *aui)
{
 unsigned int baseport=aui->card_port;
 int p,pos;

 GUS_select_voice(baseport,0);
 p=GUS_getpoint8(baseport,0);
 while (1){
  int p2;
  GUS_select_voice(baseport,0);
  p2=GUS_getpoint8(baseport,0);
  if(abs(p-p2)<0x40)
   break;
  p=p2;
 }

 pos=((p<<(bit16+stereo))+buflen2)%(buflen2<<1);
 return pos;
}

static void GUS_start(struct mpxplay_audioout_info_s *aui)
{
 unsigned int len;
 unsigned int baseport=aui->card_port;

 initgus(aui,gus_timeconst);

 GUS_select_voice(baseport,0);
 GUS_delay(baseport);
 outpGUS0(baseport,0x09);
 GUS_delay(baseport);

 len=aui->card_dmasize;
 buflen2=len>>1;

 MIrq_Start(aui->card_irq,GUS_irq_routine,&aui->card_infobits);
 //dmaupload(aui,0, 0, buflen2<<1);
 if(savepos>=0)
  dmaupload(aui,savepos,savepos,(buflen2<<1)-savepos);
 else
  //dmaupload(aui,0, 0, buflen2<<1);
  dmaupload(aui,buflen2, buflen2, buflen2);

 GUS_select_voice(baseport,0);
 GUS_setpoint8(baseport,0,0);
 GUS_setpoint8(baseport,0,1);
 GUS_setpoint8(baseport,(buflen2<<1)>>(bit16+stereo),2);
 GUS_setfreq(baseport,1024);

 GUS_select_voice(baseport,1);
 GUS_setpoint8(baseport,buflen2>>(bit16+stereo),0);
 GUS_setpoint8(baseport,0,1);
 GUS_setpoint8(baseport,(buflen2<<1)>>(bit16+stereo),2);
 GUS_setfreq(baseport,1024);

#if (stereo)
  GUS_select_voice(baseport,2);
  GUS_setpoint8(baseport,0,0);
  GUS_setpoint8(baseport,0,1);
  GUS_setpoint8(baseport,buflen2<<(1-bit16),2);
  GUS_setfreq(baseport,2048);
  GUS_setpan(baseport,0);
  gussetvol(baseport,gus_volume);

  GUS_select_voice(baseport,3);
  GUS_setpoint8(baseport,1,0);
  GUS_setpoint8(baseport,0,1);
  GUS_setpoint8(baseport,buflen2<<(1-bit16),2);
  GUS_setfreq(baseport,2048);
  GUS_setpan(baseport,15);
  gussetvol(baseport,gus_volume);

  GUS_select_voice(baseport,2);
  GUS_setmode(baseport,bit16?0x0C:0x08);
  GUS_select_voice(baseport,3);
  GUS_setmode(baseport,bit16?0x0C:0x08);
#else
  GUS_select_voice(baseport,2);
  GUS_setpoint8(baseport,0,0);
  GUS_setpoint8(baseport,0,1);
  GUS_setpoint8(baseport,buflen2<<(1-bit16),2);
  GUS_setfreq(baseport,1024);
  GUS_setpan(baseport,8);
  gussetvol(baseport,gus_volume);
  GUS_setmode(baseport,bit16?0x0C:0x08);
#endif

 GUS_select_voice(baseport,0);
 GUS_setmode(baseport,0x28);
 GUS_select_voice(baseport,1);
 GUS_setmode(baseport,0x28);

 startinitok=1;
}

static void GUS_stop(struct mpxplay_audioout_info_s *aui)
{
 MIrq_Stop(aui->card_irq,&aui->card_infobits);
 initgus(aui,14);
 MDma_ISA_Stop(aui);

 savepos=GUS_getpos(aui);
 savepos=savepos%aui->card_dmasize;
}

static int GUS_init(struct mpxplay_audioout_info_s *aui)
{
 if(!GUS_getcfg(aui))
  return 0;
 if(!GUS_testport(aui->card_port))
  return 0;
 aui->card_controlbits|=AUINFOS_CARDCNTRLBIT_DMACLEAR;  // clear gusmem
 savepos=-1;
 return 1;
}

//-------------------------------------------------------------------------
//mixer

static void GUS_write_reg(struct mpxplay_audioout_info_s *aui,unsigned long reg,unsigned long data)
{
 if(reg==0x09)
  gus_volume=data;
 outwGUS(aui->card_port,reg,data);
}

static unsigned long GUS_read_reg(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 if(reg==0x09){
  if(!gus_volume)
   gus_volume=0xe80;
  return gus_volume;
 }
 return (inwGUS(aui->card_port,reg)); // it seems this doesn't work
}

static aucards_onemixerchan_s gus_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),1,{{0x09,255,4,0}}};

static aucards_allmixerchan_s gus_mixer_info[]={
 &gus_master_vol,
 NULL
};

one_sndcard_info GUS_sndcard_info={
 "GUS",
 SNDCARD_INT08_ALLOWED|SNDCARD_SPECIAL_IRQ,

 NULL,
 &GUS_init,
 NULL,
 NULL,
 &GUS_start,
 &GUS_stop,
 &MDma_ISA_FreeMem,
 &GUS_setrate,

 &MDma_writedata,
 &GUS_getpos,
 &GUS_buffers_clear,
 &MDma_interrupt_monitor,
 &GUS_irq_routine,

 &GUS_write_reg,
 &GUS_read_reg,
 &gus_mixer_info[0]
};

#endif
