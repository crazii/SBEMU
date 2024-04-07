// EMU10K1X driver for SBEMU
// based on the Linux driver

#include "au_linux.h"

#ifdef AU_CARDS_LINK_EMU10K1X

#define EMU10K1X_DEBUG 0

#if EMU10K1X_DEBUG
#define emu10k1xdbg(...) do { DBG_Logi("EMU10K1X: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define emu10k1xdbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "sound/core.h"
#include "sound/pcm.h"
#include "sound/ac97_codec.h"

static pci_device_s emu10k1x_devices[] = {
  {"EMU10K1X", 0x1102, 0x0006, 0},
  {NULL,0,0,0}
};

struct emu10k1x_card_s {
  struct au_linux_card card;
};

extern unsigned char emu10k1x_mpu401_read (void *card, unsigned int idx);
extern void emu10k1x_mpu401_write (void *card, unsigned int idx, unsigned char data);

extern int snd_emu10k1x_playback_open (struct snd_pcm_substream *substream);
extern int snd_emu10k1x_pcm_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *hw_params);
extern int snd_emu10k1x_pcm_prepare (struct snd_pcm_substream *substream);
extern int snd_emu10k1x_pcm_trigger (struct snd_pcm_substream *substream, int cmd);
extern struct snd_pcm_ops snd_emu10k1x_playback_ops;
extern int snd_emu10k1x_probe (struct snd_card *card, struct pci_dev *pci, int enable_spdif);

extern void snd_emu10k1x_free(struct snd_card *card);

static void EMU10K1X_close (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  if (card) {
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void EMU10K1X_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "EMU10K1X : Creative %s (%4.4X) IRQ %u", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);
  pds_textdisplay_printf(sout);
}

static int EMU10K1X_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card;
  uint32_t iobase;
  int err;

  emu10k1xdbg("adetect\n");

  card = (struct emu10k1x_card_s *)pds_zalloc(sizeof(struct emu10k1x_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, emu10k1x_devices) < 0)
    goto err_adetect;
  emu10k1xdbg("PCI subsystem %4.4X:%4.4X\n", card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);
  int probe_only = aui->card_controlbits & AUINFOS_CARDCNTRLBIT_TESTCARD;
  int spdif = !((aui->card_select_config & 1) == 0);
  err = snd_emu10k1x_probe(card->card.linux_snd_card, card->card.linux_pci_dev, spdif);
  if (err)
    goto err_adetect;
  aui->freq_card = 48000;
  aui->mpu401 = 1;

  emu10k1xdbg("EMU10K1X : Creative %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  if (!aui->card_select_index || aui->card_select_index == aui->card_test_index) {
    if (!probe_only && spdif) {
      printf("EMU10K1X WARNING: S/PDIF ouput enabled. Analog out disabled. Use /O0 to enable analog output.\n");
    }
  }

  return 1;

err_adetect:
  EMU10K1X_close(aui);
  return 0;
}

static void EMU10K1X_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  int err;

  emu10k1xdbg("setrate %u -> 48000\n", aui->freq_card);
  aui->freq_card = 48000; // 48KHz only
  aui->card_dmasize = 512;
  //aui->card_dmasize = 1024;
  aui->card_dma_buffer_size = 4096;
  aui->dma_addr_bits = 32;
  aui->buffer_size_shift = 0;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, &snd_emu10k1x_playback_ops);
  if (err) goto err_setrate;
  snd_emu10k1x_pcm_prepare(card->card.pcm_substream);
  return;

 err_setrate:
  emu10k1xdbg("setrate error\n");
}

static void EMU10K1X_start (struct mpxplay_audioout_info_s *aui)
{
  emu10k1xdbg("start\n");
  struct emu10k1x_card_s *card = aui->card_private_data;
  snd_emu10k1x_pcm_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void EMU10K1X_stop (struct mpxplay_audioout_info_s *aui)
{
  emu10k1xdbg("stop\n");
  struct emu10k1x_card_s *card = aui->card_private_data;
  snd_emu10k1x_pcm_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int emu10k1x_int_cnt = 0;
extern snd_pcm_uframes_t snd_emu10k1x_pcm_pointer(struct snd_pcm_substream *substream);

static long EMU10K1X_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  unsigned long bufpos = snd_emu10k1x_pcm_pointer(card->card.pcm_substream);
  bufpos <<= 1;
#if EMU10K1X_DEBUG > 1
  if ((emu10k1x_int_cnt % 950) == 0)
    emu10k1xdbg("getbufpos %u / %u\n", bufpos, aui->card_dmasize);
  //if (bufpos == aui->card_dmasize)
  //  emu10k1xdbg("getbufpos %u == dmasize\n", bufpos);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s EMU10K1X_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,15,4,0},{0,15,0,0}}};
static aucards_allmixerchan_s EMU10K1X_mixerset[] = {
  &EMU10K1X_master_vol,
  NULL
};

extern u16 emu10k1x_ac97_read (struct snd_card *card, u8 reg);
extern void emu10k1x_ac97_write (struct snd_card *card, u8 reg, u16 val);

static void EMU10K1X_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  emu10k1xdbg("write mixer val: %X\n", val);
  // warning: uses only one channel's volume
  val = ((val & 15) << 1) | 1;
  u16 lval = 31 - (val & 31);
  u16 ac97val = (lval << 8) | lval;
  if (val <= 1) ac97val |= 0x8000;
  emu10k1xdbg("write mixer ac97val: %4.4X\n", ac97val);
  emu10k1x_ac97_write(card->card.linux_snd_card, AC97_MASTER, 0x0000); // MAX
  emu10k1x_ac97_write(card->card.linux_snd_card, AC97_LINE, 0x0000); // MAX
  emu10k1x_ac97_write(card->card.linux_snd_card, AC97_PCM, ac97val);
}

static unsigned long EMU10K1X_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  u16 ac97val = emu10k1x_ac97_read(card->card.linux_snd_card, AC97_PCM);
  emu10k1xdbg("read ac97val %4.4X\n", ac97val);
  u16 lval = 31 - (ac97val & 31);
  lval >>= 1;
  u16 val = (lval << 4) | lval;
  if (ac97val & 0x8000)
    return 0;
  emu10k1xdbg("read mixer returning %X\n", val);
  return val;
}

extern irqreturn_t snd_emu10k1x_interrupt(int irq, void *dev_id);

static int EMU10K1X_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  irqreturn_t handled = snd_emu10k1x_interrupt(card->card.irq, card->card.linux_snd_card->private_data);
#if EMU10K1X_DEBUG
  if (handled) {
    if ((emu10k1x_int_cnt % 500) == 0) DBG_Logi("emu10k1xirq %u\n", emu10k1x_int_cnt);
    emu10k1x_int_cnt++;
  }
#endif
  return handled;
}

static uint8_t EMU10K1X_mpu401_read (struct mpxplay_audioout_info_s *aui, unsigned int idx)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  return emu10k1x_mpu401_read(card->card.linux_snd_card, idx);
}

static void EMU10K1X_mpu401_write (struct mpxplay_audioout_info_s *aui, unsigned int idx, uint8_t data)
{
  struct emu10k1x_card_s *card = aui->card_private_data;
  emu10k1x_mpu401_write(card->card.linux_snd_card, idx, data);
}

one_sndcard_info EMU10K1X_sndcard_info = {
  "EMU10K1X",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &EMU10K1X_adetect,
  &EMU10K1X_card_info,
  &EMU10K1X_start,
  &EMU10K1X_stop,
  &EMU10K1X_close,
  &EMU10K1X_setrate,

  &MDma_writedata,
  &EMU10K1X_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &EMU10K1X_IRQRoutine,

  &EMU10K1X_writeMIXER,
  &EMU10K1X_readMIXER,
  &EMU10K1X_mixerset[0],

  NULL,
  NULL,
  &EMU10K1X_mpu401_write,
  &EMU10K1X_mpu401_read,
};

#endif // AUCARDS_LINK_EMU10K1X
