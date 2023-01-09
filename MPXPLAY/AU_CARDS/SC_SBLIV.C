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
//function: SB Live/Audigy 1,2,4 (EMU10Kx,CA0151) low level routines
//based on the Creative (http://sourceforge.net/projects/emu10k1)
//         and ALSA (http://www.alsa-project.org) drivers

//#define MPXPLAY_USE_DEBUGF 1
//#define SBL_DEBUG_OUTPUT stdout

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_SBLIVE

#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"
#include "emu10k1.h"
#include "sc_sbliv.h"

#define VOICE_FLAGS_MASTER      0x01
#define VOICE_FLAGS_STEREO    0x02
#define VOICE_FLAGS_16BIT    0x04

//#define AUDIGY1_USE_AC97 1 // it's for testing only

#define AUDIGY_PCMVOLUME_DEFAULT  66  // 0-100

#define emu10k1_writefn0(card,reg,data) outl(card->iobase+reg,data)

#define A_PTR_ADDRESS_MASK 0x0fff0000

static void snd_emu10kx_fx_init(struct emu10k1_card *card);

static void emu10k1_writeptr(struct emu10k1_card *card, uint32_t reg, uint32_t channel, uint32_t data)
{
 uint32_t regptr;

 regptr = ((reg << 16) & A_PTR_ADDRESS_MASK) | (channel & PTR_CHANNELNUM_MASK);

 if(reg & 0xff000000) {
  uint32_t mask;
  uint8_t size, offset;

  size = (reg >> 24) & 0x3f;
  offset = (reg >> 16) & 0x1f;
  mask = ((1 << size) - 1) << offset;
  data = (data << offset) & mask;

  outl(card->iobase + PTR, regptr);
  data |= inl(card->iobase + DATA) & ~mask;
  outl(card->iobase + DATA, data);
 }else{
  outl(card->iobase + PTR, regptr);
  outl(card->iobase + DATA, data);
 }
}

static uint32_t emu10k1_readptr(struct emu10k1_card *card, uint32_t reg, uint32_t channel)
{
 uint32_t regptr, val;

 regptr = ((reg << 16) & A_PTR_ADDRESS_MASK) | (channel & PTR_CHANNELNUM_MASK);

 if(reg & 0xff000000) {
  uint32_t mask;
  uint8_t size, offset;

  size = (reg >> 24) & 0x3f;
  offset = (reg >> 16) & 0x1f;
  mask = ((1 << size) - 1) << offset;

  outl(card->iobase + PTR, regptr);
  val = inl(card->iobase + DATA);

  return (val & mask) >> offset;
 }else{
  outl(card->iobase + PTR, regptr);
  val = inl(card->iobase + DATA);

  return val;
 }
}

static void emu10k1_ptr20_write(struct emu10k1_card *card,uint32_t reg,uint32_t chn,uint32_t data)
{
 uint32_t regptr = (reg << 16) | chn;
 outl(card->iobase + PTR2 , regptr);
 outl(card->iobase + DATA2, data);
}

static uint32_t emu10k1_ptr20_read(struct emu10k1_card *card,uint32_t reg,uint32_t chn)
{
 uint32_t regptr, val;
 regptr = (reg << 16) | chn;
 outl(card->iobase + PTR2, regptr);
 val = inl(card->iobase + DATA2);
 return val;
}

//-----------------------------------------------------------------------
// init & close

static void snd_emu10k1_hw_init(struct emu10k1_card *card)
{
 int ch;
 uint32_t silent_page;

 // disable audio and lock cache
 emu10k1_writefn0(card,HCFG,HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE_MASK | HCFG_MUTEBUTTONENABLE);

 // reset recording buffers
 emu10k1_writeptr(card, MICBS, 0, ADCBS_BUFSIZE_NONE);
 emu10k1_writeptr(card, MICBA, 0, 0);
 emu10k1_writeptr(card, FXBS, 0, ADCBS_BUFSIZE_NONE);
 emu10k1_writeptr(card, FXBA, 0, 0);
 emu10k1_writeptr(card, ADCBS, 0, ADCBS_BUFSIZE_NONE);
 emu10k1_writeptr(card, ADCBA, 0, 0);

 // disable channel interrupt
 emu10k1_writefn0(card, INTE, 0);
 emu10k1_writeptr(card, CLIEL, 0, 0);
 emu10k1_writeptr(card, CLIEH, 0, 0);
 emu10k1_writeptr(card, SOLEL, 0, 0);
 emu10k1_writeptr(card, SOLEH, 0, 0);

 if(card->chips&EMU_CHIPS_10K2){
  emu10k1_writeptr(card, SPBYPASS, 0, SPBYPASS_FORMAT);
#ifdef AUDIGY1_USE_AC97
  emu10k1_writeptr(card, AC97SLOT, 0, AC97SLOT_REAR_RIGHT|AC97SLOT_REAR_LEFT); // ?? no ac97 rear out on Audigy
#endif
 }

 // init envelope engine
 for (ch = 0; ch < NUM_G; ch++){
  emu10k1_writeptr(card, DCYSUSV, ch, 0);
  emu10k1_writeptr(card, IP, ch, 0);
  emu10k1_writeptr(card, VTFT, ch, 0xffff);
  emu10k1_writeptr(card, CVCF, ch, 0xffff);
  emu10k1_writeptr(card, PTRX, ch, 0);
  emu10k1_writeptr(card, CPF, ch, 0);
  emu10k1_writeptr(card, CCR, ch, 0);

  emu10k1_writeptr(card, PSST, ch, 0);
  emu10k1_writeptr(card, DSL, ch, 0x10);
  emu10k1_writeptr(card, CCCA, ch, 0);
  emu10k1_writeptr(card, Z1, ch, 0);
  emu10k1_writeptr(card, Z2, ch, 0);
  emu10k1_writeptr(card, FXRT, ch, 0x32100000);

  emu10k1_writeptr(card, ATKHLDM, ch, 0);
  emu10k1_writeptr(card, DCYSUSM, ch, 0);
  emu10k1_writeptr(card, IFATN, ch, 0xffff);
  emu10k1_writeptr(card, PEFE, ch, 0);
  emu10k1_writeptr(card, FMMOD, ch, 0);
  emu10k1_writeptr(card, TREMFRQ, ch, 24);  // 1 Hz
  emu10k1_writeptr(card, FM2FRQ2, ch, 24);  // 1 Hz
  emu10k1_writeptr(card, TEMPENV, ch, 0);

  // these are last so OFF prevents writing
  emu10k1_writeptr(card, LFOVAL2, ch, 0);
  emu10k1_writeptr(card, LFOVAL1, ch, 0);
  emu10k1_writeptr(card, ATKHLDV, ch, 0);
  emu10k1_writeptr(card, ENVVOL, ch, 0);
  emu10k1_writeptr(card, ENVVAL, ch, 0);

  // Audigy extra stuffs
  if (card->chips&EMU_CHIPS_10K2) {
   emu10k1_writeptr(card, 0x4c, ch, 0); // ??
   emu10k1_writeptr(card, 0x4d, ch, 0); // ??
   emu10k1_writeptr(card, 0x4e, ch, 0); // ??
   emu10k1_writeptr(card, 0x4f, ch, 0); // ??
   emu10k1_writeptr(card, A_FXRT1, ch, 0x03020100);
   emu10k1_writeptr(card, A_FXRT2, ch, 0x3f3f3f3f);
   emu10k1_writeptr(card, A_SENDAMOUNTS, ch, 0);
  }
 }

 /*
  *  Init to 0x02109204 :
  *  Clock accuracy    = 0     (1000ppm)
  *  Sample Rate       = 2     (48kHz)
  *  Audio Channel     = 1     (Left of 2)
  *  Source Number     = 0     (Unspecified)
  *  Generation Status = 1     (Original for Cat Code 12)
  *  Cat Code          = 12    (Digital Signal Mixer)
  *  Mode              = 0     (Mode 0)
  *  Emphasis          = 0     (None)
  *  CP                = 1     (Copyright unasserted)
  *  AN                = 0     (Audio data)
  *  P                 = 0     (Consumer)
  */
 emu10k1_writeptr(card, SPCS0, 0,
            SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
            SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
            SPCS_GENERATIONSTATUS | 0x00001200 |
            0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);
 emu10k1_writeptr(card, SPCS1, 0,
            SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
            SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
            SPCS_GENERATIONSTATUS | 0x00001200 |
            0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);
 emu10k1_writeptr(card, SPCS2, 0,
            SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
            SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
            SPCS_GENERATIONSTATUS | 0x00001200 |
            0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT);

 if(card->card_capabilities->chips&EMU_CHIPS_0151) { // audigy2,4 (24 bit)
  // Hacks for Alice3 to work independent of haP16V driver
  uint32_t tmp;

  //Setup SRCMulti_I2S SamplingRate
  tmp = emu10k1_readptr(card, A_SPDIF_SAMPLERATE, 0);
  tmp &= 0xfffff1ff;
  tmp |= (0x2<<9);
  emu10k1_writeptr(card, A_SPDIF_SAMPLERATE, 0, tmp);

  // Setup SRCSel (Enable Spdif,I2S SRCMulti)
  emu10k1_ptr20_write(card, SRCSel, 0, 0x14);
  // Setup SRCMulti Input Audio Enable
  emu10k1_ptr20_write(card, SRCMULTI_ENABLE, 0, 0xFFFFFFFF);

  // Enabled Phased (8-channel) P16V playback
  //outl(card->iobase + HCFG2, 0x0201); // in ALSA
  // enable 2 channel P16V playback
  outl(card->iobase + HCFG2, 0x0001); // in Mpxplay

  // Set playback routing.
  emu10k1_ptr20_write(card, CAPTURE_P16V_SOURCE, 0, 0x0000); // ??? in Mpxplay (was 0x0400)
  //emu10k1_ptr20_write(card, CAPTURE_P16V_SOURCE, 0, 0x78e4); // in ALSA
 }

 if(card->chips&EMU_CHIPS_0108){ // Audigy2 Value
  // Hacks for Alice3 to work independent of haP16V driver
  uint32_t tmp;

  //Setup SRCMulti_I2S SamplingRate
  tmp = emu10k1_readptr(card, A_SPDIF_SAMPLERATE, 0);
  tmp &= 0xfffff1ff;
  tmp |= (0x2<<9);
  emu10k1_writeptr(card, A_SPDIF_SAMPLERATE, 0, tmp);

  // Setup SRCSel (Enable Spdif,I2S SRCMulti)
  outl(card->iobase + 0x20, 0x600000);
  outl(card->iobase + 0x24, 0x14);

  // Setup SRCMulti Input Audio Enable
  outl(card->iobase + 0x20, 0x7b0000);
  outl(card->iobase + 0x24, 0xFF000000);

  // Setup SPDIF Out Audio Enable
  // The Audigy 2 Value has a separate SPDIF out,
  // so no need for a mixer switch
  outl(card->iobase + 0x20, 0x7a0000);
  outl(card->iobase + 0x24, 0xFF000000);
  tmp = inl(card->iobase + A_IOCFG) & ~0x8; // Clear bit 3
  outl(card->iobase + A_IOCFG, tmp);
 }

 if(card->chip_select&EMU_CHIPS_10KX){
  //buffer config
  emu10k1_writeptr(card, PTB, 0, (uint32_t) card->virtualpagetable);
  emu10k1_writeptr(card, TCB, 0, 0);
  emu10k1_writeptr(card, TCBS, 0, 4);

  silent_page = (((uint32_t)card->silentpage) << 1) | MAP_PTI_MASK;

  for (ch = 0; ch < NUM_G; ch++) {
   emu10k1_writeptr(card, MAPA, ch, silent_page);
   emu10k1_writeptr(card, MAPB, ch, silent_page);
  }
 }

 // setup HCFG
 if(card->chips&EMU_CHIPS_10KX){
  if(card->chips&EMU_CHIPS_10K2){ // Audigy
   if(card->chiprev == 4) // Audigy 2,4
    emu10k1_writefn0(card,HCFG,HCFG_AC3ENABLE_CDSPDIF | HCFG_AC3ENABLE_GPSPDIF | HCFG_AUTOMUTE | HCFG_JOYENABLE);
   else                   // Audigy 1
    emu10k1_writefn0(card,HCFG,HCFG_AUTOMUTE | HCFG_JOYENABLE);
  }else{ // SB Live
   if(card->model == 0x20 || card->model == 0xc400 || (card->model == 0x21 && card->chiprev < 6))
    emu10k1_writefn0(card,HCFG,HCFG_LOCKTANKCACHE_MASK | HCFG_AUTOMUTE);
   else
    emu10k1_writefn0(card,HCFG,HCFG_LOCKTANKCACHE_MASK | HCFG_AUTOMUTE | HCFG_JOYENABLE);
  }
 }

 if(card->chips&EMU_CHIPS_10K2) {    // enable analog output
  uint32_t tmp = inl(card->iobase + A_IOCFG);
  outl(card->iobase + A_IOCFG, tmp | A_IOCFG_GPOUT0);
 }

 //mixer (routing) config
 if(card->chip_select&EMU_CHIPS_10KX)
  snd_emu10kx_fx_init(card);

 //Enable the audio bit
 outl(card->iobase + HCFG, inl(card->iobase + HCFG) | HCFG_AUDIOENABLE);

 if(card->chips&EMU_CHIPS_10K2){
  uint32_t tmp = inl(card->iobase + A_IOCFG);
  tmp&=~0x44;
  if(card->chiprev==4) // Audigy2,4 Unmute Analog now.  Set GPO6 to 1 for Apollo.
   tmp|=0x0040;
  else
   if(card->chips&EMU_CHIPS_0108) // Audigy2 Value
    tmp|=0x0060; // unmute
   else         // Audigy 1
#ifdef AUDIGY1_USE_AC97
    tmp&=~0x0080; // enable routing from AC97 line out to Front speakers
#else
    tmp|=0x0080; // disable routing from AC97 line out to Front speakers
#endif
  outl(card->iobase + A_IOCFG, tmp);
 }
}

static void snd_emu10k1_hw_close(struct emu10k1_card *card)
{
 int ch;

 emu10k1_writefn0(card,INTE,0);

 // Shutdown the chip
 for (ch = 0; ch < NUM_G; ch++)
  emu10k1_writeptr(card, DCYSUSV, ch, 0);
 for (ch = 0; ch < NUM_G; ch++) {
  emu10k1_writeptr(card, VTFT, ch, 0);
  emu10k1_writeptr(card, CVCF, ch, 0);
  emu10k1_writeptr(card, PTRX, ch, 0);
  emu10k1_writeptr(card, CPF, ch, 0);
 }

 // reset recording buffers
 emu10k1_writeptr(card, MICBS, 0, 0);
 emu10k1_writeptr(card, MICBA, 0, 0);
 emu10k1_writeptr(card, FXBS, 0, 0);
 emu10k1_writeptr(card, FXBA, 0, 0);
 emu10k1_writeptr(card, FXWC, 0, 0);
 emu10k1_writeptr(card, ADCBS, 0, ADCBS_BUFSIZE_NONE);
 emu10k1_writeptr(card, ADCBA, 0, 0);
 emu10k1_writeptr(card, TCBS, 0, TCBS_BUFFSIZE_16K);
 emu10k1_writeptr(card, TCB, 0, 0);
 if(card->chips&EMU_CHIPS_10K2)
  emu10k1_writeptr(card, A_DBG, 0, A_DBG_SINGLE_STEP);
 else
  emu10k1_writeptr(card, DBG, 0, EMU10K1_DBG_SINGLE_STEP);

 // disable channel interrupt
 emu10k1_writeptr(card, CLIEL, 0, 0);
 emu10k1_writeptr(card, CLIEH, 0, 0);
 emu10k1_writeptr(card, SOLEL, 0, 0);
 emu10k1_writeptr(card, SOLEH, 0, 0);

 // disable audio and lock cache
 outl(card->iobase+HCFG,HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE_MASK | HCFG_MUTEBUTTONENABLE);
 emu10k1_writeptr(card, PTB, 0, 0);
}

//--------------------------------------------------------------------------
// mixer (FX)
static void snd_emu_set_spdif_freq(struct emu10k1_card *card,unsigned long freq)
{
 uint32_t tmp=emu10k1_readptr(card,A_SPDIF_SAMPLERATE,0) & (~A_SPDIF_RATE_MASK);
 switch(freq){
  case 44100 :tmp|= A_SPDIF_44100;break;
  case 96000 :tmp|= A_SPDIF_96000;break;
  case 192000:tmp|= A_SPDIF_192000;break;
  default    :tmp|= A_SPDIF_48000;break;
 }
 emu10k1_writeptr(card, A_SPDIF_SAMPLERATE, 0, tmp);
}

static unsigned int snd_emu_ac97_read(struct emu10k1_card *card, unsigned int reg)
{
 outb(card->iobase + AC97ADDRESS, reg);
 return inw(card->iobase + AC97DATA);
}

static void snd_emu_ac97_write(struct emu10k1_card *card,unsigned int reg, unsigned int value)
{
 outb(card->iobase + AC97ADDRESS, reg);
 outw(card->iobase + AC97DATA, value);
 //outb(iobase + AC97ADDRESS, AC97_EXTENDED_ID); // ???
}

static void snd_emu_ac97_mute(struct emu10k1_card *card,unsigned int reg)
{
 uint32_t tmp=snd_emu_ac97_read(card,reg);
 tmp|=AC97_MUTE;
 snd_emu_ac97_write(card,reg,tmp);
}

/*static void snd_emu_ac97_unmute(struct emu10k1_card *card,unsigned int reg)
{
 uint32_t tmp=snd_emu_ac97_read(card,reg);
 tmp&=~AC97_MUTE;
 snd_emu_ac97_write(card,reg,tmp);
}*/

static void snd_emu_ac97_init(struct emu10k1_card *card)
{
 snd_emu_ac97_write(card, AC97_RESET, 0); // resets the volumes too
 snd_emu_ac97_read(card, AC97_RESET);

 // initial ac97 volumes (and clear mute flag)
 snd_emu_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
 snd_emu_ac97_write(card, AC97_SURROUND_MASTER,   0x0202);
 snd_emu_ac97_write(card, AC97_PCMOUT_VOL,        0x0202);
 snd_emu_ac97_write(card, AC97_HEADPHONE_VOL,     0x0202);

 /*snd_emu_ac97_unmute(card, AC97_MASTER_VOL_STEREO);
 snd_emu_ac97_unmute(card, AC97_SURROUND_MASTER);
 snd_emu_ac97_unmute(card, AC97_PCMOUT_VOL);
 snd_emu_ac97_unmute(card, AC97_HEADPHONE_VOL);*/

 snd_emu_ac97_write(card, AC97_EXTENDED_STATUS, AC97_EA_SPDIF);
}

#ifndef AUDIGY1_USE_AC97
static unsigned int snd_emu10kx_read_control_gpr(struct emu10k1_card *card, unsigned int addr)
{
 uint32_t retval;
 if(card->chips&EMU_CHIPS_10K2)
  retval=emu10k1_readptr(card, A_FXGPREGBASE + addr, 0);
 else
  retval=emu10k1_readptr(card, FXGPREGBASE + addr, 0);
 return retval;
}
#endif

static void snd_emu10kx_set_control_gpr(struct emu10k1_card *card, unsigned int addr, unsigned int val)
{
 if(card->chips&EMU_CHIPS_10K2)
  emu10k1_writeptr(card, A_FXGPREGBASE + addr, 0, val);
 else
  emu10k1_writeptr(card, FXGPREGBASE + addr, 0, val);
}

static void snd_emu10kx_fx_init(struct emu10k1_card *card)
{
 unsigned int i, pc = 0;

 if(card->chips&EMU_CHIPS_10K2){ // Audigy
  emu10k1_writeptr(card, A_DBG, 0, A_DBG_SINGLE_STEP); // stop fx

  for (i = 0; i < 512 ; i++)
   emu10k1_writeptr(card, A_FXGPREGBASE+i,0,0);  // clear gpr

#ifdef AUDIGY1_USE_AC97
  if(card->chiprev!=4){ // Audigy1
   // ac97 front
   A_OP(iACC3, A_EXTOUT(A_EXTOUT_AC97_L), A_C_00000000, A_C_00000000, A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iACC3, A_EXTOUT(A_EXTOUT_AC97_R), A_C_00000000, A_C_00000000, A_FXBUS(FXBUS_PCM_RIGHT));
  }else
#endif
  {
   // Front Output + Master Volume
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_AFRONT_L), 0xc0, A_GPR(8), A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_AFRONT_R), 0xc0, A_GPR(9), A_FXBUS(FXBUS_PCM_RIGHT));

   // Digital Front + Master Volume
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_FRONT_L),  0xc0, A_GPR(8), A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_FRONT_R),  0xc0, A_GPR(9), A_FXBUS(FXBUS_PCM_RIGHT));

   // Audigy Drive, Headphone out + Master Volume
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_HEADPHONE_L),0xc0,A_GPR(8),A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_HEADPHONE_R),0xc0,A_GPR(9),A_FXBUS(FXBUS_PCM_RIGHT));

   // Rear output + Master Volume
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_AREAR_L),  0xc0, A_GPR(8), A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_AREAR_R),  0xc0, A_GPR(9), A_FXBUS(FXBUS_PCM_RIGHT));

   // Digital Rear + Master Volume
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_REAR_L),   0xc0, A_GPR(8), A_FXBUS(FXBUS_PCM_LEFT));
   A_OP(iMAC0, A_EXTOUT(A_EXTOUT_REAR_R),   0xc0, A_GPR(9), A_FXBUS(FXBUS_PCM_RIGHT));
  }

  for( ; pc<1024; pc++)
   A_OP(0xf, 0xc0, 0xc0, 0xcf, 0xc0);

  emu10k1_writeptr(card, A_DBG, 0, 0); // start fx

#ifdef AUDIGY1_USE_AC97
  if(card->chiprev!=4) // Audigy1
   snd_emu_ac97_init(card);
  else
#endif
  {
   //Master volume
   i=((float)AUDIGY_PCMVOLUME_DEFAULT*(float)0x7fffffff+50.0)/100.0;
   if(i>0x7fffffff)
    i=0x7fffffff;
   snd_emu10kx_set_control_gpr(card,8,i);
   snd_emu10kx_set_control_gpr(card,9,i);

   snd_emu_ac97_mute(card,AC97_MASTER_VOL_STEREO); // for Audigy, we mute ac97 and use the philips 6 channel DAC instead
  }

 }else{ // SB Live
  emu10k1_writeptr(card, DBG, 0, EMU10K1_DBG_SINGLE_STEP); // stop fx

  for (i = 0; i < 256; i++)
   emu10k1_writeptr(card,FXGPREGBASE + i, 0, 0);

  // ac97 analog front
  L_OP(iACC3, EXTOUT(EXTOUT_AC97_L), 0x40, 0x40, FXBUS(FXBUS_PCM_LEFT));
  L_OP(iACC3, EXTOUT(EXTOUT_AC97_R), 0x40, 0x40, FXBUS(FXBUS_PCM_RIGHT));

  // ac97 analog rear
  L_OP(iACC3, EXTOUT(EXTOUT_AC97_REAR_L), 0x40, 0x40, FXBUS(FXBUS_PCM_LEFT));
  L_OP(iACC3, EXTOUT(EXTOUT_AC97_REAR_R), 0x40, 0x40, FXBUS(FXBUS_PCM_RIGHT));

  // digital out
  L_OP(iACC3, EXTOUT(EXTOUT_TOSLINK_L), 0x40, 0x40, FXBUS(FXBUS_PCM_LEFT));
  L_OP(iACC3, EXTOUT(EXTOUT_TOSLINK_R), 0x40, 0x40, FXBUS(FXBUS_PCM_RIGHT));

  // Livedrive, headphone out
  L_OP(iACC3, EXTOUT(EXTOUT_HEADPHONE_L), 0x40, 0x40, FXBUS(FXBUS_PCM_LEFT));
  L_OP(iACC3, EXTOUT(EXTOUT_HEADPHONE_R), 0x40, 0x40, FXBUS(FXBUS_PCM_RIGHT));

  for( ; pc < 512 ; pc++)
   L_OP(iACC3, 0x40, 0x40, 0x40, 0x40);

  emu10k1_writeptr(card, DBG, 0, 0); // start fx

  snd_emu_ac97_init(card); // for the Live we use ac97
 }
}

//--------------------------------------------------------------------------
// emu10kx (k1,k2) routines
static void emu10k1_clear_stop_on_loop(struct emu10k1_card *card, uint32_t voicenum)
{
 if (voicenum >= 32)
  emu10k1_writeptr(card, SOLEH | ((0x0100 | (voicenum - 32)) << 16), 0, 0);
 else
  emu10k1_writeptr(card, SOLEL | ((0x0100 | voicenum) << 16), 0, 0);
}

static uint32_t emu10k1_srToPitch(uint32_t sampleRate)
{
 static uint32_t logMagTable[128] = {
  0x00000, 0x02dfc, 0x05b9e, 0x088e6, 0x0b5d6, 0x0e26f, 0x10eb3, 0x13aa2,
  0x1663f, 0x1918a, 0x1bc84, 0x1e72e, 0x2118b, 0x23b9a, 0x2655d, 0x28ed5,
  0x2b803, 0x2e0e8, 0x30985, 0x331db, 0x359eb, 0x381b6, 0x3a93d, 0x3d081,
  0x3f782, 0x41e42, 0x444c1, 0x46b01, 0x49101, 0x4b6c4, 0x4dc49, 0x50191,
  0x5269e, 0x54b6f, 0x57006, 0x59463, 0x5b888, 0x5dc74, 0x60029, 0x623a7,
  0x646ee, 0x66a00, 0x68cdd, 0x6af86, 0x6d1fa, 0x6f43c, 0x7164b, 0x73829,
  0x759d4, 0x77b4f, 0x79c9a, 0x7bdb5, 0x7dea1, 0x7ff5e, 0x81fed, 0x8404e,
  0x86082, 0x88089, 0x8a064, 0x8c014, 0x8df98, 0x8fef1, 0x91e20, 0x93d26,
  0x95c01, 0x97ab4, 0x9993e, 0x9b79f, 0x9d5d9, 0x9f3ec, 0xa11d8, 0xa2f9d,
  0xa4d3c, 0xa6ab5, 0xa8808, 0xaa537, 0xac241, 0xadf26, 0xafbe7, 0xb1885,
  0xb3500, 0xb5157, 0xb6d8c, 0xb899f, 0xba58f, 0xbc15e, 0xbdd0c, 0xbf899,
  0xc1404, 0xc2f50, 0xc4a7b, 0xc6587, 0xc8073, 0xc9b3f, 0xcb5ed, 0xcd07c,
  0xceaec, 0xd053f, 0xd1f73, 0xd398a, 0xd5384, 0xd6d60, 0xd8720, 0xda0c3,
  0xdba4a, 0xdd3b4, 0xded03, 0xe0636, 0xe1f4e, 0xe384a, 0xe512c, 0xe69f3,
  0xe829f, 0xe9b31, 0xeb3a9, 0xecc08, 0xee44c, 0xefc78, 0xf148a, 0xf2c83,
  0xf4463, 0xf5c2a, 0xf73da, 0xf8b71, 0xfa2f0, 0xfba57, 0xfd1a7, 0xfe8df
 };

 static char logSlopeTable[128] = {
  0x5c, 0x5c, 0x5b, 0x5a, 0x5a, 0x59, 0x58, 0x58,
  0x57, 0x56, 0x56, 0x55, 0x55, 0x54, 0x53, 0x53,
  0x52, 0x52, 0x51, 0x51, 0x50, 0x50, 0x4f, 0x4f,
  0x4e, 0x4d, 0x4d, 0x4d, 0x4c, 0x4c, 0x4b, 0x4b,
  0x4a, 0x4a, 0x49, 0x49, 0x48, 0x48, 0x47, 0x47,
  0x47, 0x46, 0x46, 0x45, 0x45, 0x45, 0x44, 0x44,
  0x43, 0x43, 0x43, 0x42, 0x42, 0x42, 0x41, 0x41,
  0x41, 0x40, 0x40, 0x40, 0x3f, 0x3f, 0x3f, 0x3e,
  0x3e, 0x3e, 0x3d, 0x3d, 0x3d, 0x3c, 0x3c, 0x3c,
  0x3b, 0x3b, 0x3b, 0x3b, 0x3a, 0x3a, 0x3a, 0x39,
  0x39, 0x39, 0x39, 0x38, 0x38, 0x38, 0x38, 0x37,
  0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x36, 0x35,
  0x35, 0x35, 0x35, 0x34, 0x34, 0x34, 0x34, 0x34,
  0x33, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
  0x32, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x30,
  0x30, 0x30, 0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f
 };

 int i;

 if (sampleRate == 0)
  return 0;

 sampleRate *= 11185;    // Scale 48000 to 0x20002380

 for (i = 31; i > 0; i--) {
  if (sampleRate & 0x80000000) {
   return (uint32_t) (((int32_t) (i - 15) << 20)
                      +logMagTable[0x7f & (sampleRate >> 24)]
                      + (0x7f & (sampleRate >> 17)) * (int)logSlopeTable[0x7f & (sampleRate >> 24)]);
  }
  sampleRate = sampleRate << 1;
 }

 return 0;  // Should never reach this point
}

static uint32_t emu10k1_samplerate_to_linearpitch(uint32_t samplingrate)
{
 samplingrate = (samplingrate << 8) / 375;
 return (samplingrate >> 1) + (samplingrate & 1);
}

#define PITCH_48000 0x00004000
#define PITCH_96000 0x00008000
#define PITCH_85000 0x00007155
#define PITCH_80726 0x00006ba2
#define PITCH_67882 0x00005a82
#define PITCH_57081 0x00004c1c

static uint32_t emu10k1_select_interprom(struct emu10k1_card *card, unsigned int pitch_target)
{
 if(pitch_target==PITCH_48000)
  return CCCA_INTERPROM_0;
 if(pitch_target<PITCH_48000)
  return CCCA_INTERPROM_1;
 if(pitch_target>=PITCH_96000)
  return CCCA_INTERPROM_0;
 if(pitch_target>=PITCH_85000)
  return CCCA_INTERPROM_6;
 if(pitch_target>=PITCH_80726)
  return CCCA_INTERPROM_5;
 if(pitch_target>=PITCH_67882)
  return CCCA_INTERPROM_4;
 if(pitch_target>=PITCH_57081)
  return CCCA_INTERPROM_3;
 return CCCA_INTERPROM_2;
}

static void emu10k1_pcm_init_voice(struct emu10k1_card *card,
                                       unsigned int voice, unsigned int flags,
                       unsigned int start_addr,
                       unsigned int end_addr)
{
 unsigned int ccca_start = 0;
 uint32_t silent_page;
 int vol_left, vol_right;

 if(flags&VOICE_FLAGS_STEREO){
  start_addr >>= 1;
  end_addr >>= 1;
 }
 if(flags&VOICE_FLAGS_16BIT){
  start_addr >>= 1;
  end_addr >>= 1;
 }

 vol_left = vol_right = 255;
 if(flags&VOICE_FLAGS_STEREO){
  if(flags&VOICE_FLAGS_MASTER)
   vol_right = 0;
  else
   vol_left = 0;
 }

 emu10k1_writeptr(card,DCYSUSV, voice, 0);
 emu10k1_writeptr(card,VTFT, voice, 0xffff);
 emu10k1_writeptr(card,CVCF, voice, 0xffff);
 // Stop CA
 // assumption that PT is already 0 so no harm overwritting ???
 emu10k1_writeptr(card,PTRX, voice, (vol_left << 8) | vol_right);
 if(flags&VOICE_FLAGS_MASTER) {
  unsigned int ccis;
  if(flags&VOICE_FLAGS_STEREO){
   emu10k1_writeptr(card, CPF, voice    , CPF_STEREO_MASK);
   emu10k1_writeptr(card, CPF, voice + 1, CPF_STEREO_MASK);
   ccis=28;
  }else{
   emu10k1_writeptr(card, CPF, voice, 0);
   ccis=30;
  }
  if(flags&VOICE_FLAGS_16BIT)
   ccis *= 2;
  ccca_start  = start_addr + ccis;
  //ccca_start |= CCCA_INTERPROM_0;
  ccca_start |= emu10k1_select_interprom(card,card->voice_pitch_target);
  ccca_start |= (flags&VOICE_FLAGS_16BIT)? 0 : CCCA_8BITSELECT;
 }
 emu10k1_writeptr(card,DSL,  voice, end_addr);// | (vol_right<<24)); // ???
 emu10k1_writeptr(card,PSST, voice, start_addr);// | (vol_left<<24)); // ???
 emu10k1_writeptr(card,CCCA, voice, ccca_start);
 // Clear filter delay memory
 emu10k1_writeptr(card,Z1, voice, 0);
 emu10k1_writeptr(card,Z2, voice, 0);
 // invalidate maps
 silent_page = (((uint32_t)card->silentpage) << 1) | MAP_PTI_MASK;
 emu10k1_writeptr(card,MAPA, voice, silent_page);
 emu10k1_writeptr(card,MAPB, voice, silent_page);
 // modulation envelope
 emu10k1_writeptr(card, CVCF,    voice, 0xffff);     // ???
 emu10k1_writeptr(card, VTFT,    voice, 0xffff);     // ???
 emu10k1_writeptr(card, ATKHLDM, voice, 0x7f00);     // was 0
 emu10k1_writeptr(card, DCYSUSM, voice, 0);          // was DCYSUSM_DECAYTIME_MASK
 emu10k1_writeptr(card, LFOVAL1, voice, 0x8000);
 emu10k1_writeptr(card, LFOVAL2, voice, 0x8000);
 emu10k1_writeptr(card, FMMOD,   voice, 0x0000);     // (may be 0x7000)
 emu10k1_writeptr(card, TREMFRQ, voice, 0);
 emu10k1_writeptr(card, FM2FRQ2, voice, 0);
 emu10k1_writeptr(card, ENVVAL,  voice, 0xefff);     // was 0x8000
 // volume envelope
 emu10k1_writeptr(card, ATKHLDV, voice, ATKHLDV_HOLDTIME_MASK | ATKHLDV_ATTACKTIME_MASK);
 emu10k1_writeptr(card, ENVVOL,  voice, 0x8000);
 // filter envelope
 emu10k1_writeptr(card, PEFE_FILTERAMOUNT, voice, 0); // was 0x7f
 // pitch envelope
 emu10k1_writeptr(card, PEFE_PITCHAMOUNT, voice, 0);
}

static void snd_emu10k1_playback_start_voice(struct emu10k1_card *card,
                                               int voice, int flags)
{
 emu10k1_writeptr(card, IFATN, voice, IFATN_FILTERCUTOFF_MASK);
 emu10k1_writeptr(card, VTFT, voice,  0xffff); // ???
 emu10k1_writeptr(card, CVCF, voice,  0xffff); // ???
 emu10k1_clear_stop_on_loop(card, voice);      // ???
 emu10k1_writeptr(card, DCYSUSV, voice, (DCYSUSV_CHANNELENABLE_MASK|0x7f00)); // was 0x7f7f
 emu10k1_writeptr(card, PTRX_PITCHTARGET, voice, card->voice_pitch_target);
 if(flags&VOICE_FLAGS_MASTER) // ???
  emu10k1_writeptr(card, CPF_CURRENTPITCH, voice, card->voice_pitch_target);
 emu10k1_writeptr(card, IP, voice, card->voice_initial_pitch);
}

static void snd_emu10k1_playback_stop_voice(struct emu10k1_card *card, int voice)
{
 emu10k1_writeptr(card,IP, voice, 0);
 emu10k1_writeptr(card,CPF_CURRENTPITCH, voice, 0);
}

static void snd_emu10k1_playback_invalidate_cache(struct emu10k1_card *card, int voice, int flags)
{
 unsigned int i, ccis, cra = 64, cs, sample;

 if(flags&VOICE_FLAGS_STEREO){
  ccis = 28;
  cs = 4;
 }else{
  ccis = 30;
  cs = 2;
 }
 if(flags&VOICE_FLAGS_16BIT)
  sample=0x00000000;
 else{
  sample=0x80808080;
  ccis *= 2;
 }
 for(i = 0; i < cs; i++){
  emu10k1_writeptr(card, CD0 + i, voice, sample);
  if(flags&VOICE_FLAGS_STEREO)
   emu10k1_writeptr(card, CD0 + i, voice + 1, sample);
 }
 // reset cache
 emu10k1_writeptr(card, CCR_CACHEINVALIDSIZE, voice, 0);
 if(flags&VOICE_FLAGS_STEREO)
  emu10k1_writeptr(card, CCR_CACHEINVALIDSIZE, voice + 1, 0);

 emu10k1_writeptr(card, CCR_READADDRESS, voice, cra);
 if(flags&VOICE_FLAGS_STEREO)
  emu10k1_writeptr(card, CCR_READADDRESS, voice + 1, cra);
 // fill cache
 emu10k1_writeptr(card, CCR_CACHEINVALIDSIZE, voice, ccis);
 if(flags&VOICE_FLAGS_STEREO)
  emu10k1_writeptr(card, CCR_CACHEINVALIDSIZE, voice + 1, ccis);
}

//------------------------------------------------------------------------
//emu10kx api (SB Live 5.1, SB Audigy 1,2,4)
static unsigned int snd_emu10k1_selector(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 if(card->chips&EMU_CHIPS_10K1)
  return 1;
 return 0;
}

static unsigned int snd_emu10k2_selector(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 if((card->chips&EMU_CHIPS_10K2) && ((aui->bits_set<=16) || !(card->chips&EMU_CHIPS_0151))){
  funcbit_disable(card->chip_select,EMU_CHIPS_0151);
  return 1;
 }
 return 0;
}

static unsigned int snd_emu10kx_buffer_init(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 uint32_t pagecount,pcmbufp;

 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,EMUPAGESIZE,2,0);
 card->dm=MDma_alloc_cardmem( MAXPAGES*sizeof(uint32_t)       // virtualpage
                            +EMUPAGESIZE                     // silentpage
                            +card->pcmout_bufsize            // pcm output
                            +0x1000 );                       // to round

 card->silentpage=(void *)(((uint32_t)card->dm->linearptr+0x0fff)&0xfffff000); // buffer begins on page boundary
 card->virtualpagetable=(uint32_t *)((uint32_t)card->silentpage+EMUPAGESIZE);
 card->pcmout_buffer=(char *)(card->virtualpagetable+MAXPAGES);

 pcmbufp=(uint32_t)card->pcmout_buffer;
 pcmbufp<<=1;
 for(pagecount = 0; pagecount < (card->pcmout_bufsize/EMUPAGESIZE); pagecount++){
  card->virtualpagetable[pagecount] = pcmbufp | pagecount;
  pcmbufp+=EMUPAGESIZE*2;
 }
 for( ; pagecount<MAXPAGES; pagecount++)
  card->virtualpagetable[pagecount] = ((uint32_t)card->silentpage)<<1;

 return 1;
}

static void snd_emu10kx_setrate(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int dmabufsize;

 aui->chan_card=2;
 aui->bits_card=16;

 if(aui->freq_card<4000)
  aui->freq_card=4000;
 else{
  unsigned int limit=(card->chips&EMU_CHIPS_10K2)? 96000:48000;
  if(aui->freq_card>limit)
   aui->freq_card=limit;
 }

 dmabufsize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,EMUPAGESIZE,0);

 if(aui->freq_card==44100)
  snd_emu_set_spdif_freq(card,44100);
 else
  if(aui->freq_card<=48000)
   snd_emu_set_spdif_freq(card,48000);
  else
   snd_emu_set_spdif_freq(card,96000);

 card->voice_initial_pitch = emu10k1_srToPitch(aui->freq_card) >> 8;
 card->voice_pitch_target  = emu10k1_samplerate_to_linearpitch(aui->freq_card);

 emu10k1_pcm_init_voice(card,0,VOICE_FLAGS_MASTER|VOICE_FLAGS_STEREO|VOICE_FLAGS_16BIT,0,dmabufsize);
 emu10k1_pcm_init_voice(card,1,VOICE_FLAGS_STEREO|VOICE_FLAGS_16BIT,0,dmabufsize);
}

static void snd_emu10kx_pcm_start_playback(struct emu10k1_card *card)
{
 snd_emu10k1_playback_start_voice(card,0,VOICE_FLAGS_MASTER);
 snd_emu10k1_playback_start_voice(card,1,0);
}

static void snd_emu10kx_pcm_stop_playback(struct emu10k1_card *card)
{
 snd_emu10k1_playback_stop_voice(card,0);
 snd_emu10k1_playback_stop_voice(card,1);
}

static unsigned int snd_emu10kx_pcm_pointer_playback(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 return emu10k1_readptr(card,CCCA_CURRADDR,0);
}

static void snd_emu10kx_clear_cache(struct emu10k1_card *card)
{
 snd_emu10k1_playback_invalidate_cache(card,0,VOICE_FLAGS_STEREO|VOICE_FLAGS_16BIT);
}

static struct emu_driver_func_s emu_driver_10k1_funcs={
 &snd_emu10k1_selector,
 &snd_emu10k1_hw_init,
 &snd_emu10k1_hw_close,
 &snd_emu10kx_buffer_init,
 &snd_emu10kx_setrate,
 &snd_emu10kx_pcm_start_playback,
 &snd_emu10kx_pcm_stop_playback,
 &snd_emu10kx_pcm_pointer_playback,
 &snd_emu10kx_clear_cache,
 &snd_emu_ac97_read,
 &snd_emu_ac97_write,
 &mpxplay_aucards_ac97chan_mixerset[0]
};

#ifndef AUDIGY1_USE_AC97
static aucards_onemixerchan_s emu_10k2_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{
  {8,0x7fffffff,0,0},
  {9,0x7fffffff,0,0}
 }
};

aucards_allmixerchan_s emu_10k2_mixerset[]={
 &emu_10k2_master_vol,
 NULL
};

#endif

static struct emu_driver_func_s emu_driver_10k2_funcs={
 &snd_emu10k2_selector,
 &snd_emu10k1_hw_init,
 &snd_emu10k1_hw_close,
 &snd_emu10kx_buffer_init,
 &snd_emu10kx_setrate,
 &snd_emu10kx_pcm_start_playback,
 &snd_emu10kx_pcm_stop_playback,
 &snd_emu10kx_pcm_pointer_playback,
 &snd_emu10kx_clear_cache,
#ifdef AUDIGY1_USE_AC97 // !!! it doesn't check the Audigy type here (A2,A4 will not sound)
 &snd_emu_ac97_read,
 &snd_emu_ac97_write,
 &mpxplay_aucards_ac97chan_mixerset[0]
#else
 &snd_emu10kx_read_control_gpr,
 &snd_emu10kx_set_control_gpr,
 &emu_10k2_mixerset[0]
#endif
};

//--------------------------------------------------------------------------
//p16v api (Audigy 2 & 4  24-bit output)
#define AUDIGY2_P16V_PERIODS   8 // max
#define AUDIGY2_P16V_MAX_CHANS 8 // used only 2 yet
#define AUDIGY2_P16V_BYTES_PER_SAMPLE 4 // constant
#define AUDIGY2_P16V_DMABUF_ALIGN (AUDIGY2_P16V_PERIODS*AUDIGY2_P16V_MAX_CHANS*AUDIGY2_P16V_BYTES_PER_SAMPLE) // 256

static unsigned int snd_p16v_selector(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 if((card->chips&EMU_CHIPS_0151) && ((aui->bits_set>16) || !(card->chips&EMU_CHIPS_10KX))){
  funcbit_disable(card->chip_select,EMU_CHIPS_10KX);
  return 1;
 }
 return 0;
}

static unsigned int snd_p16v_buffer_init(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,AUDIGY2_P16V_DMABUF_ALIGN,AUDIGY2_P16V_BYTES_PER_SAMPLE,0);
 card->dm=MDma_alloc_cardmem(AUDIGY2_P16V_PERIODS*2*sizeof(uint32_t)+card->pcmout_bufsize);
 card->virtualpagetable=(uint32_t *)card->dm->linearptr;
 card->pcmout_buffer=((char *)card->virtualpagetable)+AUDIGY2_P16V_PERIODS*2*sizeof(uint32_t);
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"buffer init: pagetable:%8.8X pcmoutbuf:%8.8X size:%d",(unsigned long)card->virtualpagetable,(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
 return 1;
}

static void snd_p16v_pcm_prepare_playback(struct emu10k1_card *card,unsigned int freq)
{
 uint32_t *table_base =card->virtualpagetable;
 uint32_t period_size_bytes=card->period_size;
 const uint32_t channel=0;
 uint32_t i;

 snd_emu_set_spdif_freq(card,freq);

 for(i=0; i<AUDIGY2_P16V_PERIODS; i++) {
  table_base[i*2]=(uint32_t)((char *)card->pcmout_buffer+(i*period_size_bytes));
  table_base[i*2+1]=period_size_bytes<<16;
 }

 emu10k1_ptr20_write(card, PLAYBACK_LIST_ADDR, channel, (uint32_t)(table_base));
 emu10k1_ptr20_write(card, PLAYBACK_LIST_SIZE, channel, (AUDIGY2_P16V_PERIODS - 1) << 19);
 emu10k1_ptr20_write(card, PLAYBACK_LIST_PTR, channel, 0);
 emu10k1_ptr20_write(card, PLAYBACK_DMA_ADDR, channel, (uint32_t)card->pcmout_buffer);
 emu10k1_ptr20_write(card, PLAYBACK_PERIOD_SIZE, channel, period_size_bytes<<16);
 emu10k1_ptr20_write(card, PLAYBACK_POINTER, channel, 0);
 emu10k1_ptr20_write(card, 0x07, channel, 0x0);
 emu10k1_ptr20_write(card, 0x08, channel, 0);
}

static void snd_p16v_setrate(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int dmabufsize;

 aui->chan_card=2;
 aui->bits_card=32;

 if(aui->freq_set==44100)     // forced 44.1k dac output
  aui->freq_card=44100;
 else
  if(aui->freq_card!=48000){
   if(aui->freq_card<=22050)
    aui->freq_card=48000;
   else
    if(aui->freq_card<=96000) // (44.1->96) because 44.1k dac out sounds bad
     aui->freq_card=96000;
    else
     aui->freq_card=192000;
  }

 dmabufsize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,AUDIGY2_P16V_DMABUF_ALIGN,0);
 card->period_size=(dmabufsize/AUDIGY2_P16V_PERIODS);
 mpxplay_debugf(SBL_DEBUG_OUTPUT,"buffer config: bufsize:%d period_size:%d",dmabufsize,card->period_size);

 snd_p16v_pcm_prepare_playback(card,aui->freq_card);
}

static void snd_p16v_pcm_start_playback(struct emu10k1_card *card)
{
 const uint32_t channel=0;
 emu10k1_ptr20_write(card, BASIC_INTERRUPT, 0, emu10k1_ptr20_read(card, BASIC_INTERRUPT, 0) | (0x1<<channel));
}

static void snd_p16v_pcm_stop_playback(struct emu10k1_card *card)
{
 const uint32_t channel=0;
 emu10k1_ptr20_write(card, BASIC_INTERRUPT, 0, emu10k1_ptr20_read(card, BASIC_INTERRUPT, 0) & (~(0x1<<channel)));
}

static unsigned int snd_p16v_pcm_pointer_playback(struct emu10k1_card *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int ptr,ptr1,ptr3,ptr4;
 const uint32_t channel=0;

 ptr3 = emu10k1_ptr20_read(card, PLAYBACK_LIST_PTR, channel);
 ptr1 = emu10k1_ptr20_read(card, PLAYBACK_POINTER, channel);
 ptr4 = emu10k1_ptr20_read(card, PLAYBACK_LIST_PTR, channel);
 if(ptr3!=ptr4)
  ptr1=emu10k1_ptr20_read(card, PLAYBACK_POINTER, channel);

 ptr4/=(2*sizeof(uint32_t));

 ptr=(ptr4*card->period_size)+ptr1;

 ptr/=aui->chan_card;
 ptr/=AUDIGY2_P16V_BYTES_PER_SAMPLE;

 return ptr;
}

static unsigned int snd_p16v_mixer_read(struct emu10k1_card *card,unsigned int reg)
{
 return emu10k1_ptr20_read(card,reg,0);
}

static void snd_p16v_mixer_write(struct emu10k1_card *card,unsigned int reg,unsigned int value)
{
 emu10k1_ptr20_write(card,reg,0,value);
}

static aucards_onemixerchan_s emu_p16v_analog_out_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,
 {
  {PLAYBACK_VOLUME_MIXER9 ,0xff,8 ,SUBMIXCH_INFOBIT_REVERSEDVALUE},// front left
  {PLAYBACK_VOLUME_MIXER9 ,0xff,0 ,SUBMIXCH_INFOBIT_REVERSEDVALUE} // front right
  //{PLAYBACK_VOLUME_MIXER10,0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // rear
  //{PLAYBACK_VOLUME_MIXER10,0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_onemixerchan_s emu_p16v_spdif_out_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_SPDIFOUT,AU_MIXCHANFUNC_VOLUME),2,
 {
  {PLAYBACK_VOLUME_MIXER7,0xff, 8,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // front
  {PLAYBACK_VOLUME_MIXER7,0xff, 0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
  //{PLAYBACK_VOLUME_MIXER8,0xff,24,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // rear
  //{PLAYBACK_VOLUME_MIXER8,0xff,16,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_allmixerchan_s emu_p16v_mixerset[]={
 &emu_p16v_analog_out_vol,
 &emu_p16v_spdif_out_vol,
 NULL
};

static struct emu_driver_func_s emu_driver_p16v_funcs={
 &snd_p16v_selector,
 &snd_emu10k1_hw_init,
 &snd_emu10k1_hw_close,
 &snd_p16v_buffer_init,
 &snd_p16v_setrate,
 &snd_p16v_pcm_start_playback,
 &snd_p16v_pcm_stop_playback,
 &snd_p16v_pcm_pointer_playback,
 NULL,
 &snd_p16v_mixer_read,
 &snd_p16v_mixer_write,
 &emu_p16v_mixerset[0]
};

//-------------------------------------------------------------------------
static pci_device_s creative_devices[]={
 {"Live!"         ,PCI_VENDOR_ID_CREATIVE,PCI_DEVICE_ID_CREATIVE_EMU10K1},
 {"Audigy"        ,PCI_VENDOR_ID_CREATIVE,PCI_DEVICE_ID_CREATIVE_AUDIGY},
 {"Audigy 2 value",PCI_VENDOR_ID_CREATIVE,0x0008}, // SB0400
 {"Live! 24bit"   ,PCI_VENDOR_ID_CREATIVE,0x0007}, // Live 24 , Audigy LS
 {NULL,0,0}
};

static emu_card_version_s emucard_versions[]={
 {"Audigy 4 [SB0610]"          ,0x0008,0,0x10211102,EMU_CHIPS_10K2|EMU_CHIPS_0108,8},
 {"Audigy 2 Value [SB0400]"    ,0x0008,0,0x10011102,EMU_CHIPS_10K2|EMU_CHIPS_0108,8},
 //{"Audigy 2 ZS Notebook [SB0530]",0x0008,0,0x20011102,EMU_CHIPS_10K2|EMU_CHIPS_0108,8},
 {"Audigy 2 Value [unknown]"   ,0x0008,0,0         ,EMU_CHIPS_10K2|EMU_CHIPS_0108,6},
 //{"E-mu 1212m [4001]"          ,0x0004,0,0x40011102,EMU_CHIPS_10K2|EMU_CHIPS_0102,6},

 {"Audigy 4 PRO [SB0380]"      ,0x0004,0,0x20071102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},
 {"Audigy 2 [SB0350b]"         ,0x0004,0,0x20061102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},
 {"Audigy 2 ZS [SB0350]"       ,0x0004,0,0x20021102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},
 {"Audigy 2 ZS [SB0360]"       ,0x0004,0,0x20011102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},
 {"Audigy 2 [SB0240]"          ,0x0004,0,0x10071102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,7},//??? 6.1,7.1
 {"Audigy 2 EX [SB0280]"       ,0x0004,0,0x10051102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,6},
 {"Audigy 2 ZS [SB0353]"       ,0x0004,0,0x10031102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},
 {"Audigy 2 Platinum [SB0240P]",0x0004,0,0x10021102,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,8},//??? 6.1,7.1
 {"Audigy 2 [unknown]"         ,0x0004,4,0         ,EMU_CHIPS_10K2|EMU_CHIPS_0102|EMU_CHIPS_0151,6},

 {"Audigy 1 [SB0092]"          ,0x0004,0,0x00531102,EMU_CHIPS_10K2|EMU_CHIPS_0102,6},
 {"Audigy 1 ES [SB0160]"       ,0x0004,0,0x00521102,EMU_CHIPS_10K2|EMU_CHIPS_0102,6},
 {"Audigy 1 [SB0090]"          ,0x0004,0,0x00511102,EMU_CHIPS_10K2|EMU_CHIPS_0102,6},
 {"Audigy 1 [unknown]"         ,0x0004,0,0         ,EMU_CHIPS_10K2|EMU_CHIPS_0102,6},

 {"Live! [SB0105]"             ,0x0002,0,0x806B1102,EMU_CHIPS_10K1,6},
 {"Live! Value [SB0103]"       ,0x0002,0,0x806A1102,EMU_CHIPS_10K1,6},
 {"Live! Value [SB0101]"       ,0x0002,0,0x80691102,EMU_CHIPS_10K1,6},
 {"Live 5.1 Dell OEM [SB0220]" ,0x0002,0,0x80661102,EMU_CHIPS_10K1,6},
 {"Live 5.1 [SB0220]"          ,0x0002,0,0x80651102,EMU_CHIPS_10K1,6},
 {"Live 5.1 [SB0220b]"         ,0x0002,0,0x100a1102,EMU_CHIPS_10K1,6},
 {"Live! 5.1"                  ,0x0002,0,0x80641102,EMU_CHIPS_10K1,6},
 {"Live! Player 5.1 [SB0060]"  ,0x0002,0,0x80611102,EMU_CHIPS_10K1,6},//??? no AC97
 {"Live! Value [CT4850]"       ,0x0002,0,0x80511102,EMU_CHIPS_10K1,6},
 {"Live! Platinum [CT4760P]"   ,0x0002,0,0x80401102,EMU_CHIPS_10K1,6},//??? 5.1
 {"Live! Value [CT4871]"       ,0x0002,0,0x80321102,EMU_CHIPS_10K1,6},
 {"Live! Value [CT4831]"       ,0x0002,0,0x80311102,EMU_CHIPS_10K1,6},
 {"Live! Value [CT4870]"       ,0x0002,0,0x80281102,EMU_CHIPS_10K1,6},
 {"Live! Value [CT4832]"       ,0x0002,0,0x80271102,EMU_CHIPS_10K1,6},//??? 5.1
 {"Live! Value [CT4830]"       ,0x0002,0,0x80261102,EMU_CHIPS_10K1,6},
 {"PCI512 [CT4790]"            ,0x0002,0,0x80231102,EMU_CHIPS_10K1,6},
 {"Live! Value [CT4780]"       ,0x0002,0,0x80221102,EMU_CHIPS_10K1,6},
 {"Live! [CT4620]"             ,0x0002,0,0x00211102,EMU_CHIPS_10K1,6},
 {"Live! Value [CT4670]"       ,0x0002,0,0x00201102,EMU_CHIPS_10K1,6},
 {"Live [unknown]"             ,0x0002,0,0         ,EMU_CHIPS_10K1,2},

 {"Audigy LS [SB0310]"         ,0x0007,0,0x10021102,EMU_CHIPS_0106,8},
 {"Audigy LS [SB0310b]"        ,0x0007,0,0x10051102,EMU_CHIPS_0106,8},
 {"Live! 7.1 24bit [SB0410]"   ,0x0007,0,0x10061102,EMU_CHIPS_0106,8},
 {"Live! 7.1 24bit [SB0413]"   ,0x0007,0,0x10071102,EMU_CHIPS_0106,8},
 {"Live24 (MSI K8N Diamond)"   ,0x0007,0,0x10091462,EMU_CHIPS_0106,8}, // SB0438
 {"Live24 (Shuttle XPC SD31P)" ,0x0007,0,0x30381297,EMU_CHIPS_0106,8},
 {"X-Fi Xtreme Audio [SB0790]" ,0x0007,0,0x10121102,EMU_CHIPS_0106,8},
 {"Live! 7.1 24bit [unknown]"  ,0x0007,0,0         ,EMU_CHIPS_0106,8},
 {NULL}
};

// from sc_sbl24.c
extern struct emu_driver_func_s emu_driver_audigyls_funcs;
extern struct emu_driver_func_s emu_driver_live24_funcs;

static struct emu_driver_func_s *emu_driver_all_funcs[]={
 &emu_driver_10k1_funcs,
 &emu_driver_10k2_funcs,
 &emu_driver_p16v_funcs,
 &emu_driver_audigyls_funcs,
 &emu_driver_live24_funcs,
 NULL
};

static void SBLIVE_close(struct mpxplay_audioout_info_s *aui);
static void sblive_select_mixer(struct emu10k1_card *card);

static void SBLIVE_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"SBA : SB %s (%8.8X)(bits:16%s) on port:%4.4X irq:%d",
         ((card->card_capabilities->longname)? card->card_capabilities->longname:card->pci_dev->device_name),
         card->serial,((card->chips&EMU_CHIPS_24BIT)? ",24":""),
         (int)card->iobase,(int)card->irq);
 pds_textdisplay_printf(sout);
}

static int SBLIVE_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card;
 struct emu_card_version_s *emucv;
 struct emu_driver_func_s **edaf;

 card=(struct emu10k1_card *)pds_calloc(1,sizeof(struct emu10k1_card));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(&creative_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 pcibios_set_master(card->pci_dev);

 card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR)&0xfff0;
 if(!card->iobase)
  goto err_adetect;

 card->irq    = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
 card->chiprev= pcibios_ReadConfig_Byte(card->pci_dev, PCIR_RID);
 card->model  = pcibios_ReadConfig_Word(card->pci_dev, PCIR_SSID);
 card->serial = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_SSVID);

 emucv=&emucard_versions[0];
 do{
  if(emucv->device==card->pci_dev->device_id)
   if( (emucv->subsystem==card->serial)
    || (emucv->revision && (emucv->revision==card->chiprev))
    || (!emucv->revision && !emucv->subsystem) // unknown but supported card
   ){
    card->card_capabilities=emucv;
    break;
   }
  emucv++;
 }while(emucv->longname);

 if(!card->card_capabilities)
  goto err_adetect;

 card->chip_select=card->chips=card->card_capabilities->chips;

 edaf=&emu_driver_all_funcs[0];
 card->driver_funcs=*edaf;
 do{
  if(card->driver_funcs->selector(card,aui))
   break;
  edaf++;
  card->driver_funcs=*edaf;
 }while(card->driver_funcs);

 if(!card->driver_funcs)
  goto err_adetect;

 if(!card->driver_funcs->buffer_init(card,aui))
  goto err_adetect;

 aui->card_DMABUFF=card->pcmout_buffer;

 if(card->driver_funcs->hw_init)
  card->driver_funcs->hw_init(card);

 sblive_select_mixer(card);

 return 1;

err_adetect:
 SBLIVE_close(aui);
 return 0;
}

static void SBLIVE_close(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;
 if(card){
  if(card->iobase)
   if(card->driver_funcs->hw_close)
    card->driver_funcs->hw_close(card);
  MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void SBLIVE_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;

 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 if(card->driver_funcs->setrate)
  card->driver_funcs->setrate(card,aui);
}

static void SBLIVE_start(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;

 if(card->driver_funcs->start_playback)
  card->driver_funcs->start_playback(card);
}

static void SBLIVE_stop(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;

 if(card->driver_funcs->stop_playback)
  card->driver_funcs->stop_playback(card);
}

static long SBLIVE_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;
 unsigned long bufpos;

 if(card->driver_funcs->pcm_pointer_playback)
  bufpos=card->driver_funcs->pcm_pointer_playback(card,aui);
 else
  bufpos=0;

 bufpos*=aui->chan_card;
 bufpos*=aui->bits_card>>3;

 if(bufpos<aui->card_dmasize)
  aui->card_dma_lastgoodpos=bufpos;

 return aui->card_dma_lastgoodpos;
}

static void SBLIVE_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 struct emu10k1_card *card=aui->card_private_data;
 MDma_clearbuf(aui);
 if(card->driver_funcs->clear_cache)
  card->driver_funcs->clear_cache(card);
}

static void SBLIVE_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct emu10k1_card *card=aui->card_private_data;
 if(card->driver_funcs->mixer_write)
  card->driver_funcs->mixer_write(card,reg,val);
}

static unsigned long SBLIVE_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct emu10k1_card *card=aui->card_private_data;
 unsigned int retval;
 if(card->driver_funcs->mixer_read)
  retval=card->driver_funcs->mixer_read(card,reg);
 else
  retval=0;
 return retval;
}

one_sndcard_info SBLIVE_sndcard_info={
 "SBA",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                  // no init
 &SBLIVE_adetect,       // only autodetect
 &SBLIVE_card_info,
 &SBLIVE_start,
 &SBLIVE_stop,
 &SBLIVE_close,
 &SBLIVE_setrate,

 &MDma_writedata,
 &SBLIVE_getbufpos,
 &SBLIVE_clearbuf,
 &MDma_interrupt_monitor,
 NULL,

 &SBLIVE_writeMIXER,
 &SBLIVE_readMIXER,
 NULL
};

static void sblive_select_mixer(struct emu10k1_card *card)
{
 SBLIVE_sndcard_info.card_mixerchans=card->driver_funcs->mixerset;
}

#endif
