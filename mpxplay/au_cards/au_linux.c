#include "au_linux.h"

#define AU_LINUX_DEBUG 0

#if AU_LINUX_DEBUG
#define aulinuxdbg(...) do { DBG_Logi("au_linux: "); DBG_Logi(__VA_ARGS__); } while (0)
#else
#define aulinuxdbg(...)
#endif

cardmem_t *MDma_alloc_cardmem_noexit (size_t size)
{
  cardmem_t *dm;
  dm = calloc(1, sizeof(cardmem_t));
  if (!dm) {
    return NULL;
  }
  if (!
#ifndef DJGPP
      pds_dpmi_dos_allocmem(dm, size)
#else
      pds_dpmi_xms_allocmem(dm, size)
#endif
      ) {
    free(dm);
    return NULL;
  }
  memset(dm->linearptr, 0, size);
  return dm;
}

#define CHECK_BAR 0

int au_linux_find_card (struct mpxplay_audioout_info_s *aui, struct au_linux_card *card, pci_device_s devices[])
{
#if CHECK_BAR
  u32 iobase;
#endif
  u32 class;
  u8 revision;
  u8 irq;

  aui->card_private_data = card;

  card->pci_dev = (struct pci_config_s *)pds_zalloc(sizeof(struct pci_config_s));
  if (!card->pci_dev)
    goto err;

  if (pcibios_search_devices(devices, card->pci_dev) != PCI_SUCCESSFUL)
    goto err;

  card->linux_pci_dev = pds_zalloc(sizeof(struct pci_dev));
  card->linux_pci_dev->pcibios_dev = card->pci_dev;

  pci_read_config_dword(card->linux_pci_dev, 0x08, &class); // PCI_CLASS_REVISION
  revision = class & 0xff;
  class >>= 8;
  // Ignore modems
  //if (((class >> 8) & 0xffff) != 0x0401) // PCI_CLASS_MULTIMEDIA_AUDIO
  if (((class >> 16) & 0xff) != 0x04) // PCI_BASE_CLASS_MULTIMEDIA
    goto err;
#if CHECK_BAR
  pci_read_config_dword(card->linux_pci_dev, 0x10, &iobase); // PCI_BASE_ADDRESS_0
  iobase &= 0xfffffff8;
  if (!iobase)
    goto err;
#endif

  pci_read_config_byte(card->linux_pci_dev, 0x3c, &irq); // PCI_INTERRUPT_LINE
  aui->card_irq = card->irq = irq;
  aui->card_pci_dev = card->pci_dev;
  card->linux_snd_card = pds_zalloc(sizeof(struct snd_card));
  card->linux_pci_dev->irq = card->irq;
  card->linux_pci_dev->vendor = card->pci_dev->vendor_id;
  card->linux_pci_dev->device = card->pci_dev->device_id;
  pci_read_config_word(card->linux_pci_dev, 0x2c, &card->linux_pci_dev->subsystem_vendor); // PCI_SUBSYSTEM_VENDOR_ID
  pci_read_config_word(card->linux_pci_dev, 0x2e, &card->linux_pci_dev->subsystem_device); // PCI_SUBSYSTEM_ID
  card->linux_pci_dev->revision = revision;

  return 0;

 err:
  if (card->pci_dev) {
    pds_free(card->pci_dev);
    card->pci_dev = NULL;
  }
  if (card->linux_pci_dev) {
    pds_free(card->linux_pci_dev);
    card->linux_pci_dev = NULL;
  }
  if (card->linux_snd_card) {
    pds_free(card->linux_snd_card);
    card->linux_snd_card = NULL;
  }
  return -1;
}

void au_linux_close_card (struct au_linux_card *card)
{
  if (card) {
    if (card->pcm_substream) {
      if (card->pcm_substream->runtime) {
        if (card->pcm_substream->runtime->dma_buffer_p)
          snd_dma_free_pages(card->pcm_substream->runtime->dma_buffer_p);
        kfree(card->pcm_substream->runtime);
      }
      if (card->pcm_substream->pcm)
        kfree(card->pcm_substream->pcm);
      kfree(card->pcm_substream);
    }
    if (card->linux_snd_card) {
      if (card->linux_snd_card->private_free)
        card->linux_snd_card->private_free(card->linux_snd_card);
      pds_free(card->linux_snd_card);
    }
    if (card->linux_pci_dev)
      pds_free(card->linux_pci_dev);
    if (card->pci_dev)
      pds_free(card->pci_dev);
  }
}

int au_linux_make_snd_pcm_substream (struct mpxplay_audioout_info_s *aui, struct au_linux_card *card, struct snd_pcm_ops *ops)
{
  struct snd_pcm_substream *substream;
  struct snd_pcm_runtime *runtime;
  struct snd_pcm_hw_params hwparams;
  struct snd_interval *intervalparam;
  int err;

  substream = kzalloc(sizeof(*substream), GFP_KERNEL);
  if (!substream) {
    return -1;
  }
  substream->stream = SNDRV_PCM_STREAM_PLAYBACK;
  substream->ops = ops;
  substream->pcm = kzalloc(sizeof(struct snd_pcm), GFP_KERNEL);
  substream->pcm->card = card->linux_snd_card;
  substream->pcm->device = 0; // FRONT
  runtime = kzalloc(sizeof(struct snd_pcm_runtime), GFP_KERNEL);
  if (!runtime) {
    goto err;
  }
  substream->runtime = runtime;
  void *substream_private_data = aui->substream_private_data;
  if (!substream_private_data)
    substream_private_data = card->linux_snd_card->private_data;
  substream->private_data = substream_private_data;
  
  substream->group = &substream->self_group;
  snd_pcm_group_init(&substream->self_group);
  list_add_tail(&substream->link_list, &substream->self_group.substreams);
  runtime->dma_buffer_p = kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
  if (!runtime->dma_buffer_p) {
    goto err;
  }
  aui->chan_card = 2;
  aui->bits_card = 16;
  aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE;
  //size_t pcmbuffpagesize = 512;
  //size_t dmabuffsize = 4096;
  size_t pcmbuffpagesize = aui->card_dmasize;
  size_t dmabuffsize = aui->card_dma_buffer_size;
  if (dmabuffsize <= 4096)
    dmabuffsize = MDma_get_max_pcmoutbufsize(aui, 0, pcmbuffpagesize, 2, 0);
  aulinuxdbg("max dmabuffsize: %u\n", dmabuffsize);
  err = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, 
                            &card->linux_pci_dev->dev,
                            dmabuffsize,
                            runtime->dma_buffer_p);
  if (err) {
    goto err;
  }
  aulinuxdbg("DMA buffer: size %u physical address: %8.8X\n", dmabuffsize, runtime->dma_buffer_p->addr);
  int retry_idx = 0, max_tries = 20;
  struct snd_dma_buffer *dmabuffers[20];
  uint32_t maxaddr = 1 << aui->dma_addr_bits;
  if (aui->dma_addr_bits < 32 && runtime->dma_buffer_p->addr >= maxaddr) {
    dmabuffers[0] = runtime->dma_buffer_p;
    runtime->dma_buffer_p = NULL;
    for (retry_idx = 1; retry_idx < max_tries; retry_idx++) {
      runtime->dma_buffer_p = kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
      if (!runtime->dma_buffer_p) {
        goto retry_err;
      }
      err = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, 
                                &card->linux_pci_dev->dev,
                                dmabuffsize,
                                runtime->dma_buffer_p);
      if (err) {
        goto retry_err;
      }
      aulinuxdbg("retrying DMA buffer physical address: %8.8X\n", runtime->dma_buffer_p->addr);
      if (runtime->dma_buffer_p->addr >= maxaddr) {
        dmabuffers[retry_idx] = runtime->dma_buffer_p;
        runtime->dma_buffer_p = NULL;
      } else {
        break;
      }
    }
  }
  while (retry_idx--) {
    aulinuxdbg("freeing dma buffer index %i\n", retry_idx);
    snd_dma_free_pages(dmabuffers[retry_idx]);
  }
  retry_idx = 0;
  if (runtime->dma_buffer_p == NULL) {
  retry_err:
    while (retry_idx--) {
      aulinuxdbg("freeing dma buffer index %i\n", retry_idx);
      snd_dma_free_pages(dmabuffers[retry_idx]);
    }
    printf("Aulinux: Could not allocate DMA buffer with physical address below 0x40000000, try using HimemX2 or HimemX /MAX=32768\n");
    goto err;
  }
  aui->card_DMABUFF = (char *)runtime->dma_buffer_p->area;
  if (dmabuffsize <= 4096)
    dmabuffsize = MDma_init_pcmoutbuf(aui, dmabuffsize, pcmbuffpagesize, 0);
  aulinuxdbg("dmabuffsize: %u   buff: %8.8X\n", dmabuffsize, aui->card_DMABUFF);
  snd_pcm_set_runtime_buffer(substream, runtime->dma_buffer_p);
  runtime->buffer_size = dmabuffsize >> aui->buffer_size_shift;
  // XXX emu10k1x: runtime->buffer_size = dmabuffsize;
  // runtime->buffer_size = dmabuffsize / 4; // size in samples
  runtime->channels = 2;
  runtime->frame_bits = 16;
  runtime->sample_bits = 16;
  runtime->rate = aui->freq_card;
  runtime->format = SNDRV_PCM_FORMAT_S16_LE;
  //aulinuxdbg("runtime: %8.8X\n", runtime);
  int periods = max(1, dmabuffsize / pcmbuffpagesize);
  runtime->periods = periods;
  // XXX >> 1 should be correct ...  >> 3 for XFi??
  if (aui->period_size_shift == 0) aui->period_size_shift = 1;
  runtime->period_size = (dmabuffsize / periods) >> aui->period_size_shift;
  //runtime->period_size = (dmabuffsize / periods);
  //runtime->period_size = (dmabuffsize / periods) >> 3;
  card->pcm_substream = substream;
  aulinuxdbg("periods: %u  size: %u\n", runtime->periods, runtime->period_size);
  aulinuxdbg("open substream\n");
  err = ops->open(substream);
  if (err) {
    goto err;
  }
  aulinuxdbg("opened substream\n");
  
  aui->card_dmasize = aui->card_dma_buffer_size = dmabuffsize;
  aui->card_samples_per_int = runtime->period_size;
  if (ops->hw_params) {
    intervalparam = hw_param_interval(&hwparams, SNDRV_PCM_HW_PARAM_BUFFER_BYTES);
    intervalparam->min = dmabuffsize;
    intervalparam = hw_param_interval(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    intervalparam->min = runtime->period_size;
    intervalparam = hw_param_interval(&hwparams, SNDRV_PCM_HW_PARAM_RATE);
    intervalparam->min = aui->freq_card;
    intervalparam = hw_param_interval(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS);
    intervalparam->min = 2;
    aulinuxdbg("hw params\n");
    err = ops->hw_params(substream, &hwparams);
    if (err) {
      goto err;
    }
  }

  return 0;

 err:
  if (runtime && runtime->dma_buffer_p) snd_dma_free_pages(runtime->dma_buffer_p);
  if (runtime) kfree(runtime);
  if (substream) kfree(substream);
  return -1;
}
