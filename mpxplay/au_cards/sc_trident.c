// Trident 4D Wave driver for SBEMU
// based on the Linux driver

#include "au_linux.h"

#ifdef AU_CARDS_LINK_TRIDENT

#define TRIDENT_DEBUG 0

#if TRIDENT_DEBUG
#define tridentdbg(...) do { DBG_Logi("TRIDENT: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define tridentdbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "sound/core.h"
#include "sound/pcm.h"
#include "sound/ac97_codec.h"
#include "../../drivers/trident/trident.h"

static pci_device_s trident_devices[] = {
  {"Trident 4DWave DX", PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_TRIDENT_4DWAVE_DX, 1},
  {"Trident 4dWave NX", PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_TRIDENT_4DWAVE_NX, 1},
  {"SI 7018", PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_7018, 2},
  {NULL,0,0,0}
};

struct trident_card_s {
  struct au_linux_card card;
};

extern unsigned char trident_mpu401_read (void *card, unsigned int idx);
extern void trident_mpu401_write (void *card, unsigned int idx, unsigned char data);

extern int snd_trident_synth_open (struct snd_pcm_substream *substream);

extern int snd_trident_playback_open (struct snd_pcm_substream *substream);
extern int snd_trident_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *hw_params);
extern int snd_trident_playback_prepare (struct snd_pcm_substream *substream);
extern int snd_trident_trigger (struct snd_pcm_substream *substream, int cmd);
extern struct snd_pcm_ops snd_trident_playback_ops;
extern int snd_trident_probe (struct snd_card *card, struct pci_dev *pci);

extern void snd_trident_free(struct snd_trident *trident);

static void TRIDENT_close (struct mpxplay_audioout_info_s *aui)
{
  tridentdbg("close");
  struct trident_card_s *card = aui->card_private_data;
  if (card) {
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void TRIDENT_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct trident_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "TRIDENT : %s (%4.4X) IRQ %u", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);
  pds_textdisplay_printf(sout);
}

static int TRIDENT_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct trident_card_s *card;
  uint32_t iobase;
  int err;

  tridentdbg("adetect\n");

  card = (struct trident_card_s *)pds_zalloc(sizeof(struct trident_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, trident_devices) < 0)
    goto err_adetect;
  tridentdbg("pci subsystem %4.4X:%4.4X\n", card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);

  struct snd_trident *trident;
  int pcm_channels = 32;
  int wavetable_size = 8192;
  err = snd_trident_create(card->card.linux_snd_card,
                           card->card.linux_pci_dev,
                           pcm_channels,
                           card->card.pci_dev->device_type == 2 ? 1 : 2,
                           wavetable_size,
                           &trident);
  if (err < 0)
    goto err_adetect;

  card->card.linux_snd_card->private_data = trident;

  aui->mpu401_port = trident->midi_port;
  aui->mpu401 = 1;

  tridentdbg("TRIDENT : %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  return 1;

err_adetect:
  TRIDENT_close(aui);
  return 0;
}

static void TRIDENT_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct trident_card_s *card = aui->card_private_data;
  int err;

  tridentdbg("setrate %u\n", aui->freq_card);
  if (aui->freq_card < 4000) {
    aui->freq_card = 4000;
  } else if (aui->freq_card > 48000) {
    aui->freq_card = 48000;
  }
  aui->card_dmasize = 512;
  aui->card_dma_buffer_size = 4096;
  aui->dma_addr_bits = 30;
  aui->buffer_size_shift = 2;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, &snd_trident_playback_ops);
  if (err) goto err_setrate;

  snd_trident_playback_prepare(card->card.pcm_substream);
  return;

 err_setrate:
  tridentdbg("setrate error\n");
}

static void TRIDENT_start (struct mpxplay_audioout_info_s *aui)
{
  tridentdbg("start\n");
  struct trident_card_s *card = aui->card_private_data;
  snd_trident_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void TRIDENT_stop (struct mpxplay_audioout_info_s *aui)
{
  tridentdbg("stop\n");
  struct trident_card_s *card = aui->card_private_data;
  snd_trident_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int trident_int_cnt = 0;
extern snd_pcm_uframes_t snd_trident_playback_pointer(struct snd_pcm_substream *substream);

static long TRIDENT_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct trident_card_s *card = aui->card_private_data;
  unsigned long bufpos = snd_trident_playback_pointer(card->card.pcm_substream);
  bufpos <<= 2;
#if TRIDENT_DEBUG > 1
  if ((trident_int_cnt % 90) == 0)
    tridentdbg("getbufpos %u / %u\n", bufpos, aui->card_dmasize);
  if (bufpos == aui->card_dmasize)
    tridentdbg("getbufpos %u == dmasize\n", bufpos);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s TRIDENT_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,15,4,0},{0,15,0,0}}};
static aucards_allmixerchan_s TRIDENT_mixerset[] = {
  &TRIDENT_master_vol,
  NULL
};

extern u16 snd_trident_ac97_read (struct snd_card *card, u16 reg);
extern void snd_trident_ac97_write (struct snd_card *card, u16 reg, u16 val);

static void TRIDENT_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct trident_card_s *card = aui->card_private_data;
  tridentdbg("write mixer val: %X\n", val);
  // warning: uses only one channel's volume
  val = ((val & 15) << 1) | 1;
  u16 lval = 31 - (val & 31);
  u16 ac97val = (lval << 8) | lval;
  if (val <= 1) ac97val |= 0x8000;
  tridentdbg("write mixer ac97val: %4.4X\n", ac97val);
  snd_trident_ac97_write(card->card.linux_snd_card, AC97_MASTER, ac97val);
}

static unsigned long TRIDENT_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct trident_card_s *card = aui->card_private_data;
  u16 ac97val = snd_trident_ac97_read(card->card.linux_snd_card, AC97_MASTER);
  tridentdbg("read ac97val %4.4X\n", ac97val);
  u16 lval = 31 - (ac97val & 31);
  lval >>= 1;
  u16 val = (lval << 4) | lval;
  if (ac97val & 0x8000)
    return 0;
  tridentdbg("read mixer returning %X\n", val);
  return val;
}

extern irqreturn_t snd_trident_interrupt(int irq, void *dev_id);

static int TRIDENT_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct trident_card_s *card = aui->card_private_data;
  int handled = snd_trident_interrupt(card->card.irq, card->card.linux_snd_card->private_data);
#if TRIDENT_DEBUG
  if (handled) {
    if ((trident_int_cnt % 500) == 0) DBG_Logi("tridentirq %u\n", trident_int_cnt);
    trident_int_cnt++;
  }
#endif
  return handled;
}

one_sndcard_info TRIDENT_sndcard_info = {
  "TRIDENT",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &TRIDENT_adetect,
  &TRIDENT_card_info,
  &TRIDENT_start,
  &TRIDENT_stop,
  &TRIDENT_close,
  &TRIDENT_setrate,

  &MDma_writedata,
  &TRIDENT_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &TRIDENT_IRQRoutine,

  &TRIDENT_writeMIXER,
  &TRIDENT_readMIXER,
  &TRIDENT_mixerset[0],

  NULL,
  NULL,
  &ioport_mpu401_write,
  &ioport_mpu401_read,
};

#endif // AUCARDS_LINK_TRIDENT
