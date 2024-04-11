#ifndef au_linux_h
#define au_linux_h

#include "sound/core.h"
#include "sound/pcm.h"
#include "sound/ac97_codec.h"

struct au_linux_card {
  struct snd_card *linux_snd_card;
  struct pci_dev *linux_pci_dev;
  struct snd_pcm_substream *pcm_substream;
  struct pci_config_s *pci_dev;
  unsigned int irq;
};

#define AUI_LINUX_CARD(_aui) (struct au_linux_card *)((_aui)->card_private_data)

extern int au_linux_find_card (struct mpxplay_audioout_info_s *aui, struct au_linux_card *card, pci_device_s devices[]);
extern void au_linux_close_card (struct au_linux_card *card);
extern int au_linux_make_snd_pcm_substream (struct mpxplay_audioout_info_s *aui, struct au_linux_card *card, struct snd_pcm_ops *ops);

#endif
