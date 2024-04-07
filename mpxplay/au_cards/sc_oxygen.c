// OXYGEN(CMI8788) driver for SBEMU
// based on the Linux driver

#include "au_linux.h"

#ifdef AU_CARDS_LINK_OXYGEN

#define OXYGEN_DEBUG 0

#if OXYGEN_DEBUG
#define oxygendbg(...) do { DBG_Logi("OXYGEN: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define oxygendbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "sound/core.h"
#include "sound/pcm.h"
#include "sound/ac97_codec.h"

// Only tested with Xonar DG S/PDIF
#define OXYGEN_TYPE_XONAR_DG 0
static pci_device_s oxygen_devices[] = {
  {"Asus Xonar DG", 0x13F6, 0x8788, OXYGEN_TYPE_XONAR_DG},
  {"OXYGEN C-Media reference design", 0x10b0, 0x0216, 1},
  {"OXYGEN C-Media reference design", 0x10b0, 0x0217, 1},
  {"OXYGEN C-Media reference design", 0x10b0, 0x0218, 1},
  {"OXYGEN C-Media reference design", 0x10b0, 0x0219, 1},
  {"OXYGEN C-Media reference design", 0x13f6, 0x0001, 1},
  {"OXYGEN C-Media reference design", 0x13f6, 0x0010, 1},
  {"OXYGEN C-Media reference design", 0x13f6, 0x8788, 1},
  {"OXYGEN C-Media reference design", 0x147a, 0xa017, 1},
  {"OXYGEN C-Media reference design", 0x1a58, 0x0910, 1},
  {"Asus Xonar DGX", 0x1043, 0x8521, 2},
  {"PCI 2.0 HD Audio", 0x13f6, 0x8782, 3},
  {"Kuroutoshikou CMI8787-HG2PCI", 0x13f6, 0xffff, 4},
  {"TempoTec HiFier Fantasia", 0x14c3, 0x1710, 5},
  {"TempoTec HiFier Serenade", 0x14c3, 0x1711, 6},
  {"AuzenTech X-Meridian", 0x415a, 0x5431, 7},
  {"AuzenTech X-Meridian 2G", 0x5431, 0x017a, 8},
  {"HT-Omega Claro", 0x7284, 0x9761, 9},
  {"HT-Omega Claro halo", 0x7284, 0x9781, 10},
  {NULL,0,0,0}
};

struct oxygen_card_s {
  struct au_linux_card card;
};

extern struct snd_pcm_ops oxygen_spdif_ops;
extern struct snd_pcm_ops oxygen_multich_ops;
extern struct snd_pcm_ops oxygen_ac97_ops;
//static struct snd_pcm_ops *oxygen_ops = &oxygen_multich_ops;
//static struct snd_pcm_ops *oxygen_ops = &oxygen_ac97_ops;
static struct snd_pcm_ops *oxygen_ops = &oxygen_spdif_ops;
extern int snd_oxygen_probe (struct snd_card *card, struct pci_dev *pci, int probe_only);
extern irqreturn_t oxygen_interrupt(int irq, void *dev_id);

static void OXYGEN_close (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card = aui->card_private_data;
  if (card) {
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void OXYGEN_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "OXYGEN : %s (%4.4X) IRQ %u", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);
  pds_textdisplay_printf(sout);
}

static int OXYGEN_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card;
  uint32_t iobase;
  int err;

  oxygendbg("adetect\n");

  card = (struct oxygen_card_s *)pds_zalloc(sizeof(struct oxygen_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, oxygen_devices) < 0)
    goto err_adetect;

  oxygendbg("PCI subsystem %4.4X:%4.4X\n", card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);
  if ((aui->card_select_config & 1) == 0) {
    oxygen_ops = &oxygen_multich_ops;
  } else {
    oxygen_ops = &oxygen_spdif_ops;
  }
  int probe_only = aui->card_controlbits & AUINFOS_CARDCNTRLBIT_TESTCARD;
  err = snd_oxygen_probe(card->card.linux_snd_card, card->card.linux_pci_dev, probe_only);
  if (err) goto err_adetect;

  oxygendbg("OXYGEN : %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  return 1;

err_adetect:
  OXYGEN_close(aui);
  return 0;
}

static void OXYGEN_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card = aui->card_private_data;
  int err;

  oxygendbg("setrate %u\n", aui->freq_card);
  if (oxygen_ops == &oxygen_ac97_ops) {
    aui->freq_card = 48000;
  } else {
    if (aui->freq_card < 32000) {
      aui->freq_card = 32000;
    } else if (aui->freq_card > 192000) {
      aui->freq_card = 192000;
    }
  }
  oxygendbg("-> %u\n", aui->freq_card);
  aui->card_dmasize = 512;
  aui->card_dma_buffer_size = 1024 * 8;
  aui->dma_addr_bits = 32;
  aui->buffer_size_shift = 1;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, oxygen_ops);
  if (err) goto err_setrate;
  oxygen_ops->prepare(card->card.pcm_substream);
  return;

 err_setrate:
  oxygendbg("setrate error\n");
}

static void OXYGEN_start (struct mpxplay_audioout_info_s *aui)
{
  oxygendbg("start\n");
  struct oxygen_card_s *card = aui->card_private_data;
  oxygen_ops->trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void OXYGEN_stop (struct mpxplay_audioout_info_s *aui)
{
  oxygendbg("stop\n");
  struct oxygen_card_s *card = aui->card_private_data;
  oxygen_ops->trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int oxygen_int_cnt = 0;

static long OXYGEN_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card = aui->card_private_data;
  unsigned long bufpos = oxygen_ops->pointer(card->card.pcm_substream);
  bufpos <<= 1;
#if OXYGEN_DEBUG > 1
  if ((oxygen_int_cnt % 45) == 0)
    oxygendbg("getbufpos %u / %u\n", bufpos, aui->card_dmasize);
  //if (bufpos == aui->card_dmasize)
  //  oxygendbg("getbufpos %u == dmasize\n", bufpos);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s OXYGEN_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,255,8,0},{0,255,0,0}}};
static aucards_allmixerchan_s OXYGEN_mixerset[] = {
 &OXYGEN_master_vol,
 NULL
};

extern uint16_t xonar_stereo_volume_get (struct snd_card *card);
extern int xonar_stereo_volume_put (struct snd_card *card, uint16_t val);

static void OXYGEN_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct oxygen_card_s *card = aui->card_private_data;
  // Xonar DG Analog (DAC) only
  if (card->card.pci_dev->device_type == OXYGEN_TYPE_XONAR_DG) {
    // map 0-255 to 0,128-255
    uint16_t val1 = val & 0xff;
    uint16_t val2 = (val >> 8) & 0xff;
    val1 = 128 + (val1 >> 1);
    val2 = 128 + (val2 >> 1);
    if (val1 == 128) val1 = 0;
    if (val2 == 128) val2 = 0;
    val = ((val2 << 8) & 0xff00) | (val1 & 0xff);
    oxygendbg("write mixer val: %X\n", val);
    xonar_stereo_volume_put(card->card.linux_snd_card, val);
  }
}

static unsigned long OXYGEN_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct oxygen_card_s *card = aui->card_private_data;
  if (card->card.pci_dev->device_type == OXYGEN_TYPE_XONAR_DG) {
    uint16_t val = xonar_stereo_volume_get(card->card.linux_snd_card);
    // map 0,128-255 to 0-255
    uint16_t val1 = val & 0xff;
    uint16_t val2 = (val >> 8) & 0xff;
    val1 = ((val1 - 128) << 1) + 1;
    val2 = ((val2 - 128) << 1) + 1;
    if (val1 == 1) val1 = 0;
    if (val2 == 1) val2 = 0;
    val = ((val2 << 8) & 0xff00) | (val1 & 0xff);
    oxygendbg("read mixer returning %X\n", val);
    return (unsigned long)val;
  } else {
    return 0xffff;
  }
}

static int OXYGEN_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct oxygen_card_s *card = aui->card_private_data;
  int handled = oxygen_interrupt(card->card.irq, card->card.linux_snd_card->private_data);
#if OXYGEN_DEBUG
  if (handled) {
    if ((oxygen_int_cnt % 500) == 0) DBG_Logi("oxygenirq %u\n", oxygen_int_cnt);
    oxygen_int_cnt++;
  }
#endif
  return handled;
}

one_sndcard_info OXYGEN_sndcard_info = {
 "OXYGEN",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,
 &OXYGEN_adetect,
 &OXYGEN_card_info,
 &OXYGEN_start,
 &OXYGEN_stop,
 &OXYGEN_close,
 &OXYGEN_setrate,

 &MDma_writedata,
 &OXYGEN_getbufpos,
 &MDma_clearbuf,
 &MDma_interrupt_monitor,
 &OXYGEN_IRQRoutine,

 &OXYGEN_writeMIXER,
 &OXYGEN_readMIXER,
 &OXYGEN_mixerset[0],

 NULL,
 NULL,
 NULL,
 NULL,
};

#endif // AUCARDS_LINK_OXYGEN
