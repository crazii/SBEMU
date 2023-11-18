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
//function: CMI 8338/8738 (PCI) low level routines
//based on the ALSA (http://www.alsa-project.org)

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_CMI8X38

#include <string.h>
#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"

#define CMI8X38_LINK_MULTICHAN 1

#define PCMBUFFERPAGESIZE      4096

/*
 * CM8x38 registers definition
 */

#define CM_REG_FUNCTRL0        0x00
#define CM_RST_CH1        0x00080000
#define CM_RST_CH0        0x00040000
#define CM_CHEN1        0x00020000    /* ch1: enable */
#define CM_CHEN0        0x00010000    /* ch0: enable */
#define CM_PAUSE1        0x00000008    /* ch1: pause */
#define CM_PAUSE0        0x00000004    /* ch0: pause */
#define CM_CHADC1        0x00000002    /* ch1, 0:playback, 1:record */
#define CM_CHADC0        0x00000001    /* ch0, 0:playback, 1:record */

#define CM_REG_FUNCTRL1        0x04
#define CM_ASFC_MASK        0x0000E000    /* ADC sampling frequency */
#define CM_ASFC_SHIFT        13
#define CM_DSFC_MASK        0x00001C00    /* DAC sampling frequency */
#define CM_DSFC_SHIFT        10
#define CM_SPDF_1        0x00000200    /* SPDIF IN/OUT at channel B */
#define CM_SPDF_0        0x00000100    /* SPDIF OUT only channel A */
#define CM_SPDFLOOP        0x00000080    /* ext. SPDIIF/OUT -> IN loopback */
#define CM_SPDO2DAC        0x00000040    /* SPDIF/OUT can be heard from internal DAC */
#define CM_INTRM        0x00000020    /* master control block (MCB) interrupt enabled */
#define CM_BREQ            0x00000010    /* bus master enabled */
#define CM_VOICE_EN        0x00000008    /* legacy voice (SB16,FM) */
#define CM_UART_EN        0x00000004    /* UART */
#define CM_JYSTK_EN        0x00000002    /* joy stick */

#define CM_REG_CHFORMAT        0x08

#define CM_CHB3D5C        0x80000000    /* 5,6 channels */
#define CM_CHB3D        0x20000000    /* 4 channels */

#define CM_CHIP_MASK1        0x1f000000
#define CM_CHIP_037        0x01000000

#define CM_SPDIF_SELECT1    0x00080000    /* for model <= 037 ? */
#define CM_AC3EN1        0x00100000    /* enable AC3: model 037 */
#define CM_SPD24SEL        0x00020000    /* 24bit spdif: model 037 */
/* #define CM_SPDIF_INVERSE    0x00010000 */ /* ??? */

#define CM_ADCBITLEN_MASK    0x0000C000
#define CM_ADCBITLEN_16        0x00000000
#define CM_ADCBITLEN_15        0x00004000
#define CM_ADCBITLEN_14        0x00008000
#define CM_ADCBITLEN_13        0x0000C000

#define CM_ADCDACLEN_MASK    0x00003000
#define CM_ADCDACLEN_060    0x00000000
#define CM_ADCDACLEN_066    0x00001000
#define CM_ADCDACLEN_130    0x00002000
#define CM_ADCDACLEN_280    0x00003000

#define CM_CH1_SRATE_176K    0x00000800
#define CM_CH1_SRATE_88K    0x00000400
#define CM_CH0_SRATE_176K    0x00000200
#define CM_CH0_SRATE_88K    0x00000100

#define CM_SPDIF_INVERSE2    0x00000080    /* model 055? */

#define CM_CH1FMT_MASK        0x0000000C
#define CM_CH1FMT_SHIFT        2
#define CM_CH0FMT_MASK        0x00000003
#define CM_CH0FMT_SHIFT        0

#define CM_REG_INT_HLDCLR    0x0C
#define CM_CHIP_MASK2        0xff000000
#define CM_CHIP_039        0x04000000
#define CM_CHIP_039_6CH        0x01000000
#define CM_TDMA_INT_EN        0x00040000
#define CM_CH1_INT_EN        0x00020000
#define CM_CH0_INT_EN        0x00010000
#define CM_INT_HOLD        0x00000002
#define CM_INT_CLEAR        0x00000001

#define CM_REG_INT_STATUS    0x10
#define CM_INTR            0x80000000
#define CM_VCO            0x08000000    /* Voice Control? CMI8738 */
#define CM_MCBINT        0x04000000    /* Master Control Block abort cond.? */
#define CM_UARTINT        0x00010000
#define CM_LTDMAINT        0x00008000
#define CM_HTDMAINT        0x00004000
#define CM_XDO46        0x00000080    /* Modell 033? Direct programming EEPROM (read data register) */
#define CM_LHBTOG        0x00000040    /* High/Low status from DMA ctrl register */
#define CM_LEG_HDMA        0x00000020    /* Legacy is in High DMA channel */
#define CM_LEG_STEREO        0x00000010    /* Legacy is in Stereo mode */
#define CM_CH1BUSY        0x00000008
#define CM_CH0BUSY        0x00000004
#define CM_CHINT1        0x00000002
#define CM_CHINT0        0x00000001

#define CM_REG_LEGACY_CTRL    0x14
#define CM_NXCHG        0x80000000    /* h/w multi channels? */
#define CM_VMPU_MASK        0x60000000    /* MPU401 i/o port address */
#define CM_VMPU_330        0x00000000
#define CM_VMPU_320        0x20000000
#define CM_VMPU_310        0x40000000
#define CM_VMPU_300        0x60000000
#define CM_VSBSEL_MASK        0x0C000000    /* SB16 base address */
#define CM_VSBSEL_220        0x00000000
#define CM_VSBSEL_240        0x04000000
#define CM_VSBSEL_260        0x08000000
#define CM_VSBSEL_280        0x0C000000
#define CM_FMSEL_MASK        0x03000000    /* FM OPL3 base address */
#define CM_FMSEL_388        0x00000000
#define CM_FMSEL_3C8        0x01000000
#define CM_FMSEL_3E0        0x02000000
#define CM_FMSEL_3E8        0x03000000
#define CM_ENSPDOUT        0x00800000    /* enable XPDIF/OUT to I/O interface */
#define CM_SPDCOPYRHT        0x00400000    /* set copyright spdif in/out */
#define CM_DAC2SPDO        0x00200000    /* enable wave+fm_midi -> SPDIF/OUT */
#define CM_SETRETRY        0x00010000    /* 0: legacy i/o wait (default), 1: legacy i/o bus retry */
#define CM_CHB3D6C        0x00008000    /* 5.1 channels support */
#define CM_LINE_AS_BASS        0x00006000    /* use line-in as bass */

#define CM_REG_MISC_CTRL    0x18
#define CM_PWD            0x80000000
#define CM_RESET        0x40000000
#define CM_SFIL_MASK        0x30000000
#define CM_TXVX            0x08000000
#define CM_N4SPK3D        0x04000000    /* 4ch output */
#define CM_SPDO5V        0x02000000    /* 5V spdif output (1 = 0.5v (coax)) */
#define CM_SPDIF48K        0x01000000    /* write */
#define CM_SPATUS48K        0x01000000    /* read */
#define CM_ENDBDAC        0x00800000    /* enable dual dac */
#define CM_XCHGDAC        0x00400000    /* 0: front=ch0, 1: front=ch1 */
#define CM_SPD32SEL        0x00200000    /* 0: 16bit SPDIF, 1: 32bit */
#define CM_SPDFLOOPI        0x00100000    /* int. SPDIF-IN -> int. OUT */
#define CM_FM_EN        0x00080000    /* enalbe FM */
#define CM_AC3EN2        0x00040000    /* enable AC3: model 039 */
#define CM_VIDWPDSB        0x00010000 
#define CM_SPDF_AC97        0x00008000    /* 0: SPDIF/OUT 44.1K, 1: 48K */
#define CM_MASK_EN        0x00004000
#define CM_VIDWPPRT        0x00002000
#define CM_SFILENB        0x00001000
#define CM_MMODE_MASK        0x00000E00
#define CM_SPDIF_SELECT2    0x00000100    /* for model > 039 ? */
#define CM_ENCENTER        0x00000080
#define CM_FLINKON        0x00000040
#define CM_FLINKOFF        0x00000020
#define CM_MIDSMP        0x00000010
#define CM_UPDDMA_MASK        0x0000000C
#define CM_TWAIT_MASK        0x00000003

    /* byte */
#define CM_REG_MIXER0        0x20

#define CM_REG_SB16_DATA    0x22
#define CM_REG_SB16_ADDR    0x23

#define CM_REFFREQ_XIN        (315*1000*1000)/22    /* 14.31818 Mhz reference clock frequency pin XIN */
#define CM_ADCMULT_XIN        512            /* Guessed (487 best for 44.1kHz, not for 88/176kHz) */
#define CM_TOLERANCE_RATE    0.001            /* Tolerance sample rate pitch (1000ppm) */
#define CM_MAXIMUM_RATE        80000000        /* Note more than 80MHz */

#define CM_REG_MIXER1        0x24
#define CM_FMMUTE        0x80    /* mute FM */
#define CM_FMMUTE_SHIFT        7
#define CM_WSMUTE        0x40    /* mute PCM */
#define CM_WSMUTE_SHIFT        6
#define CM_SPK4            0x20    /* lin-in -> rear line out */
#define CM_SPK4_SHIFT        5
#define CM_REAR2FRONT        0x10    /* exchange rear/front */
#define CM_REAR2FRONT_SHIFT    4
#define CM_WAVEINL        0x08    /* digital wave rec. left chan */
#define CM_WAVEINL_SHIFT    3
#define CM_WAVEINR        0x04    /* digical wave rec. right */
#define CM_WAVEINR_SHIFT    2
#define CM_X3DEN        0x02    /* 3D surround enable */
#define CM_X3DEN_SHIFT        1
#define CM_CDPLAY        0x01    /* enable SPDIF/IN PCM -> DAC */
#define CM_CDPLAY_SHIFT        0

#define CM_REG_MIXER2        0x25
#define CM_RAUXREN        0x80    /* AUX right capture */
#define CM_RAUXREN_SHIFT    7
#define CM_RAUXLEN        0x40    /* AUX left capture */
#define CM_RAUXLEN_SHIFT    6
#define CM_VAUXRM        0x20    /* AUX right mute */
#define CM_VAUXRM_SHIFT        5
#define CM_VAUXLM        0x10    /* AUX left mute */
#define CM_VAUXLM_SHIFT        4
#define CM_VADMIC_MASK        0x0e    /* mic gain level (0-3) << 1 */
#define CM_VADMIC_SHIFT        1
#define CM_MICGAINZ        0x01    /* mic boost */
#define CM_MICGAINZ_SHIFT    0

#define CM_REG_AUX_VOL        0x26
#define CM_VAUXL_MASK        0xf0
#define CM_VAUXR_MASK        0x0f

#define CM_REG_MISC        0x27
#define CM_XGPO1        0x20
// #define CM_XGPBIO        0x04
#define CM_MIC_CENTER_LFE    0x04    /* mic as center/lfe out? (model 039 or later?) */
#define CM_SPDIF_INVERSE    0x04    /* spdif input phase inverse (model 037) */
#define CM_SPDVALID        0x02    /* spdif input valid check */
#define CM_DMAUTO        0x01

#define CM_REG_AC97        0x28    /* hmmm.. do we have ac97 link? */
/*
 * For CMI-8338 (0x28 - 0x2b) .. is this valid for CMI-8738
 * or identical with AC97 codec?
 */
#define CM_REG_EXTERN_CODEC    CM_REG_AC97

/*
 * MPU401 pci port index address 0x40 - 0x4f (CMI-8738 spec ver. 0.6)
 */
#define CM_REG_MPU_PCI        0x40

/*
 * FM pci port index address 0x50 - 0x5f (CMI-8738 spec ver. 0.6)
 */
#define CM_REG_FM_PCI        0x50

/*
 * for CMI-8338 .. this is not valid for CMI-8738.
 */
#define CM_REG_EXTENT_IND    0xf0
#define CM_VPHONE_MASK        0xe0    /* Phone volume control (0-3) << 5 */
#define CM_VPHONE_SHIFT        5
#define CM_VPHOM        0x10    /* Phone mute control */
#define CM_VSPKM        0x08    /* Speaker mute control, default high */
#define CM_RLOOPREN        0x04    /* Rec. R-channel enable */
#define CM_RLOOPLEN        0x02    /* Rec. L-channel enable */

/*
 * CMI-8338 spec ver 0.5 (this is not valid for CMI-8738):
 * the 8 registers 0xf8 - 0xff are used for programming m/n counter by the PLL
 * unit (readonly?).
 */
#define CM_REG_PLL        0xf8

/*
 * extended registers
 */
#define CM_REG_CH0_FRAME1    0x80    /* base address */
#define CM_REG_CH0_FRAME2    0x84
#define CM_REG_CH1_FRAME1    0x88    /* 0-15: count of samples at bus master; buffer size */
#define CM_REG_CH1_FRAME2    0x8C    /* 16-31: count of samples at codec; fragment size */

/*
 * size of i/o region
 */
#define CM_EXTENT_CODEC      0x100
#define CM_EXTENT_MIDI      0x2
#define CM_EXTENT_SYNTH      0x4

/* fixed legacy joystick address */
#define CM_JOYSTICK_ADDR    0x200


/*
 * pci ids
 */
#define PCI_DEVICE_ID_CMEDIA_CM8738  0x0111
#define PCI_DEVICE_ID_CMEDIA_CM8738B 0x0112

/*
 * channels for playback / capture
 */
#define CM_CH_PLAY    0
#define CM_CH_CAPT    1

/*
 * flags to check device open/close
 */
#define CM_OPEN_NONE    0
#define CM_OPEN_CH_MASK    0x01
#define CM_OPEN_DAC    0x10
#define CM_OPEN_ADC    0x20
#define CM_OPEN_SPDIF    0x40
#define CM_OPEN_MCHAN    0x80
#define CM_OPEN_PLAYBACK    (CM_CH_PLAY | CM_OPEN_DAC)
#define CM_OPEN_PLAYBACK2    (CM_CH_CAPT | CM_OPEN_DAC)
#define CM_OPEN_PLAYBACK_MULTI    (CM_CH_PLAY | CM_OPEN_DAC | CM_OPEN_MCHAN)
#define CM_OPEN_CAPTURE        (CM_CH_CAPT | CM_OPEN_ADC)
#define CM_OPEN_SPDIF_PLAYBACK    (CM_CH_PLAY | CM_OPEN_DAC | CM_OPEN_SPDIF)
#define CM_OPEN_SPDIF_CAPTURE    (CM_CH_CAPT | CM_OPEN_ADC | CM_OPEN_SPDIF)


#if CM_CH_PLAY == 1
#define CM_PLAYBACK_SRATE_176K    CM_CH1_SRATE_176K
#define CM_PLAYBACK_SPDF    CM_SPDF_1
#define CM_CAPTURE_SPDF        CM_SPDF_0
#else
#define CM_PLAYBACK_SRATE_176K CM_CH0_SRATE_176K
#define CM_PLAYBACK_SPDF    CM_SPDF_0
#define CM_CAPTURE_SPDF        CM_SPDF_1
#endif


typedef struct cmi8x38_card
{
 unsigned long   iobase;
 unsigned short     model;
 unsigned int    irq;
 unsigned char   chiprev;
 unsigned char   device_type;
 struct pci_config_s  *pci_dev;

 dosmem_t *dm;
 char *pcmout_buffer;
 long pcmout_bufsize;

 unsigned int ctrl;

 int chip_version;
 int max_channels;
 //unsigned int has_dual_dac;
 //unsigned int can_ac3_sw;
 //unsigned int can_ac3_hw;
 unsigned int can_multi_ch;
 //unsigned int do_soft_ac3;

 //unsigned int spdif_playback_avail;    /* spdif ready? */
 //unsigned int spdif_playback_enabled;    /* spdif switch enabled? */
 //int spdif_counter;    /* for software AC3 */

 //unsigned int dig_status;
 //unsigned int dig_pcm_status;

 unsigned int dma_size;          /* in frames */
 unsigned int period_size;    /* in frames */
 unsigned int fmt;          /* format bits */
 //int bytes_per_frame;
 int shift;
 //int ac3_shift;    /* extra shift: 1 on soft ac3 mode */

}cmi8x38_card;

static void cmi8x38_ac97_write(unsigned int baseport,unsigned int reg, unsigned int value);
static unsigned int cmi8x38_ac97_read(unsigned int baseport, unsigned int reg);

extern unsigned int intsoundconfig,intsoundcontrol;

//-------------------------------------------------------------------------
// low level write & read

#define snd_cmipci_write_8(cm,reg,data)  outb(cm->iobase+reg,data)
#define snd_cmipci_write_16(cm,reg,data) outw(cm->iobase+reg,data)
#define snd_cmipci_write_32(cm,reg,data) outl(cm->iobase+reg,data)
#define snd_cmipci_read_8(cm,reg)  inb(cm->iobase+reg)
#define snd_cmipci_read_16(cm,reg) inw(cm->iobase+reg)
#define snd_cmipci_read_32(cm,reg) inl(cm->iobase+reg)

static void snd_cmipci_set_bit(cmi8x38_card *cm, unsigned int cmd, unsigned int flag)
{
 unsigned int val;
 val = snd_cmipci_read_32(cm, cmd);
 val|= flag;
 snd_cmipci_write_32(cm, cmd, val);
}

static void snd_cmipci_clear_bit(cmi8x38_card *cm, unsigned int cmd, unsigned int flag)
{
 unsigned int val;
 val = snd_cmipci_read_32(cm, cmd);
 val&= ~flag;
 snd_cmipci_write_32(cm, cmd, val);
}

static void snd_cmipci_mixer_write(cmi8x38_card *cm, unsigned char idx, unsigned char data)
{
 snd_cmipci_write_8(cm, CM_REG_SB16_ADDR, idx);
 snd_cmipci_write_8(cm, CM_REG_SB16_DATA, data);
}

static unsigned int snd_cmipci_mixer_read(cmi8x38_card *cm, unsigned char idx)
{
 snd_cmipci_write_8(cm, CM_REG_SB16_ADDR, idx);
 return snd_cmipci_read_8(cm, CM_REG_SB16_DATA);
}

//-------------------------------------------------------------------------

static unsigned int cmi_rates[] = { 5512, 11025, 22050, 44100, 8000, 16000, 32000, 48000 };

static unsigned int snd_cmipci_rate_freq(unsigned int rate)
{
 unsigned int i;
 for(i = 0; i < 8; i++) {
  if(cmi_rates[i] == rate)
   return i;
 }
 return 7; // 48k
}

static void snd_cmipci_ch_reset(cmi8x38_card *cm, int ch)
{
 int reset = CM_RST_CH0 << ch;
 snd_cmipci_write_32(cm, CM_REG_FUNCTRL0, CM_CHADC0 | reset);
 snd_cmipci_write_32(cm, CM_REG_FUNCTRL0, CM_CHADC0 & (~reset));
 pds_delay_10us(1);
}

static int set_dac_channels(cmi8x38_card *cm, int channels)
{
 /*if(channels > 2){
  if(!cm->can_multi_ch)
   return -1;
  if(cm->fmt != 0x03) // stereo 16bit only
   return -1;

  snd_cmipci_set_bit(cm, CM_REG_LEGACY_CTRL, CM_NXCHG);
  if(channels > 4){
   snd_cmipci_clear_bit(cm, CM_REG_CHFORMAT, CM_CHB3D);
   snd_cmipci_set_bit(cm, CM_REG_CHFORMAT, CM_CHB3D5C);
  }else{
   snd_cmipci_clear_bit(cm, CM_REG_CHFORMAT, CM_CHB3D5C);
   snd_cmipci_set_bit(cm, CM_REG_CHFORMAT, CM_CHB3D);
  }
  if(channels == 6){
   snd_cmipci_set_bit(cm, CM_REG_LEGACY_CTRL, CM_CHB3D6C);
   snd_cmipci_set_bit(cm, CM_REG_MISC_CTRL, CM_ENCENTER);
  }else{
   snd_cmipci_clear_bit(cm, CM_REG_LEGACY_CTRL, CM_CHB3D6C);
   snd_cmipci_clear_bit(cm, CM_REG_MISC_CTRL, CM_ENCENTER);
  }
 }else{*/
  if(cm->can_multi_ch){
   snd_cmipci_clear_bit(cm, CM_REG_LEGACY_CTRL, CM_NXCHG);
   snd_cmipci_clear_bit(cm, CM_REG_CHFORMAT, CM_CHB3D);
   snd_cmipci_clear_bit(cm, CM_REG_CHFORMAT, CM_CHB3D5C);
   snd_cmipci_clear_bit(cm, CM_REG_LEGACY_CTRL, CM_CHB3D6C);
   snd_cmipci_clear_bit(cm, CM_REG_MISC_CTRL, CM_ENCENTER);
  }
 //}
 return 0;
}

static void query_chip(cmi8x38_card *cm)
{
 unsigned int detect;

 /* check reg 0Ch, bit 24-31 */
 detect = snd_cmipci_read_32(cm, CM_REG_INT_HLDCLR) & CM_CHIP_MASK2;
 if(!detect){
  /* check reg 08h, bit 24-28 */
  detect = snd_cmipci_read_32(cm, CM_REG_CHFORMAT) & CM_CHIP_MASK1;
  if(!detect) {
   cm->chip_version = 33;
   cm->max_channels = 2;
   //if(cm->do_soft_ac3)
   // cm->can_ac3_sw = 1;
   //else
   // cm->can_ac3_hw = 1;
   //cm->has_dual_dac = 1;
  }else{
   cm->chip_version = 37;
   cm->max_channels = 2;
   //cm->can_ac3_hw = 1;
   //cm->has_dual_dac = 1;
  }
 }else{
  /* check reg 0Ch, bit 26 */
  if(detect&CM_CHIP_039){
   cm->chip_version = 39;
   if(detect & CM_CHIP_039_6CH)
    cm->max_channels  = 6;
   else
    cm->max_channels = 4;
   //cm->can_ac3_hw = 1;
   //cm->has_dual_dac = 1;
   cm->can_multi_ch = 1;
  }else{
   cm->chip_version = 55; /* 4 or 6 channels */
   cm->max_channels  = 6;
   //cm->can_ac3_hw = 1;
   //cm->has_dual_dac = 1;
   cm->can_multi_ch = 1;
  }
 }
}

static void cmi8x38_chip_init(struct cmi8x38_card *cm)
{
 unsigned int val;
 cm->ctrl  = 0; // ch0 playback

 query_chip(cm);

 /* initialize codec registers */
 snd_cmipci_write_32(cm, CM_REG_INT_HLDCLR, 0);    /* disable ints */
 snd_cmipci_ch_reset(cm, CM_CH_PLAY);
 snd_cmipci_ch_reset(cm, CM_CH_CAPT);
 snd_cmipci_write_32(cm, CM_REG_FUNCTRL0, 0);    /* disable channels */
 snd_cmipci_write_32(cm, CM_REG_FUNCTRL1, 0);

 snd_cmipci_write_32(cm, CM_REG_CHFORMAT, 0);
 snd_cmipci_set_bit(cm, CM_REG_MISC_CTRL, CM_ENDBDAC|CM_N4SPK3D);
 snd_cmipci_clear_bit(cm, CM_REG_MISC_CTRL, CM_XCHGDAC);
 /* Set Bus Master Request */
 snd_cmipci_set_bit(cm, CM_REG_FUNCTRL1, CM_BREQ);

 /* Assume TX and compatible chip set (Autodetection required for VX chip sets) */
 switch(cm->pci_dev->device_id) {
  case PCI_DEVICE_ID_CMEDIA_CM8738:
  case PCI_DEVICE_ID_CMEDIA_CM8738B:
       /* PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82437VX */
       if(pcibios_FindDevice(0x8086, 0x7030,NULL)==PCI_SUCCESSFUL)
        snd_cmipci_set_bit(cm, CM_REG_MISC_CTRL, CM_TXVX);
       break;
 }

 /* disable FM */
 val = snd_cmipci_read_32(cm, CM_REG_LEGACY_CTRL) & (~CM_FMSEL_MASK);
 snd_cmipci_write_32(cm, CM_REG_LEGACY_CTRL, val);
 snd_cmipci_clear_bit(cm, CM_REG_MISC_CTRL, CM_FM_EN);

 /* reset mixer */
 snd_cmipci_mixer_write(cm, 0, 0);
}

static void cmi8x38_chip_close(struct cmi8x38_card *cm)
{
 if(cm->iobase){
  snd_cmipci_clear_bit(cm, CM_REG_MISC_CTRL, CM_FM_EN);
  snd_cmipci_clear_bit(cm, CM_REG_LEGACY_CTRL, CM_ENSPDOUT);
  snd_cmipci_write_32(cm, CM_REG_INT_HLDCLR, 0);  /* disable ints */
  snd_cmipci_ch_reset(cm, CM_CH_PLAY);
  snd_cmipci_ch_reset(cm, CM_CH_CAPT);
  snd_cmipci_write_32(cm, CM_REG_FUNCTRL0, 0); /* disable channels */
  snd_cmipci_write_32(cm, CM_REG_FUNCTRL1, 0);

  /* reset mixer */
  snd_cmipci_mixer_write(cm, 0, 0);
 }
}

//-------------------------------------------------------------------------
static pci_device_s cmi_devices[]={
 {"8338A",0x13F6,0x0100},
 {"8338B",0x13F6,0x0101},
 {"8738" ,0x13F6,0x0111},
 {"8738B",0x13F6,0x0112},
 {NULL,0,0}
};

static void CMI8X38_close(struct mpxplay_audioout_info_s *aui);

static void CMI8X38_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"CMI : %s soundcard found on port:%4.4X irq:%d chipver:%d max-chans:%d",
         card->pci_dev->device_name,card->iobase,card->irq,card->chip_version,card->max_channels);
 pds_textdisplay_printf(sout);
}

static int CMI8X38_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card;

 card=(struct cmi8x38_card *)pds_calloc(1,sizeof(struct cmi8x38_card));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(&cmi_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 pcibios_set_master(card->pci_dev);

 card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR)&0xfff0;
 if(!card->iobase)
  goto err_adetect;
 card->irq    = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
 card->chiprev=pcibios_ReadConfig_Byte(card->pci_dev, PCIR_RID);
 card->model  =pcibios_ReadConfig_Word(card->pci_dev, PCIR_SSID);

 // alloc buffers
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,PCMBUFFERPAGESIZE,2,0);

 card->dm=MDma_alloc_cardmem( card->pcmout_bufsize      // pcm output
                            +PCMBUFFERPAGESIZE );      // to round

 card->pcmout_buffer=(void *)(((uint32_t)card->dm->linearptr+PCMBUFFERPAGESIZE-1)&(~(PCMBUFFERPAGESIZE-1))); // buffer begins on page (4096 bytes) boundary

 aui->card_DMABUFF=card->pcmout_buffer;

 // init chip
 cmi8x38_chip_init(card);

 return 1;

err_adetect:
 CMI8X38_close(aui);
 return 0;
}

static void CMI8X38_close(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 if(card){
  if(card->iobase)
   cmi8x38_chip_close(card);
  MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void CMI8X38_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 unsigned int dmabufsize,val,freqnum;

 if(aui->freq_card<5512)
  aui->freq_card=5512;
 else{
  if(aui->freq_card>48000)
   aui->freq_card=48000;
 }

 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 dmabufsize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,PCMBUFFERPAGESIZE,0);

 //hw config

 //reset dac, disable ch
 card->ctrl &= ~CM_CHEN0;
 snd_cmipci_write_32(card, CM_REG_FUNCTRL0, card->ctrl | CM_RST_CH0);
 snd_cmipci_write_32(card, CM_REG_FUNCTRL0, card->ctrl & ~CM_RST_CH0);

 //format cfg
 card->fmt=0;
 card->shift=0;
 if(aui->bits_card>=16){
  card->fmt|=0x02;
  card->shift++;
  //if(aui->bits_card>16) // 24,32 bits ???
  // card->shift++;
 }
 if(aui->chan_card>1){
  card->fmt|=0x01;
  card->shift++;
 }

 //
 if(set_dac_channels(card,aui->chan_card)!=0)
  return;

 //buffer cfg
 card->dma_size    = dmabufsize >> card->shift;
 card->period_size = dmabufsize >> card->shift;
 //card->dma_size    >>= card->ac3_shift;
 //card->period_size >>= card->ac3_shift;

 //if(aui->chan_card>2){
 // card->dma_size    = (card->dma_size * aui->chan_card) / 2;
 // card->period_size = (card->period_size * aui->chan_card) / 2;
 //}

 // set buffer address
 snd_cmipci_write_32(card, CM_REG_CH0_FRAME1, (long)card->pcmout_buffer);
 // program sample counts
 snd_cmipci_write_16(card, CM_REG_CH0_FRAME2    , card->dma_size - 1);
 snd_cmipci_write_16(card, CM_REG_CH0_FRAME2 + 2, card->period_size - 1);

 // set sample rate
 freqnum = snd_cmipci_rate_freq(aui->freq_card);
 aui->freq_card=cmi_rates[freqnum]; // if the freq-config is not standard at CMI
 val = snd_cmipci_read_32(card, CM_REG_FUNCTRL1);
 val &= ~CM_DSFC_MASK;
 val |= (freqnum << CM_DSFC_SHIFT) & CM_DSFC_MASK;
 snd_cmipci_write_32(card, CM_REG_FUNCTRL1, val);

 // set format
 val = snd_cmipci_read_32(card, CM_REG_CHFORMAT);
 val &= ~CM_CH0FMT_MASK;
 val |= card->fmt << CM_CH0FMT_SHIFT;
 snd_cmipci_write_32(card, CM_REG_CHFORMAT, val);

 // set SPDIF
 //if((aui->freq_card==44100 || aui->freq_card==48000) && (aui->chan_card==2) && (aui->bits_card==16))
 // snd_cmipci_set_bit(card, CM_REG_FUNCTRL1, CM_PLAYBACK_SPDF);
 //else
  snd_cmipci_clear_bit(card, CM_REG_FUNCTRL1, CM_PLAYBACK_SPDF);
}

static void CMI8X38_start(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 card->ctrl |= CM_CHEN0;
 card->ctrl &= ~CM_PAUSE0;
 snd_cmipci_write_32(card, CM_REG_FUNCTRL0, card->ctrl);
}

static void CMI8X38_stop(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 card->ctrl |= CM_PAUSE0;
 snd_cmipci_write_32(card, CM_REG_FUNCTRL0, card->ctrl);
}

static long CMI8X38_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct cmi8x38_card *card=aui->card_private_data;
 unsigned long bufpos;

 bufpos = snd_cmipci_read_16(card, CM_REG_CH0_FRAME2);
 if(bufpos && (bufpos<=card->dma_size)){
  bufpos = card->dma_size -bufpos;
  bufpos <<= card->shift;
  //bufpos <<= card->ac3_shift;
  //if(aui->chan_card > 2)
  // bufpos = (bufpos * 2) / aui->chan_card;

  if(bufpos<aui->card_dmasize)
   aui->card_dma_lastgoodpos=bufpos;
 }

 return aui->card_dma_lastgoodpos;
}

static void CMI8X38_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 MDma_clearbuf(aui);
}

//--------------------------------------------------------------------------
//mixer

static void CMI8X38_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct cmi8x38_card *card=aui->card_private_data;
 snd_cmipci_mixer_write(card,reg,val);
}

static unsigned long CMI8X38_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct cmi8x38_card *card=aui->card_private_data;
 return snd_cmipci_mixer_read(card,reg);
}

//like SB16
static aucards_onemixerchan_s cmi8x38_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x30,31,3,0},{0x31,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_pcm_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),      2,{{0x32,31,3,0},{0x33,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_synth_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_SYNTH,AU_MIXCHANFUNC_VOLUME),  2,{{0x34,31,3,0},{0x35,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_cdin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME),    2,{{0x36,31,3,0},{0x37,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_linein_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME),2,{{0x38,31,3,0},{0x39,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_micin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MICIN,AU_MIXCHANFUNC_VOLUME),  1,{{0x3A,31,3,0}}};
static aucards_onemixerchan_s cmi8x38_auxin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_AUXIN,AU_MIXCHANFUNC_VOLUME),  2,{{0x26,15,4,0},{0x26,15,0,0}}}; //??? in or out?

static aucards_allmixerchan_s cmi8x38_mixerset[]={
 &cmi8x38_master_vol,
 &cmi8x38_pcm_vol,
 &cmi8x38_synth_vol,
 &cmi8x38_cdin_vol,
 &cmi8x38_linein_vol,
 &cmi8x38_micin_vol,
 &cmi8x38_auxin_vol,
 NULL
};

one_sndcard_info CMI8X38_sndcard_info={
 "CMI",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                   // no init
 &CMI8X38_adetect,       // only autodetect
 &CMI8X38_card_info,
 &CMI8X38_start,
 &CMI8X38_stop,
 &CMI8X38_close,
 &CMI8X38_setrate,

 &MDma_writedata,
 &CMI8X38_getbufpos,
 &CMI8X38_clearbuf,
 &MDma_interrupt_monitor,
 NULL,

 &CMI8X38_writeMIXER,
 &CMI8X38_readMIXER,
 &cmi8x38_mixerset[0]
};

#endif
