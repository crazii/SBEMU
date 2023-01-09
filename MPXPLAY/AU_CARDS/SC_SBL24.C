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
//function: SB Live24/Audigy LS (CA0106) low level routines (with sc_sbliv.c)
//based on the ALSA (http://www.alsa-project.org)

//#define MPXPLAY_USE_DEBUGF 1
//#define SBL_DEBUG_OUTPUT stdout

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_SBLIVE

#include "dmairq.h"
#include "pcibios.h"
#include "sc_sbliv.h"
#include "ac97_def.h"
#include "sc_sbl24.h"

#define CA0106_DMABUF_PERIODS    8 // max
#define CA0106_PERIOD_ALIGN     64
#define CA0106_MAX_CHANNELS      8 // 7.1 (but we use 2 only)
#define CA0106_MAX_BYTES         4 // 32 bits
#define CA0106_DMABUF_ALIGN (CA0106_DMABUF_PERIODS*CA0106_PERIOD_ALIGN) // 512

#define AUDIGYLS_USE_AC97 1 // for Audigy LS cards (set it to 0, if it doesn't work with AC97)

static void snd_emu_ac97_write(struct emu10k1_card *card,unsigned int reg, unsigned int value)
{
 outb(card->iobase + AC97ADDRESS, reg);
 outw(card->iobase + AC97DATA, value);
}

#ifdef AUDIGYLS_USE_AC97

static unsigned int snd_emu_ac97_read(struct emu10k1_card *card, unsigned int reg)
{
 outb(card->iobase + AC97ADDRESS, reg);
 return inw(card->iobase + AC97DATA);
}

static void snd_emu_ac97_init(struct emu10k1_card *card)
{
 snd_emu_ac97_write(card, AC97_RESET, 0);
 snd_emu_ac97_read(card, AC97_RESET);

 snd_emu_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
 snd_emu_ac97_write(card, AC97_PCMOUT_VOL,        0x0202);
 snd_emu_ac97_write(card, AC97_HEADPHONE_VOL,     0x0202);
 snd_emu_ac97_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
}

#else

static void snd_emu_ac97_mute(struct emu10k1_card *card)
{
 snd_emu_ac97_write(card, AC97_MASTER_VOL_STEREO, AC97_MUTE);
}

#endif // AUDIGYLS_USE_AC97

//--------------------------------------------------------------------

static unsigned int snd_ca0106_ptr_read(struct emu10k1_card *card,unsigned int reg,unsigned int chn)
{
 unsigned int regptr, val;

 regptr = (reg << 16) | chn;

 outl(card->iobase + PTR, regptr);
 val = inl(card->iobase + DATA);

 return val;
}

static void snd_ca0106_ptr_write(struct emu10k1_card *card,unsigned int reg,unsigned int chn,unsigned int data)
{
 unsigned int regptr;

 regptr = (reg << 16) | chn;

 outl(card->iobase + PTR, regptr);
 outl(card->iobase + DATA, data);
}

static unsigned int snd_audigyls_selector(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 if((card->chips&EMU_CHIPS_0106) && ((card->serial==0x10021102) || (card->serial==0x10051102))){
  mpxplay_debugf(SBL_DEBUG_OUTPUT,"selected : audigy ls");
  return 1;
 }

 return 0;
}

static unsigned int snd_live24_selector(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 if((card->chips&EMU_CHIPS_0106) && ((card->serial==0x10061102) || (card->serial==0x10071102) || (card->serial==0x10091462) || (card->serial==0x30381297) || (card->serial==0x10121102) || !card->card_capabilities->subsystem)){
  mpxplay_debugf(SBL_DEBUG_OUTPUT,"selected : live24");
  return 1;
 }

 return 0;
}

static void snd_ca0106_hw_init(struct emu10k1_card *card)
{
 unsigned int ch;

 outl(card->iobase + INTE, 0);

 snd_ca0106_ptr_write(card, SPCS0, 0,
                  SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
          SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
          SPCS_GENERATIONSTATUS | 0x00001200 |
          0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);
 // Only SPCS1 has been tested
 snd_ca0106_ptr_write(card, SPCS1, 0,
                  SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
          SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
          SPCS_GENERATIONSTATUS | 0x00001200 |
          0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);
 snd_ca0106_ptr_write(card, SPCS2, 0,
                  SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
          SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
          SPCS_GENERATIONSTATUS | 0x00001200 |
          0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);
 snd_ca0106_ptr_write(card, SPCS3, 0,
                  SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
          SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
          SPCS_GENERATIONSTATUS | 0x00001200 |
          0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);

 snd_ca0106_ptr_write(card, PLAYBACK_MUTE, 0, 0x00fc0000);
 snd_ca0106_ptr_write(card, CAPTURE_MUTE, 0, 0x00fc0000);

 outb(card->iobase + AC97ADDRESS, AC97_RECORD_GAIN);
 outw(card->iobase + AC97DATA, 0x8000); // mute

 snd_ca0106_ptr_write(card, SPDIF_SELECT1, 0, 0xf);
 snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, 0x01010001); // enable analog,spdif,ac97 front
 //snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, 0x0f0f0000); // enable analog and spdif 7.1

 snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 0, 0x40c81000); // goes to 0x40c80000 when doing SPDIF IN/OUT
 snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 1, 0xffffffff); // (Mute) CAPTURE feedback into PLAYBACK volume. Only lower 16 bits matter.

 // ??? does this work at all ?
 snd_ca0106_ptr_write(card, PLAYBACK_ROUTING1, 0, 0x32765410);
 //snd_ca0106_ptr_write(card, PLAYBACK_ROUTING1, 0, 0x10765410);
 snd_ca0106_ptr_write(card, PLAYBACK_ROUTING2, 0, 0x76767676);

 // mute unused channels
 for(ch = 0; ch < 4; ch++) {
  snd_ca0106_ptr_write(card, CAPTURE_VOLUME1, ch, 0xffffffff); // Only high 16 bits matter
  snd_ca0106_ptr_write(card, CAPTURE_VOLUME2, ch, 0xffffffff); // mute ???
 }
 for(ch = 1; ch < 4; ch++) { // !!! we keep the playback volume of channel 0
  snd_ca0106_ptr_write(card, PLAYBACK_VOLUME1, ch, 0xffffffff); // Mute
  snd_ca0106_ptr_write(card, PLAYBACK_VOLUME2, ch, 0xffffffff); // Mute
 }

 outl(card->iobase+GPIO, 0x0);
}

static void snd_audigyls_hw_init(struct emu10k1_card *card)
{
 snd_ca0106_hw_init(card);

 //outl(card->iobase+GPIO, 0x005f03a3); // analog
 outl(card->iobase+GPIO,0x005f02a2);// SPDIF

 outl(card->iobase+HCFG, HCFG_AC97 | HCFG_AUDIOENABLE); // AC97 2.0, enable outputs

#ifdef AUDIGYLS_USE_AC97
 snd_emu_ac97_init(card);
#else
 snd_emu_ac97_mute(card);
#endif
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"hw init end : audigy ls");
}

static void snd_live24_hw_init(struct emu10k1_card *card)
{
 snd_ca0106_hw_init(card);

 //outl(card->iobase+GPIO, 0x005f5301); // analog
 outl(card->iobase+GPIO, 0x005f5201); // SPDIF

 outl(card->iobase+HCFG, HCFG_AUDIOENABLE);

 mpxplay_debugf(SBL_DEBUG_OUTPUT,"hw init end : live24");
}

static void snd_live24_hw_close(struct emu10k1_card *card)
{
 snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, 0);
 outl(card->iobase + INTE, 0);
 outl(card->iobase + HCFG, 0);
}

static unsigned int snd_live24_buffer_init(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int bytes_per_sample=(aui->bits_set>=24)? 4:2;
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,CA0106_DMABUF_ALIGN,bytes_per_sample,0);
 card->dm=MDma_alloc_cardmem(CA0106_DMABUF_PERIODS*2*sizeof(uint32_t)+card->pcmout_bufsize);
 card->virtualpagetable=(uint32_t *)card->dm->linearptr;
 card->pcmout_buffer=((char *)card->virtualpagetable)+CA0106_DMABUF_PERIODS*2*sizeof(uint32_t);
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"buffer init: pagetable:%8.8X pcmoutbuf:%8.8X size:%d",(unsigned long)card->virtualpagetable,(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
 return 1;
}

static void snd_ca0106_pcm_prepare_playback(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 const uint32_t channel=0;
 uint32_t *table_base =card->virtualpagetable;
 uint32_t period_size_bytes=card->period_size;
 uint32_t i,reg40_set,reg71_set;

 switch(aui->freq_card){
  case 44100:
   reg40_set = 0x10000 << (channel<<1);
   reg71_set = 0x01010000;
   break;
  case 96000:
   reg40_set = 0x20000 << (channel<<1);
   reg71_set = 0x02020000;
   break;
  case 192000:
   reg40_set = 0x30000 << (channel<<1);
   reg71_set = 0x03030000;
   break;
  default: // 48000
   reg40_set = 0;
   reg71_set = 0;
   break;
 }

 i = snd_ca0106_ptr_read(card, 0x40, 0); // control host to fifo
 i = (i & (~(0x30000<<(channel<<1)))) | reg40_set;
 snd_ca0106_ptr_write(card, 0x40, 0, i);

 i = snd_ca0106_ptr_read(card, 0x71, 0); // control DAC rate (SPDIF)
 i = (i & (~0x03030000)) | reg71_set;
 snd_ca0106_ptr_write(card, 0x71, 0, i);

 i=inl(card->iobase+HCFG);               // control bit width
 if(aui->bits_card==32)
  i|=HCFG_PLAYBACK_S32_LE;
 else
  i&=~HCFG_PLAYBACK_S32_LE;
 outl(card->iobase+HCFG,i);

 // build pagetable
 for(i=0; i<CA0106_DMABUF_PERIODS; i++){
  table_base[i*2]=(uint32_t)((char *)card->pcmout_buffer+(i*period_size_bytes));
  table_base[i*2+1]=period_size_bytes<<16;
 }

 snd_ca0106_ptr_write(card, PLAYBACK_LIST_ADDR, channel, (uint32_t)(table_base));
 snd_ca0106_ptr_write(card, PLAYBACK_LIST_SIZE, channel, (CA0106_DMABUF_PERIODS - 1) << 19);
 snd_ca0106_ptr_write(card, PLAYBACK_LIST_PTR, channel, 0);
 snd_ca0106_ptr_write(card, PLAYBACK_DMA_ADDR, channel, (uint32_t)card->pcmout_buffer);
 snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, 0);
 //snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, period_size_bytes<<16);
 snd_ca0106_ptr_write(card, PLAYBACK_POINTER, channel, 0);
 snd_ca0106_ptr_write(card, 0x07, channel, 0x0);
 snd_ca0106_ptr_write(card, 0x08, channel, 0);
 snd_ca0106_ptr_write(card, PLAYBACK_MUTE, 0x0, 0x0); // unmute output

 mpxplay_debugf(SBL_DEBUG_OUTPUT,"prepare playback end");
}

static void snd_live24_setrate(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int dmabufsize;

 aui->chan_card=2;

 if(aui->bits_set>16)
  aui->bits_card=32;
 else
  aui->bits_card=16;

 if(aui->freq_set==44100)     // forced 44.1k dac output
  aui->freq_card=44100;
 else
  if(aui->freq_card!=48000){
   if(aui->freq_card<=22050)
    aui->freq_card=48000;
   else
    if((aui->freq_card<=96000) || (card->serial==0x10121102)) // (44.1->96) because 44.1k dac out sounds bad (?)
     aui->freq_card=96000;
    else
     aui->freq_card=192000;
  }

 dmabufsize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,CA0106_DMABUF_ALIGN,0);
 card->period_size=(dmabufsize/CA0106_DMABUF_PERIODS);
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"buffer config: bufsize:%d period_size:%d",dmabufsize,card->period_size);

 snd_ca0106_pcm_prepare_playback(card,aui);
}

static void snd_live24_pcm_start_playback(struct emu10k1_card *card)
{
 const uint32_t channel=0;
 snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, snd_ca0106_ptr_read(card, BASIC_INTERRUPT, 0) | (0x1<<channel));
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"start playback");
}

static void snd_live24_pcm_stop_playback(struct emu10k1_card *card)
{
 const uint32_t channel=0;
 snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, snd_ca0106_ptr_read(card, BASIC_INTERRUPT, 0) & (~(0x1<<channel)));
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"stop playback");
}

static unsigned int snd_live24_pcm_pointer_playback(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int ptr,ptr1,ptr3,ptr4;
 const uint32_t channel=0;

 ptr3 = snd_ca0106_ptr_read(card, PLAYBACK_LIST_PTR, channel);
 ptr1 = snd_ca0106_ptr_read(card, PLAYBACK_POINTER, channel);
 ptr4 = snd_ca0106_ptr_read(card, PLAYBACK_LIST_PTR, channel);
 if(ptr3!=ptr4)
  ptr1=snd_ca0106_ptr_read(card, PLAYBACK_POINTER, channel);

 ptr4/=(2*sizeof(uint32_t));

 ptr=(ptr4*card->period_size)+ptr1;

 mpxplay_debugf(SBL_DEBUG_OUTPUT,"list_ptr:%3d period_ptr:%4d bufpos:%d",ptr4,ptr1,ptr);

 ptr/=aui->chan_card;
 ptr/=aui->bits_card>>3;

 return ptr;
}

// Live 24 cards have no ac97

static unsigned int snd_live24_mixer_read(struct emu10k1_card *card,unsigned int reg)
{
 unsigned int channel_id=reg>>8;
 reg&=0xff;
 return snd_ca0106_ptr_read(card,reg,channel_id);
}

static void snd_live24_mixer_write(struct emu10k1_card *card,unsigned int reg,unsigned int value)
{
 unsigned int channel_id=reg>>8;
 reg&=0xff;
 snd_ca0106_ptr_write(card,reg,channel_id,value);
}

static aucards_onemixerchan_s emu_live24_analog_front={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,
 {
  {((CONTROL_FRONT_CHANNEL<<8)|PLAYBACK_VOLUME2),0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {((CONTROL_FRONT_CHANNEL<<8)|PLAYBACK_VOLUME2),0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
  //{((CONTROL_REAR_CHANNEL<<8)|PLAYBACK_VOLUME2),0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // !!! bad place
  //{((CONTROL_REAR_CHANNEL<<8)|PLAYBACK_VOLUME2),0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_onemixerchan_s emu_live24_spdif_front={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_SPDIFOUT,AU_MIXCHANFUNC_VOLUME),2,
 {
  {((CONTROL_FRONT_CHANNEL<<8)|PLAYBACK_VOLUME1),0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {((CONTROL_FRONT_CHANNEL<<8)|PLAYBACK_VOLUME1),0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
  //{((CONTROL_REAR_CHANNEL<<8)|PLAYBACK_VOLUME1),0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // !!! bad place
  //{((CONTROL_REAR_CHANNEL<<8)|PLAYBACK_VOLUME1),0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_allmixerchan_s emu_live24_mixerset[]={
 &emu_live24_analog_front,
 &emu_live24_spdif_front,
 NULL
};

struct emu_driver_func_s emu_driver_audigyls_funcs={
 &snd_audigyls_selector,
 &snd_audigyls_hw_init,
 &snd_live24_hw_close,
 &snd_live24_buffer_init,
 &snd_live24_setrate,
 &snd_live24_pcm_start_playback,
 &snd_live24_pcm_stop_playback,
 &snd_live24_pcm_pointer_playback,
 NULL,
#ifdef AUDIGYLS_USE_AC97
 &snd_emu_ac97_read,
 &snd_emu_ac97_write,
 &mpxplay_aucards_ac97chan_mixerset[0]
#else
 &snd_live24_mixer_read,
 &snd_live24_mixer_write,
 &emu_live24_mixerset[0]
#endif
};

struct emu_driver_func_s emu_driver_live24_funcs={
 &snd_live24_selector,
 &snd_live24_hw_init,
 &snd_live24_hw_close,
 &snd_live24_buffer_init,
 &snd_live24_setrate,
 &snd_live24_pcm_start_playback,
 &snd_live24_pcm_stop_playback,
 &snd_live24_pcm_pointer_playback,
 NULL,
 &snd_live24_mixer_read,
 &snd_live24_mixer_write,
 &emu_live24_mixerset[0]
};

#endif // AU_CARDS_LINK_SBLIVE
