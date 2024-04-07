// YMF7xx driver for SBEMU
// based on the Linux driver

#include "au_cards.h"

#ifdef SBEMU //AU_CARDS_LINK_YMF

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dmairq.h"
#include "pcibios.h"
#include "fw_ymf.h"
#include <dpmi.h>

#define VOICE_TO_USE 0

#define YMF_DEBUG 0

//#define YSBEMU_CONFIG_UTIL 1  // define on make command line to build ysbemu

#if defined(YSBEMU_CONFIG_UTIL) && YSBEMU_CONFIG_UTIL == 1
#undef YMF_DEBUG
#define YMF_DEBUG 1
#endif
#include "dpmi/dbgutil.h"
#if YMF_DEBUG
#define YMF_printf(...) do { printf("YMF: "); printf(__VA_ARGS__); } while (0)
#else
#define YMF_printf(...)
#endif
#if YSBEMU_CONFIG_UTIL
#define DBG_Logi(...) do { printf(__VA_ARGS__); } while (0) // ysbemu
#endif

#ifdef SBEMU
#define PCMBUFFERPAGESIZE      512
//#define PCMBUFFERPAGESIZE      1024
#else
#define PCMBUFFERPAGESIZE      4096
#endif

#define ALIGN(x, a) __ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (__typeof__(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))

#define cpu_to_le32(x) x // assumes Little Endian CPU
#define le32_to_cpu(x) x // assumes Little Endian CPU
#define SHIFTCONSTANT_2(x) 2 // Number of channels

#define spin_lock(x) // do nothing
#define spin_unlock(x) // do nothing
#define spin_lock_irq(x) // do nothing
#define spin_unlock_irq(x) // do nothing
#define spin_lock_irqsave(x,f) // do nothing
#define spin_unlock_irqrestore(x,f) // do nothing

//#include "linux/pci.h"

#ifndef SBEMU_LINUX_PCI_H
#ifndef EINVAL
#define EINVAL -2
#endif
#ifndef ENOMEM
#define ENOMEM -3
#endif
typedef unsigned long dma_addr_t;
#endif

static inline int snd_BUG_ON (int _cond) { return _cond; }

#if YSBEMU_CONFIG_UTIL
#define YMF_ENABLE_PCM 0
#else
#define YMF_ENABLE_PCM 1
#endif

one_sndcard_info YMF_sndcard_info;
#if YMF_ENABLE_PCM
one_sndcard_info YMFSB_sndcard_info;
#endif

#define YDSXG_PLAYBACK_VOICES    2 // In Linux this is 64
#define YDSXG_CAPTURE_VOICES     0 // Linux: 2
#define YDSXG_EFFECT_VOICES      5 // Linux: 5
#define YDSXG_DSPLENGTH          0x0080
#define YDSXG_CTRLLENGTH         0x3000
#define YDSXG_DEFAULT_WORK_SIZE  0x0400

struct snd_ymfpci_playback_bank {
  uint32_t format;
  uint32_t loop_default;
  uint32_t base;            // 32-bit address
  uint32_t loop_start;      // 32-bit offset
  uint32_t loop_end;        // 32-bit offset
  uint32_t loop_frac;       // 8-bit fraction - loop_start
  uint32_t delta_end;       // pitch delta end
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
  uint32_t base;          // 32-bit address
  uint32_t loop_end;      // 32-bit offset
  uint32_t start;         // 32-bit offset
  uint32_t num_of_loops;  // counter
};

struct snd_ymfpci_effect_bank {
  uint32_t base;       // 32-bit address
  uint32_t loop_end;   // 32-bit offset
  uint32_t start;      // 32-bit offset
  uint32_t temp;
};

enum snd_ymfpci_voice_type {
  YMFPCI_PCM,
  YMFPCI_SYNTH,
  YMFPCI_MIDI
};

struct ymf_card_s;

struct snd_ymfpci_voice {
  dma_addr_t bank_addr;
  int number;
  unsigned int use: 1,
    pcm: 1,
    synth: 1,
    midi: 1;
  struct snd_ymfpci_playback_bank *bank;
  void (*interrupt)(struct mpxplay_audioout_info_s *aui, struct snd_ymfpci_voice *voice);
};

struct ymf_card_s {
  unsigned long pci_iobase;
  unsigned long iobase;
  unsigned long legacy_iobase;
  struct pci_config_s  *pci_dev;
  cardmem_t *dm;
  cardmem_t *dm2;
  unsigned long dma_size;

  unsigned long period_size;
  unsigned long period_pos;
  unsigned long last_pos;
  uint8_t *pcmout_buffer;
  uint32_t pcmout_buffer_physaddr;
  long pcmout_bufsize;
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
  volatile uint32_t *ctrl_playback;
  volatile struct snd_ymfpci_playback_bank *bank_playback[YDSXG_PLAYBACK_VOICES][2];
  volatile struct snd_ymfpci_capture_bank *bank_capture[YDSXG_CAPTURE_VOICES][2];
  volatile struct snd_ymfpci_effect_bank *bank_effect[YDSXG_EFFECT_VOICES][2];
  struct snd_ymfpci_voice voices[YDSXG_PLAYBACK_VOICES];
  int src441_used;
  int use_441_slot;
  uint32_t active_bank;
  uint16_t spdif_bits, spdif_pcm_bits;
  int start_count;
  int running;
  int update_pcm_vol;
  int weird;
  uint16_t sbport;
  uint16_t fmport;
  uint16_t mpuport;
};

static pci_device_s ymf_devices[] = {
  {"Yamaha YMF724",   0x1073, 0x0004, 0x724  },
  {"Yamaha YMF724F",  0x1073, 0x000d, 0x724F },
  {"Yamaha YMF740",   0x1073, 0x000a, 0x740  },
  {"Yamaha YMF740C",  0x1073, 0x000c, 0x740C },
  {"Yamaha YMF744",   0x1073, 0x0010, 0x744  },
  {"Yamaha YMF754",   0x1073, 0x0012, 0x754  },
  {NULL,0,0,0}
};

#define YDSXGR_INTFLAG           0x0004
#define YDSXGR_ACTIVITY          0x0006
#define YDSXGR_GLOBALCTRL        0x0008
#define YDSXGR_ZVCTRL            0x000A
#define YDSXGR_TIMERCTRL         0x0010
#define YDSXGR_TIMERCOUNT        0x0012
#define YDSXGR_SPDIFOUTCTRL      0x0018
#define YDSXGR_SPDIFOUTSTATUS    0x001C
#define YDSXGR_EEPROMCTRL        0x0020
#define YDSXGR_SPDIFINCTRL       0x0034
#define YDSXGR_SPDIFINSTATUS     0x0038
#define YDSXGR_DSPPROGRAMDL      0x0048
#define YDSXGR_DLCNTRL           0x004C
#define YDSXGR_GPIOININTFLAG     0x0050
#define YDSXGR_GPIOININTENABLE   0x0052
#define YDSXGR_GPIOINSTATUS      0x0054
#define YDSXGR_GPIOOUTCTRL       0x0056
#define YDSXGR_GPIOFUNCENABLE    0x0058
#define YDSXGR_GPIOTYPECONFIG    0x005A
#define YDSXGR_AC97CMDDATA       0x0060
#define YDSXGR_AC97CMDADR        0x0062
#define YDSXGR_PRISTATUSDATA     0x0064
#define YDSXGR_PRISTATUSADR      0x0066
#define YDSXGR_SECSTATUSDATA     0x0068
#define YDSXGR_SECSTATUSADR      0x006A
#define YDSXGR_SECCONFIG         0x0070
#define YDSXGR_LEGACYOUTVOL      0x0080
#define YDSXGR_LEGACYOUTVOLL     0x0080
#define YDSXGR_LEGACYOUTVOLR     0x0082
#define YDSXGR_NATIVEDACOUTVOL   0x0084
#define YDSXGR_NATIVEDACOUTVOLL  0x0084
#define YDSXGR_NATIVEDACOUTVOLR  0x0086
#define YDSXGR_ZVOUTVOL          0x0088
#define YDSXGR_ZVOUTVOLL         0x0088
#define YDSXGR_ZVOUTVOLR         0x008A
#define YDSXGR_SECADCOUTVOL      0x008C
#define YDSXGR_SECADCOUTVOLL     0x008C
#define YDSXGR_SECADCOUTVOLR     0x008E
#define YDSXGR_PRIADCOUTVOL      0x0090
#define YDSXGR_PRIADCOUTVOLL     0x0090
#define YDSXGR_PRIADCOUTVOLR     0x0092
#define YDSXGR_LEGACYLOOPVOL     0x0094
#define YDSXGR_LEGACYLOOPVOLL    0x0094
#define YDSXGR_LEGACYLOOPVOLR    0x0096
#define YDSXGR_NATIVEDACLOOPVOL  0x0098
#define YDSXGR_NATIVEDACLOOPVOLL 0x0098
#define YDSXGR_NATIVEDACLOOPVOLR 0x009A
#define YDSXGR_ZVLOOPVOL         0x009C
#define YDSXGR_ZVLOOPVOLL        0x009E
#define YDSXGR_ZVLOOPVOLR        0x009E
#define YDSXGR_SECADCLOOPVOL     0x00A0
#define YDSXGR_SECADCLOOPVOLL    0x00A0
#define YDSXGR_SECADCLOOPVOLR    0x00A2
#define YDSXGR_PRIADCLOOPVOL     0x00A4
#define YDSXGR_PRIADCLOOPVOLL    0x00A4
#define YDSXGR_PRIADCLOOPVOLR    0x00A6
#define YDSXGR_NATIVEADCINVOL    0x00A8
#define YDSXGR_NATIVEADCINVOLL   0x00A8
#define YDSXGR_NATIVEADCINVOLR   0x00AA
#define YDSXGR_NATIVEDACINVOL    0x00AC
#define YDSXGR_NATIVEDACINVOLL   0x00AC
#define YDSXGR_NATIVEDACINVOLR   0x00AE
#define YDSXGR_BUF441OUTVOL      0x00B0
#define YDSXGR_BUF441OUTVOLL     0x00B0
#define YDSXGR_BUF441OUTVOLR     0x00B2
#define YDSXGR_BUF441LOOPVOL     0x00B4
#define YDSXGR_BUF441LOOPVOLL    0x00B4
#define YDSXGR_BUF441LOOPVOLR    0x00B6
#define YDSXGR_SPDIFOUTVOL       0x00B8
#define YDSXGR_SPDIFOUTVOLL      0x00B8
#define YDSXGR_SPDIFOUTVOLR      0x00BA
#define YDSXGR_SPDIFLOOPVOL      0x00BC
#define YDSXGR_SPDIFLOOPVOLL     0x00BC
#define YDSXGR_SPDIFLOOPVOLR     0x00BE
#define YDSXGR_ADCSLOTSR         0x00C0
#define YDSXGR_RECSLOTSR         0x00C4
#define YDSXGR_ADCFORMAT         0x00C8
#define YDSXGR_RECFORMAT         0x00CC
#define YDSXGR_P44SLOTSR         0x00D0
#define YDSXGR_STATUS            0x0100
#define YDSXGR_CTRLSELECT        0x0104
#define YDSXGR_MODE              0x0108
#define YDSXGR_SAMPLECOUNT       0x010C
#define YDSXGR_NUMOFSAMPLES      0x0110
#define YDSXGR_CONFIG            0x0114
#define YDSXGR_PLAYCTRLSIZE      0x0140
#define YDSXGR_RECCTRLSIZE       0x0144
#define YDSXGR_EFFCTRLSIZE       0x0148
#define YDSXGR_WORKSIZE          0x014C
#define YDSXGR_MAPOFREC          0x0150
#define YDSXGR_MAPOFEFFECT       0x0154
#define YDSXGR_PLAYCTRLBASE      0x0158
#define YDSXGR_RECCTRLBASE       0x015C
#define YDSXGR_EFFCTRLBASE       0x0160
#define YDSXGR_WORKBASE          0x0164
#define YDSXGR_DSPINSTRAM        0x1000
#define YDSXGR_CTRLINSTRAM       0x4000

#define YDSXG_AC97READCMD     0x8000
#define YDSXG_AC97WRITECMD    0x0000

#define PCIR_DSXG_LEGACY      0x40
#define PCIR_DSXG_ELEGACY     0x42
#define PCIR_DSXG_CTRL        0x48
#define PCIR_DSXG_PWRCTRL1    0x4a
#define PCIR_DSXG_PWRCTRL2    0x4e
#define PCIR_DSXG_FMBASE      0x60
#define PCIR_DSXG_SBBASE      0x62
#define PCIR_DSXG_MPU401BASE  0x64
#define PCIR_DSXG_JOYBASE     0x66

#define YMFPCI_LEGACY_SBEN    (1 << 0)   // soundblaster enable
#define YMFPCI_LEGACY_FMEN    (1 << 1)   // OPL3 enable
#define YMFPCI_LEGACY_JPEN    (1 << 2)   // joystick enable
#define YMFPCI_LEGACY_MEN     (1 << 3)   // MPU401 enable
#define YMFPCI_LEGACY_MIEN    (1 << 4)   // MPU RX irq enable
#define YMFPCI_LEGACY_IOBITS  (1 << 5)   // i/o bits range, 0 = 16bit, 1 =10bit
#define YMFPCI_LEGACY_SDMA    (3 << 6)   // SB DMA select
#define YMFPCI_LEGACY_SBIRQ   (7 << 8)   // SB IRQ select
#define YMFPCI_LEGACY_MPUIRQ  (7 << 11)  // MPU IRQ select
#define YMFPCI_LEGACY_SIEN    (1 << 14)  // serialized IRQ
#define YMFPCI_LEGACY_LAD     (1 << 15)  // legacy audio disable

#define YMFPCI_LEGACY2_FMIO   (3 << 0)   // OPL3 i/o address (724/740)
#define YMFPCI_LEGACY2_SBIO   (3 << 2)   // SB i/o address (724/740)
#define YMFPCI_LEGACY2_MPUIO  (3 << 4)   // MPU401 i/o address (724/740)
#define YMFPCI_LEGACY2_JSIO   (3 << 6)   // joystick i/o address (724/740)
#define YMFPCI_LEGACY2_MAIM   (1 << 8)   // MPU401 ack intr mask
#define YMFPCI_LEGACY2_SMOD   (3 << 11)  // SB DMA mode
#define YMFPCI_LEGACY2_SBVER  (3 << 13)  // SB version select
#define YMFPCI_LEGACY2_IMOD   (1 << 15)  // legacy IRQ mode
// SIEN:IMOD 0:0 = legacy irq, 0:1 = INTA, 1:0 = serialized IRQ

static void YMF_close (struct mpxplay_audioout_info_s *aui)
{
#if YMF_DEBUG
  DBG_Logi("YMF_close\n");
#endif
  struct ymf_card_s *card = aui->card_private_data;
  if (card) {
    if (card->iobase) {
      pds_dpmi_unmap_physycal_memory(card->iobase);
    }
  }
  if (card->dm)
    MDma_free_cardmem(card->dm);
  if (card->dm2)
    MDma_free_cardmem(card->dm2);
  if (card->pci_dev)
    pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data = NULL;
}

#define linux_writel(addr,value) PDS_PUTB_LE32((volatile char *)(addr),value)
#define linux_readl(addr) PDS_GETB_LE32((volatile char *)(addr))
#define linux_writew(addr,value) PDS_PUTB_LE16((volatile char *)(addr), value)
#define linux_readw(addr) PDS_GETB_LE16((volatile char *)(addr))
#define linux_writeb(addr,value) *((volatile unsigned char *)(addr))=value
#define linux_readb(addr) PDS_GETB_8U((volatile char *)(addr))

static inline uint32_t mapreg (struct ymf_card_s *card, uint32_t offset) {
  return card->iobase + offset;
}

static inline uint16_t snd_ymfpci_readw (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint16_t r = linux_readw(addr);
  return r;
}

static inline void snd_ymfpci_writew (struct ymf_card_s *card, uint32_t offset, uint16_t val)
{
  uint32_t addr = mapreg(card, offset);
  linux_writew(addr, val);
}

static inline uint32_t snd_ymfpci_readl (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint32_t r = linux_readl(addr);
  return r;
}

static inline void snd_ymfpci_writel (struct ymf_card_s *card, uint32_t offset, uint32_t val)
{
  uint32_t addr = mapreg(card, offset);
  linux_writel(addr, val);
}

static inline uint8_t snd_ymfpci_readb (struct ymf_card_s *card, uint32_t offset)
{
  uint32_t addr = mapreg(card, offset);
  uint8_t r = linux_readb(addr);
  return r;
}

static inline void snd_ymfpci_writeb (struct ymf_card_s *card, uint32_t offset, uint8_t val)
{
  uint32_t addr = mapreg(card, offset);
  linux_writeb(addr, val);
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

static void snd_ymfpci_disable_dsp (struct ymf_card_s *card)
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

#if YMF_DEBUG
unsigned int ymf_int_cnt = 0;
struct ymf_card_s *ymfdump_card = NULL;
static int dumped = 0;

void ymfdump ()
{
  struct ymf_card_s *card = ymfdump_card;
  int sy, sx;
  for (sy = 0; sy < 16; sy++) {
    DBG_Logi("%8.8X ", 0x000+sy*16);
    for (sx = 0; sx < 16; sx += 4) {
      DBG_Logi(" %8.8X", snd_ymfpci_readl(card, 0x000+sy*16+sx));
    }
    DBG_Logi("\n");
  }
  for (sy = 0; sy < 7; sy++) {
    DBG_Logi("%8.8X ", 0x100+sy*16);
    for (sx = 0; sx < 16; sx += 4) {
      DBG_Logi(" %8.8X", snd_ymfpci_readl(card, 0x100+sy*16+sx));
    }
    DBG_Logi("\n");
  }
}

#include <go32.h>
#include <sys/farptr.h>

void ymfdumpbanks ()
{
  struct ymf_card_s *card = ymfdump_card;
  uint32_t pb = snd_ymfpci_readl(card, YDSXGR_PLAYCTRLBASE);
  uint32_t table[3];
  if (pb > 0 && pb < 0xa0000) {
    DBG_Logi("table at %X:\n", pb);
    volatile uint32_t *tab = (uint32_t *)pb;
    for (int i = 0; i < 3; i++) {
      uint32_t val = _farpeekl(_dos_ds, (unsigned long)(tab+i));
      DBG_Logi(" %8.8X", val);
      table[i] = val;
    }
    DBG_Logi("\n");
  } else {
    return;
  }
  uint32_t base = 0;
  for (int i = 1; i < 3; i++) {
    uint32_t bank = base + table[i];
    DBG_Logi("bank %d %X\n", i, bank);
    int r = 0;
#define GETVAL(x) _farpeekl(_dos_ds, (unsigned long)(bank+4*x))
    DBG_Logi("format %8.8X\n", GETVAL(r++));
    DBG_Logi("loop_default %8.8X\n", GETVAL(r++));
    DBG_Logi("base %8.8X\n", GETVAL(r++));            // 32-bit address
    DBG_Logi("loop_start %8.8X\n", GETVAL(r++));      // 32-bit offset
    DBG_Logi("loop_end %8.8X\n", GETVAL(r++));        // 32-bit offset
    DBG_Logi("loop_frac %8.8X\n", GETVAL(r++));       // 8-bit fraction - loop_start
    DBG_Logi("delta_end %8.8X\n", GETVAL(r++));       // pitch delta end
    DBG_Logi("lpfK_end %8.8X\n", GETVAL(r++));
    DBG_Logi("eg_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("left_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("right_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("eff1_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("eff2_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("eff3_gain_end %8.8X\n", GETVAL(r++));
    DBG_Logi("lpfQ %8.8X\n", GETVAL(r++));
    DBG_Logi("status %8.8X\n", GETVAL(r++));
    DBG_Logi("num_of_frames %8.8X\n", GETVAL(r++));
    DBG_Logi("loop_count %8.8X\n", GETVAL(r++));
    DBG_Logi("start %8.8X\n", GETVAL(r++));
    DBG_Logi("start_frac %8.8X\n", GETVAL(r++));
    DBG_Logi("delta %8.8X\n", GETVAL(r++));
    DBG_Logi("lpfK %8.8X\n", GETVAL(r++));
    DBG_Logi("eg_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("left_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("right_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("eff1_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("eff2_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("eff3_gain %8.8X\n", GETVAL(r++));
    DBG_Logi("lpfD1 %8.8X\n", GETVAL(r++));
    DBG_Logi("lpfD2 %8.8X\n", GETVAL(r++));
  };
}

void pcidump ()
{
  struct ymf_card_s *card = ymfdump_card;
  int sy, sx;
  for (sy = 0; sy < 16; sy++) {
    DBG_Logi("%8.8X ", sy*16);
    for (sx = 0; sx < 16; sx++) {
      DBG_Logi(" %2.2X", pcibios_ReadConfig_Byte(card->pci_dev, sy*16+sx));
    }
    DBG_Logi("\n");
  }
}
#endif

#if YMF_ENABLE_PCM
static int snd_ymfpci_timer_start (struct mpxplay_audioout_info_s *aui);
static int snd_ymfpci_timer_stop (struct mpxplay_audioout_info_s *aui);

static int YMF_IRQRoutine (mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  uint32_t status, nvoice, mode;
  struct snd_ymfpci_voice *voice;
  int handled = 0;

#if YMF_DEBUG
  //if ((ymf_int_cnt % 500) == 0) DBG_Logi("ymfirq %u\n", ymf_int_cnt);
  ymf_int_cnt++;
#endif

  status = snd_ymfpci_readl(card, YDSXGR_STATUS);
  if (status & 0x80000000) {
    handled = 1;
    card->active_bank = snd_ymfpci_readl(card, YDSXGR_CTRLSELECT) & 1;
    //if ((ymf_int_cnt % 500) == 0) DBG_Logi("bank: %8.8X\n", card->active_bank);
    spin_lock(&card->voice_lock);
    for (nvoice = 0; nvoice < YDSXG_PLAYBACK_VOICES; nvoice++) {
      voice = &card->voices[nvoice];
      if (voice->interrupt)
        voice->interrupt(aui, voice);
    }
    //for (nvoice = 0; nvoice < YDSXG_CAPTURE_VOICES; nvoice++) {
    //  if (card->capture_substream[nvoice])
    //    snd_ymfpci_pcm_capture_interrupt(card->capture_substream[nvoice]);
    //}
    spin_unlock(&card->voice_lock);
    spin_lock(&card->reg_lock);
    snd_ymfpci_writel(card, YDSXGR_STATUS, 0x80000000);
    mode = snd_ymfpci_readl(card, YDSXGR_MODE) | 2;
    snd_ymfpci_writel(card, YDSXGR_MODE, mode);
    spin_unlock(&card->reg_lock);
    
    //if (atomic_read(&card->interrupt_sleep_count)) {
    //  atomic_set(&card->interrupt_sleep_count, 0);
    //  wake_up(&card->interrupt_sleep);
    //}
  }

#if 1
  status = snd_ymfpci_readw(card, YDSXGR_INTFLAG);
  if (status & 1) {
    handled = 1;
    snd_ymfpci_timer_stop(aui);
    // Set the volume here, since 754 outputs a loud noise when it doesn't work
    if (card->weird) {
      voice = &card->voices[VOICE_TO_USE];
      uint32_t bufpos = le32_to_cpu(voice->bank[card->active_bank].start);
      if (bufpos) {
#if YMF_DEBUG > 1
        DBG_Logi("setting NATIVEDACOUTVOL\n"); // YMF754
#endif
        snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x045e045e);
        snd_ymfpci_writel(card, YDSXGR_BUF441OUTVOL, 0x045e045e);
      } else {
        uint32_t pb = snd_ymfpci_readl(card, YDSXGR_PLAYCTRLBASE);
#if 0
        if (pb > 0 && pb < 0xa0000) { // DOSMEM (first 640K)
          //ymfdump();
          if ((ymf_int_cnt % 100) == 0) {
            ymfdumpbanks();
          }
        }
#endif
        snd_ymfpci_timer_start(aui);
      }
    }
    //snd_ymfpci_timer_start(aui);
  }
  snd_ymfpci_writew(card, YDSXGR_INTFLAG, status);
#endif

  //if (card->rawmidi)
  //  snd_mpu401_uart_interrupt(irq, card->rawmidi->private_data);
  return handled;
}
#endif

static int
install_firmware (struct ymf_card_s *card, uint32_t addr, uint32_t *src, uint32_t len)
{
  int warned = 0;
  uint32_t *end = src + (len >> 2);
  uint32_t i;
  while (src < end) {
    uint32_t val = *src;
    snd_ymfpci_writel(card, addr, val);
#if 0
    // The Linux driver also fails to read back the same value
    uint32_t rval = snd_ymfpci_readl(card, addr);
    if (rval != val && !warned) {
#if YMF_DEBUG
      DBG_Logi("failed to write firmware addr %8.8X wrote %8.8X, read back %8.8X, ignoring.\n", addr, val, rval);
#endif
      YMF_printf("failed to write firmware addr %8.8X wrote %8.8X, read back %8.8X, ignoring.\n", addr, val, rval);
      warned = 1;
    }
#endif
    addr += 4;
    src++;
  }
  return !warned;
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

  if (install_firmware(card, 0x1000, dsp, dsp_size))
    YMF_printf("installed dsp FW\n");

  switch (card->pci_dev->device_type) {
  case 0x724F:
  case 0x740C:
  case 0x744:
  case 0x754:
    if (install_firmware(card, YDSXGR_CTRLINSTRAM, cntrl1E, cntrl1E_size)) {
      YMF_printf("installed cntrl1E FW\n");
    }
    break;
  default:
    if (install_firmware(card, YDSXGR_CTRLINSTRAM, cntrl, cntrl_size)) {
      YMF_printf("installed cntrl FW\n");
    }
    break;
  }

  snd_ymfpci_enable_dsp(card);
}

static int snd_ymfpci_codec_ready (struct ymf_card_s *card, int secondary)
{
  uint32_t reg = secondary ? YDSXGR_SECSTATUSADR : YDSXGR_PRISTATUSADR;
  int timeout = 7500; // 750ms
  do {
    uint16_t r = snd_ymfpci_readw(card, reg);
    if ((r & 0x8000) == 0)
      return 0;
    pds_delay_10us(10);
  } while (--timeout);
  //_LOG("YMF not ready\n");
  return -1;
}

// AC97 1.0
#define AC97_RESET               0x0000      //
#define AC97_MASTER_VOL_STEREO   0x0002      // Line Out
#define AC97_HEADPHONE_VOL       0x0004      // 
#define AC97_MASTER_VOL_MONO     0x0006      // TAD Output
#define AC97_MASTER_TONE         0x0008      //
#define AC97_PCBEEP_VOL          0x000a      // none
#define AC97_PHONE_VOL           0x000c      // TAD Input (mono)
#define AC97_MIC_VOL             0x000e      // MIC Input (mono)
#define AC97_LINEIN_VOL          0x0010      // Line Input (stereo)
#define AC97_CD_VOL              0x0012      // CD Input (stereo)
#define AC97_VIDEO_VOL           0x0014      // none
#define AC97_AUX_VOL             0x0016      // Aux Input (stereo)
#define AC97_PCMOUT_VOL          0x0018      // Wave Output (stereo)
#define AC97_RECORD_SELECT       0x001a      //
#define AC97_RECORD_GAIN         0x001c
#define AC97_RECORD_GAIN_MIC     0x001e
#define AC97_GENERAL_PURPOSE     0x0020
#define AC97_3D_CONTROL          0x0022
#define AC97_MODEM_RATE          0x0024
#define AC97_POWER_CONTROL       0x0026

#define AC97_EXTENDED_STATUS	0x2a	/* Extended Audio Status and Control */
/* extended audio status and control bit defines */
#define AC97_EA_VRA		0x0001	/* Variable bit rate enable bit */
#define AC97_EA_DRA		0x0002	/* Double-rate audio enable bit */
#define AC97_EA_SPDIF		0x0004	/* S/PDIF out enable bit */
#define AC97_EA_VRM		0x0008	/* Variable bit rate for MIC enable bit */

struct ac97_initial_values {
  uint16_t port;
  uint16_t value;
};

static struct ac97_initial_values ac97_initial_values[] = {
    { AC97_RESET,             0x0000 },
    { AC97_MASTER_VOL_STEREO, 0x0000 }, // 0x0000 dflt
    { AC97_HEADPHONE_VOL,     0x0808 }, // 0x8000
    { AC97_PCMOUT_VOL,        0x0606 }, // 0x0606
    { AC97_MASTER_VOL_MONO,   0x0000 }, // 0x0000
    { AC97_PCBEEP_VOL,        0x0000 }, // 0x0000
    { AC97_PHONE_VOL,         0x0008 }, // 0x0008
    { AC97_MIC_VOL,           0x0505 }, // 0x8000
    { AC97_LINEIN_VOL,        0x0505 }, // 0x8808
    { AC97_CD_VOL,            0x0505 }, // 0x8808
    { AC97_VIDEO_VOL,         0x0000 }, // 0x8808
    { AC97_AUX_VOL,           0x0000 }, // 0x8808
    { AC97_RECORD_SELECT,     0x0000 }, // 0x0000
    { AC97_RECORD_GAIN,       0x0B0B }, // 0x0B0B
    { AC97_GENERAL_PURPOSE,   0x0000 }, // 0x0000
    { AC97_EXTENDED_STATUS,   AC97_EA_VRA|AC97_EA_VRM },
    { 0xffff, 0xffff }
};

static uint16_t ac97_read (struct ymf_card_s *card, uint8_t reg)
{
  snd_ymfpci_writel(card, YDSXGR_AC97CMDADR, reg | (1<<15));
  if (snd_ymfpci_codec_ready(card, 0)) {
    if (card->pci_dev->device_type == 0x744) { // && chip->rev < 2
      int i;
      for (i = 0; i < 600; i++)
        snd_ymfpci_readw(card, YDSXGR_PRISTATUSDATA);
    }
    return snd_ymfpci_readw(card, YDSXGR_PRISTATUSDATA);
  } else {
    return 0xffff;
  }
}

static int ac97_write (struct ymf_card_s *card, uint16_t reg, uint16_t value)
{
  if (reg > 0x7f) {
    return -1;
  }
  snd_ymfpci_writew(card, YDSXGR_AC97CMDADR, reg);
  snd_ymfpci_writew(card, YDSXGR_AC97CMDDATA, value);
  return snd_ymfpci_codec_ready(card, 0);
}

static int ac97_reset (struct ymf_card_s *card)
{
  int i;
  for (i = 0; ac97_initial_values[i].port != 0xffff; i++) {
    ac97_write(card, ac97_initial_values[i].port, ac97_initial_values[i].value);
  }
  return 0;
}

#if YMF_ENABLE_PCM
static int snd_ymfpci_ac3_init (struct ymf_card_s *card)
{
#if YMF_DEBUG
  DBG_Logi("ac3_init...\n");
#endif
  unsigned int size = card->pcmout_bufsize;
  if (card->dm2)
    MDma_free_cardmem(card->dm2);
  card->dm2 = MDma_alloc_cardmem(size);
  if (!card->dm2)
    return -1;
  memset(card->dm2->linearptr, 0, size);
  
  uint32_t addr = (uint32_t)pds_cardmem_physicalptr(card->dm2, card->dm2->linearptr);
  card->bank_effect[3][0]->base =
    card->bank_effect[3][1]->base = cpu_to_le32((uint32_t)addr);
  card->bank_effect[3][0]->loop_end =
    card->bank_effect[3][1]->loop_end = cpu_to_le32(size >> 2);
  addr = (uint32_t)pds_cardmem_physicalptr(card->dm2, card->dm2->linearptr + (size >> 1));
  card->bank_effect[4][0]->base =
    card->bank_effect[4][1]->base = cpu_to_le32((uint32_t)addr);
  card->bank_effect[4][0]->loop_end =
    card->bank_effect[4][1]->loop_end = cpu_to_le32(size >> 2);

  spin_lock_irq(&chip->reg_lock);
  snd_ymfpci_writel(card, YDSXGR_MAPOFEFFECT, snd_ymfpci_readl(card, YDSXGR_MAPOFEFFECT) | (3 << 3));
  spin_unlock_irq(&chip->reg_lock);
#if YMF_DEBUG
  DBG_Logi("did ac3_init...\n");
#endif
  return 0;
}

static int snd_ymfpci_memalloc (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  long size, playback_ctrl_size;
  int voice, bank;
  uint8_t *ptr;
  dma_addr_t ptr_addr;

  playback_ctrl_size = 4 + 4 * YDSXG_PLAYBACK_VOICES;
  card->bank_size_playback = snd_ymfpci_readl(card, YDSXGR_PLAYCTRLSIZE) << 2;
  card->bank_size_capture = snd_ymfpci_readl(card, YDSXGR_RECCTRLSIZE) << 2;
  card->bank_size_effect = snd_ymfpci_readl(card, YDSXGR_EFFCTRLSIZE) << 2;
  card->work_size = YDSXG_DEFAULT_WORK_SIZE;
  card->pcmout_bufsize = MDma_get_max_pcmoutbufsize(aui,0,PCMBUFFERPAGESIZE,2,0);
#if YMF_DEBUG
  DBG_Logi("pcmout_bufsize: 0x%X (%u)\n", card->pcmout_bufsize, card->pcmout_bufsize);
  DBG_Logi("sizes: %u %u %u %u\n", card->bank_size_playback, card->bank_size_capture, card->bank_size_effect, card->work_size);
#endif

  size = ALIGN(playback_ctrl_size, 0x100) +
    ALIGN(card->bank_size_playback * 2 * YDSXG_PLAYBACK_VOICES, 0x100) +
    ALIGN(card->bank_size_capture * 2 * YDSXG_CAPTURE_VOICES, 0x100) +
    ALIGN(card->bank_size_effect * 2 * YDSXG_EFFECT_VOICES, 0x100) +
    card->work_size + card->pcmout_bufsize + 0x100;
#if YMF_DEBUG
  DBG_Logi("DMA size: 0x%X (%u)\n", size, size);
#endif

  card->dm = MDma_alloc_cardmem(size);
  if (!card->dm)
    return -1;

  memset(card->dm->linearptr, 0, size);
  ptr = (uint8_t *)card->dm->linearptr;
  ptr_addr = (dma_addr_t)ptr;

  ptr = (uint8_t *)ALIGN((unsigned long)ptr, 0x100);
  ptr_addr = ALIGN(ptr_addr, 0x100);

  card->bank_base_playback = ptr;
  card->bank_base_playback_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);

  card->ctrl_playback = (uint32_t *)ptr;
  card->ctrl_playback[0] = cpu_to_le32(YDSXG_PLAYBACK_VOICES);
  ptr += ALIGN(playback_ctrl_size, 0x100);
  ptr_addr += ALIGN(playback_ctrl_size, 0x100);
  for (voice = 0; voice < YDSXG_PLAYBACK_VOICES; voice++) {
    card->voices[voice].number = voice;
    card->voices[voice].bank = (struct snd_ymfpci_playback_bank *)ptr;
    card->voices[voice].bank_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);
    for (bank = 0; bank < 2; bank++) {
      card->bank_playback[voice][bank] = (struct snd_ymfpci_playback_bank *)ptr;
      ptr += card->bank_size_playback;
      ptr_addr += card->bank_size_playback;
    }
  }
  ptr = (uint8_t *)ALIGN((unsigned long)ptr, 0x100);
  ptr_addr = ALIGN(ptr_addr, 0x100);
  card->bank_base_capture = ptr;
  card->bank_base_capture_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);
  for (voice = 0; voice < YDSXG_CAPTURE_VOICES; voice++) {
    for (bank = 0; bank < 2; bank++) {
      card->bank_capture[voice][bank] = (struct snd_ymfpci_capture_bank *)ptr;
      ptr += card->bank_size_capture;
      ptr_addr += card->bank_size_capture;
    }
  }
  ptr = (uint8_t *)ALIGN((unsigned long)ptr, 0x100);
  ptr_addr = ALIGN(ptr_addr, 0x100);
  card->bank_base_effect = ptr;
  card->bank_base_effect_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);
  for (voice = 0; voice < YDSXG_EFFECT_VOICES; voice++) {
    for (bank = 0; bank < 2; bank++) {
      card->bank_effect[voice][bank] = (struct snd_ymfpci_effect_bank *)ptr;
      ptr += card->bank_size_effect;
      ptr_addr += card->bank_size_effect;
    }
  }
  ptr = (uint8_t *)ALIGN((unsigned long)ptr, 0x100);
  ptr_addr = ALIGN(ptr_addr, 0x100);
  card->work_base = ptr;
  card->work_base_addr = (uint32_t)pds_cardmem_physicalptr(card->dm, ptr_addr);

  card->pcmout_buffer = card->work_base + card->work_size;
  card->pcmout_buffer_physaddr = (uint32_t)pds_cardmem_physicalptr(card->dm, card->pcmout_buffer);
  aui->card_DMABUFF = (char *)card->pcmout_buffer;

#if YMF_DEBUG
  DBG_Logi("playback base: %8.8X\n", card->bank_base_playback_addr);
#endif
  snd_ymfpci_writel(card, YDSXGR_PLAYCTRLBASE, card->bank_base_playback_addr);
  if (YDSXG_CAPTURE_VOICES == 0) {
    card->bank_base_capture_addr = 0;
  }
  snd_ymfpci_writel(card, YDSXGR_RECCTRLBASE, card->bank_base_capture_addr);
  snd_ymfpci_writel(card, YDSXGR_EFFCTRLBASE, card->bank_base_effect_addr);
  snd_ymfpci_writel(card, YDSXGR_WORKBASE, card->work_base_addr);
  snd_ymfpci_writel(card, YDSXGR_WORKSIZE, card->work_size >> 2);

  return 0;
}

// .resolution = 10417, // 1 / 96 kHz = 10.41666...us
// .ticks = 0x10000,
static int snd_ymfpci_timer_start (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  unsigned long flags;
  unsigned int count;

  spin_lock_irqsave(&card->reg_lock, flags);
  count = 480; // 5ms: 0.005/(1/96000)
  snd_ymfpci_writew(card, YDSXGR_TIMERCOUNT, count);
  snd_ymfpci_writeb(card, YDSXGR_TIMERCTRL, 0x03);
  spin_unlock_irqrestore(&card->reg_lock, flags);

  return 0;
}

static int snd_ymfpci_timer_stop (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  unsigned long flags;

  spin_lock_irqsave(&card->reg_lock, flags);
  snd_ymfpci_writeb(card, YDSXGR_TIMERCTRL, 0x00);
  spin_unlock_irqrestore(&card->reg_lock, flags);

  return 0;
}
#endif // PCM

void YMF_mixer_init (struct ymf_card_s *card)
{
#define IEC958_AES0_CON_EMPHASIS_NONE   (0<<3) // none emphasis
#define IEC958_AES1_CON_ORIGINAL   (1<<7) // depends on the category code
#define IEC958_AES1_CON_DIGDIGCONV_ID   0x02
#define IEC958_AES1_CON_PCM_CODER   (IEC958_AES1_CON_DIGDIGCONV_ID|0x00)
#define IEC958_AES3_CON_FS_48000   (2<<0) // 48kHz
#define SNDRV_PCM_DEFAULT_CON_SPDIF (IEC958_AES0_CON_EMPHASIS_NONE|     \
                                     (IEC958_AES1_CON_ORIGINAL<<8)|     \
                                     (IEC958_AES1_CON_PCM_CODER<<8)|    \
                                     (IEC958_AES3_CON_FS_48000<<24))
  // $ iecset
  // Mode: consumer
  // Data: audio
  // Rate: 48000 Hz
  // Copyright: permitted
  // Emphasis: none
  // Category: PCM coder
  // Original: original
  // Clock: 1000 ppm
  // $ iecset -x
  // AES0=0x04,AES1=0x82,AES2=0x00,AES3=0x02

  // S/PDIF output initialization
  //card->spdif_bits = card->spdif_pcm_bits = (SNDRV_PCM_DEFAULT_CON_SPDIF & 0xffff);

  // enable recording
  //card->spdif_bits = card->spdif_pcm_bits = 0x02008204;
  // Only set the first 16 bits. The sample rate (48KHz) bits in the fourth byte are set automatically in HW.
  card->spdif_bits = card->spdif_pcm_bits = 0x8204;

  snd_ymfpci_writel(card, YDSXGR_SPDIFOUTCTRL, 1); // S/PDIF on
  snd_ymfpci_writew(card, YDSXGR_SPDIFOUTSTATUS, card->spdif_bits);

  // S/PDIF input initialization
  snd_ymfpci_writel(card, YDSXGR_SPDIFINCTRL, 0);

  // digital mixer setup
  for (unsigned int reg = 0x80; reg < 0xc0; reg += 4)
    snd_ymfpci_writel(card, reg, 0);
  //snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x3fff3fff);
  //snd_ymfpci_writel(card, YDSXGR_BUF441OUTVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_ZVOUTVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SPDIFOUTVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_NATIVEADCINVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_NATIVEDACINVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCLOOPVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_LEGACYOUTVOL, 0x3fff3fff);
}

#if YMF_ENABLE_PCM
static void snd_ymfpci_hw_start (struct ymf_card_s *card);
static void YMF_setrate (struct mpxplay_audioout_info_s *aui);
static void YMF_start (struct mpxplay_audioout_info_s *aui);
#endif

static int YMF_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card;
  unsigned int i;
  uint16_t legacy_ctrl, legacy_ctrl2, old_legacy_ctrl;

  legacy_ctrl = 0;
  legacy_ctrl2 = 0x0800;  // SBEN = 0, SMOD = 01, LAD = 0

  card = (struct ymf_card_s *)pds_calloc(1, sizeof(struct ymf_card_s));
  if (!card)
    return 0;
  aui->card_private_data = card;

  card->pci_dev = (struct pci_config_s *)pds_calloc(1, sizeof(struct pci_config_s));
  if (!card->pci_dev)
    goto err_adetect;
  if (pcibios_search_devices(ymf_devices, card->pci_dev) != PCI_SUCCESSFUL)
    goto err_adetect;

  if (card->pci_dev->device_type == 0x754) {
    card->weird = 1;
  }
#if YMF_DEBUG
  ymfdump_card = card;
  if (!dumped) {
    DBG_Logi("PCI configuration space:\n");
    pcidump();
  }
#endif

#if YMF_ENABLE_PCM
  if (!aui->pcm) {
    if (aui->card_handler == &YMFSB_sndcard_info)
      return 0;
  } else {
    if (aui->card_handler != &YMFSB_sndcard_info)
      return 0;
  }
#endif

#if !YSBEMU_CONFIG_UTIL
  switch (card->pci_dev->device_type) {
  case 0x724:
  case 0x724F:
  case 0x740:
  case 0x740C:
    card->legacy_iobase = 0;
    break;
  default:
    card->legacy_iobase = pcibios_ReadConfig_Dword(card->pci_dev, 0x14);
    card->legacy_iobase &= 0xfff0;
    break;
  }
#if YMF_DEBUG
  DBG_Logi("YMF bar 1 (SB/FM/MPU): %x\n", card->legacy_iobase);
#endif
  if (!card->legacy_iobase) {
    goto try_724;
  }

  // YMF744/754 only
  uint16_t offset = 0x00;
  uint16_t sbport = card->legacy_iobase + offset;
  legacy_ctrl |= YMFPCI_LEGACY_SBEN;
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_SBBASE, sbport);

  uint16_t fmport = card->legacy_iobase + offset;
  legacy_ctrl |= YMFPCI_LEGACY_FMEN;
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_FMBASE, fmport);

  offset += 0x20;
  uint16_t mpuport = card->legacy_iobase + offset;
  legacy_ctrl |= YMFPCI_LEGACY_MEN;
  //legacy_ctrl |= YMFPCI_LEGACY_MIEN; // enable MPU401 irq
  legacy_ctrl &= ~YMFPCI_LEGACY_MIEN; // disable MPU401 irq
  legacy_ctrl2 |= YMFPCI_LEGACY2_IMOD;
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_MPU401BASE, mpuport);

#if 0 // Joystick
  uint32_t bar2 = pcibios_ReadConfig_Dword(card->pci_dev, 0x18);
#if YMF_DEBUG
  DBG_Logi("YMF bar 2 (JS): %x\n", bar2);
#endif
#if 0
  if (bar2 > card->legacy_iobase) {
    bar2 -= 0x100;
    pcibios_WriteConfig_Dword(card->pci_dev, 0x18, bar2);
    bar2 = pcibios_ReadConfig_Dword(card->pci_dev, 0x18);
    YMF_printf("YMF changed bar 2: %x\n", bar2);
  }
  bar2 &= 0xfff0;
#endif
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_JOYBASE, bar2);
  legacy_ctrl |= YMFPCI_LEGACY_JPEN;
#endif

try_724:
  if (!card->legacy_iobase) {
    fmport = 0x3a8;
    legacy_ctrl |= YMFPCI_LEGACY_FMEN;
    legacy_ctrl2 |= 3;
    sbport = 0x280;
    legacy_ctrl |= YMFPCI_LEGACY_SBEN;
    legacy_ctrl2 |= (3 << 2);
    mpuport = 0x334;
    legacy_ctrl2 |= (3 << 4);
    legacy_ctrl |= YMFPCI_LEGACY_MEN;
    //legacy_ctrl |= YMFPCI_LEGACY_MIEN; // enable MPU401 irq
    legacy_ctrl &= ~YMFPCI_LEGACY_MIEN; // disable MPU401 irq
    legacy_ctrl2 |= YMFPCI_LEGACY2_IMOD;
  }

  card->sbport = sbport;
  card->fmport = fmport;
  card->mpuport = mpuport;

  old_legacy_ctrl = pcibios_ReadConfig_Word(card->pci_dev, PCIR_DSXG_LEGACY);
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_LEGACY, legacy_ctrl);
  pcibios_WriteConfig_Word(card->pci_dev, PCIR_DSXG_ELEGACY, legacy_ctrl2);
  //printf("old: %x  new: %x\n", old_legacy_ctrl, legacy_ctrl);

  YMF_printf("FM port: %x\n", fmport);
  YMF_printf("MPU-401 port: %x\n", mpuport);

  if (!card->legacy_iobase) {
    // Check if subtractive decode is working
#define OPL_write(reg, val) do { outp(fmport, reg); pds_delay_10us(1); outp(fmport+1, val); pds_delay_10us(3); } while (0)
#define OPL_status() (inp(fmport) & 0xe0)
    OPL_write(0x04, 0x60); // Reset Timer 1 and Timer 2
    OPL_write(0x04, 0x80); // Reset the IRQ
    uint8_t fmsts1 = OPL_status();
    //YMF_printf("fmsts1: %x\n", fmsts1);
    OPL_write(0x02, 0xff); // Set Timer 1 to ff
    OPL_write(0x04, 0x21); // Unmask and start Timer 1
    pds_delay_10us(8); // Delay at least 80us
    uint8_t fmsts2 = OPL_status();
    OPL_write(0x04, 0x60); // Reset Timer 1 and Timer 2
    OPL_write(0x04, 0x80); // Reset the IRQ
    //YMF_printf("fmsts2: %x\n", fmsts2);
    if (!(fmsts1 == 0 && fmsts2 == 0xc0)) {
      YMF_printf("No OPL detected\n");
      card->sbport = 0;
      card->fmport = 0;
      card->mpuport = 0;
    } else {
      uint8_t fmsts3 = inp(fmport) & 0x06;
      //YMF_printf("fmsts3: %x\n", fmsts3);
      if (fmsts3 == 0) {
        YMF_printf("OPL3 detected\n");
      }
    }
  }

#define PCI_CAPABILITY_LIST  0x34
#define PCI_CAP_ID_PM        0x01  // Power Management
// Power Management Registers
#define PCI_PM_CTRL                  4  // PM control and status register
#define PCI_PM_CTRL_STATE_MASK  0x0003  // Current power state (D0 to D3)
  uint8_t c;
  uint8_t c0;
  uint8_t c1;
  c = pcibios_ReadConfig_Byte(card->pci_dev, PCI_CAPABILITY_LIST);
  c0 = pcibios_ReadConfig_Byte(card->pci_dev, c);
  c1 = pcibios_ReadConfig_Byte(card->pci_dev, c+1);
  if (c0 == PCI_CAP_ID_PM) {
    uint16_t w;
    w = pcibios_ReadConfig_Word(card->pci_dev, c + PCI_PM_CTRL);
    uint16_t state = w & PCI_PM_CTRL_STATE_MASK;
    if (state != 0) {
      pcibios_WriteConfig_Word(card->pci_dev, c + PCI_PM_CTRL, 0);
      w = pcibios_ReadConfig_Word(card->pci_dev, c + PCI_PM_CTRL);
      YMF_printf("power state: %u -> %u\n", state, w & PCI_PM_CTRL_STATE_MASK);
    }
  }
#endif

  aui->card_pci_dev = card->pci_dev;
  aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
  card->pci_iobase = pcibios_ReadConfig_Dword(card->pci_dev, 0x10);
  card->pci_iobase &= 0xfffffff8;
  //YMF_printf("pci_iobase: %X\n", card->pci_iobase);
  card->iobase = pds_dpmi_map_physical_memory(card->pci_iobase, 0x8000);
  if (!card->iobase)
    goto err_adetect;
#if YMF_DEBUG
  DBG_Logi("YMF registers before:\n");
  ymfdump();
#endif
  
  pcibios_enable_memmap_set_master_all(card->pci_dev);
  card->src441_used = -1;
  card->use_441_slot = 0;
  card->running = 0;
  //unsigned int cmd;
  //pcibios_enable_interrupt(card->pci_dev); // called in main.c, not used anyway
  //cmd = pcibios_ReadConfig_Word(card->pci_dev, PCIR_PCICMD);
  //pcibios_WriteConfig_Word(card->pci_dev, PCIR_PCICMD, cmd | (1<<8) | (1<<6));
  //cmd = pcibios_ReadConfig_Word(card->pci_dev, PCIR_PCICMD);
  //YMF_printf("pci command word: %4.4X\n", cmd);

#if YSBEMU_CONFIG_UTIL
  YMF_printf("Yamaha DS-XG sound card %X IRQ %d\n",
             card->pci_dev->device_type, card->irq);
#else
  YMF_printf("Yamaha DS-XG sound card %X FM %X MPU %X IRQ %d\n",
             card->pci_dev->device_type, fmport, mpuport, card->irq);
  //YMF_printf("iobase: %8.8X -> %8.8X\n", card->pci_iobase, card->iobase);

  snd_ymfpci_aclink_reset(card);
  pds_delay_10us(10);
  snd_ymfpci_codec_ready(card, 0);
  pds_delay_10us(10);
  snd_ymfpci_download_image(card);
  pds_delay_10us(10);
  snd_ymfpci_codec_ready(card, 0);

#if YMF_ENABLE_PCM
  if (aui->card_handler == &YMFSB_sndcard_info) {
    if (snd_ymfpci_memalloc(aui)) {
      goto err_adetect;
    }
    //if (snd_ymfpci_ac3_init(card)) {
    //  goto err_adetect;
    //}
  }
#endif
#endif

  YMF_mixer_init(card);
  //YMF_printf("S/PDIF first 16 status bits: %4.4X\n", snd_ymfpci_readw(card, YDSXGR_SPDIFOUTSTATUS));  

  ac97_reset(card);
  //YMF_printf("reset AC97\n");

#if 1
  // Line-in -> ADC -> SPDIF-out ???
  snd_ymfpci_writel(card, YDSXGR_SPDIFLOOPVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_NATIVEADCINVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_NATIVEDACINVOL, 0x3fff3fff);

  // Digitize analog inputs and output over S/PDIF
  // 3fff is the maximum volume
  snd_ymfpci_writel(card, YDSXGR_PRIADCOUTVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCOUTVOLL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCOUTVOLR, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCLOOPVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCLOOPVOLL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_PRIADCLOOPVOLR, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCOUTVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCOUTVOLL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCOUTVOLR, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCLOOPVOL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCLOOPVOLL, 0x3fff3fff);
  snd_ymfpci_writel(card, YDSXGR_SECADCLOOPVOLR, 0x3fff3fff);
#endif

#if YMF_DEBUG
  DBG_Logi("YMF registers after:\n");
  ymfdump();
  dumped = 1;
#endif

  if (!aui->card_select_index_fm || aui->card_select_index_fm == aui->card_test_index) {
    if (card->fmport) {
      aui->fm_port = card->fmport;
      aui->fm = 1;
    }
  }
  if (!aui->card_select_index_mpu401 || aui->card_select_index_mpu401 == aui->card_test_index) {
    if (card->mpuport) {
      aui->mpu401_port = card->mpuport;
      aui->mpu401 = 1;
    }
  }
#if 0 // done in au_cards.c
  // Disable HW FM/MPU if another card was selected
  if (!aui->card_select_index_fm && !(!aui->card_select_index || aui->card_select_index == aui->card_test_index)) {
    aui->fm_port = 0;
    aui->fm = 0;
  }
  if (!aui->card_select_index_mpu401 && !(!aui->card_select_index || aui->card_select_index == aui->card_test_index)) {
    aui->mpu401_port = 0;
    aui->mpu401 = 0;
  }
#endif

#if YMF_ENABLE_PCM
  if (aui->card_handler == &YMFSB_sndcard_info)
    return 1;
  else
#endif
    return 0;

 err_adetect:
  YMF_close(aui);
  return 0;
}

static void YMF_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "Yamaha DS-XG sound card %X FM %X MPU %X IRQ %d",
          card->pci_dev->device_type, card->fmport, card->mpuport, card->irq);
  pds_textdisplay_printf(sout);
}

static void snd_ymfpci_hw_start (struct ymf_card_s *card)
{
  unsigned long flags;

#if YMF_DEBUG
  DBG_Logi("hw_start %d\n", card->start_count);
#endif
  spin_lock_irqsave(&card->reg_lock, flags);
  if (card->start_count++ > 0)
    goto __end;
  snd_ymfpci_writel(card, YDSXGR_MODE,
                    snd_ymfpci_readl(card, YDSXGR_MODE) | 3);
  card->active_bank = snd_ymfpci_readl(card, YDSXGR_CTRLSELECT) & 1;
 __end:
  spin_unlock_irqrestore(&card->reg_lock, flags);
}

static void snd_ymfpci_hw_stop (struct ymf_card_s *card)
{
  unsigned long flags;
  long timeout = 1000;
#if YMF_DEBUG
  DBG_Logi("hw_stop %d\n", card->start_count);
#endif

  spin_lock_irqsave(&card->reg_lock, flags);
  if (--card->start_count > 0)
    goto __end;
  snd_ymfpci_writel(card, YDSXGR_MODE,
                    snd_ymfpci_readl(card, YDSXGR_MODE) & ~3);
  while (timeout-- > 0) {
    if ((snd_ymfpci_readl(card, YDSXGR_STATUS) & 2) == 0)
      break;
  }
  //if (atomic_read(&card->interrupt_sleep_count)) {
  //  atomic_set(&card->interrupt_sleep_count, 0);
  //  wake_up(&card->interrupt_sleep);
  //}
 __end:
  spin_unlock_irqrestore(&card->reg_lock, flags);
}

#if YMF_ENABLE_PCM
static void YMF_start (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  
#if YMF_DEBUG
  DBG_Logi("YMF_start %8.8X(%8.8X)\n", card->voices[VOICE_TO_USE].bank_addr, (uint32_t)card->voices[VOICE_TO_USE].bank);
  DBG_Logi(" voices[%u]: %8.8X(%8.8X)\n", VOICE_TO_USE+1, card->voices[VOICE_TO_USE+1].bank_addr, (uint32_t)card->voices[VOICE_TO_USE+1].bank);
#endif

  if (!card->weird) {
    //snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x3fff3fff);
    snd_ymfpci_writel(card, YDSXGR_NATIVEDACOUTVOL, 0x045e045e);
    snd_ymfpci_writel(card, YDSXGR_BUF441OUTVOL, 0x045e045e);
  }

  card->ctrl_playback[card->voices[VOICE_TO_USE].number + 1] = cpu_to_le32(card->voices[VOICE_TO_USE].bank_addr);
  if (card->voices[VOICE_TO_USE+1].use && !card->use_441_slot)
    card->ctrl_playback[card->voices[VOICE_TO_USE+1].number + 1] = cpu_to_le32(card->voices[VOICE_TO_USE+1].bank_addr);

  card->running = 1;
  card->active_bank = snd_ymfpci_readl(card, YDSXGR_CTRLSELECT) & 1;

  snd_ymfpci_timer_start(aui);
}

static void YMF_stop (struct mpxplay_audioout_info_s *aui)
{
#if YMF_DEBUG
  DBG_Logi("YMF_stop\n");
#endif
  struct ymf_card_s *card = aui->card_private_data;
  snd_ymfpci_hw_stop(card);
}

static int voice_alloc (struct ymf_card_s *card,
                        enum snd_ymfpci_voice_type type,
                        int pair,
                        struct snd_ymfpci_voice **rvoice)
{
  struct snd_ymfpci_voice *voice, *voice2;
  int idx;

  *rvoice = NULL;
  for (idx = 0; idx < YDSXG_PLAYBACK_VOICES; idx += pair ? 2 : 1) {
    voice = &card->voices[idx];
    voice2 = pair ? &card->voices[idx+1] : NULL;
    if (voice->use || (voice2 && voice2->use))
      continue;
    voice->use = 1;
    if (voice2)
      voice2->use = 1;
    switch (type) {
    case YMFPCI_PCM:
      voice->pcm = 1;
      if (voice2)
        voice2->pcm = 1;
      break;
    case YMFPCI_SYNTH:
      voice->synth = 1;
      break;
    case YMFPCI_MIDI:
      voice->midi = 1;
      break;
    }
    snd_ymfpci_hw_start(card);
    if (voice2)
      snd_ymfpci_hw_start(card);
    *rvoice = voice;
    return 0;
  }
  return -ENOMEM;
}

static int snd_ymfpci_voice_alloc (struct ymf_card_s *card,
                                   enum snd_ymfpci_voice_type type,
                                   int pair,
                                   struct snd_ymfpci_voice **rvoice)
{
  unsigned long flags;
  int result;

  if (snd_BUG_ON(!rvoice))
    return -EINVAL;
  if (snd_BUG_ON(pair && type != YMFPCI_PCM))
    return -EINVAL;

  spin_lock_irqsave(&card->voice_lock, flags);
  for (;;) {
    result = voice_alloc(card, type, pair, rvoice);
    if (result == 0 || type != YMFPCI_PCM)
      break;
    break;
  }
  spin_unlock_irqrestore(&card->voice_lock, flags);
  return result;
}

static int snd_ymfpci_voice_free (struct ymf_card_s *card, struct snd_ymfpci_voice *pvoice)
{
  unsigned long flags;

  if (snd_BUG_ON(!pvoice))
    return -EINVAL;
  snd_ymfpci_hw_stop(card);
  spin_lock_irqsave(&card->voice_lock, flags);
  if (pvoice->number == card->src441_used) {
    card->src441_used = -1;
    //pvoice->ypcm->use_441_slot = 0;
    card->use_441_slot = 0;
  }
  pvoice->use = pvoice->pcm = pvoice->synth = pvoice->midi = 0;
  //pvoice->ypcm = NULL;
  pvoice->interrupt = NULL;
  spin_unlock_irqrestore(&card->voice_lock, flags);
  return 0;
}

static void snd_ymfpci_pcm_interrupt (struct mpxplay_audioout_info_s *aui, struct snd_ymfpci_voice *voice)
{
  struct ymf_card_s *card = aui->card_private_data;
  int i;
  //struct snd_ymfpci_pcm *ypcm;
  //struct ymf_card_s *ypcm = card;
  uint32_t pos, delta, size;

  //ypcm = voice->ypcm;
  //if (!ypcm)
  //  return;
  //if (ypcm->substream == NULL)
  //  return;
  spin_lock(&card->reg_lock);
  // only called with voice 0
  if (card->running) {
    size = card->pcmout_bufsize >> SHIFTCONSTANT_2(aui->chan_card) >> 1;
    pos = le32_to_cpu(voice->bank[card->active_bank].start);
    
#if YMF_DEBUG
    //DBG_Logi("voice %u  bank %u  pos %u / %u\n", voice->number, card->active_bank, pos, size);
#endif
    if (pos < card->last_pos)
      //delta = pos + (ypcm->buffer_size - ypcm->last_pos);
      delta = pos + (size - card->last_pos);
    else
      delta = pos - card->last_pos;
    card->period_pos += delta;
    card->last_pos = pos;
    if (card->period_pos >= card->period_size) {
      card->period_pos %= card->period_size;
      //spin_unlock(&card->reg_lock);
      //snd_pcm_period_elapsed(ypcm->substream);
      //spin_lock(&card->reg_lock);
    }

#if 0
    //if (unlikely(card->update_pcm_vol)) {
    if (card->update_pcm_vol) {
      //unsigned int subs = card->substream->number;
      unsigned int next_bank = 1 - card->active_bank;
      struct snd_ymfpci_playback_bank *bank;
      uint32_t volume;

      bank = &voice->bank[next_bank];
      volume = cpu_to_le32(0x40000000);
      bank->left_gain_end = volume;
      //if (card->output_rear)
        bank->eff2_gain_end = volume;
      if (card->voices[VOICE_TO_USE+1].use)
        bank = &card->voices[VOICE_TO_USE+1].bank[next_bank];
      //volume = cpu_to_le32(card->pcm_mixer[subs].right << 15);
      bank->right_gain_end = volume;
      //if (card->output_rear)
        bank->eff3_gain_end = volume;
      card->update_pcm_vol--;
    }
#endif
  }
  spin_unlock(&card->reg_lock);
}

static uint32_t snd_ymfpci_calc_delta(uint32_t rate)
{
  switch (rate) {
  case 8000:    return 0x02aaab00;
  case 11025:   return 0x03accd00;
  case 16000:   return 0x05555500;
  case 22050:   return 0x07599a00;
  case 32000:   return 0x0aaaab00;
  case 44100:   return 0x0eb33300;
  default:      return ((rate << 16) / 375) << 5;
  }
}

static const uint32_t def_rate[8] = {
  100, 2000, 8000, 11025, 16000, 22050, 32000, 48000
};

static uint32_t snd_ymfpci_calc_lpfK(uint32_t rate)
{
  uint32_t i;
  static const uint32_t val[8] = {
    0x00570000, 0x06AA0000, 0x18B20000, 0x20930000,
    0x2B9A0000, 0x35A10000, 0x3EAA0000, 0x40000000
  };

  if (rate == 44100)
    return 0x46460000;
  for (i = 0; i < 8; i++)
    if (rate <= def_rate[i])
      return val[i];
  return val[0];
}

static uint32_t snd_ymfpci_calc_lpfQ(uint32_t rate)
{
  uint32_t i;
  static const uint32_t val[8] = {
    0x35280000, 0x34A70000, 0x32020000, 0x31770000,
    0x31390000, 0x31C90000, 0x33D00000, 0x40000000
  };

  if (rate == 44100)
    return 0x370A0000;
  for (i = 0; i < 8; i++)
    if (rate <= def_rate[i])
      return val[i];
  return val[0];
}

static void snd_ymfpci_pcm_init_voice (struct mpxplay_audioout_info_s *aui,
                                       //struct snd_ymfpci_pcm *ypcm,
                                       unsigned int voiceidx,
                                       //struct snd_pcm_runtime *runtime,
                                       int has_pcm_volume)
{
  struct ymf_card_s *card = aui->card_private_data;
  struct snd_ymfpci_voice *voice = &card->voices[voiceidx];
  uint32_t format;
  uint32_t delta = snd_ymfpci_calc_delta(aui->freq_card);
  uint32_t lpfQ = snd_ymfpci_calc_lpfQ(aui->freq_card);
  uint32_t lpfK = snd_ymfpci_calc_lpfK(aui->freq_card);
  struct snd_ymfpci_playback_bank *bank;
  unsigned int nbank;
  uint32_t vol_left, vol_right;
  uint8_t use_left, use_right;
  unsigned long flags;

  //if (snd_BUG_ON(!voice))
  //  return;
  if (aui->chan_card == 1) {
    use_left = 1;
    use_right = 1;
  } else {
    use_left = (voiceidx & 1) == 0;
    use_right = !use_left;
  }
  //if (has_pcm_volume) {
  //  vol_left = cpu_to_le32(ypcm->card->pcm_mixer
  //                         [ypcm->substream->number].left << 15);
  //  vol_right = cpu_to_le32(ypcm->card->pcm_mixer
  //                          [ypcm->substream->number].right << 15);
  //} else {
  vol_left = cpu_to_le32(0x40000000);
  vol_right = cpu_to_le32(0x40000000);
  //}
  spin_lock_irqsave(&ypcm->card->voice_lock, flags);
  format = aui->chan_card == 2 ? 0x00010000 : 0;
  if (aui->bits_card == 8)
    format |= 0x80000000;
  else if (card->pci_dev->device_type == 0x754 &&
           aui->freq_card == 44100 &&
           aui->chan_card == 2 &&
           voiceidx == 0 && (card->src441_used == -1 ||
                             card->src441_used == voice->number)) {
    card->src441_used = voice->number;
    //ypcm->use_441_slot = 1;
    card->use_441_slot = 1;
    format |= 0x10000000;
  }
  if (card->src441_used == voice->number &&
      (format & 0x10000000) == 0) {
    card->src441_used = -1;
    card->use_441_slot = 0;
  }
  if (aui->chan_card == 2 && (voiceidx & 1) != 0)
    format |= 1;
#if YMF_DEBUG
  DBG_Logi("init_voice freq %u %u (use %u) format: %8.8X   vols: %x (%u)  /  %x (%u)\n", aui->freq_card, voiceidx, voice->use, format, vol_left, use_left, vol_right, use_right);
#endif
  spin_unlock_irqrestore(&ypcm->card->voice_lock, flags);
  for (nbank = 0; nbank < 2; nbank++) {
    bank = &voice->bank[nbank];
    memset(bank, 0, sizeof(*bank));
    bank->format = cpu_to_le32(format);
    bank->base = card->pcmout_buffer_physaddr;
    bank->loop_end = cpu_to_le32(card->pcmout_bufsize >> SHIFTCONSTANT_2(aui->chan_card));
    bank->lpfQ = cpu_to_le32(lpfQ);
    bank->delta = bank->delta_end = cpu_to_le32(delta);
    bank->lpfK = bank->lpfK_end = cpu_to_le32(lpfK);
    bank->eg_gain = bank->eg_gain_end = cpu_to_le32(0x40000000);
    if (1) { // ypcm->output_front) {
      if (use_left) {
        bank->left_gain = bank->left_gain_end = vol_left;
        bank->eff2_gain = bank->eff2_gain_end = vol_left;
      }
      if (use_right) {
        bank->right_gain = bank->right_gain_end = vol_right;
        bank->eff3_gain = bank->eff3_gain_end = vol_right;
      }
      // rear?
      if (use_left) {
        bank->eff2_gain = bank->eff2_gain_end = vol_left;
      }
      if (use_right) {
        bank->eff3_gain = bank->eff3_gain_end = vol_right;
      }
    }
#if 0
    if (ypcm->output_rear) {
      if (!ypcm->swap_rear) {
        if (use_left) {
          bank->eff2_gain = bank->eff2_gain_end = vol_left;
        }
        if (use_right) {
          bank->eff3_gain = bank->eff3_gain_end = vol_right;
        }
      } else {
        // The SPDIF out channels seem to be swapped, so we have
        // to swap them here, too.  The rear analog out channels
        // will be wrong, but otherwise AC3 would not work.
        if (use_left) {
          bank->eff3_gain = bank->eff3_gain_end = vol_left;
        }
        if (use_right) {
          bank->eff2_gain = bank->eff2_gain_end = vol_right;
        }
      }
    }
#endif
  }
}

static int snd_ymfpci_pcm_voice_alloc (struct ymf_card_s *card, int voices)
{
#if YMF_DEBUG
  DBG_Logi("voice_alloc %d\n", voices);
#endif
  int err;

  if (card->voices[VOICE_TO_USE+1].use && voices < 2) {
    snd_ymfpci_voice_free(card, &card->voices[VOICE_TO_USE+1]);
  }
  if (voices == 1 && card->voices[VOICE_TO_USE].use)
    return 0; // already allocated
  if (voices == 2 && card->voices[VOICE_TO_USE].use && card->voices[VOICE_TO_USE+1].use)
    return 0; // already allocated
  if (voices > 1) {
    if (card->voices[VOICE_TO_USE].use && card->voices[VOICE_TO_USE+1].use) {
      snd_ymfpci_voice_free(card, &card->voices[VOICE_TO_USE]);
    }
  }
  struct snd_ymfpci_voice *rvoice;
  err = snd_ymfpci_voice_alloc(card, YMFPCI_PCM, voices > 1, &rvoice);
  if (err < 0)
    return err;
  //ypcm->voices[VOICE_TO_USE]->ypcm = ypcm;
  card->voices[VOICE_TO_USE].interrupt = snd_ymfpci_pcm_interrupt;
  //if (voices > 1) {
  //  ypcm->voices[VOICE_TO_USE+1] = &card->voices[ypcm->voices[VOICE_TO_USE]->number + 1];
  //  ypcm->voices[VOICE_TO_USE+1]->ypcm = ypcm;
  //}
  return 0;
}

//static int snd_ymfpci_playback_prepare (struct snd_pcm_substream *substream)
static int snd_ymfpci_playback_prepare (struct mpxplay_audioout_info_s *aui)
{
  //struct snd_pcm_runtime *runtime = substream->runtime;
  struct ymf_card_s *card = aui->card_private_data;
  //struct snd_ymfpci_pcm *ypcm = runtime->private_data;
  //struct snd_kcontrol *kctl;
  unsigned int nvoice;

  //ypcm->period_size = runtime->period_size;
  //ypcm->buffer_size = runtime->buffer_size;
  card->period_pos = 0;
  card->last_pos = 0;
  for (nvoice = 0; nvoice < aui->chan_card; nvoice++)
    snd_ymfpci_pcm_init_voice(aui, nvoice, 0);
  
  //if (substream->pcm == card->pcm && !ypcm->use_441_slot) {
  //  kctl = card->pcm_mixer[substream->number].ctl;
  //  kctl->vd[0].access &= ~SNDRV_CTL_ELEM_ACCESS_INACTIVE;
  //  snd_ctl_notify(card, SNDRV_CTL_EVENT_MASK_INFO, &kctl->id);
  //}
  return 0;
}

static long YMF_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct ymf_card_s *card = aui->card_private_data;
  unsigned long bufpos;

#if 0
  // dummy
  card->last_pos += aui->card_samples_per_int;
  bufpos = card->last_pos;
  bufpos <<= SHIFTCONSTANT_2(aui->chan_card);
  if (bufpos >= aui->card_dmasize) {
    card->last_pos = 0;
    bufpos = 0;
  }
#endif

#if 1
  struct snd_ymfpci_voice *voice = &card->voices[VOICE_TO_USE];
  bufpos = le32_to_cpu(voice->bank[card->active_bank].start);
  bufpos <<= SHIFTCONSTANT_2(aui->chan_card);
#endif
  
#if YMF_DEBUG > 1
  if ((ymf_int_cnt % 500) == 0) {
    DBG_Logi("bp %u / %u\n", bufpos, aui->card_dmasize);
  }
  if (bufpos == aui->card_dmasize) {
    DBG_Logi("getbufpos %u == dmasize\n", bufpos);
  }
#endif

  if (bufpos < aui->card_dmasize) 
    aui->card_dma_lastgoodpos = bufpos;

  return aui->card_dma_lastgoodpos;
}

static void YMF_setrate (struct mpxplay_audioout_info_s *aui)
{
#if YMF_DEBUG
  DBG_Logi("setrate\n");
#endif
  struct ymf_card_s *card = aui->card_private_data; 
  if (aui->freq_card < 8000) {
    aui->freq_card = 8000;
  } else if (aui->freq_card > 48000) {
    aui->freq_card = 48000;
  }
  aui->chan_card = 2;
  aui->bits_card = 16;
  aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE;
#if YMF_DEBUG
  DBG_Logi("pcmout_bufsize: %d max: %d\n", card->pcmout_bufsize, MDma_get_max_pcmoutbufsize(aui, 0, PCMBUFFERPAGESIZE, 2, 0));
#endif
  unsigned int dmabufsize = MDma_init_pcmoutbuf(aui, card->pcmout_bufsize, PCMBUFFERPAGESIZE, 0);
  card->pcmout_bufsize = dmabufsize;
  
  int periods = max(1, dmabufsize / PCMBUFFERPAGESIZE);
  card->dma_size    = dmabufsize >> SHIFTCONSTANT_2(aui->chan_card);
  card->period_size = (dmabufsize / periods) >> SHIFTCONSTANT_2(aui->chan_card);
  aui->card_samples_per_int = card->period_size >> SHIFTCONSTANT_2(aui->chan_card);
#if YMF_DEBUG
  DBG_Logi("buffer config: dmabufsize:%d period_size:%d aui->card_dmasize: %d periods: %d\n",dmabufsize,card->period_size, aui->card_dmasize,periods);
#endif

  snd_ymfpci_ac3_init(card);

  snd_ymfpci_pcm_voice_alloc(card, aui->chan_card);
 
  snd_ymfpci_playback_prepare(aui);
}

static aucards_onemixerchan_s YMF_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0x22,7,5,0},{0x22,7,1,0}}};
static aucards_onemixerchan_s YMF_pcm_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),      2,{{0x04,7,5,0},{0x04,7,1,0}}};
static aucards_onemixerchan_s YMF_cdin_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME),    2,{{0x28,7,5,0},{0x28,7,1,0}}};
static aucards_onemixerchan_s YMF_linein_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME),2,{{0x2E,7,5,0},{0x2E,7,1,0}}};

static aucards_allmixerchan_s YMF_mixerset[] = {
  &YMF_master_vol,
  //&YMF_pcm_vol,
  //&YMF_cdin_vol,
  //&YMF_linein_vol,
  NULL
};

static void YMF_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct ymf_card_s *card = aui->card_private_data;
  if (card->sbport == 0) {
    return;
  }
  val |= 1<<4;
  val |= 1<<0;
  outp(card->sbport + 0x4, reg & 0xff);
  unsigned long curval = inp(card->sbport + 0x5);
#if YMF_DEBUG
  DBG_Logi("write mixer: %x, %x->%x\n", reg, curval, val);
#endif
  outp(card->sbport + 0x4, reg & 0xff);
  outp(card->sbport + 0x5, val & 0xff);

#if YMF_DEBUG
  if (reg == 0x22 && !dumped)
    ymfdump();
  dumped = 1;
#endif
}

static unsigned long YMF_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct ymf_card_s *card = aui->card_private_data;
  if (card->sbport == 0) {
    return 0;
  }
  outp(card->sbport + 0x4, reg & 0xff);
  unsigned long curval = inp(card->sbport + 0x5) & 0xff;
#if YMF_DEBUG
  DBG_Logi("read mixer: %x, %x\n", reg, curval);
#endif
  return curval;
}
#endif

one_sndcard_info YMF_sndcard_info={
  "YMF",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &YMF_adetect,
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

  &ioport_fm_write,
  &ioport_fm_read,
  &ioport_mpu401_write,
  &ioport_mpu401_read,
};

#if YMF_ENABLE_PCM
one_sndcard_info YMFSB_sndcard_info={
  "YMF",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &YMF_adetect,
  &YMF_card_info,
  &YMF_start,
  &YMF_stop,
  &YMF_close,
  &YMF_setrate,

  &MDma_writedata,
  &YMF_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &YMF_IRQRoutine,

  &YMF_writeMIXER,
  &YMF_readMIXER,
  &YMF_mixerset[0],

  &ioport_fm_write,
  &ioport_fm_read,
  &ioport_mpu401_write,
  &ioport_mpu401_read,
};
#else
one_sndcard_info YMFSB_sndcard_info={
  "YMF",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &YMF_adetect,
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

  &ioport_fm_write,
  &ioport_fm_read,
  &ioport_mpu401_write,
  &ioport_mpu401_read,
};
#endif

#endif // AU_CARDS_LINK_IHD
