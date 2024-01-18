//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2014 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Intel HD audio driver for Mpxplay
//based on ALSA (http://www.alsa-project.org) and WSS libs

//#define MPXPLAY_USE_DEBUGF 1
#define IHD_DEBUG_OUTPUT stdout

#include "au_cards.h"

#ifdef SBEMU //AU_CARDS_LINK_YMF

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dpmi.h"
#include "pic.h"
#include "../hdpmipt.h"
#include "fw_ymf.h"
#include <dpmi.h>
#include <sys/nearptr.h>
#include <sys/exceptn.h>
//extern unsigned long __djgpp_selector_limit;

#define YDSXG_PLAYBACK_VOICES		64
#define YDSXG_CAPTURE_VOICES		2
#define YDSXG_EFFECT_VOICES		5
#define YDSXG_DSPLENGTH			0x0080
#define YDSXG_CTRLLENGTH		0x3000
#define YDSXG_DEFAULT_WORK_SIZE		0x0400

typedef unsigned long dma_addr_t;

struct snd_ymfpci_playback_bank {
	uint32_t format;
	uint32_t loop_default;
	uint32_t base;			/* 32-bit address */
	uint32_t loop_start;			/* 32-bit offset */
	uint32_t loop_end;			/* 32-bit offset */
	uint32_t loop_frac;			/* 8-bit fraction - loop_start */
	uint32_t delta_end;			/* pitch delta end */
	uint32_t lpfK_end;
	uint32_t eg_gain_end;
	uint32_t left_gain_end;
	uint32_t right_gain_end;
	uint32_t eff1_gain_end;
	uint32_t eff2_gain_end;
	uint32_t eff3_gain_end;
	uint32_t lpfQ;
	uint32_t status;
	uint32_t num_of_frames;
	uint32_t loop_count;
	uint32_t start;
	uint32_t start_frac;
	uint32_t delta;
	uint32_t lpfK;
	uint32_t eg_gain;
	uint32_t left_gain;
	uint32_t right_gain;
	uint32_t eff1_gain;
	uint32_t eff2_gain;
	uint32_t eff3_gain;
	uint32_t lpfD1;
	uint32_t lpfD2;
 };

struct snd_ymfpci_capture_bank {
	uint32_t base;			/* 32-bit address */
	uint32_t loop_end;			/* 32-bit offset */
	uint32_t start;			/* 32-bit offset */
	uint32_t num_of_loops;		/* counter */
};

struct snd_ymfpci_effect_bank {
	uint32_t base;			/* 32-bit address */
	uint32_t loop_end;			/* 32-bit offset */
	uint32_t start;			/* 32-bit offset */
	uint32_t temp;
};

enum snd_ymfpci_voice_type {
	YMFPCI_PCM,
	YMFPCI_SYNTH,
	YMFPCI_MIDI
};

struct snd_ymfpci_voice {
	struct snd_ymfpci *chip;
	int number;
	unsigned int use: 1,
	    pcm: 1,
	    synth: 1,
	    midi: 1;
	struct snd_ymfpci_playback_bank *bank;
	dma_addr_t bank_addr;
	void (*interrupt)(struct snd_ymfpci *chip, struct snd_ymfpci_voice *voice);
	struct snd_ymfpci_pcm *ypcm;
};

struct ymf_card_s {
  unsigned long pci_iobase;
  unsigned long iobase;
  //unsigned long iobase2;
  //unsigned long iobase3;
  unsigned long legacy_iobase;
  struct pci_config_s  *pci_dev;
  cardmem_t *dm;
  cardmem_t *dm2;
  unsigned int config_select;
  int irq;
  void *bank_base_playback;
  void *bank_base_capture;
  void *bank_base_effect;
  void *work_base;
  unsigned int bank_size_playback;
  unsigned int bank_size_capture;
  unsigned int bank_size_effect;
  unsigned int work_size;
  dma_addr_t bank_base_playback_addr;
  dma_addr_t bank_base_capture_addr;
  dma_addr_t bank_base_effect_addr;
  dma_addr_t work_base_addr;
  uint32_t *ctrl_playback;
  struct snd_ymfpci_playback_bank *bank_playback[YDSXG_PLAYBACK_VOICES][2];
  struct snd_ymfpci_capture_bank *bank_capture[YDSXG_CAPTURE_VOICES][2];
  struct snd_ymfpci_effect_bank *bank_effect[YDSXG_EFFECT_VOICES][2];
  struct snd_ymfpci_voice voices[64];
  unsigned short spdif_bits, spdif_pcm_bits;
};

static pci_device_s ymf_devices[] = {
  {"Yamaha YMF724",               0x1073, 0x0004, 0x724 },
  {"Yamaha YMF724F",              0x1073, 0x000d, 0x724F },
  {"Yamaha YMF740",               0x1073, 0x000a, 0x740 },
  {"Yamaha YMF740C",              0x1073, 0x000c, 0x740C },
  {"Yamaha YMF744",               0x1073, 0x0010, 0x744 },
  {"Yamaha YMF754",               0x1073, 0x0012, 0x754 },
  {NULL,0,0,0}
};

#define	YDSXGR_INTFLAG			0x0004
#define	YDSXGR_ACTIVITY			0x0006
#define	YDSXGR_GLOBALCTRL		0x0008
#define	YDSXGR_ZVCTRL			0x000A
#define	YDSXGR_TIMERCTRL		0x0010
#define	YDSXGR_TIMERCOUNT		0x0012
#define	YDSXGR_SPDIFOUTCTRL		0x0018
#define	YDSXGR_SPDIFOUTSTATUS		0x001C
#define	YDSXGR_EEPROMCTRL		0x0020
#define	YDSXGR_SPDIFINCTRL		0x0034
#define	YDSXGR_SPDIFINSTATUS		0x0038
#define	YDSXGR_DSPPROGRAMDL		0x0048
#define	YDSXGR_DLCNTRL			0x004C
#define	YDSXGR_GPIOININTFLAG		0x0050
#define	YDSXGR_GPIOININTENABLE		0x0052
#define	YDSXGR_GPIOINSTATUS		0x0054
#define	YDSXGR_GPIOOUTCTRL		0x0056
#define	YDSXGR_GPIOFUNCENABLE		0x0058
#define	YDSXGR_GPIOTYPECONFIG		0x005A
#define	YDSXGR_AC97CMDDATA		0x0060
#define	YDSXGR_AC97CMDADR		0x0062
#define	YDSXGR_PRISTATUSDATA		0x0064
#define	YDSXGR_PRISTATUSADR		0x0066
#define	YDSXGR_SECSTATUSDATA		0x0068
#define	YDSXGR_SECSTATUSADR		0x006A
#define	YDSXGR_SECCONFIG		0x0070
#define	YDSXGR_LEGACYOUTVOL		0x0080
#define	YDSXGR_LEGACYOUTVOLL		0x0080
#define	YDSXGR_LEGACYOUTVOLR		0x0082
#define	YDSXGR_NATIVEDACOUTVOL		0x0084
#define	YDSXGR_NATIVEDACOUTVOLL		0x0084
#define	YDSXGR_NATIVEDACOUTVOLR		0x0086
#define	YDSXGR_ZVOUTVOL			0x0088
#define	YDSXGR_ZVOUTVOLL		0x0088
#define	YDSXGR_ZVOUTVOLR		0x008A
#define	YDSXGR_SECADCOUTVOL		0x008C
#define	YDSXGR_SECADCOUTVOLL		0x008C
#define	YDSXGR_SECADCOUTVOLR		0x008E
#define	YDSXGR_PRIADCOUTVOL		0x0090
#define	YDSXGR_PRIADCOUTVOLL		0x0090
#define	YDSXGR_PRIADCOUTVOLR		0x0092
#define	YDSXGR_LEGACYLOOPVOL		0x0094
#define	YDSXGR_LEGACYLOOPVOLL		0x0094
#define	YDSXGR_LEGACYLOOPVOLR		0x0096
#define	YDSXGR_NATIVEDACLOOPVOL		0x0098
#define	YDSXGR_NATIVEDACLOOPVOLL	0x0098
#define	YDSXGR_NATIVEDACLOOPVOLR	0x009A
#define	YDSXGR_ZVLOOPVOL		0x009C
#define	YDSXGR_ZVLOOPVOLL		0x009E
#define	YDSXGR_ZVLOOPVOLR		0x009E
#define	YDSXGR_SECADCLOOPVOL		0x00A0
#define	YDSXGR_SECADCLOOPVOLL		0x00A0
#define	YDSXGR_SECADCLOOPVOLR		0x00A2
#define	YDSXGR_PRIADCLOOPVOL		0x00A4
#define	YDSXGR_PRIADCLOOPVOLL		0x00A4
#define	YDSXGR_PRIADCLOOPVOLR		0x00A6
#define	YDSXGR_NATIVEADCINVOL		0x00A8
#define	YDSXGR_NATIVEADCINVOLL		0x00A8
#define	YDSXGR_NATIVEADCINVOLR		0x00AA
#define	YDSXGR_NATIVEDACINVOL		0x00AC
#define	YDSXGR_NATIVEDACINVOLL		0x00AC
#define	YDSXGR_NATIVEDACINVOLR		0x00AE
#define	YDSXGR_BUF441OUTVOL		0x00B0
#define	YDSXGR_BUF441OUTVOLL		0x00B0
#define	YDSXGR_BUF441OUTVOLR		0x00B2
#define	YDSXGR_BUF441LOOPVOL		0x00B4
#define	YDSXGR_BUF441LOOPVOLL		0x00B4
#define	YDSXGR_BUF441LOOPVOLR		0x00B6
#define	YDSXGR_SPDIFOUTVOL		0x00B8
#define	YDSXGR_SPDIFOUTVOLL		0x00B8
#define	YDSXGR_SPDIFOUTVOLR		0x00BA
#define	YDSXGR_SPDIFLOOPVOL		0x00BC
#define	YDSXGR_SPDIFLOOPVOLL		0x00BC
#define	YDSXGR_SPDIFLOOPVOLR		0x00BE
#define	YDSXGR_ADCSLOTSR		0x00C0
#define	YDSXGR_RECSLOTSR		0x00C4
#define	YDSXGR_ADCFORMAT		0x00C8
#define	YDSXGR_RECFORMAT		0x00CC
#define	YDSXGR_P44SLOTSR		0x00D0
#define	YDSXGR_STATUS			0x0100
#define	YDSXGR_CTRLSELECT		0x0104
#define	YDSXGR_MODE			0x0108
#define	YDSXGR_SAMPLECOUNT		0x010C
#define	YDSXGR_NUMOFSAMPLES		0x0110
#define	YDSXGR_CONFIG			0x0114
#define	YDSXGR_PLAYCTRLSIZE		0x0140
#define	YDSXGR_RECCTRLSIZE		0x0144
#define	YDSXGR_EFFCTRLSIZE		0x0148
#define	YDSXGR_WORKSIZE			0x014C
#define	YDSXGR_MAPOFREC			0x0150
#define	YDSXGR_MAPOFEFFECT		0x0154
#define	YDSXGR_PLAYCTRLBASE		0x0158
#define	YDSXGR_RECCTRLBASE		0x015C
#define	YDSXGR_EFFCTRLBASE		0x0160
#define	YDSXGR_WORKBASE			0x0164
#define	YDSXGR_DSPINSTRAM		0x1000
#define	YDSXGR_CTRLINSTRAM		0x4000

#define YDSXG_AC97READCMD		0x8000
#define YDSXG_AC97WRITECMD		0x0000

#define PCIR_DSXG_LEGACY		0x40
#define PCIR_DSXG_ELEGACY		0x42
#define PCIR_DSXG_CTRL			0x48
#define PCIR_DSXG_PWRCTRL1		0x4a
#define PCIR_DSXG_PWRCTRL2		0x4e
#define PCIR_DSXG_FMBASE		0x60
#define PCIR_DSXG_SBBASE		0x62
#define PCIR_DSXG_MPU401BASE		0x64
#define PCIR_DSXG_JOYBASE		0x66

#define YMFPCI_LEGACY_SBEN	(1 << 0)	/* soundblaster enable */
#define YMFPCI_LEGACY_FMEN	(1 << 1)	/* OPL3 enable */
#define YMFPCI_LEGACY_JPEN	(1 << 2)	/* joystick enable */
#define YMFPCI_LEGACY_MEN	(1 << 3)	/* MPU401 enable */
#define YMFPCI_LEGACY_MIEN	(1 << 4)	/* MPU RX irq enable */
#define YMFPCI_LEGACY_IOBITS	(1 << 5)	/* i/o bits range, 0 = 16bit, 1 =10bit */
#define YMFPCI_LEGACY_SDMA	(3 << 6)	/* SB DMA select */
#define YMFPCI_LEGACY_SBIRQ	(7 << 8)	/* SB IRQ select */
#define YMFPCI_LEGACY_MPUIRQ	(7 << 11)	/* MPU IRQ select */
#define YMFPCI_LEGACY_SIEN	(1 << 14)	/* serialized IRQ */
#define YMFPCI_LEGACY_LAD	(1 << 15)	/* legacy audio disable */

#define YMFPCI_LEGACY2_FMIO	(3 << 0)	/* OPL3 i/o address (724/740) */
#define YMFPCI_LEGACY2_SBIO	(3 << 2)	/* SB i/o address (724/740) */
#define YMFPCI_LEGACY2_MPUIO	(3 << 4)	/* MPU401 i/o address (724/740) */
#define YMFPCI_LEGACY2_JSIO	(3 << 6)	/* joystick i/o address (724/740) */
#define YMFPCI_LEGACY2_MAIM	(1 << 8)	/* MPU401 ack intr mask */
#define YMFPCI_LEGACY2_SMOD	(3 << 11)	/* SB DMA mode */
#define YMFPCI_LEGACY2_SBVER	(3 << 13)	/* SB version select */
#define YMFPCI_LEGACY2_IMOD	(1 << 15)	/* legacy IRQ mode */
/* SIEN:IMOD 0:0 = legacy irq, 0:1 = INTA, 1:0 = serialized IRQ */

static void YMF_close(struct mpxplay_audioout_info_s *aui)
{
#if 0
  struct ymf_card_s *card=aui->card_private_data;
  if (card) {
    if (card->iobase) {
      pds_dpmi_unmap_physycal_memory(card->iobase);
    }
  }
  if (card->dm)
    MDma_free_cardmem(card->dm);
  if(card->pci_dev)
    pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data = NULL;
#endif
}

#define linux_writel(reg,value) PDS_PUTB_LE32((char *)(addr),value)
#define linux_readl(reg) PDS_GETB_LE32((char *)(addr))
#define linux_writew(reg,value) PDS_PUTB_LE16((char *)(addr), value)
#define linux_readw(reg) PDS_GETB_LE16((char *)(addr))
#define linux_writeb(reg,value) *((unsigned char *)(addr))=value
#define linux_readb(reg) PDS_GETB_8U((char *)(addr))

static uint32_t regtab[128]; // 0x8000 / 0x100
static unsigned int last_pg = 0;

uint32_t g_iobase = 0;

static inline uint32_t mapreg (struct ymf_card_s *card, uint32_t offset) {
  if (offset < 0x4000)
    return card->iobase + offset;
  else
    return g_iobase - 0x4000 + offset;
  //if (offset < 0x100) return card->iobase + offset;
  //if (offset < 0x200) return card->iobase2 + offset - 0x100;
  //return card->iobase3 + offset - 0x4000;
  //if (offset < 0x4000) return card->iobase + offset;
  //if (offset < 0x200) return card->iobase2 + offset - 0x100;
  //return card->iobase3 + offset - 0x4000;
#if 0
  unsigned int pg = offset / 0x100;
  if (regtab[pg] == 0) {
    regtab[pg] = pds_dpmi_map_physical_memory(card->pci_iobase + pg*0x100, 0x100);
#if 1
    if (regtab[pg] == 0) {
      printf("failed to map page %u offset 0x%X\n", pg, offset);
    } else {
      //printf("mapped pg %u offset 0x%X to 0x%X\n", pg, offset, regtab[pg]);
    }
#endif
    if (last_pg && pg != last_pg) {
      pds_dpmi_unmap_physycal_memory(regtab[last_pg]);
    }
    last_pg = pg;
  }
  return regtab[pg] + offset - pg*0x100;
#endif
}

static inline uint16_t snd_ymfpci_readw (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint16_t r = linux_readw(addr);
  //if (offset && offset != 0x66) printf("%4.4X: %8.8X\n", offset, r);
  return r;
}

static inline void snd_ymfpci_writew (struct ymf_card_s *card, uint32_t offset, uint16_t val)
{
  uint32_t addr = mapreg(card, offset);
  linux_writew(addr, val);
  //*((uint16_t *)addr) = val;
}

static inline uint32_t snd_ymfpci_readl (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint32_t r = linux_readl(addr);
  //if (offset && offset != 0x100) printf("%4.4X: %8.8X\n", offset, r);
  return r;
}

static inline void snd_ymfpci_writel (struct ymf_card_s *card, uint32_t offset, uint32_t val)
{
  uint32_t addr = mapreg(card, offset);
  //outl(addr, val);
  //*((uint32_t *)addr) = val;
  linux_writel(addr, val);
}

static inline uint8_t snd_ymfpci_readb (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint8_t r = linux_readb(addr);
  //if (offset && offset != 0x100) printf("%4.4X: %8.8X\n", offset, r);
  return r;
}

static void snd_ymfpci_aclink_reset (struct ymf_card_s *card)
{
  uint8_t cmd;

  cmd = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_DSXG_CTRL);
  //printf("cmd: %x\n", cmd);
  if ((cmd & 0x03) || 1==1) {
    pcibios_WriteConfig_Byte(card->pci_dev, PCIR_DSXG_CTRL, cmd & 0xfc);
    pds_delay_10us(10*50);
    pcibios_WriteConfig_Byte(card->pci_dev, PCIR_DSXG_CTRL, cmd | 0x03);
    pds_delay_10us(10*50);
    pcibios_WriteConfig_Byte(card->pci_dev, PCIR_DSXG_CTRL, cmd & 0xfc);
    pds_delay_10us(10*50);
    pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_PWRCTRL1, 0);
    pds_delay_10us(10*50);
    pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_PWRCTRL2, 0);
    pds_delay_10us(10*50);
  }
}

static void snd_ymfpci_enable_dsp (struct ymf_card_s *card)
{
  snd_ymfpci_writel(card, YDSXGR_CONFIG, 0x00000001);
}

static void snd_ymfpci_disable_dsp(struct ymf_card_s *card)
{
  uint32_t val;
  int timeout = 1000;

  val = snd_ymfpci_readl(card, YDSXGR_CONFIG);
  if (val)
    snd_ymfpci_writel(card, YDSXGR_CONFIG, 0x00000000);
  while (timeout-- > 0) {
    val = snd_ymfpci_readl(card, YDSXGR_STATUS);
    if ((val & 0x00000002) == 0)
      break;
  }
}

unsigned int ymf_int_cnt = 0;
static unsigned int xint_cnt = 0;

static int
install_ucode (struct ymf_card_s *card, uint32_t addr, uint32_t *src, uint32_t len)
{
  int i;
  for (i = 0; i < len; i++) {
    //printf("int cnt %u %u\n", xint_cnt, ymf_int_cnt);
    uint32_t val = *src;
    //printf("write ucode to addr %8.8X val: %8.8X\n", addr, val);
    snd_ymfpci_writel(card, addr, val);
    //printf("done\n");
    uint32_t rval = snd_ymfpci_readl(card, addr);
    if (rval != val) {
      printf("failed to write ucode addr %8.8X val: %8.8X rval: %8.8X\n", addr, val, rval);
      return 0;
    }
    addr += 4;
    src++;
  }
  return 1;
}

static void snd_ymfpci_download_image (struct ymf_card_s *card)
{
  int i;
  uint16_t ctrl;

  snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x00000000);
  snd_ymfpci_disable_dsp(card);
  snd_ymfpci_writel(card, YDSXGR_MODE, 0x00010000);
  snd_ymfpci_writel(card, YDSXGR_MODE, 0x00000000);
  snd_ymfpci_writel(card, YDSXGR_MAPOFREC, 0x00000000);
  snd_ymfpci_writel(card, YDSXGR_MAPOFEFFECT, 0x00000000);
  snd_ymfpci_writel(card, YDSXGR_PLAYCTRLBASE, 0x00000000);
  snd_ymfpci_writel(card, YDSXGR_RECCTRLBASE, 0x00000000);
  snd_ymfpci_writel(card, YDSXGR_EFFCTRLBASE, 0x00000000);
  ctrl = snd_ymfpci_readw(card, YDSXGR_GLOBALCTRL);
  snd_ymfpci_writew(card, YDSXGR_GLOBALCTRL, ctrl & ~0x0007);

  if (install_ucode(card, 0x1000, dsp, dsp_size))
    printf("installed dsp FW\n");

  void *buf = pds_malloc(0x4000);
  if (buf == NULL) {
    printf("could not allocate buf\n");
    return;
  }
  int mr = __djgpp_map_physical_memory(buf, 0x4000, card->pci_iobase + 0x4000);
  //printf("mr: %d\n", mr); // XXX fails
  g_iobase = (uint32_t)buf;
  
  switch (card->pci_dev->device_type) {
  case 0x724F:
  case 0x740C:
  case 0x744:
  case 0x754:
    if (install_ucode(card, YDSXGR_CTRLINSTRAM, cntrl1E, cntrl1E_size)) {
      //printf("installed cntrl1E FW\n");
    }
    break;
  default:
    if (install_ucode(card, YDSXGR_CTRLINSTRAM, cntrl, cntrl_size)) {
      //printf("installed cntrl FW\n");
    }
    break;
  }

  pds_free(buf);

  snd_ymfpci_enable_dsp(card);
}

static int snd_ymfpci_codec_ready (struct ymf_card_s *card, int secondary)
{
  uint32_t reg = secondary ? YDSXGR_SECSTATUSADR : YDSXGR_PRISTATUSADR;
  int timeout = 7500; // 750ms
  do {
    uint8_t r0 = inp(card->iobase+reg);
    uint8_t r1 = inp(card->iobase+reg+1);
    uint16_t r = snd_ymfpci_readw(card, reg);
    if ((r & 0x8000) == 0)
      return 0;
    pds_delay_10us(10);
  } while (--timeout);
  //DBG_Log("YMF not ready\n");
  return -1;
}

static int wanted_irq = 0;
static struct ymf_card_s *g_card = NULL;

static int ymfirq (struct ymf_card_s *card)
{
  ymf_int_cnt++;
  uint32_t status, nvoice, mode;
  struct snd_ymfpci_voice *voice;
  int handled = 0;

  status = snd_ymfpci_readl(card, YDSXGR_STATUS);
  //if ((ymf_int_cnt % 100) == 0)
  //  DBG_Log("%u status: %8.8X\n", ymf_int_cnt, status);
  if (status & 0x80000000) {
    handled = 1;
    uint32_t active_bank = snd_ymfpci_readl(card, YDSXGR_CTRLSELECT) & 1;
    //if ((ymf_int_cnt % 10000) == 0)
    //  DBG_Log("bank: %8.8X\n", active_bank);
    //spin_lock(&card->voice_lock);
    //for (nvoice = 0; nvoice < YDSXG_PLAYBACK_VOICES; nvoice++) {
    //  voice = &card->voices[nvoice];
    //  if (voice->interrupt)
    //    voice->interrupt(card, voice);
    //}
    //for (nvoice = 0; nvoice < YDSXG_CAPTURE_VOICES; nvoice++) {
    //  if (card->capture_substream[nvoice])
    //    snd_ymfpci_pcm_capture_interrupt(card->capture_substream[nvoice]);
    //}
    //spin_unlock(&card->voice_lock);
    //spin_lock(&card->reg_lock);
    snd_ymfpci_writel(card, YDSXGR_STATUS, 0x80000000);
    mode = snd_ymfpci_readl(card, YDSXGR_MODE) | 2;
    snd_ymfpci_writel(card, YDSXGR_MODE, mode);
    //spin_unlock(&card->reg_lock);
    
    //if (atomic_read(&card->interrupt_sleep_count)) {
    //  atomic_set(&card->interrupt_sleep_count, 0);
    //  wake_up(&card->interrupt_sleep);
    //}
  }
  
  status = snd_ymfpci_readw(card, YDSXGR_INTFLAG);
  //if ((ymf_int_cnt % 100) == 0)
  //  DBG_Log("%u status2: %8.8X\n", ymf_int_cnt, status);
  if (status & 1) {
    //if (card->timer)
    //  snd_timer_interrupt(card->timer, card->timer_ticks);
  }
  snd_ymfpci_writew(card, YDSXGR_INTFLAG, status);
  
  //if (status==0xFFFFFFFF) return 0;
  //if (card->rawmidi)
  //  snd_mpu401_uart_interrupt(irq, card->rawmidi->private_data);
  return handled;
}

static INTCONTEXT MAIN_IntContext;
static uint8_t MAIN_InINT;
static DPMI_ISR_HANDLE __IntHandlePM;
static void __inthandlerPM () {
  xint_cnt++;
    HDPMIPT_GetInterrupContext(&MAIN_IntContext);
    if(!MAIN_InINT && ymfirq(g_card)) //check if the irq belong the sound card
    {
      //MAIN_Interrupt();
      PIC_SendEOIWithIRQ(wanted_irq);
    }
    else
    {
        BOOL InInt = MAIN_InINT;
        MAIN_InINT = TRUE;
        if(MAIN_IntContext.EFLAGS&CPU_VMFLAG)
            DPMI_CallOldISR(&__IntHandlePM);
        else
            DPMI_CallOldISRWithContext(&__IntHandlePM, &MAIN_IntContext.regs);
        PIC_UnmaskIRQ(wanted_irq);
        MAIN_InINT = InInt;
    }
}

/* AC97 1.0 */
#define  AC97_RESET               0x0000      //
#define  AC97_MASTER_VOL_STEREO   0x0002      // Line Out
#define  AC97_HEADPHONE_VOL       0x0004      // 
#define  AC97_MASTER_VOL_MONO     0x0006      // TAD Output
#define  AC97_MASTER_TONE         0x0008      //
#define  AC97_PCBEEP_VOL          0x000a      // none
#define  AC97_PHONE_VOL           0x000c      // TAD Input (mono)
#define  AC97_MIC_VOL             0x000e      // MIC Input (mono)
#define  AC97_LINEIN_VOL          0x0010      // Line Input (stereo)
#define  AC97_CD_VOL              0x0012      // CD Input (stereo)
#define  AC97_VIDEO_VOL           0x0014      // none
#define  AC97_AUX_VOL             0x0016      // Aux Input (stereo)
#define  AC97_PCMOUT_VOL          0x0018      // Wave Output (stereo)
#define  AC97_RECORD_SELECT       0x001a      //
#define  AC97_RECORD_GAIN         0x001c
#define  AC97_RECORD_GAIN_MIC     0x001e
#define  AC97_GENERAL_PURPOSE     0x0020
#define  AC97_3D_CONTROL          0x0022
#define  AC97_MODEM_RATE          0x0024
#define  AC97_POWER_CONTROL       0x0026

struct initialValues
{
    unsigned short port;
    unsigned short value;
};

static struct initialValues ymfsb_ac97_initial_values[] =
{
    { AC97_RESET,             0x0000 },
    { AC97_MASTER_VOL_STEREO, 0x0000 }, // 0x0000 dflt
    { AC97_HEADPHONE_VOL,     0x0606 }, // 0x8000
    { AC97_PCMOUT_VOL,        0x0000 }, // 0x0606
    { AC97_MASTER_VOL_MONO,   0x0000 }, // 0x0000
    { AC97_PCBEEP_VOL,        0x0000 }, // 0x0000
    { AC97_PHONE_VOL,         0x0008 }, // 0x0008
    { AC97_MIC_VOL,           0x8000 }, // 0x8000
    { AC97_LINEIN_VOL,        0x0808 }, // 0x8808
    { AC97_CD_VOL,            0x0808 }, // 0x8808
    { AC97_VIDEO_VOL,         0x8808 }, // 0x8808
    { AC97_AUX_VOL,           0x8808 }, // 0x8808
    { AC97_RECORD_SELECT,     0x0000 },
    { AC97_RECORD_GAIN,       0x0B0B },
    { AC97_GENERAL_PURPOSE,   0x0000 }, // 0x0000
    { 0xffff, 0xffff }
};

static int ymfsb_writeAC97Reg (struct ymf_card_s *card, uint8_t reg, uint16_t value)
{
	unsigned long flags;

	if ( reg > 0x7f ) return -1;

	//save_flags(flags);
	//cli();
	//if ( checkPrimaryBusy( card ) ) {
	//  restore_flags(flags);
	//  return -EBUSY;
	//}

	snd_ymfpci_writel( card, YDSXGR_AC97CMDADR, 0x0000 | reg );
	snd_ymfpci_writel( card, YDSXGR_AC97CMDDATA, value );

	//restore_flags(flags);
	return 0;
}

static int ymfsb_resetAC97 (struct ymf_card_s *card)
{
	int i;

	for ( i=0 ; ymfsb_ac97_initial_values[i].port != 0xffff ; i++ )
	{
		ymfsb_writeAC97Reg ( card,
				    ymfsb_ac97_initial_values[i].port,
				    ymfsb_ac97_initial_values[i].value );
	}

	return 0;
}


extern uint16_t ymfbase;
extern uint16_t fmymfbase;

/**************************************
// stuff from vgmplay-legacy
#define DELAY_OPL2_REG	 1 //3.3f
#define DELAY_OPL2_DATA	3 //23.0f
#define DELAY_OPL3_REG	 0 //0.0f
//#define DELAY_OPL3_DATA	 0.28f	// fine for ISA cards (like SoundBlaster 16)
#define DELAY_OPL3_DATA	 2 //13.3f	// required for PCI cards (CMI8738)

uint8_t OPL_HW_GetStatus(void)
{
	uint8_t RetStatus;
	
	RetStatus = inp(fmymfbase);
	
	return RetStatus;
}

void OPL_HW_WaitDelay(int64_t StartTime, int Delay)
{
	OPL_HW_GetStatus();	// read once, then wait should work
        pds_delay_10us(Delay);
}

int OPL_MODE = 3;

void OPL_HW_WriteReg(uint16_t Reg, uint8_t Data)
{
  outb(fmymfbase + 0x00, Reg & 0xFF);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(0, DELAY_OPL2_REG);
		break;
	case 0x03:
		OPL_HW_WaitDelay(0, DELAY_OPL3_REG);
		break;
	}
	outp(fmymfbase + 0x01, Data);
	switch(OPL_MODE)
	{
	case 0x02:
		OPL_HW_WaitDelay(0, DELAY_OPL2_DATA);
		break;
	case 0x03:
		OPL_HW_WaitDelay(0, DELAY_OPL3_DATA);
		break;
	}
}
void OPL_Hardware_Detecton(void)
{
	uint8_t Status1;
	uint8_t Status2;
	
	OPL_MODE = 0x02;	// must be set to activate OPL2-Delays
	
	// OPL2 Detection
	OPL_HW_WriteReg(0x04, 0x60);
	OPL_HW_WriteReg(0x04, 0x80);
	Status1 = OPL_HW_GetStatus();
        //printf("%x\n", Status1);
	Status1 &= 0xE0;
	
	OPL_HW_WriteReg(0x02, 0xFF);
	OPL_HW_WriteReg(0x04, 0x21);
	OPL_HW_WaitDelay(0, 80);
	
	Status2 = OPL_HW_GetStatus();
        //printf("%x\n", Status2);
	Status2 &= 0xE0;
	
	OPL_HW_WriteReg(0x04, 0x60);
	OPL_HW_WriteReg(0x04, 0x80);
	
	if (! ((Status1 == 0x00) && (Status2 == 0xC0)))
	{
		// Detection failed
		OPL_MODE = 0x00;
		printf("No OPL Chip detected!\n");
		goto FinishDetection;
	}
	
	// OPL3 Detection
	Status1 = OPL_HW_GetStatus();
        //printf("%x\n", Status1);
	Status1 &= 0x06;
	if (! Status1)
	{
		OPL_MODE = 0x03;
		//OPL_CHIPS = 0x01;
		printf("OPL 3 Chip found.\n");
		goto FinishDetection;
	}
	
	// OPL2 Dual Chip Detection
	OPL_HW_WriteReg(0x104, 0x60);
	OPL_HW_WriteReg(0x104, 0x80);
	Status1 = OPL_HW_GetStatus();
	Status1 &= 0xE0;
	
	OPL_HW_WriteReg(0x102, 0xFF);
	OPL_HW_WriteReg(0x104, 0x21);
	OPL_HW_WaitDelay(0, 80);
	
	Status2 = OPL_HW_GetStatus();
	Status2 &= 0xE0;
	
	OPL_HW_WriteReg(0x104, 0x60);
	OPL_HW_WriteReg(0x104, 0x80);
	
	if ((Status1 == 0x00) && (Status2 == 0xC0))
	{
          //OPL_CHIPS = 0x02;
		printf("Dual OPL 2 Chip found.\n");
	}
	else
	{
          //OPL_CHIPS = 0x01;
		printf("OPL 2 Chip found.\n");
	}
	
FinishDetection:
}
********************/

#define ALIGN(x, a)		__ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a)		__ALIGN_KERNEL_MASK(x, (__typeof__(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define cpu_to_le32(x) x // assumes Little Endian CPU

static int snd_ymfpci_ac3_init (struct ymf_card_s *card)
{
  /***
  if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(card->pci),
                          4096, &card->ac3_tmp_base) < 0)
    return -ENOMEM;
  ***/
  unsigned int size = 4096;
  card->dm2 = MDma_alloc_cardmem(size);
  if (!card->dm2)
    return 0;
  uint32_t addr = (uint32_t)pds_cardmem_physicalptr(card->dm2, card->dm2->linearptr);
  card->bank_effect[3][0]->base =
    card->bank_effect[3][1]->base = cpu_to_le32((uint32_t)addr);
  card->bank_effect[3][0]->loop_end =
    card->bank_effect[3][1]->loop_end = cpu_to_le32(1024);
  card->bank_effect[4][0]->base =
    card->bank_effect[4][1]->base = cpu_to_le32((uint32_t)addr + 2048);
  card->bank_effect[4][0]->loop_end =
    card->bank_effect[4][1]->loop_end = cpu_to_le32(1024);

  //spin_lock_irq(&chip->reg_lock);
  snd_ymfpci_writel(card, YDSXGR_MAPOFEFFECT,
                    snd_ymfpci_readl(card, YDSXGR_MAPOFEFFECT) | 3 << 3);
  //spin_unlock_irq(&chip->reg_lock);
  return 0;
}

static int snd_ymfpci_memalloc(struct ymf_card_s *card)
{
	long size, playback_ctrl_size;
	int voice, bank, reg;
	uint8_t *ptr;
	dma_addr_t ptr_addr;

	playback_ctrl_size = 4 + 4 * YDSXG_PLAYBACK_VOICES;
	card->bank_size_playback = snd_ymfpci_readl(card, YDSXGR_PLAYCTRLSIZE) << 2;
	card->bank_size_capture = snd_ymfpci_readl(card, YDSXGR_RECCTRLSIZE) << 2;
	card->bank_size_effect = snd_ymfpci_readl(card, YDSXGR_EFFCTRLSIZE) << 2;
	card->bank_size_playback = 0x1000;
	card->bank_size_capture = 1024;
	card->bank_size_effect = 2 * 8192;
	card->work_size = YDSXG_DEFAULT_WORK_SIZE;
        printf("sizes: %u %u %u %u\n", card->bank_size_playback, card->bank_size_capture, card->bank_size_effect, card->work_size);
	
	size = ALIGN(playback_ctrl_size, 0x100) +
	       ALIGN(card->bank_size_playback * 2 * YDSXG_PLAYBACK_VOICES, 0x100) +
	       ALIGN(card->bank_size_capture * 2 * YDSXG_CAPTURE_VOICES, 0x100) +
	       ALIGN(card->bank_size_effect * 2 * YDSXG_EFFECT_VOICES, 0x100) +
	       card->work_size;
        printf("DMA size: 0x%X (%u)\n", size, size);
        card->dm = MDma_alloc_cardmem(size);
        if (!card->dm)
          return 0;
	/* work_ptr must be aligned to 256 bytes, but it's already
	   covered with the kernel page allocation mechanism */
	//if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(card->pci),
	//			size, &card->work_ptr) < 0) 
	//	return -ENOMEM;
	//ptr = card->work_ptr.area;
	//ptr_addr = card->work_ptr.addr;
	//memset(ptr, 0, size);	/* for sure */
        memset(card->dm->linearptr, 0, size);

	card->bank_base_playback = ptr;
        //ptr_addr = (((unsigned long)card->dm->linearptr+1023)&(~1023));
        ptr = card->dm->linearptr;
        ptr_addr = (((unsigned long)ptr));
	card->bank_base_playback_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);
	card->ctrl_playback = (uint32_t *)ptr;
	card->ctrl_playback[0] = cpu_to_le32(YDSXG_PLAYBACK_VOICES);
	ptr += ALIGN(playback_ctrl_size, 0x100);
	ptr_addr += ALIGN(playback_ctrl_size, 0x100);
	for (voice = 0; voice < YDSXG_PLAYBACK_VOICES; voice++) {
		card->voices[voice].number = voice;
		card->voices[voice].bank = (struct snd_ymfpci_playback_bank *)ptr;
		card->voices[voice].bank_addr = ptr_addr;
		for (bank = 0; bank < 2; bank++) {
			card->bank_playback[voice][bank] = (struct snd_ymfpci_playback_bank *)ptr;
			ptr += card->bank_size_playback;
			ptr_addr += card->bank_size_playback;
		}
	}
	ptr = (char *)ALIGN((unsigned long)ptr, 0x100);
	ptr_addr = ALIGN(ptr_addr, 0x100);
	card->bank_base_capture = ptr;
	card->bank_base_capture_addr = ptr_addr;
	for (voice = 0; voice < YDSXG_CAPTURE_VOICES; voice++)
		for (bank = 0; bank < 2; bank++) {
			card->bank_capture[voice][bank] = (struct snd_ymfpci_capture_bank *)ptr;
			ptr += card->bank_size_capture;
			ptr_addr += card->bank_size_capture;
		}
	ptr = (char *)ALIGN((unsigned long)ptr, 0x100);
	ptr_addr = ALIGN(ptr_addr, 0x100);
	card->bank_base_effect = ptr;
	card->bank_base_effect_addr = ptr_addr;
	for (voice = 0; voice < YDSXG_EFFECT_VOICES; voice++)
		for (bank = 0; bank < 2; bank++) {
			card->bank_effect[voice][bank] = (struct snd_ymfpci_effect_bank *)ptr;
			ptr += card->bank_size_effect;
			ptr_addr += card->bank_size_effect;
		}
	ptr = (char *)ALIGN((unsigned long)ptr, 0x100);
	ptr_addr = ALIGN(ptr_addr, 0x100);
	card->work_base = ptr;
	card->work_base_addr = ptr_addr;
	
	//snd_BUG_ON(ptr + card->work_size !=
	//	   card->work_ptr.area + card->work_ptr.bytes);

	snd_ymfpci_writel(card, YDSXGR_PLAYCTRLBASE, card->bank_base_playback_addr);
	snd_ymfpci_writel(card, YDSXGR_RECCTRLBASE, card->bank_base_capture_addr);
	snd_ymfpci_writel(card, YDSXGR_EFFCTRLBASE, card->bank_base_effect_addr);
	snd_ymfpci_writel(card, YDSXGR_WORKBASE, card->work_base_addr);
	snd_ymfpci_writel(card, YDSXGR_WORKSIZE, card->work_size >> 2);

#define IEC958_AES0_CON_EMPHASIS_NONE   (0<<3) /* none emphasis */
#define IEC958_AES1_CON_ORIGINAL   (1<<7) /* this bits depends on the category code */
#define IEC958_AES1_CON_DIGDIGCONV_ID   0x02
#define IEC958_AES1_CON_PCM_CODER   (IEC958_AES1_CON_DIGDIGCONV_ID|0x00)
#define IEC958_AES3_CON_FS_48000   (2<<0) /* 48kHz */
#define SNDRV_PCM_DEFAULT_CON_SPDIF (IEC958_AES0_CON_EMPHASIS_NONE|     \
                                     (IEC958_AES1_CON_ORIGINAL<<8)|     \
                                     (IEC958_AES1_CON_PCM_CODER<<8)|    \
                                     (IEC958_AES3_CON_FS_48000<<24))

	/* S/PDIF output initialization */
	card->spdif_bits = card->spdif_pcm_bits = SNDRV_PCM_DEFAULT_CON_SPDIF & 0xffff;
	snd_ymfpci_writew(card, YDSXGR_SPDIFOUTCTRL, 0);
	snd_ymfpci_writew(card, YDSXGR_SPDIFOUTSTATUS, card->spdif_bits);

	/* S/PDIF input initialization */
	snd_ymfpci_writew(card, YDSXGR_SPDIFINCTRL, 0);

	/* digital mixer setup */
	for (reg = 0x80; reg < 0xc0; reg += 4)
		snd_ymfpci_writel(card, reg, 0);
	snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_BUF441OUTVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_ZVOUTVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_SPDIFOUTVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_NATIVEADCINVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_NATIVEDACINVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_PRIADCLOOPVOL, 0x3fff3fff);
	snd_ymfpci_writel(card, YDSXGR_LEGACYOUTVOL, 0x3fff3fff);
	
	return 0;
}

static int YMF_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card;
  unsigned int i;
#ifdef MPXPLAY_USE_DEBUGF
  unsigned long prevtime = pds_gettimem();
#endif
  uint16_t legacy_ctrl, legacy_ctrl2, old_legacy_ctrl;

  legacy_ctrl = 0;
  legacy_ctrl2 = 0x0800;	/* SBEN = 0, SMOD = 01, LAD = 0 */

  card = (struct ymf_card_s *)pds_calloc(1, sizeof(struct ymf_card_s));
  if (!card)
    return 0;
  aui->card_private_data = card;

  card->pci_dev = (struct pci_config_s *)pds_calloc(1, sizeof(struct pci_config_s));
  if (!card->pci_dev)
    goto err_adetect;
  if (pcibios_search_devices(ymf_devices, card->pci_dev) != PCI_SUCCESSFUL)
    goto err_adetect;

  card->legacy_iobase = pcibios_ReadConfig_Dword(card->pci_dev, 0x14);
  //printf("YMF bar 1 (FM/MPU): %x\n", card->legacy_iobase);
  card->legacy_iobase &= 0xfff0;
  if (!card->legacy_iobase)
    goto err_adetect;
  uint32_t bar2 = pcibios_ReadConfig_Dword(card->pci_dev, 0x18);
  //printf("YMF bar 2 (JS): %x\n", bar2);
#if 0
  if (bar2 > card->legacy_iobase) {
    bar2 -= (0x100 + 0x40);
    pcibios_WriteConfig_Dword(card->pci_dev, 0x18, bar2);
    bar2 = pcibios_ReadConfig_Dword(card->pci_dev, 0x18);
    printf("YMF changed bar 2: %x\n", bar2);
  }
#endif
  bar2 &= 0xfff0;

  // YMF744/754 only?
  uint16_t fmport = card->legacy_iobase + 0x00;
  fmymfbase = fmport;
  legacy_ctrl |= YMFPCI_LEGACY_FMEN;
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_FMBASE, fmport);
  printf("fmport: %x\n", fmport);

  uint16_t mpuport = card->legacy_iobase + 0x20;
  ymfbase = mpuport;
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_MPU401BASE, mpuport);
  legacy_ctrl |= YMFPCI_LEGACY_MEN;
  legacy_ctrl |= YMFPCI_LEGACY_MIEN;
  legacy_ctrl2 |= YMFPCI_LEGACY2_IMOD;

  //pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_JOYBASE, bar2);
  //legacy_ctrl |= YMFPCI_LEGACY_JPEN;
  
  old_legacy_ctrl = pcibios_ReadConfig_Word(card->pci_dev, PCIR_DSXG_LEGACY);
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_LEGACY, legacy_ctrl);
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_ELEGACY, legacy_ctrl2);
  //printf("old: %x  new: %x\n", old_legacy_ctrl, legacy_ctrl);
  printf("mpuport: %x\n", mpuport);

#define PCI_CAPABILITY_LIST	0x34
#define  PCI_CAP_ID_PM		0x01	/* Power Management */

/* Power Management Registers */

#define PCI_PM_PMC		2	/* PM Capabilities Register */
#define  PCI_PM_CAP_VER_MASK	0x0007	/* Version */
#define  PCI_PM_CAP_PME_CLOCK	0x0008	/* PME clock required */
#define  PCI_PM_CAP_RESERVED    0x0010  /* Reserved field */
#define  PCI_PM_CAP_DSI		0x0020	/* Device specific initialization */
#define  PCI_PM_CAP_AUX_POWER	0x01C0	/* Auxiliary power support mask */
#define  PCI_PM_CAP_D1		0x0200	/* D1 power state support */
#define  PCI_PM_CAP_D2		0x0400	/* D2 power state support */
#define  PCI_PM_CAP_PME		0x0800	/* PME pin supported */
#define  PCI_PM_CAP_PME_MASK	0xF800	/* PME Mask of all supported states */
#define  PCI_PM_CAP_PME_D0	0x0800	/* PME# from D0 */
#define  PCI_PM_CAP_PME_D1	0x1000	/* PME# from D1 */
#define  PCI_PM_CAP_PME_D2	0x2000	/* PME# from D2 */
#define  PCI_PM_CAP_PME_D3hot	0x4000	/* PME# from D3 (hot) */
#define  PCI_PM_CAP_PME_D3cold	0x8000	/* PME# from D3 (cold) */
#define  PCI_PM_CAP_PME_SHIFT	11	/* Start of the PME Mask in PMC */
#define PCI_PM_CTRL		4	/* PM control and status register */
#define  PCI_PM_CTRL_STATE_MASK	0x0003	/* Current power state (D0 to D3) */
#define  PCI_PM_CTRL_NO_SOFT_RESET	0x0008	/* No reset for D3hot->D0 */
#define  PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define  PCI_PM_CTRL_DATA_SEL_MASK	0x1e00	/* Data select (??) */
#define  PCI_PM_CTRL_DATA_SCALE_MASK	0x6000	/* Data scale (??) */
#define  PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */
#define PCI_PM_PPB_EXTENSIONS	6	/* PPB support extensions (??) */
#define  PCI_PM_PPB_B2_B3	0x40	/* Stop clock when in D3hot (??) */
#define  PCI_PM_BPCC_ENABLE	0x80	/* Bus power/clock control enable (??) */
#define PCI_PM_DATA_REGISTER	7	/* (??) */
#define PCI_PM_SIZEOF		8

  uint8_t c;
  uint8_t c0;
  uint8_t c1;
  c = pcibios_ReadConfig_Byte(card->pci_dev, PCI_CAPABILITY_LIST);
  //printf("cap: %X\n", c);
  c0 = pcibios_ReadConfig_Byte(card->pci_dev, c);
  //printf("c0: %X\n", c0);
  c1 = pcibios_ReadConfig_Byte(card->pci_dev, c+1);
  //printf("c1: %X\n", c1);
  if (c0 == PCI_CAP_ID_PM) {
    uint16_t w;
    w = pcibios_ReadConfig_Word(card->pci_dev, c + PCI_PM_CTRL);
    //printf("w: %4.4X\n", w);
    uint16_t state = w & PCI_PM_CTRL_STATE_MASK;
    printf("power state: %4.4X\n", state);
    pcibios_WriteConfig_Word(card->pci_dev, c + PCI_PM_CTRL, 0);
  }

  aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
  card->pci_iobase = pcibios_ReadConfig_Dword(card->pci_dev, 0x10);
  //printf("YMF bar 0: %x\n", card->pci_iobase);
  card->pci_iobase &= 0xfffffff8;
  card->iobase = pds_dpmi_map_physical_memory(card->pci_iobase, 0x4000); // 0x8000 0x100 ???
  if (!card->iobase)
    goto err_adetect;
  //card->iobase2 = pds_dpmi_map_physical_memory(card->pci_iobase + 0x100, 0x100);
  //printf("iobase2: %x\n", card->iobase2);
  // need to map iobase3 after iobase2 ???
  //card->iobase3 = pds_dpmi_map_physical_memory(card->pci_iobase + 0x4000, 0x4000); // 0x8000 0x100 ???
  //printf("iobase3: %x\n", card->iobase3);
  memset(regtab, 0, sizeof(regtab));
  
  unsigned int cmd;
  //cmd = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_PCICMD);
  //printf("cmd byte: %4.4X\n", cmd);
  //cmd = pcibios_ReadConfig_Word(card->pci_dev, PCIR_PCICMD);
  //printf("cmd word: %4.4X\n", cmd);
  //pcibios_set_master(card->pci_dev);
  pcibios_enable_memmap_set_master_all(card->pci_dev);
  //pcibios_enable_interrupt(card->pci_dev);
  cmd = pcibios_ReadConfig_Word(card->pci_dev, PCIR_PCICMD);
  printf("pci command word: %4.4X\n", cmd);
  
  printf("Yamaha YMF sound card: %4.4X (%X) legacy_iobase: %4.4X  IRQ: %d\n",
         card->pci_dev->device_id, card->pci_dev->device_type, card->legacy_iobase, card->irq);
  printf("iobase: %8.8X -> %8.8X\n", card->pci_iobase, card->iobase);

#if 0
  //if (g_card != NULL) {
  //  return 0;
  //}
  wanted_irq = card->irq;
  g_card = card;
  //setvect_newirq(card->irq, serirq, &cardinfobits);
  //MIrq_OnOff(card->irq,1);
  BOOL PM_ISR = DPMI_InstallISR(PIC_IRQ2VEC(card->irq), __inthandlerPM, &__IntHandlePM) == 0;
  //printf("pm_isr: %d\n", PM_ISR);
  HDPMIPT_InstallIRQACKHandler(card->irq, __IntHandlePM.wrapper_cs, __IntHandlePM.wrapper_offset);
  //#if MAIN_INSTALL_RM_ISR
  //BOOL RM_ISR = DPMI_InstallRealModeISR(PIC_IRQ2VEC(aui.card_irq), MAIN_InterruptRM, &MAIN_IntREG, &MAIN_IntHandleRM) == 0;
  //#else
  //BOOL RM_ISR = TRUE;
  //#endif
  PIC_UnmaskIRQ(card->irq);
#endif

  snd_ymfpci_aclink_reset(card);
  int ready = snd_ymfpci_codec_ready(card, 0);
  printf("ready: %d\n", ready);

  pds_delay_10us(1000);
  snd_ymfpci_download_image(card);
  pds_delay_10us(1000);

  /***
  card->work_size = YDSXG_DEFAULT_WORK_SIZE;
  unsigned long allbufsize = card->work_size + 1024;
  card->dm = MDma_alloc_cardmem(allbufsize);
  if (!card->dm)
    return 0;
  unsigned long beginmem_aligned = (((unsigned long)card->dm->linearptr+1023)&(~1023));
  card->work_base_addr = beginmem_aligned;
  ***/

  //snd_ymfpci_memalloc(card);printf("did memalloc\n");
  //snd_ymfpci_ac3_init(card);printf("did ac3\n");

  ymfsb_resetAC97(card);
  printf("did ac97\n");
  
  //OPL_Hardware_Detecton();
  //printf("int cnt %u %u\n", xint_cnt, ymf_int_cnt);
  snd_ymfpci_writew(card, YDSXGR_SPDIFOUTCTRL, 1); // SPDIF ON
  //snd_ymfpci_writew(card, YDSXGR_ZVCTRL, 1); // Zoomed Video
  //ymfsb_writeAC97Reg(card, 2, 0x0101);
  //ymfsb_writeAC97Reg(card, 0x2a, 0x0080); // XXX
  //snd_ymfpci_writel(card, 0x0020, 0x00008200); // XXX
  //snd_ymfpci_writel(card, 0x0024, 0x00008200); // XXX
  //snd_ymfpci_writel(card, 0x0028, 0x80008200); // XXX
  snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x045e045e); // XXX
  //snd_ymfpci_writel(card, YDSXGR_CTRLSELECT, 1); // XXX
  snd_ymfpci_enable_dsp(card);

#if 1
  int sy, sx;
#if 0
  for (sy = 0; sy < 8; sy++) {
    for (sx = 0; sx < 16; sx++) {
      printf(" %2.2X", snd_ymfpci_readb(card, sy*16+sx));
    }
    printf("\n");
  }
  printf("scanned\n");
#endif
#if 0
  for (sy = 0; sy < 1; sy++) {
    for (sx = 0; sx < 16; sx++) {
      printf(" %8.8X", snd_ymfpci_readl(card, 0x100+sy*16+sx*4));
    }
    printf("\n");
  }
  printf("scanned iobase2\n");
#endif
#if 0
  for (sy = 0; sy < 1; sy++) {
    for (sx = 0; sx < 16; sx++) {
      printf(" %8.8X", snd_ymfpci_readl(card, 0x4000+sy*16+sx*4));
    }
    printf("\n");
  }
  printf("scanned FW\n");
#endif
#endif
  //ready = snd_ymfpci_codec_ready(card, 0);
  //printf("ready: %d\n", ready);
  //exit(1); // XXX
#if 0
  for (int i = 0; i < 0x40; i++) {
    printf("%2.2X ", inp(card->legacy_iobase+i));
  }
  printf("\nscanned\n");
#endif

  return 0;

 err_adetect:
  YMF_close(aui);
  return 0;
}

one_sndcard_info YMF_sndcard_info={
  "Yamaha YMF",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,                  // card_config
  NULL,                  // no init
  &YMF_adetect,      // only autodetect
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,

  NULL,
  NULL,
  NULL,
  NULL,
  NULL,

  NULL,
  NULL,
  NULL,
};

#endif // AU_CARDS_LINK_IHD
