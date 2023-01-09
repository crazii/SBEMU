//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Creative X-Fi EMU20KX (Music,Gamer) handling
//based on ALSA (http://www.alsa-project.org)
// doesn't work yet

//#define MPXPLAY_USE_DEBUGF 1
//#define XFI_DEBUG_OUTPUT stdout

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_EMU20KX

#include "dmairq.h"
#include "pcibios.h"

#define EMU20KX_MAX_CHANNELS     8
#define EMU20KX_MAX_BYTES        4

#define EMU20KX_PAGESIZE     4096 // ???
#define EMU20KX_MAXPAGES     8192 // ???

//#define PCI_SUBDEVICE_ID_CREATIVE_SB0760    0x0024
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08801    0x0041
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08802    0x0042
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08803    0x0043
//#define PCI_SUBDEVICE_ID_CREATIVE_HENDRIX    0x6000

// sample formats
#define SRC_SF_U8    0x0
#define SRC_SF_S16    0x1
#define SRC_SF_S24    0x2
#define SRC_SF_S32    0x3
#define SRC_SF_F32    0x4

// EMU20K1 registers
#define    PTPAHX        0x13B000
#define    PTPALX        0x13B004
#define    TRNCTL        0x13B404
#define    TRNIS        0x13B408
#define    AMOPLO        0x140000
#define    AMOPHI        0x140004
#define    PMOPLO        0x148000
#define    PMOPHI        0x148004
#define    PRING_LO_HI    0x198000
#define    SRCCTL        0x1B0000
#define    SRCCCR        0x1B0004
#define    SRCCA        0x1B0010
#define    SRCCF        0x1B0014
#define    SRCSA        0x1B0018
#define    SRCLA        0x1B001C
#define    SRCMCTL        0x1B012C
#define    SRCIP        0x1B102C
#define    I2SCTL        0x1C5420
#define    SPOCTL        0x1C5480
#define    SPICTL        0x1C5484
#define    GIE        0x1C6014
#define    GPIO        0x1C6020
#define    GPIOCTL        0x1C6024
#define    PLLCTL        0x1C6060
#define    GCTL        0x1C6070

// EMU20K1 card initialization bits
#define GCTL_EAC    0x00000001
#define GCTL_EAI    0x00000002
#define GCTL_BEP    0x00000004
#define GCTL_BES    0x00000008
#define GCTL_DSP    0x00000010
#define GCTL_DBP    0x00000020
#define GCTL_ABP    0x00000040
#define GCTL_TBP    0x00000080
#define GCTL_SBP    0x00000100
#define GCTL_FBP    0x00000200
#define GCTL_XA        0x00000400
#define GCTL_ET        0x00000800
#define GCTL_PR        0x00001000
#define GCTL_MRL    0x00002000
#define GCTL_SDE    0x00004000
#define GCTL_SDI    0x00008000
#define GCTL_SM        0x00010000
#define GCTL_SR        0x00020000
#define GCTL_SD        0x00040000
#define GCTL_SE        0x00080000
#define GCTL_AID    0x00100000

// SRC resource control block
#define SRCCTL_STATE    0x00000007
#define SRCCTL_BM    0x00000008
#define SRCCTL_RSR    0x00000030
#define SRCCTL_SF    0x000001C0
#define SRCCTL_WR    0x00000200
#define SRCCTL_PM    0x00000400
#define SRCCTL_ROM    0x00001800
#define SRCCTL_VO    0x00002000
#define SRCCTL_ST    0x00004000
#define SRCCTL_IE    0x00008000
#define SRCCTL_ILSZ    0x000F0000
#define SRCCTL_BP    0x00100000

// SRCCTL_STATE
#define SRC_STATE_OFF    0x0
#define SRC_STATE_INIT    0x4

#define SRCCA_CA    0x03FFFFFF

// mixer defs

#define BLANK_SLOT        4094

#define INIT_VOL    0x1c00

#define AMIXER_Y_IMMEDIATE    1

#define AMOPLO_M    0x00000003 // amoplo mode
#define AMOPLO_X    0x0003FFF0
#define AMOPLO_Y    0xFFFC0000

#define AMOPHI_SADR    0x000000FF
#define AMOPHI_SE    0x80000000

enum CT_AMIXER_CTL{
 // volume control mixers
 AMIXER_MASTER_F,
 AMIXER_MASTER_R,
 AMIXER_MASTER_C,
 AMIXER_MASTER_S,
 AMIXER_PCM_F,
 AMIXER_PCM_R,
 AMIXER_PCM_C,
 AMIXER_PCM_S,
 AMIXER_SPDIFI,
 AMIXER_LINEIN,
 AMIXER_MIC,
 AMIXER_SPDIFO,
 AMIXER_WAVE_F,
 AMIXER_WAVE_R,
 AMIXER_WAVE_C,
 AMIXER_WAVE_S,
 AMIXER_MASTER_F_C,
 AMIXER_PCM_F_C,
 AMIXER_SPDIFI_C,
 AMIXER_LINEIN_C,
 AMIXER_MIC_C,
 // this should always be the last one
 NUM_CT_AMIXERS
};

enum MIXER_PORT_T {
 MIX_WAVE_FRONT,
 MIX_WAVE_REAR,
 MIX_WAVE_CENTLFE,
 MIX_WAVE_SURROUND,
 MIX_SPDIF_OUT,
 MIX_PCMO_FRONT,
 MIX_MIC_IN,
 MIX_LINE_IN,
 MIX_SPDIF_IN,
 MIX_PCMI_FRONT,
 MIX_PCMI_REAR,
 MIX_PCMI_CENTLFE,
 MIX_PCMI_SURROUND,

 NUM_MIX_PORTS
};

typedef struct emu20kx_card_s
{
 unsigned long   iobase;
 unsigned int    irq;
 unsigned int    subsys_id;
 struct pci_config_s  *pci_dev;

 dosmem_t *dm;
 char *pcmout_buffer;
 long pcmout_bufsize;
 uint32_t *virtualpagetable;
 void *silentpage;

 unsigned long rsr; // reference sampling rate (44100 or 48000)
 unsigned long last_rsr_cfg;
 unsigned int  msr; // multiply of rsr (1,2,4)
 unsigned int  last_msr_cfg;
 unsigned int  sfnum; // sample format (SRC_SF_XXX)
 unsigned int  max_cisz;
 unsigned int  src_idx;  // stream idx?
 unsigned int  src_ctl;

 unsigned long dac_output_freq; // DAC is set to this, other freqs are converted to this by card

}ensoniq_card_s;

//-------------------------------------------------------------------------
static inline uint32_t hw_read_20kx(struct emu20kx_card_s *card,uint32_t reg)
{
 outl(card->iobase + 0x0,reg);
 return ((uint32_t)inl(card->iobase + 0x4));
}

static inline void hw_write_20kx(struct emu20kx_card_s *card,uint32_t reg,uint32_t data)
{
 outl(card->iobase + 0x0,reg);
 outl(card->iobase + 0x4,data);
}

static inline uint32_t hw_read_pci(struct emu20kx_card_s *card,uint32_t reg)
{
 outl(card->iobase + 0x10,reg);
 return ((uint32_t)inl(card->iobase + 0x14));
}

static inline void hw_write_pci(struct emu20kx_card_s *card,uint32_t reg,uint32_t data)
{
 outl(card->iobase + 0x10,reg);
 outl(card->iobase + 0x14,data);
}

static unsigned int get_field(unsigned int data, unsigned int field)
{
 int i;
 for(i=0; !(field & (1 << i)); )
  i++;
 return ((data & field) >> i);
}

static void set_field(unsigned int *data, unsigned int field, unsigned int value)
{
 int i;
 for (i = 0; !(field & (1 << i)); )
  i++;
 *data = (*data & (~field)) | ((value << i) & field);
}

//-------------------------------------------------------------------------
#define AR_SLOT_SIZE        4096
#define AR_SLOT_BLOCK_SIZE    16
#define AR_PTS_PITCH        6
#define AR_PARAM_SRC_OFFSET    0x60

static unsigned int src_param_pitch_mixer(unsigned int src_idx)
{
 return ((src_idx << 4) + AR_PTS_PITCH + AR_SLOT_SIZE - AR_PARAM_SRC_OFFSET) % AR_SLOT_SIZE;
}

static unsigned int snd_emu20kx_get_pitch(unsigned int input_rate, unsigned int output_rate)
{
 unsigned int pitch = 0;
 int b = 0;

 // get pitch and convert to fixed-point 8.24 format
 pitch = (input_rate / output_rate) << 24;
 input_rate %= output_rate;
 input_rate /= 100;
 output_rate /= 100;
 for(b = 31; ((b >= 0) && !(input_rate >> b)); )
  b--;

 if(b >= 0) {
  input_rate <<= (31 - b);
  input_rate /= output_rate;
  b = 24 - (31 - b);
  if (b >= 0)
   input_rate <<= b;
  else
   input_rate >>= -b;

  pitch |= input_rate;
 }

 return pitch;
}

static int snd_emu20kx_select_rom(unsigned int pitch)
{
 if ((pitch > 0x00428f5c) && (pitch < 0x01b851ec)) { // 0.26 <= pitch <= 1.72
  return 1;
 }else if ((0x01d66666 == pitch) || (0x01d66667 == pitch)) { // pitch == 1.8375
  return 2;
 }else if (0x02000000 == pitch) { // pitch == 2
  return 3;
 }else if (pitch <= 0x08000000) { // 0 <= pitch <= 8
  return 0;
 }
 return -1;
}

// Map integer value ranging from 0 to 65535 to 14-bit float value ranging from 2^-6 to (1+1023/1024)
static unsigned int uint16_to_float14(unsigned int x)
{
 unsigned int i;

 if(x < 17)
  return 0;

 x *= 2031;
 x /= 65535;
 x += 16;

 for (i = 0; !(x & 0x400); i++)
  x <<= 1;

 x = (((7 - i) & 0x7) << 10) | (x & 0x3ff);

 return x;
}

static unsigned int float14_to_uint16(unsigned int x)
{
 unsigned int e;

 if(!x)
  return x;

 e = (x >> 10) & 0x7;
 x &= 0x3ff;
 x += 1024;
 x >>= (7 - e);
 x -= 16;
 x *= 65535;
 x /= 2031;

 return x;
}

//-------------------------------------------------------------------------
static int hw_pll_init(struct emu20kx_card_s *card)
{
 unsigned int i,pllctl;
 pllctl=(card->rsr==48000)? 0x1480a001 : 0x1480a731;
 for(i=0;i<3;i++){
  if(hw_read_20kx(card, PLLCTL) == pllctl)
   break;
  hw_write_20kx(card, PLLCTL, pllctl);
  pds_delay_10us(40*100);
 }
 if(i>=3)
  return -1;
 return 0;
}

static void hw_daio_init(struct emu20kx_card_s *card)
{
 uint32_t i2sorg,spdorg;

 /* Read I2S CTL.  Keep original value. */
 /*i2sorg = hw_read_20kx(hw, I2SCTL);*/
 i2sorg = 0x94040404; /* enable all audio out and I2S-D input */
 /* Program I2S with proper master sample rate and enable
  * the correct I2S channel. */
 i2sorg &= 0xfffffffc;

 /* Enable S/PDIF-out-A in fixed 24-bit data
  * format and default to 48kHz. */
 /* Disable all before doing any changes. */
 hw_write_20kx(card, SPOCTL, 0x0);
 spdorg = 0x05;

 switch(card->msr) {
  case 1:
   i2sorg |= 1;
   spdorg |= (0x0 << 6);
   break;
  case 2:
   i2sorg |= 2;
   spdorg |= (0x1 << 6);
   break;
  case 4:
   i2sorg |= 3;
   spdorg |= (0x2 << 6);
   break;
  default:
   i2sorg |= 1;
   break;
 }

 hw_write_20kx(card, I2SCTL, i2sorg);
 hw_write_20kx(card, SPOCTL, spdorg);

 /* Enable S/PDIF-in-A in fixed 24-bit data format. */
 /* Disable all before doing any changes. */
 hw_write_20kx(card, SPICTL, 0x0);
 pds_delay_10us(1*100);
 spdorg = 0x0a0a0a0a;
 hw_write_20kx(card, SPICTL, spdorg);
 pds_delay_10us(1*100);
}

static int i2c_unlock(struct emu20kx_card_s *card)
{
 if((hw_read_pci(card, 0xcc) & 0xff) == 0xaa)
  return 0;

 hw_write_pci(card, 0xcc, 0x8c);
 hw_write_pci(card, 0xcc, 0x0e);
 if((hw_read_pci(card, 0xcc) & 0xff) == 0xaa)
  return 0;

 hw_write_pci(card, 0xcc, 0xee);
 hw_write_pci(card, 0xcc, 0xaa);
 if((hw_read_pci(card, 0xcc) & 0xff) == 0xaa)
  return 0;

 return -1;
}

static void i2c_lock(struct emu20kx_card_s *card)
{
 if((hw_read_pci(card, 0xcc) & 0xff) == 0xaa)
  hw_write_pci(card, 0xcc, 0x00);
}

static void i2c_write(struct emu20kx_card_s *card, uint32_t device, uint32_t addr, uint32_t data)
{
 unsigned int ret = 0;

 do{
  ret = hw_read_pci(card, 0xEC);
 }while(!(ret & 0x800000));
 hw_write_pci(card, 0xE0, device);
 hw_write_pci(card, 0xE4, (data << 8) | (addr & 0xff));
}

/* DAC operations */

static int hw_reset_dac(struct emu20kx_card_s *card)
{
 uint32_t i = 0;
 uint16_t gpioorg = 0;
 unsigned int ret = 0;

 if(i2c_unlock(card))
  return -1;

 do{
  ret = hw_read_pci(card, 0xEC);
 }while(!(ret & 0x800000));
 hw_write_pci(card, 0xEC, 0x05);  /* write to i2c status control */

 /* To be effective, need to reset the DAC twice. */
 for(i = 0; i < 2;  i++){
  /* set gpio */
  pds_delay_10us(100*100);
  gpioorg = (uint16_t)hw_read_20kx(card, GPIO);
  gpioorg &= 0xfffd;
  hw_write_20kx(card, GPIO, gpioorg);
  pds_delay_10us(1);
  hw_write_20kx(card, GPIO, gpioorg | 0x2);
 }

 i2c_write(card, 0x00180080, 0x01, 0x80);
 i2c_write(card, 0x00180080, 0x02, 0x10);

 i2c_lock(card);

 return 0;
}

static int hw_dac_init(struct emu20kx_card_s *card)
{
 uint32_t data = 0;
 uint16_t gpioorg = 0;
 unsigned int ret = 0;

 if((card->subsys_id == 0x0022) || (card->subsys_id == 0x002F)) {
  /* SB055x, unmute outputs */
  gpioorg = (uint16_t)hw_read_20kx(card, GPIO);
  gpioorg &= 0xffbf;    /* set GPIO6 to low */
  gpioorg |= 2;        /* set GPIO1 to high */
  hw_write_20kx(card, GPIO, gpioorg);
  return 0;
 }

 /* mute outputs */
 gpioorg = (uint16_t)hw_read_20kx(card, GPIO);
 gpioorg &= 0xffbf;
 hw_write_20kx(card, GPIO, gpioorg);

 hw_reset_dac(card);

 if(i2c_unlock(card)<0)
  return -1;

 hw_write_pci(card, 0xEC, 0x05);  /* write to i2c status control */
 do {
  ret = hw_read_pci(card, 0xEC);
 } while (!(ret & 0x800000));

 switch(card->msr) {
  case 1:data = 0x24;break;
  case 2:data = 0x25;break;
  case 4:data = 0x26;break;
  default:data = 0x24;break;
 }

 i2c_write(card, 0x00180080, 0x06, data);
 i2c_write(card, 0x00180080, 0x09, data);
 i2c_write(card, 0x00180080, 0x0c, data);
 i2c_write(card, 0x00180080, 0x0f, data);

 i2c_lock(card);

 /* unmute outputs */
 gpioorg = (uint16_t)hw_read_20kx(card, GPIO);
 gpioorg = gpioorg | 0x40;
 hw_write_20kx(card, GPIO, gpioorg);

 return 0;
}

//-------------------------------------------------------------------------
/*#include "ac97_def.h"

#define     AC97D        0x1C5400
#define     AC97A        0x1C5404

static void emu20kx_ac97_write(struct emu20kx_card_s *card,unsigned int reg,unsigned int data)
{
 hw_write_20kx(card,AC97A,reg);
 hw_write_20kx(card,AC97D,data);
}

static void snd_emu20kx_ac97_init(struct emu20kx_card_s *card)
{
 emu20kx_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0404);
 emu20kx_ac97_write(card, AC97_PCMOUT_VOL,        0x0404);
 emu20kx_ac97_write(card, AC97_HEADPHONE_VOL,     0x0404);
 emu20kx_ac97_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
 mpxplay_debugf(XFI_DEBUG_OUTPUT,"ac97 init end");
}*/

//-------------------------------------------------------------------------

static void snd_emu20kx_set_output_format(struct emu20kx_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 switch(aui->card_wave_id){
  case MPXPLAY_WAVEID_PCM_FLOAT:card->sfnum=SRC_SF_F32;break;
  default:
   switch(aui->bits_card){
    case 32:card->sfnum=SRC_SF_S32;break;
    case 24:card->sfnum=SRC_SF_S24;break;
    default:card->sfnum=SRC_SF_S16;aui->bits_card=16;break;
   }
   aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 }

 if(aui->freq_card<3000)
  aui->freq_card=3000;
 else if((aui->freq_card>96000) && (aui->chan_set>2))
  aui->freq_card=96000;
 else if(aui->freq_card>192000)
  aui->freq_card=192000;

 if(aui->freq_card==44100){
  card->dac_output_freq=44100;
  card->rsr=44100;
  card->msr=1;
 }else if(aui->freq_card<=48000){
  card->dac_output_freq=48000;
  card->rsr=48000;
  card->msr=1;
 }else if(aui->freq_card<=96000){
  card->dac_output_freq=96000;
  card->rsr=48000;
  card->msr=2;
 }else{
  card->dac_output_freq=192000;
  card->rsr=48000;
  card->msr=4;
 }

 card->max_cisz=aui->chan_card*card->msr;
 card->max_cisz=128*((card->max_cisz<8)? card->max_cisz:8);
}


//-------------------------------------------------------------------------

static unsigned int snd_emu20kx_buffer_init(struct emu20kx_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int bytes_per_sample=((aui->bits_set>16) || (aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT))? 4:2;
 uint32_t pagecount,pcmbufp;

 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,EMU20KX_PAGESIZE,bytes_per_sample,0);
 card->dm=MDma_alloc_cardmem( EMU20KX_MAXPAGES*sizeof(uint32_t)// virtualpage
                            +EMU20KX_PAGESIZE                // silentpage
                            +card->pcmout_bufsize            // pcm output
                            +0x1000 );                       // to round

 card->silentpage=(void *)(((uint32_t)card->dm->linearptr+0x0fff)&0xfffff000); // buffer begins on page boundary
 card->virtualpagetable=(uint32_t *)((uint32_t)card->silentpage+EMU20KX_PAGESIZE);
 card->pcmout_buffer=(char *)(card->virtualpagetable+EMU20KX_MAXPAGES);

 pcmbufp=(uint32_t)card->pcmout_buffer;
 pcmbufp<<=1;
 for(pagecount = 0; pagecount < (card->pcmout_bufsize/EMU20KX_PAGESIZE); pagecount++){
  card->virtualpagetable[pagecount] = pcmbufp | pagecount;
  pcmbufp+=EMU20KX_PAGESIZE*2;
 }
 for( ; pagecount<EMU20KX_MAXPAGES; pagecount++)
  card->virtualpagetable[pagecount] = ((uint32_t)card->silentpage)<<1;

 aui->card_DMABUFF=card->pcmout_buffer;
 mpxplay_debugf(XFI_DEBUG_OUTPUT,"buffer init: pcmoutbuf:%8.8X size:%d",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
 return 1;
}

static unsigned int snd_emu20kx_chip_init(struct emu20kx_card_s *card)
{
 unsigned int i,gctl,trnctl,ctl_amoplo;

 // PLL init
 if(hw_pll_init(card)<0){
  mpxplay_debugf(XFI_DEBUG_OUTPUT,"chip_init: pll-init failed");
  return 0;
 }

 // auto-init
 gctl = hw_read_20kx(card, GCTL);
 set_field(&gctl, GCTL_EAI, 0);
 hw_write_20kx(card, GCTL, gctl);
 set_field(&gctl, GCTL_EAI, 1);
 hw_write_20kx(card, GCTL, gctl);
 pds_delay_10us(10*100);
 for(i = 0; i < 400000; i++) {
  gctl = hw_read_20kx(card, GCTL);
  if(get_field(gctl, GCTL_AID))
   break;
 }
 if(!get_field(gctl, GCTL_AID)){
  mpxplay_debugf(XFI_DEBUG_OUTPUT,"chip_init: auto-init failed %8.8X",gctl);
  return 0;
 }

 // Enable audio ring
 gctl = hw_read_20kx(card, GCTL);
 set_field(&gctl, GCTL_EAC, 1);
 set_field(&gctl, GCTL_DBP, 1);
 set_field(&gctl, GCTL_TBP, 1);
 set_field(&gctl, GCTL_FBP, 1);
 set_field(&gctl, GCTL_ET, 1);
 hw_write_20kx(card, GCTL, gctl);
 pds_delay_10us(10*100);

 hw_write_20kx(card, GIE, 0); // Reset all global pending interrupts
 hw_write_20kx(card, SRCIP, 0); // Reset all SRC pending interrupts
 pds_delay_10us(30*100);

 // Detect the card ID and configure GPIO accordingly
 if((card->subsys_id == 0x0022) || (card->subsys_id == 0x002F)) { // SB055x cards
  hw_write_20kx(card, GPIOCTL, 0x13fe);
 }else if ((card->subsys_id == 0x0029) || (card->subsys_id == 0x0031)) { // SB073x cards
  hw_write_20kx(card, GPIOCTL, 0x00e6);
 }else if ((card->subsys_id & 0xf000) == 0x6000) { // Vista compatible cards
  hw_write_20kx(card, GPIOCTL, 0x00c2);
 }else{
  hw_write_20kx(card, GPIOCTL, 0x01e6);
 }

 // init transport operations
 trnctl = 0x13;  // 32-bit, 4k-size page
 hw_write_20kx(card, PTPALX, (uint32_t) card->virtualpagetable);// ptp_phys_low
 hw_write_20kx(card, PTPAHX, 0); // ptp_phys_high
 hw_write_20kx(card, TRNCTL, trnctl);
 hw_write_20kx(card, TRNIS, 0x200c01); /* realy needed? */

 hw_daio_init(card);

 if(hw_dac_init(card)<0){
  mpxplay_debugf(XFI_DEBUG_OUTPUT,"chip_init: dac-init failed");
  return 0;
 }

 // ??? adc init

 i = hw_read_20kx(card, SRCMCTL);
 i|= 0x1; /* Enables input from the audio ring */
 hw_write_20kx(card, SRCMCTL, i);

 /*i=0;
 ctl_amoplo=0;
 set_field(&ctl_amoplo, AMOPLO_M, AMIXER_Y_IMMEDIATE);
 set_field(&ctl_amoplo, AMOPLO_X, AMIXER_MASTER_F);
 set_field(&ctl_amoplo, AMOPLO_Y, INIT_VOL);
 hw_write_20kx(card, AMOPLO+i*8, ctl_amoplo);
 i=1;
 ctl_amoplo=0;
 set_field(&ctl_amoplo, AMOPLO_M, AMIXER_Y_IMMEDIATE);
 set_field(&ctl_amoplo, AMOPLO_X, AMIXER_WAVE_F);
 set_field(&ctl_amoplo, AMOPLO_Y, INIT_VOL);
 hw_write_20kx(card, AMOPLO+i*8, ctl_amoplo);*/

 card->last_rsr_cfg=card->rsr;
 card->last_msr_cfg=card->msr;

 mpxplay_debugf(XFI_DEBUG_OUTPUT,"chip init end");
 return 1;
}

static void snd_emu20kx_chip_close(struct emu20kx_card_s *card)
{
}

static void snd_emu20kx_prepare_playback(struct emu20kx_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int pitch,pm_idx;

 if(card->last_rsr_cfg!=card->rsr){
  hw_pll_init(card);
  card->last_rsr_cfg=card->rsr;
 }
 if(card->last_msr_cfg!=card->msr){
  hw_daio_init(card);
  hw_dac_init(card);
  card->last_msr_cfg=card->msr;
 }

 pitch=snd_emu20kx_get_pitch(aui->freq_card,card->dac_output_freq);

 pm_idx = src_param_pitch_mixer(card->src_idx);
 hw_write_20kx(card, PRING_LO_HI+4*pm_idx, pitch);
 hw_write_20kx(card, PMOPLO+8*pm_idx, 0x3);
 hw_write_20kx(card, PMOPHI+8*pm_idx, 0x0);

 hw_write_20kx(card, SRCSA+card->src_idx*0x100, (unsigned long)card->pcmout_buffer); // !!! & SRCSA_SA
 hw_write_20kx(card, SRCLA+card->src_idx*0x100, (unsigned long)card->pcmout_buffer + aui->card_dmasize); // !!! & SRCLA_LA
 hw_write_20kx(card, SRCCA+card->src_idx*0x100, (unsigned long)card->pcmout_buffer + card->max_cisz); // !!! & SRCCA_CA
 hw_write_20kx(card, SRCCF+card->src_idx*0x100, 0x0);

 hw_write_20kx(card, SRCCCR+card->src_idx*0x100, card->max_cisz);

 set_field(&card->src_ctl,SRCCTL_ROM, snd_emu20kx_select_rom(pitch));
 set_field(&card->src_ctl,SRCCTL_SF, card->sfnum);
 set_field(&card->src_ctl,SRCCTL_PM, 0); // ??? (src->ops->next_interleave(src) != NULL)

 mpxplay_debugf(XFI_DEBUG_OUTPUT,"prepare playback end");
}

//-------------------------------------------------------------------------
static pci_device_s emu20kx_devices[]={
 {"EMU20K1",0x1102,0x0005, 0},
 {"EMU20K2",0x1102,0x000b, 0},
 {NULL,0,0,0}
};

static void EMU20KX_close(struct mpxplay_audioout_info_s *aui);

static void EMU20KX_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"XFI : Creative %s (%4.4X) found on port:%4.4X irq:%d",
         card->pci_dev->device_name,card->subsys_id,card->iobase,card->irq);
 pds_textdisplay_printf(sout);
}

static int EMU20KX_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card;

 card=(struct emu20kx_card_s *)calloc(1,sizeof(struct emu20kx_card_s));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(&emu20kx_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 pcibios_set_master(card->pci_dev);

 card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
 card->iobase&=0xfffffff8;
 if(!card->iobase)
  goto err_adetect;

 card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
 card->subsys_id=pcibios_ReadConfig_Word(card->pci_dev,PCIR_SSID);

 mpxplay_debugf(XFI_DEBUG_OUTPUT,"vend_id:%4.4X dev_id:%4.4X subid:%8.8X port:%8.8X",
  card->pci_dev->vendor_id,card->pci_dev->device_id,card->subsys_id,card->iobase);

 if(!snd_emu20kx_buffer_init(card,aui))
  goto err_adetect;

 snd_emu20kx_set_output_format(card,aui);

 if(!snd_emu20kx_chip_init(card))
  goto err_adetect;
 //snd_emu20kx_ac97_init(card);

 return 1;

err_adetect:
 EMU20KX_close(aui);
 return 0;
}

static void EMU20KX_close(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 if(card){
  if(card->iobase){
   snd_emu20kx_chip_close(card);
   pds_dpmi_unmap_physycal_memory(card->iobase);
  }
  MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   free(card->pci_dev);
  free(card);
  aui->card_private_data=NULL;
 }
}

static void EMU20KX_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 snd_emu20kx_set_output_format(card,aui);
 MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,EMU20KX_PAGESIZE,0);
 snd_emu20kx_prepare_playback(card,aui);
}

static void EMU20KX_start(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 set_field(&card->src_ctl,SRCCTL_BM, 1);
 set_field(&card->src_ctl,SRCCTL_STATE, SRC_STATE_INIT);
 hw_write_20kx(card, SRCCTL+card->src_idx*0x100, card->src_ctl);
}

static void EMU20KX_stop(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 set_field(&card->src_ctl,SRCCTL_BM, 0);
 set_field(&card->src_ctl,SRCCTL_STATE, SRC_STATE_OFF);
 hw_write_20kx(card, SRCCTL+card->src_idx*0x100, card->src_ctl);
}

//------------------------------------------------------------------------

static long EMU20KX_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct emu20kx_card_s *card=aui->card_private_data;
 unsigned long bufpos=0,ctl_ca;

 ctl_ca=hw_read_20kx(card, SRCCA+card->src_idx*0x100);
 bufpos=get_field(ctl_ca, SRCCA_CA);

 bufpos=(bufpos + aui->card_dmasize - card->max_cisz - (unsigned long)card->pcmout_buffer) % aui->card_dmasize;

 aui->card_dma_lastgoodpos=bufpos;

 mpxplay_debugf(XFI_DEBUG_OUTPUT,"bufpos:%5d dmasize:%5d",bufpos,aui->card_dmasize);

 return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------
//mixer

static void EMU20KX_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 //struct emu20kx_card_s *card=aui->card_private_data;
}

static unsigned long EMU20KX_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 //struct emu20kx_card_s *card=aui->card_private_data;
 return 0;
}

one_sndcard_info EMU20KX_sndcard_info={
 "XFI",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                 // no init
 &EMU20KX_adetect,     // only autodetect
 &EMU20KX_card_info,
 &EMU20KX_start,
 &EMU20KX_stop,
 &EMU20KX_close,
 &EMU20KX_setrate,

 &MDma_writedata,
 &EMU20KX_getbufpos,
 &MDma_clearbuf,
 &MDma_interrupt_monitor,
 NULL,

 &EMU20KX_writeMIXER,
 &EMU20KX_readMIXER,
 NULL
};

#endif // AUCARDS_LINK_EMU20KX
