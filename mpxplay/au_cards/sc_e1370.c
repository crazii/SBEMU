//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//**************************************************************************
//function: Ensoniq ES1370 low level routines (SB PCI64/128 [CT4700/CT5803],
//          Creative "5507" chip). Predecessor of the ES1371/ES1373 already
//          supported by sc_e1371.c.
//
//Ported from a working, hardware-tested VSBHDA (github.com/Baron-von-
//Riedesel/VSBHDA) driver of the same name, itself based on register
//information from the ALSA driver sound/pci/ens1370.c (chip revision
//"CHIP1370"). Every non-obvious step below was needed to get real audio
//out of a genuine ES1370/CT4700 card - see the comments at each one.
//
//Unlike the ES1371/1373, the ES1370 does NOT have an AC'97 codec or a
//hardware sample rate converter. It uses a simple, write-only AK4531 serial
//mixer codec (see ak4531_def.h/ak4531_def.c) and a programmable clock
//divider (derived from a fixed 1.4112 MHz reference) for arbitrary
//playback rates.

//#define MPXPLAY_USE_DEBUGF 1
//#define ENS_DEBUG_OUTPUT stdout

#include "au_cards.h"

#ifdef AU_CARDS_LINK_ES1370

#include "dmairq.h"
#include "pcibios.h"
#include "ak4531_def.h"

#define ES1370_DMABUF_PERIODS  32
#define ES1370_MAX_CHANNELS     2
#define ES1370_MAX_BYTES        4
#define ES1370_DMABUF_ALIGN (ES1370_DMABUF_PERIODS*ES1370_MAX_CHANNELS*ES1370_MAX_BYTES) // 256

#define POLL_COUNT 0x1000

/* ports (same host-interface layout as sc_e1371.c, except 10-17):
 * 00-07 interrupt/chip select
 * 08-0B UART
 * 0C-0F host interface - memory page
 * 10-11 AK4531 codec (write-only)
 * 18-1F legacy
 * 20-2F serial interface
 * 30-3F host interface - memory
 */

#define ES_REG_CONTROL   0x00   /* R/W: Interrupt/Chip select control register */
#define  ES_1370_ADC_STOP     (1<<31) /* disable capture buffer transfers */
#define  ES_1370_XCTL1        (1<<30) /* general purpose output bit */
#define  ES_1370_PCLKDIVO(o)  (((o)&0x1fff)<<16) /* clock divide ratio for DAC2 */
#define  ES_1370_PCLKDIVM     (0x1fff<<16)
#define  ES_1370_WTSRSEL(o)   (((o)&0x03)<<12) /* fixed frequency clock for DAC1 */
#define  ES_1370_M_SBB        (1<<14) /* DAC clock source: 0=clock generator, 1=MPEG clocks */
#define  ES_1370_DAC_SYNC     (1<<11) /* DAC1/DAC2 are synchronous */
#define  ES_1370_M_CB         (1<<9)  /* capture clock source */
#define  ES_1370_XCTL0        (1<<8)  /* general purpose output bit */
#define  ES_BREQ              (1<<7)  /* memory bus request enable */
#define  ES_DAC1_EN           (1<<6)  /* DAC1 playback channel enable */
#define  ES_DAC2_EN           (1<<5)  /* DAC2 playback channel enable */
#define  ES_ADC_EN            (1<<4)  /* ADC capture channel enable */
#define  ES_UART_EN           (1<<3)  /* UART enable */
#define  ES_JYSTK_EN          (1<<2)  /* joystick module enable */
#define  ES_1370_CDC_EN       (1<<1)  /* codec (AK4531) interface enable */
#define  ES_1370_SERR_DISABLE (1<<0)  /* PCI SERR signal disable */

#define ES_REG_STATUS    0x04   /* R/O: Interrupt/Chip select status register */
#define  ES_INTR         (1<<31)      /* interrupt is pending */
#define  ES_1370_CSTAT   (1<<10)      /* AK4531 codec busy or write in progress */
#define  ES_UART         (1<<3)       /* UART interrupt pending */
#define  ES_DAC1         (1<<2)       /* DAC1 channel interrupt pending */
#define  ES_DAC2         (1<<1)       /* DAC2 channel interrupt pending */
#define  ES_ADC          (1<<0)       /* ADC channel interrupt pending */

#define ES_REG_MEM_PAGE  0x0c   /* R/W: memory page register (bits 0-3, ports 30-3F) */
#define  ES_MEM_PAGEO(o) (((o)&0x0f)<<0)

#define ES_REG_1370_CODEC 0x10  /* W/O: AK4531 codec write register (word access) */
#define  ES_1370_CODEC_WRITE(a,d) ((((a)&0xff)<<8)|((d)&0xff))

#define ES_PAGE_DAC      0x0c
#define ES_PAGE_ADC      0x0d

#define ES_REG_SERIAL    0x20   /* R/W: Serial interface control register */
#define  ES_R1_LOOP_SEL  (1<<15)
#define  ES_P2_LOOP_SEL  (1<<14)
#define  ES_P1_LOOP_SEL  (1<<13)
#define  ES_P2_PAUSE     (1<<12)
#define  ES_P1_PAUSE     (1<<11)
#define  ES_R1_INT_EN    (1<<10)
#define  ES_P2_INT_EN    (1<<9)
#define  ES_P1_INT_EN    (1<<8)
#define  ES_P1_SCT_RLD   (1<<7)
#define  ES_P2_DAC_SEN   (1<<6)
#define  ES_R1_MODEO(o)  (((o)&0x03)<<4)
#define  ES_P2_MODEO(o)  (((o)&0x03)<<2)
#define  ES_P1_MODEO(o)  (((o)&0x03)<<0)
/* DAC2 (P2) only: per-frame address increment. Unlike DAC1, the P2 channel
 * does NOT advance its DMA fetch pointer correctly unless these are set
 * explicitly - without them, the chip does not report progress and no
 * audio is heard, even though nothing crashes (confirmed by testing).
 * END_INC must match the sample width: 1 for 8-bit, 2 for 16-bit; ST_INC
 * is always 0 for a linear PCM stream (see ALSA's ens1370.c, CHIP1370
 * playback2_prepare()).
 */
#define  ES_P2_END_INCO(o) (((o)&0x07)<<19)
#define  ES_P2_END_INCM    (0x07<<19)
#define  ES_P2_ST_INCO(o)  (((o)&0x07)<<16)
#define  ES_P2_ST_INCM     (0x07<<16)

#define ES_REG_DAC1_COUNT 0x24
#define ES_REG_DAC2_COUNT 0x28
#define ES_REG_ADC_COUNT  0x2c
#define ES_REG_DAC1_FRAME 0x30  /* PAGE 0x0c */
#define ES_REG_DAC1_SIZE  0x34  /* PAGE 0x0c */
#define ES_REG_DAC2_FRAME 0x38  /* PAGE 0x0c */
#define ES_REG_DAC2_SIZE  0x3c  /* PAGE 0x0c */
#define ES_REG_PHANTOM_FRAME 0x38 /* PAGE 0x0d */
#define ES_REG_PHANTOM_COUNT 0x3c /* PAGE 0x0d */

/* hiword of DACx_SIZE = number of "longwords" transferred so far */
#define ES_REG_FCURR_COUNTI(i) (((i)>>14) & 0x3fffc)

/* fixed reference clock used for the DAC2/ADC programmable divider */
#define ES_1370_SRCLOCK  1411200UL
#define ES_1370_SRTODIV(x) (ES_1370_SRCLOCK/(x)-2)

/* use DAC2 (the "P2" channel), driven by the programmable clock divider,
 * rather than DAC1 (which only supports 4 fixed rates: 5512/11025/22050/
 * 44100 via ES_1370_WTSRSEL). This keeps arbitrary playback rates working,
 * matching what sc_e1371.c offers via its hardware sample rate converter.
 */

typedef struct es1370_card_s
{
 unsigned long   port;
 unsigned int    irq;
 struct pci_config_s *pci_dev;

 cardmem_t *dm;
 cardmem_t *dm_phantom; /* dummy target for the ADC "phantom" DMA erratum */
 char *pcmout_buffer;
 long pcmout_bufsize;

 unsigned long ctrl;   /* value written to ES_REG_CONTROL */
 unsigned long sctrl;  /* value written to ES_REG_SERIAL */
 uint8_t regs[0x1a];   /* cached AK4531 register values (write-only codec!) */
}es1370_card_s;

//-------------------------------------------------------------------------
// AK4531 codec access (write-only - values are cached in card->regs[])

static void es1370_codec_write(struct es1370_card_s *card, unsigned short reg, unsigned short val)
{
 unsigned int t;

 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_codec_write(%X,%X)",reg,val);
 for (t = 0; t < POLL_COUNT; t++) {
  if (!(inl(card->port + ES_REG_STATUS) & ES_1370_CSTAT))
   break;
  pds_delay_10us(1);
 }
 if (t == POLL_COUNT) {
  mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_codec_write: timeout, status=%8.8X",inl(card->port + ES_REG_STATUS));
  return;
 }
 outw((card->port + ES_REG_1370_CODEC), ES_1370_CODEC_WRITE(reg, val));
 if (reg < sizeof(card->regs))
  card->regs[reg] = (uint8_t)val;
}

/* Raw/blind codec write - does NOT wait for ES_1370_CSTAT to clear first.
 *
 * CRITICAL: right after power-up, the AK4531's busy status is stuck/
 * undefined until the very first reset pulse below is sent - waiting for
 * CSTAT to clear before that pulse times out forever on real hardware
 * (confirmed by testing), because the codec never reports "not busy"
 * while it has never been taken out of its power-on reset state. Real
 * drivers (ALSA's snd_ensoniq_1370_mixer(), OSS's apci_mixer_reset())
 * send this initial pulse blind, with a dummy read back to flush the
 * write, before ever doing a normal (busy-checked) codec write.
 */
static void es1370_codec_write_raw(struct es1370_card_s *card, unsigned short reg, unsigned short val)
{
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_codec_write_raw(%X,%X)",reg,val);
 outw((card->port + ES_REG_1370_CODEC), ES_1370_CODEC_WRITE(reg, val));
 inw(card->port + ES_REG_1370_CODEC); /* dummy read, flushes the write on the PCI bus */
 pds_delay_10us(10); /* ~100us */
}

/* default power-up map of the AK4531 (all channels muted, max attenuation);
 * see ALSA's snd_ak4531_initial_map[]. Index = AK4531 register number.
 */
static const uint8_t es1370_ak4531_initial_map[0x1a] = {
 0x9f,0x9f, /* 00-01: master vol L/R  */
 0x9f,0x9f, /* 02-03: voice(PCM) vol L/R */
 0x9f,0x9f, /* 04-05: FM vol L/R */
 0x9f,0x9f, /* 06-07: CD vol L/R */
 0x9f,0x9f, /* 08-09: line vol L/R */
 0x9f,0x9f, /* 0a-0b: aux vol L/R */
 0x9f,      /* 0c: mono1 vol */
 0x9f,      /* 0d: mono2 vol */
 0x9f,      /* 0e: mic vol */
 0x87,      /* 0f: mono-out vol */
 0x00,      /* 10: output mixer switch 1 */
 0x00,      /* 11: output mixer switch 2 */
 0x00,0x00, /* 12-13: input mixer switch 1 L/R */
 0x00,0x00, /* 14-15: input mixer switch 2 L/R */
 0x00,      /* 16: reset & power down */
 0x00,      /* 17: clock select */
 0x00,      /* 18: AD input select */
 0x01       /* 19: mic amp gain */
};

static void es1370_ak4531_init(struct es1370_card_s *card)
{
 int idx;

 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_ak4531_init: enter");

 /* step 1: blind reset pulse (bypasses the CSTAT busy-wait - see
  * es1370_codec_write_raw() above). Without this exact pulse,
  * ES_1370_CSTAT stays stuck set from power-up forever, so
  * es1370_codec_write()'s busy-wait times out on every single
  * register write attempt and nothing done afterwards has any
  * effect (confirmed by testing).
  */
 es1370_codec_write_raw(card, AK4531_RESET, 0x02); /* assert reset pulse */
 es1370_codec_write_raw(card, AK4531_RESET, 0x03); /* release: not in reset, powered on */

 /* step 2: now that the codec responds, the normal (busy-checked)
  * path works. Brought out of reset AND powered up, with a short
  * settle delay, before any other register write.
  */
 es1370_codec_write(card, AK4531_RESET, AK4531_RESET_NORMAL);
 pds_delay_10us(10); /* ~100us settle time */
 es1370_codec_write(card, AK4531_CLOCK, 0x00);

 for (idx = 0; idx < 0x1a; idx++) {
  if (idx == AK4531_RESET || idx == AK4531_CLOCK)
   continue; /* already programmed above - don't undo it */
  es1370_codec_write(card, idx, es1370_ak4531_initial_map[idx]);
 }

 /* unmute master and PCM(voice/DAC) channels at 0dB (loudest); the
  * actual volume is set right afterwards by the generic mixer-init
  * code (via the mixer API/-VOL), which only ever touches the 5
  * attenuation bits and leaves the mute bit as programmed here.
  */
 es1370_codec_write(card, AK4531_LMASTER, 0x00);
 es1370_codec_write(card, AK4531_RMASTER, 0x00);
 es1370_codec_write(card, AK4531_LVOICE,  0x00);
 es1370_codec_write(card, AK4531_RVOICE,  0x00);

 /* route the PCM(voice/DAC) channel into the output mix - without
  * this, DAC audio stays silent no matter what the volumes are.
  */
 es1370_codec_write(card, AK4531_OUT_SW2, AK4531_OUTSW2_PCM_L | AK4531_OUTSW2_PCM_R);
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_ak4531_init: exit");
}

//-------------------------------------------------------------------------

static void es1370_dac2_rate(struct es1370_card_s *card, unsigned int rate)
{
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_dac2_rate(%d)",rate);
 if (rate < 4000)
  rate = 4000;
 else if (rate > 48000)
  rate = 48000;
 funcbit_disable(card->ctrl,ES_1370_PCLKDIVM);
 funcbit_enable(card->ctrl,ES_1370_PCLKDIVO(ES_1370_SRTODIV(rate)));
 outl((card->port + ES_REG_CONTROL), card->ctrl);
}

//-------------------------------------------------------------------------

static unsigned int es1370_buffer_init(struct es1370_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int bytes_per_sample=2; // 16 bit
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,ES1370_DMABUF_ALIGN,bytes_per_sample,0);
 card->dm=MDma_alloc_cardmem(card->pcmout_bufsize);
 if(!card->dm)
  return 0;
 card->pcmout_buffer=(char *)card->dm->linearptr;
 aui->card_DMABUFF=card->pcmout_buffer;

 /* dummy 16-byte buffer for the ADC "phantom" DMA erratum - without a
  * valid target, the ES1370 can perform stray bus-master writes into
  * random memory even though the ADC channel is never enabled by this
  * driver (documented hardware bug, see comment in es1370_chip_init()).
  */
 card->dm_phantom=MDma_alloc_cardmem(16);
 if(!card->dm_phantom)
  return 0;

 mpxplay_debugf(ENS_DEBUG_OUTPUT,"buffer init: pcmoutbuf:%8.8X size:%d",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
 return 1;
}

static void es1370_chip_init(struct es1370_card_s *card)
{
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_chip_init: enter");
 outl((card->port + ES_REG_CONTROL), card->ctrl);
 outl((card->port + ES_REG_SERIAL), card->sctrl);

 /* program the (unused) ADC "phantom" target - see es1370_buffer_init() */
 outl((card->port + ES_REG_MEM_PAGE), ES_MEM_PAGEO(ES_PAGE_ADC));
 outl((card->port + ES_REG_PHANTOM_FRAME), (unsigned long)pds_cardmem_physicalptr(card->dm_phantom, card->dm_phantom->linearptr));
 outl((card->port + ES_REG_PHANTOM_COUNT), 0);

 es1370_ak4531_init(card);
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_chip_init: exit");
}

static void es1370_chip_close(struct es1370_card_s *card)
{
 if(card->port)
 {
  outl((card->port + ES_REG_CONTROL), ES_1370_SERR_DISABLE); /* switch (almost) everything off */
  outl((card->port + ES_REG_SERIAL), 0);
 }
}

static void es1370_prepare_playback(struct es1370_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_prepare_playback: enter, dmasize=%d",aui->card_dmasize);
 funcbit_disable(card->ctrl,ES_DAC2_EN);
 outl((card->port + ES_REG_CONTROL), card->ctrl);
 outl((card->port + ES_REG_MEM_PAGE), ES_MEM_PAGEO(ES_PAGE_DAC));

 /* DAC2: FRAME=port 38h, SIZE=port 3Ch, COUNT=port 28h */
 outl((card->port + ES_REG_DAC2_FRAME), (unsigned long) pds_cardmem_physicalptr(card->dm, card->pcmout_buffer));
 outl((card->port + ES_REG_DAC2_SIZE), (aui->card_dmasize >> 2) - 1);

#if SBEMU
 {
  int periods = max(1,aui->card_dmasize / ES1370_DMABUF_ALIGN);
  outl((card->port + ES_REG_DAC2_COUNT), ((aui->card_dmasize/periods) >> 2) - 1);
  aui->card_samples_per_int = ES1370_DMABUF_ALIGN / 4; //used for SB direct mode
 }
#else
 outl((card->port + ES_REG_DAC2_COUNT), (aui->card_dmasize >> 2) - 1);
#endif

 funcbit_disable(card->sctrl,(ES_P2_LOOP_SEL | ES_P2_PAUSE | ES_P2_DAC_SEN | ES_P2_END_INCM | ES_P2_ST_INCM | (0x03<<2)));
 funcbit_enable(card->sctrl,ES_P2_MODEO(0x03) | ES_P2_END_INCO(2) | ES_P2_ST_INCO(0)); // stereo, 16 bits
 outl((card->port + ES_REG_SERIAL), card->sctrl);

 es1370_dac2_rate(card, aui->freq_card);
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"es1370_prepare_playback: exit");
}

//-------------------------------------------------------------------------
static pci_device_s es1370_devices[]={
 {"ES1370",0x1274,0x5000, 0},
 {NULL,0,0,0}
};

static void ES1370_close(struct mpxplay_audioout_info_s *aui);

static void ES1370_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"Ensoniq ES1370 : found on port:%4.4X irq:%d",card->port,card->irq);
 pds_textdisplay_printf(sout);
}

static int ES1370_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card;

 card=(struct es1370_card_s *)pds_calloc(1,sizeof(struct es1370_card_s));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(es1370_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 mpxplay_debugf(ENS_DEBUG_OUTPUT,"ES1370_adetect: known card found, enable PCI io and busmaster");
 pcibios_set_master(card->pci_dev);

 card->port = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
 if(!card->port)
  goto err_adetect;
 aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
#ifdef SBEMU
 aui->card_pci_dev = card->pci_dev;
#endif

 mpxplay_debugf(ENS_DEBUG_OUTPUT,"ES1370_adetect: vend_id:%4.4X dev_id:%4.4X port:%8.8X irq:%d",
  card->pci_dev->vendor_id,card->pci_dev->device_id,card->port,card->irq);
 card->port&=0xfff0;

 /* ctrl: enable the AK4531 codec interface, the joystick/gameport
  * interface (pure hardware pass-through of the standard ISA gameport
  * protocol at port 201h - no driver-side joystick logic needed), and
  * DAC2's clock source = programmable divider (default: 8kHz until
  * es1370_dac2_rate() runs).
  */
 card->ctrl = ES_1370_CDC_EN | ES_JYSTK_EN | ES_1370_PCLKDIVO(ES_1370_SRTODIV(8000));
 card->sctrl = 0;

 if(!es1370_buffer_init(card,aui))
  goto err_adetect;

 es1370_chip_init(card);

 return 1;

err_adetect:
 ES1370_close(aui);
 return 0;
}

static void ES1370_close(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;
 if(card){
  es1370_chip_close(card);
  if(card->dm)
   MDma_free_cardmem(card->dm);
  if(card->dm_phantom)
   MDma_free_cardmem(card->dm_phantom);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void ES1370_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;

 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 aui->chan_card=2;
 aui->bits_card=16;

 if(aui->freq_card<4000)
  aui->freq_card=4000;
 else if(aui->freq_card>48000)
  aui->freq_card=48000;

 MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,ES1370_DMABUF_ALIGN,0);

 es1370_prepare_playback(card,aui);
}

static void ES1370_start(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;
 funcbit_enable(card->ctrl,ES_DAC2_EN);
 outl(card->port + ES_REG_CONTROL, card->ctrl);
 funcbit_disable(card->sctrl,ES_P2_PAUSE);
#ifdef SBEMU
 funcbit_enable(card->sctrl,ES_P2_INT_EN); //enable interrupt for DAC2
#endif
 outl(card->port + ES_REG_SERIAL, card->sctrl);
}

static void ES1370_stop(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;
 funcbit_enable(card->sctrl,ES_P2_PAUSE);
#ifdef SBEMU
 funcbit_disable(card->sctrl,ES_P2_INT_EN);
#endif
 outl(card->port + ES_REG_SERIAL, card->sctrl);
}

//-------------------------------------------------------------------------

static long ES1370_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct es1370_card_s *card=aui->card_private_data;
 unsigned long bufpos=0;
 if(inl(card->port + ES_REG_CONTROL) & ES_DAC2_EN) {
  outl((card->port + ES_REG_MEM_PAGE), ES_MEM_PAGEO(ES_PAGE_DAC));
  bufpos = ES_REG_FCURR_COUNTI(inl(card->port + ES_REG_DAC2_SIZE));
  if(bufpos<aui->card_dmasize)
   aui->card_dma_lastgoodpos=bufpos;
 }
 mpxplay_debugf(ENS_DEBUG_OUTPUT,"bufpos:%5d gpos:%5d dmasize:%5d",bufpos,aui->card_dma_lastgoodpos,aui->card_dmasize);

 return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------
//mixer - AK4531 is write-only, so "reading" a register just returns the
//cached value written earlier (see es1370_codec_write()).

static void ES1370_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct es1370_card_s *card=aui->card_private_data;
 es1370_codec_write(card,(unsigned short)reg,(unsigned short)val);
}

static unsigned long ES1370_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct es1370_card_s *card=aui->card_private_data;
 if (reg < sizeof(card->regs))
  return card->regs[reg];
 return 0;
}

#ifdef SBEMU
static int ES1370_IRQRoutine(mpxplay_audioout_info_s* aui)
{
 es1370_card_s *card=aui->card_private_data;
 int status = inl(card->port + ES_REG_STATUS);
 if(status&ES_DAC2)
 {
  //clear DAC2 interrupt status by the spec
  outl(card->port + ES_REG_SERIAL, card->sctrl&(~ES_P2_INT_EN));
  outl(card->port + ES_REG_SERIAL, card->sctrl); //re-enable
 }
 return (status&ES_INTR);
}
#endif

one_sndcard_info ES1370_sndcard_info={
 "Ensoniq ES1370",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                 // no init
 &ES1370_adetect,      // only autodetect
 &ES1370_card_info,
 &ES1370_start,
 &ES1370_stop,
 &ES1370_close,
 &ES1370_setrate,

 &MDma_writedata,
 &ES1370_getbufpos,
 &MDma_clearbuf,
 &MDma_interrupt_monitor,
 #ifdef SBEMU
 &ES1370_IRQRoutine,
 #else
 NULL,
 #endif

 &ES1370_writeMIXER,
 &ES1370_readMIXER,
 &mpxplay_aucards_ak4531chan_mixerset[0]
};

#endif // AU_CARDS_LINK_ES1370
