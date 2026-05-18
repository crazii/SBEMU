// ESS Allegro driver for SBEMU
// based on the Linux driver(maestro3)

#include "au_linux.h"

#ifdef AU_CARDS_LINK_ALLEGRO

#define ALLEGRO_DEBUG 0

#if ALLEGRO_DEBUG
#define allegrodbg(...) do { DBG_Logi("Allegro: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define allegrodbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "../../drivers/maestro3/maestro3.h"

static pci_device_s allegro_devices[] = {
  {"Allegro-1",         0x125D, 0x1988, 0}, // ES1988S/ES1989S
  {"Allegro",           0x125D, 0x1989, 0}, // ES1988S/ES1989S
  {"Maestro 2",         0x125D, 0x1968, 0}, // ES1968
  {"Maestro 2E",        0x125D, 0x1978, 0}, // ES1978
  {"Maestro-3i",        0x125D, 0x1998, 0}, // ES1983/ES1983S
  {"Canyon3D 2 LE",     0x125D, 0x1990, 0}, // ES1990
  {"Canyon3D 2",        0x125D, 0x1992, 0}, // ES1992
  {NULL,0,0,0}
};

struct allegro_card_s {
  struct au_linux_card card;
};

extern struct snd_pcm_ops snd_m3_playback_ops;
static struct snd_pcm_ops *allegro_ops = &snd_m3_playback_ops;
extern int snd_m3_probe (struct snd_card *card, struct pci_dev *pci,
                         int probe_only,
                         int spdif,
                         int enable_amp,
                         int amp_gpio);
extern irqreturn_t snd_m3_interrupt(int irq, void *dev_id);
extern void snd_m3_ac97_init (struct snd_card *card);

static void ALLEGRO_close (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card = aui->card_private_data;
  if (card) {
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void ALLEGRO_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "ALLEGRO : %s (%4.4X) IRQ %u", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);
  pds_textdisplay_printf(sout);
}

static int ALLEGRO_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card;
  uint32_t iobase;
  int err;

  allegrodbg("adetect\n");

  card = (struct allegro_card_s *)pds_zalloc(sizeof(struct allegro_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, allegro_devices) < 0)
    goto err_adetect;
  allegrodbg("PCI subsystem %4.4X:%4.4X\n", card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);
  int probe_only = aui->card_controlbits & AUINFOS_CARDCNTRLBIT_TESTCARD;
  int spdif = 0; // Not implemented
  err = snd_m3_probe(card->card.linux_snd_card, card->card.linux_pci_dev, probe_only, spdif, 1, -1);
  if (err) goto err_adetect;

#if 0 // An OPL3 will be detected, but it doesn't make any sound
  if (ioport_detect_opl(0x388)) {
    aui->fm_port = 0x388;
    aui->fm = 1;
  } else {
    if (ioport_detect_opl(0x240)) {
      aui->fm_port = 0x240;
      aui->fm = 1;
    }
  }
#endif
  struct snd_m3 *chip = (struct snd_m3 *)card->card.linux_snd_card->private_data;
  aui->mpu401_port = chip->iobase + 0x98;
  aui->mpu401 = 1;

  if (!probe_only)
    snd_m3_ac97_init(card->card.linux_snd_card);

  allegrodbg("ALLEGRO : %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  return 1;

err_adetect:
  ALLEGRO_close(aui);
  return 0;
}

static void ALLEGRO_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card = aui->card_private_data;
  int err;

  allegrodbg("setrate %u\n", aui->freq_card);
  if (aui->freq_card < 8000) {
    aui->freq_card = 8000;
  } else if (aui->freq_card > 48000) {
    aui->freq_card = 48000;
  }
  allegrodbg("-> %u\n", aui->freq_card);
  aui->card_dmasize = 512;
  aui->card_dma_buffer_size = 4096; // was OK with 8KB buffer / 512B page size
  aui->dma_addr_bits = 28;
  aui->buffer_size_shift = 1;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, allegro_ops);
  if (err) goto err_setrate;
  allegro_ops->prepare(card->card.pcm_substream);
  return;

 err_setrate:
  allegrodbg("setrate error\n");
}

static void ALLEGRO_start (struct mpxplay_audioout_info_s *aui)
{
  allegrodbg("start\n");
  struct allegro_card_s *card = aui->card_private_data;
  allegro_ops->trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void ALLEGRO_stop (struct mpxplay_audioout_info_s *aui)
{
  allegrodbg("stop\n");
  struct allegro_card_s *card = aui->card_private_data;
  allegro_ops->trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int allegro_int_cnt = 0;

static long ALLEGRO_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card = aui->card_private_data;
#if 0 // need to update hwptr in snd_m3_interrupt
  struct snd_m3 *chip = (struct snd_m3 *)card->card.linux_snd_card->private_data;
  struct m3_dma *s = &chip->substreams[0];
  unsigned long bufpos = s->hwptr;
#endif
#if 1 // just use the pointer directly
  unsigned long bufpos = allegro_ops->pointer(card->card.pcm_substream);
  bufpos <<= 1;
#endif
#if ALLEGRO_DEBUG > 1
  if ((allegro_int_cnt % (1900)) == 0)
    allegrodbg("getbufpos %u / %u\n", bufpos, aui->card_dmasize);
  //if (bufpos == aui->card_dmasize)
  //  allegrodbg("getbufpos %u == dmasize\n", bufpos);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s ALLEGRO_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,255,8,0},{0,255,0,0}}};
static aucards_allmixerchan_s ALLEGRO_mixerset[] = {
 &ALLEGRO_master_vol,
 NULL
};

static void ALLEGRO_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct allegro_card_s *card = aui->card_private_data;
  allegrodbg("write mixer val: %X\n", val);
}

static unsigned long ALLEGRO_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct allegro_card_s *card = aui->card_private_data;
  return 0xffff;
}

static int ALLEGRO_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct allegro_card_s *card = aui->card_private_data;
  int handled = snd_m3_interrupt(card->card.irq, card->card.linux_snd_card->private_data);
#if ALLEGRO_DEBUG
  if (handled) {
    if ((allegro_int_cnt % (2000)) == 0) DBG_Logi("allegroirq %u\n", allegro_int_cnt);
    allegro_int_cnt++;
  }
#endif
  return handled;
}

one_sndcard_info ALLEGRO_sndcard_info = {
  "Allegro",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &ALLEGRO_adetect,
  &ALLEGRO_card_info,
  &ALLEGRO_start,
  &ALLEGRO_stop,
  &ALLEGRO_close,
  &ALLEGRO_setrate,

  &MDma_writedata,
  &ALLEGRO_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &ALLEGRO_IRQRoutine,

  &ALLEGRO_writeMIXER,
  &ALLEGRO_readMIXER,
  &ALLEGRO_mixerset[0],

  NULL,
  NULL,
  &ioport_mpu401_write,
  &ioport_mpu401_read,
};

#endif // AUCARDS_LINK_ALLEGRO
