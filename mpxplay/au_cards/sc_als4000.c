// ALS4000 driver for SBEMU
// based on the Linux driver

#include "au_linux.h"

#ifdef AU_CARDS_LINK_ALS4000

#define ALS4000_DEBUG 0

#if ALS4000_DEBUG
#define als4000dbg(...) do { DBG_Logi("ALS4000: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define als4000dbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "sound/core.h"
#include "sound/pcm.h"
#include "sound/ac97_codec.h"
#include "sound/sb.h"
#include "../../drivers/als4000/als4000.h"

struct als4000_card_s {
  struct au_linux_card card;
};

extern int snd_card_als4000_create (struct snd_card *card,
                                    struct pci_dev *pci,
                                    struct snd_card_als4000 **racard);

extern int snd_als4000_playback_open (struct snd_pcm_substream *substream);
extern int snd_als4000_playback_prepare (struct snd_pcm_substream *substream);
extern int snd_als4000_playback_trigger (struct snd_pcm_substream *substream, int cmd);
extern snd_pcm_uframes_t snd_als4000_playback_pointer(struct snd_pcm_substream *substream);
extern struct snd_pcm_ops snd_als4000_playback_ops;

//-------------------------------------------------------------------------
static pci_device_s als4000_devices[] = {
  {"ALS4000", 0x4005, 0x4000, 0},
  {NULL,0,0,0}
};

static void ALS4000_close (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  if (card) {
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void ALS4000_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "ALS4000 IRQ %u", card->card.irq);
  pds_textdisplay_printf(sout);
}

static int ALS4000_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card;
  struct snd_card_als4000 *als4000;
  uint32_t iobase;
  int err;

  als4000dbg("adetect\n");

  card = (struct als4000_card_s *)pds_zalloc(sizeof(struct als4000_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, als4000_devices) < 0)
    goto err_adetect;

  err = snd_card_als4000_create(card->card.linux_snd_card,
                                card->card.linux_pci_dev,
                                &als4000);
  if (err < 0)
    goto err_adetect;

  als4000dbg("iobase %X pci subsystem %4.4X:%4.4X\n", als4000->iobase, card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);

  card->card.linux_snd_card->private_data = als4000;
  aui->fm_port = als4000->iobase + ALS4K_IOB_10_ADLIB_ADDR0;
  aui->fm = 1;
  aui->mpu401_port = als4000->iobase + ALS4K_IOB_30_MIDI_DATA;
  aui->mpu401 = 1;
  aui->mpu401_softread = 1; // Needed for Duke Nukem 3D

  als4000dbg("ALS4000 : %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  return 1;

err_adetect:
  ALS4000_close(aui);
  return 0;
}

static void ALS4000_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  struct snd_card_als4000 *als4000 = card->card.linux_snd_card->private_data;
  int err;

  als4000dbg("setrate %u\n", aui->freq_card);
  if (aui->freq_card < 4000) {
    aui->freq_card = 4000;
  } else if (aui->freq_card > 48000) {
    aui->freq_card = 48000;
  }
  aui->card_dmasize = 512;
  aui->card_dma_buffer_size = 4096;
  aui->dma_addr_bits = 24;
  aui->buffer_size_shift = 1;
  aui->substream_private_data = als4000->chip;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, &snd_als4000_playback_ops);
  if (err) goto err_setrate;
  snd_als4000_playback_prepare(card->card.pcm_substream);
  return;

 err_setrate:
  als4000dbg("setrate error\n");
}

static void ALS4000_start (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  als4000dbg("start\n");
  snd_als4000_playback_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void ALS4000_stop (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  als4000dbg("stop\n");
  snd_als4000_playback_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int als4000_int_cnt = 0;

static long ALS4000_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  dma_addr_t physaddr = card->card.pcm_substream->runtime->dma_buffer_p->addr;
  unsigned long bufpos = snd_als4000_playback_pointer(card->card.pcm_substream);
  bufpos -= physaddr;
  bufpos = bufpos % aui->card_dmasize;
#if ALS4000_DEBUG > 1
  if ((als4000_int_cnt % 90) == 0)
    als4000dbg("getbufpos %u (%X) / %u\n", bufpos, bufpos, aui->card_dmasize);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s ALS4000_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,15,4,0},{0,15,0,0}}};
static aucards_allmixerchan_s ALS4000_mixerset[] = {
  &ALS4000_master_vol,
  NULL
};

// XXX need to defer writing until start is called???
static void ALS4000_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct als4000_card_s *card = aui->card_private_data;
  als4000dbg("write mixer val: %X\n", val);
}

static unsigned long ALS4000_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct als4000_card_s *card = aui->card_private_data;
  return 0;
}

#if 0
extern void als4000_interrupt_mask (void *dev_id, int mask);
void ALS4000_mask (struct mpxplay_audioout_info_s *aui, int mask)
{
  struct als4000_card_s *card = aui->card_private_data;
  struct snd_card_als4000 *als4000 = card->card.linux_snd_card->private_data;
  als4000_interrupt_mask(als4000->chip, mask);
}
#endif

extern irqreturn_t snd_als4000_interrupt(int irq, void *dev_id);

static int ALS4000_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct als4000_card_s *card = aui->card_private_data;
  struct snd_card_als4000 *als4000 = (struct snd_card_als4000 *)card->card.linux_snd_card->private_data;
  int handled = snd_als4000_interrupt(card->card.irq, als4000->chip);
#if ALS4000_DEBUG
  if (handled) {
    if ((als4000_int_cnt % 500) == 0) DBG_Logi("als4000irq %u\n", als4000_int_cnt);
    als4000_int_cnt++;
  }
#endif
  return handled;
}

one_sndcard_info ALS4000_sndcard_info = {
  "ALS4000",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &ALS4000_adetect,
  &ALS4000_card_info,
#if 1
  &ALS4000_start,
  &ALS4000_stop,
  &ALS4000_close,
  &ALS4000_setrate,

  &MDma_writedata,
  &ALS4000_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &ALS4000_IRQRoutine,
#else
  NULL,NULL,NULL,NULL,
  NULL,NULL,NULL,NULL,NULL,
#endif

  //&ALS4000_writeMIXER,
  //&ALS4000_readMIXER,
  //&ALS4000_mixerset[0],
  NULL,NULL,NULL,

  &ioport_fm_write,
  &ioport_fm_read,
  &ioport_mpu401_write_when_ready,
  &ioport_mpu401_read,
};

#endif // AUCARDS_LINK_ALS4000
