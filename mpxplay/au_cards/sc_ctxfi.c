// CTXFI driver for SBEMU
// based on the Linux driver

#include "au_linux.h"

#ifdef AU_CARDS_LINK_CTXFI

#define CTXFI_DEBUG 0

#if CTXFI_DEBUG
#define ctxfidbg(...) do { DBG_Logi("CTXFI: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define ctxfidbg(...)
#endif

#include "dmairq.h"
#include "pcibios.h"
#include "dpmi/dbgutil.h"
#include "sound/core.h"
#include "sound/pcm.h"
#include "../../drivers/ctxfi/ctatc.h"

static pci_device_s ctxfi_devices[] = {
  {"EMU20K1", 0x1102, 0x0005, 1},
  {"EMU20K2", 0x1102, 0x000b, 2},
  {NULL,0,0,0}
};

struct ctxfi_card_s {
  struct au_linux_card card;
};

static unsigned int reference_rate = 48000;
static unsigned int multiple = 2;

extern int ct_atc_destroy(struct ct_atc *atc);
extern int ct_pcm_playback_open (struct snd_pcm_substream *substream);
extern int ct_pcm_playback_prepare (struct snd_pcm_substream *substream);
extern int ct_pcm_playback_trigger (struct snd_pcm_substream *substream, int cmd);
extern snd_pcm_uframes_t ct_pcm_playback_pointer(struct snd_pcm_substream *substream);
extern struct snd_pcm_ops ct_pcm_playback_ops;

extern int ctxfi_alsa_mix_volume_get (struct ct_atc *atc, int type);
extern int ctxfi_alsa_mix_volume_put (struct ct_atc *atc, int type, int val);
extern irqreturn_t ct_20k1_interrupt(int irq, void *dev_id);
extern irqreturn_t ct_20k2_interrupt(int irq, void *dev_id);

static void CTXFI_close (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  if (card) {
    if (card->card.linux_snd_card) {
      struct ct_atc *atc = (struct ct_atc *)card->card.linux_snd_card->private_data;
      if (atc)
        ct_atc_destroy(atc);
    }
    au_linux_close_card(&card->card);
    pds_free(card);
    aui->card_private_data = NULL;
  }
}

static void CTXFI_card_info (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  char sout[100];
  sprintf(sout, "CTXFI : Creative %s (%4.4X) IRQ %u", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);
  pds_textdisplay_printf(sout);
}

static int CTXFI_adetect (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card;
  struct ct_atc *atc;
  uint32_t iobase;
  int err;

  ctxfidbg("adetect\n");

  card = (struct ctxfi_card_s *)pds_zalloc(sizeof(struct ctxfi_card_s));
  if (!card)
    return 0;
  if (au_linux_find_card(aui, &card->card, ctxfi_devices) < 0)
    goto err_adetect;
  ctxfidbg("atc %4.4X:%4.4X\n", card->card.linux_pci_dev->subsystem_vendor, card->card.linux_pci_dev->subsystem_device);
  err = ct_atc_create(card->card.linux_snd_card,
                      card->card.linux_pci_dev,
                      reference_rate,
                      multiple,
                      card->card.pci_dev->device_type,
                      0,
                      &atc);
  if (err) goto err_adetect;
  card->card.linux_snd_card->private_data = atc;

  ctxfidbg("CTXFI : Creative %s (%4.4X) IRQ %u\n", card->card.pci_dev->device_name, card->card.pci_dev->device_id, card->card.irq);

  return 1;

err_adetect:
  CTXFI_close(aui);
  return 0;
}

static void CTXFI_setrate (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  int err;

  ctxfidbg("setrate %u\n", aui->freq_card);
  if (aui->freq_card < 8000) {
    aui->freq_card = 8000;
  } else if (aui->freq_card > 192000) {
    aui->freq_card = 192000;
  }
  aui->card_dmasize = 4096;
  aui->card_dma_buffer_size = 4096;
  aui->dma_addr_bits = 32;
  aui->buffer_size_shift = 1;
  aui->period_size_shift = 3;
  err = au_linux_make_snd_pcm_substream(aui, &card->card, &ct_pcm_playback_ops);
  if (err) goto err_setrate;
  ct_pcm_playback_prepare(card->card.pcm_substream);
  return;

 err_setrate:
  ctxfidbg("setrate error\n");
}

static void CTXFI_start (struct mpxplay_audioout_info_s *aui)
{
  ctxfidbg("start\n");
  struct ctxfi_card_s *card = aui->card_private_data;
  ct_pcm_playback_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_START);
}

static void CTXFI_stop (struct mpxplay_audioout_info_s *aui)
{
  ctxfidbg("stop\n");
  struct ctxfi_card_s *card = aui->card_private_data;
  ct_pcm_playback_trigger(card->card.pcm_substream, SNDRV_PCM_TRIGGER_STOP);
}

unsigned int ctxfi_int_cnt = 0;

static long CTXFI_getbufpos (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  unsigned long bufpos = ct_pcm_playback_pointer(card->card.pcm_substream);
  bufpos <<= 1;
#if CTXFI_DEBUG > 1
  if ((ctxfi_int_cnt % (2000)) == 0)
    ctxfidbg("getbufpos %u / %u\n", bufpos, aui->card_dmasize);
  if (bufpos == aui->card_dmasize)
    ctxfidbg("getbufpos %u == dmasize\n", bufpos);
#endif
  if (bufpos < aui->card_dmasize)
    aui->card_dma_lastgoodpos = bufpos;
  return aui->card_dma_lastgoodpos;
}

static aucards_onemixerchan_s CTXFI_master_vol={AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{{0,15,4,0},{0,15,0,0}}};
static aucards_allmixerchan_s CTXFI_mixerset[] = {
  &CTXFI_master_vol,
  NULL
};

// from ctmixer.c
enum CT_AMIXER_CTL {
	/* volume control mixers */
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

	/* this should always be the last one */
	NUM_CT_AMIXERS
};

static void CTXFI_writeMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  struct ct_atc *atc = (struct ct_atc *)card->card.linux_snd_card->private_data;
  ctxfidbg("write mixer val: %X\n", val);
  // warning: uses only one channel's volume
  unsigned int lval = (val & 15) << 4;
  ctxfi_alsa_mix_volume_put(atc, AMIXER_MASTER_F, 0x100); // set Master to MAX
  ctxfi_alsa_mix_volume_put(atc, AMIXER_LINEIN, 0x100); // set Line-In to MAX
  ctxfi_alsa_mix_volume_put(atc, AMIXER_PCM_F, lval); // Set PCM volume
}

static unsigned long CTXFI_readMIXER (struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  struct ct_atc *atc = (struct ct_atc *)card->card.linux_snd_card->private_data;
  unsigned int lval = ctxfi_alsa_mix_volume_get(atc, AMIXER_PCM_F);
  unsigned long val = (lval >> 4) & 15;
  val |= (val << 4);
  ctxfidbg("read mixer returning %X\n", val);
  return val;
}

static int CTXFI_IRQRoutine (struct mpxplay_audioout_info_s *aui)
{
  struct ctxfi_card_s *card = aui->card_private_data;
  struct ct_atc *atc = (struct ct_atc *)card->card.linux_snd_card->private_data;
  irqreturn_t handled;
  if (atc->chip_type == ATC20K1)
    handled = ct_20k1_interrupt(card->card.irq, atc->hw);
  else
    handled = ct_20k2_interrupt(card->card.irq, atc->hw);
  if ((ctxfi_int_cnt % (2000)) == 0) DBG_Logi("ctxfiirq %u\n", ctxfi_int_cnt);
  if (handled)
    ctxfi_int_cnt++;
  return handled;
}

one_sndcard_info CTXFI_sndcard_info = {
  "CTXFI",
  SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

  NULL,
  NULL,
  &CTXFI_adetect,
  &CTXFI_card_info,
  &CTXFI_start,
  &CTXFI_stop,
  &CTXFI_close,
  &CTXFI_setrate,

  &MDma_writedata,
  &CTXFI_getbufpos,
  &MDma_clearbuf,
  &MDma_interrupt_monitor,
  &CTXFI_IRQRoutine,

  &CTXFI_writeMIXER,
  &CTXFI_readMIXER,
  &CTXFI_mixerset[0]
};

#endif // AUCARDS_LINK_CTXFI
