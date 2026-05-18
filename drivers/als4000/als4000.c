// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  card-als4000.c - driver for Avance Logic ALS4000 based soundcards.
 *  Copyright (C) 2000 by Bart Hartgers <bart@etpmod.phys.tue.nl>,
 *			  Jaroslav Kysela <perex@perex.cz>
 *  Copyright (C) 2002, 2008 by Andreas Mohr <hw7oshyuv3001@sneakemail.com>
 *
 *  Framework borrowed from Massimo Piccioni's card-als100.c.
 *
 * NOTES
 *
 *  Since Avance does not provide any meaningful documentation, and I
 *  bought an ALS4000 based soundcard, I was forced to base this driver
 *  on reverse engineering.
 *
 *  Note: this is no longer true (thank you!):
 *  pretty verbose chip docu (ALS4000a.PDF) can be found on the ALSA web site.
 *  Page numbers stated anywhere below with the "SPECS_PAGE:" tag
 *  refer to: ALS4000a.PDF specs Ver 1.0, May 28th, 1998.
 *
 *  The ALS4000 seems to be the PCI-cousin of the ALS100. It contains an
 *  ALS100-like SB DSP/mixer, an OPL3 synth, a MPU401 and a gameport 
 *  interface. These subsystems can be mapped into ISA io-port space, 
 *  using the PCI-interface. In addition, the PCI-bit provides DMA and IRQ 
 *  services to the subsystems.
 * 
 * While ALS4000 is very similar to a SoundBlaster, the differences in
 * DMA and capturing require more changes to the SoundBlaster than
 * desirable, so I made this separate driver.
 * 
 * The ALS4000 can do real full duplex playback/capture.
 *
 * FMDAC:
 * - 0x4f -> port 0x14
 * - port 0x15 |= 1
 *
 * Enable/disable 3D sound:
 * - 0x50 -> port 0x14
 * - change bit 6 (0x40) of port 0x15
 *
 * Set QSound:
 * - 0xdb -> port 0x14
 * - set port 0x15:
 *   0x3e (mode 3), 0x3c (mode 2), 0x3a (mode 1), 0x38 (mode 0)
 *
 * Set KSound:
 * - value -> some port 0x0c0d
 *
 * ToDo:
 * - by default, don't enable legacy game and use PCI game I/O
 * - power management? (card can do voice wakeup according to datasheet!!)
 */

//#include <linux/io.h>
//#include <linux/init.h>
#include "linux/pci.h"
//#include <linux/gameport.h>
#include "linux/module.h"
//#include <linux/dma-mapping.h>
#include "sound/core.h"
#include "sound/pcm.h"
//#include <sound/rawmidi.h>
//#include <sound/mpu401.h>
//#include <sound/opl3.h>
//#include <sound/initval.h>
#include "als4000.h"

#if 0
MODULE_AUTHOR("Bart Hartgers <bart@etpmod.phys.tue.nl>, Andreas Mohr");
MODULE_DESCRIPTION("Avance Logic ALS4000");
MODULE_LICENSE("GPL");

#if IS_REACHABLE(CONFIG_GAMEPORT)
#define SUPPORT_JOYSTICK 1
#endif

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;	/* Enable this card */
#ifdef SUPPORT_JOYSTICK
static int joystick_port[SNDRV_CARDS];
#endif

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for ALS4000 soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for ALS4000 soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable ALS4000 soundcard.");
#ifdef SUPPORT_JOYSTICK
module_param_hw_array(joystick_port, int, ioport, NULL, 0444);
MODULE_PARM_DESC(joystick_port, "Joystick port address for ALS4000 soundcard. (0 = disabled)");
#endif
#endif

static const struct pci_device_id snd_als4000_ids[] = {
	{ 0x4005, 0x4000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0, },   /* ALS4000 */
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, snd_als4000_ids);


static void snd_als4000_set_rate(struct snd_sb *chip, unsigned int rate)
{
	if (!(chip->mode & SB_RATE_LOCK)) {
		snd_sbdsp_command(chip, SB_DSP_SAMPLE_RATE_OUT);
		snd_sbdsp_command(chip, rate>>8);
		snd_sbdsp_command(chip, rate);
	}
}

static inline void snd_als4000_set_capture_dma(struct snd_sb *chip,
					       dma_addr_t addr, unsigned size)
{
	/* SPECS_PAGE: 40 */
	snd_als4k_gcr_write(chip, ALS4K_GCRA2_FIFO2_PCIADDR, addr);
	snd_als4k_gcr_write(chip, ALS4K_GCRA3_FIFO2_COUNT, (size-1));
}

static inline void snd_als4000_set_playback_dma(struct snd_sb *chip,
						dma_addr_t addr,
						unsigned size)
{
	/* SPECS_PAGE: 38 */
	snd_als4k_gcr_write(chip, ALS4K_GCR91_DMA0_ADDR, addr);
	snd_als4k_gcr_write(chip, ALS4K_GCR92_DMA0_MODE_COUNT,
							(size-1)|0x180000);
}

#define ALS4000_FORMAT_SIGNED	(1<<0)
#define ALS4000_FORMAT_16BIT	(1<<1)
#define ALS4000_FORMAT_STEREO	(1<<2)

static int snd_als4000_get_format(struct snd_pcm_runtime *runtime)
{
	int result;

	result = 0;
	if (snd_pcm_format_signed(runtime->format))
		result |= ALS4000_FORMAT_SIGNED;
	if (snd_pcm_format_physical_width(runtime->format) == 16)
		result |= ALS4000_FORMAT_16BIT;
	if (runtime->channels > 1)
		result |= ALS4000_FORMAT_STEREO;
	return result;
}

/* structure for setting up playback */
static const struct {
	unsigned char dsp_cmd, dma_on, dma_off, format;
} playback_cmd_vals[]={
/* ALS4000_FORMAT_U8_MONO */
{ SB_DSP4_OUT8_AI, SB_DSP_DMA8_ON, SB_DSP_DMA8_OFF, SB_DSP4_MODE_UNS_MONO },
/* ALS4000_FORMAT_S8_MONO */	
{ SB_DSP4_OUT8_AI, SB_DSP_DMA8_ON, SB_DSP_DMA8_OFF, SB_DSP4_MODE_SIGN_MONO },
/* ALS4000_FORMAT_U16L_MONO */
{ SB_DSP4_OUT16_AI, SB_DSP_DMA16_ON, SB_DSP_DMA16_OFF, SB_DSP4_MODE_UNS_MONO },
/* ALS4000_FORMAT_S16L_MONO */
{ SB_DSP4_OUT16_AI, SB_DSP_DMA16_ON, SB_DSP_DMA16_OFF, SB_DSP4_MODE_SIGN_MONO },
/* ALS4000_FORMAT_U8_STEREO */
{ SB_DSP4_OUT8_AI, SB_DSP_DMA8_ON, SB_DSP_DMA8_OFF, SB_DSP4_MODE_UNS_STEREO },
/* ALS4000_FORMAT_S8_STEREO */	
{ SB_DSP4_OUT8_AI, SB_DSP_DMA8_ON, SB_DSP_DMA8_OFF, SB_DSP4_MODE_SIGN_STEREO },
/* ALS4000_FORMAT_U16L_STEREO */
{ SB_DSP4_OUT16_AI, SB_DSP_DMA16_ON, SB_DSP_DMA16_OFF, SB_DSP4_MODE_UNS_STEREO },
/* ALS4000_FORMAT_S16L_STEREO */
{ SB_DSP4_OUT16_AI, SB_DSP_DMA16_ON, SB_DSP_DMA16_OFF, SB_DSP4_MODE_SIGN_STEREO },
};
#define playback_cmd(chip) (playback_cmd_vals[(chip)->playback_format])

#if 0
/* structure for setting up capture */
enum { CMD_WIDTH8=0x04, CMD_SIGNED=0x10, CMD_MONO=0x80, CMD_STEREO=0xA0 };
static const unsigned char capture_cmd_vals[]=
{
CMD_WIDTH8|CMD_MONO,			/* ALS4000_FORMAT_U8_MONO */
CMD_WIDTH8|CMD_SIGNED|CMD_MONO,		/* ALS4000_FORMAT_S8_MONO */	
CMD_MONO,				/* ALS4000_FORMAT_U16L_MONO */
CMD_SIGNED|CMD_MONO,			/* ALS4000_FORMAT_S16L_MONO */
CMD_WIDTH8|CMD_STEREO,			/* ALS4000_FORMAT_U8_STEREO */
CMD_WIDTH8|CMD_SIGNED|CMD_STEREO,	/* ALS4000_FORMAT_S8_STEREO */	
CMD_STEREO,				/* ALS4000_FORMAT_U16L_STEREO */
CMD_SIGNED|CMD_STEREO,			/* ALS4000_FORMAT_S16L_STEREO */
};	
#define capture_cmd(chip) (capture_cmd_vals[(chip)->capture_format])

static int snd_als4000_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long size;
	unsigned count;

	chip->capture_format = snd_als4000_get_format(runtime);
		
	size = snd_pcm_lib_buffer_bytes(substream);
	count = snd_pcm_lib_period_bytes(substream);
	
	if (chip->capture_format & ALS4000_FORMAT_16BIT)
		count >>= 1;
	count--;

	spin_lock_irq(&chip->reg_lock);
	snd_als4000_set_rate(chip, runtime->rate);
	snd_als4000_set_capture_dma(chip, runtime->dma_addr, size);
	spin_unlock_irq(&chip->reg_lock);
	spin_lock_irq(&chip->mixer_lock);
	snd_als4_cr_write(chip, ALS4K_CR1C_FIFO2_BLOCK_LENGTH_LO, count & 0xff);
	snd_als4_cr_write(chip, ALS4K_CR1D_FIFO2_BLOCK_LENGTH_HI, count >> 8);
	spin_unlock_irq(&chip->mixer_lock);
	return 0;
}
#endif

int snd_als4000_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long size;
	unsigned count;

	chip->playback_format = snd_als4000_get_format(runtime);
	
	size = snd_pcm_lib_buffer_bytes(substream);
	count = snd_pcm_lib_period_bytes(substream);

        // spec says Length = # of byte transfer - 1   but we divide by two here????
	if (chip->playback_format & ALS4000_FORMAT_16BIT)
		count >>= 1;
	count--;
	
	/* FIXME: from second playback on, there's a lot more clicks and pops
	 * involved here than on first playback. Fiddling with
	 * tons of different settings didn't help (DMA, speaker on/off,
	 * reordering, ...). Something seems to get enabled on playback
	 * that I haven't found out how to disable again, which then causes
	 * the switching pops to reach the speakers the next time here. */
	spin_lock_irq(&chip->reg_lock);
	snd_als4000_set_rate(chip, runtime->rate);
	snd_als4000_set_playback_dma(chip, runtime->dma_addr, size);
	
	/* SPEAKER_ON not needed, since dma_on seems to also enable speaker */
	/* snd_sbdsp_command(chip, SB_DSP_SPEAKER_ON); */
	snd_sbdsp_command(chip, playback_cmd(chip).dsp_cmd);
	snd_sbdsp_command(chip, playback_cmd(chip).format);
	snd_sbdsp_command(chip, count & 0xff);
	snd_sbdsp_command(chip, count >> 8);
	snd_sbdsp_command(chip, playback_cmd(chip).dma_off);	
	spin_unlock_irq(&chip->reg_lock);
	
	return 0;
}

#if 0
static int snd_als4000_capture_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	int result = 0;
	
	/* FIXME race condition in here!!!
	   chip->mode non-atomic update gets consistently protected
	   by reg_lock always, _except_ for this place!!
	   Probably need to take reg_lock as outer (or inner??) lock, too.
	   (or serialize both lock operations? probably not, though... - racy?)
	*/
	spin_lock(&chip->mixer_lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		chip->mode |= SB_RATE_LOCK_CAPTURE;
		snd_als4_cr_write(chip, ALS4K_CR1E_FIFO2_CONTROL,
							 capture_cmd(chip));
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		chip->mode &= ~SB_RATE_LOCK_CAPTURE;
		snd_als4_cr_write(chip, ALS4K_CR1E_FIFO2_CONTROL,
							 capture_cmd(chip));
		break;
	default:
		result = -EINVAL;
		break;
	}
	spin_unlock(&chip->mixer_lock);
	return result;
}
#endif

extern void als4000_mixer_init (struct snd_sb *chip);

// Need to call this after playback_trigger, or else PCM interrupts do not fire and FM sound volume is extremely low
static void snd_als4000_configure2(struct snd_sb *chip)
{
	u8 tmp;
	int i;

	/* do some more configuration */
	spin_lock_irq(&chip->mixer_lock);
	tmp = snd_als4_cr_read(chip, ALS4K_CR0_SB_CONFIG);
	snd_als4_cr_write(chip, ALS4K_CR0_SB_CONFIG,
				tmp|ALS4K_CR0_MX80_81_REG_WRITE_ENABLE);
	/* always select DMA channel 0, since we do not actually use DMA
	 * SPECS_PAGE: 19/20 */
	snd_sbmixer_write(chip, SB_DSP4_DMASETUP, SB_DMASETUP_DMA0);
        // CR0 set to IRQ controlled mode
	//snd_sbmixer_write(chip, 0x80, 0);
	//snd_sbmixer_write(chip, 0x80, 2);
	//snd_sbmixer_write(chip, 0x80, 0); // XXX
	snd_als4_cr_write(chip, ALS4K_CR0_SB_CONFIG,
				 tmp & ~ALS4K_CR0_MX80_81_REG_WRITE_ENABLE);
	spin_unlock_irq(&chip->mixer_lock);

	spin_lock_irq(&chip->reg_lock);
	/* enable interrupts */
        // Version 3(ALS200/ALS110//ALS4000/ALS120) FIFO controlled continuous DMA mode
	snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | ALS4K_GCR8C_IRQ_MASK_CTRL_ENABLE);

#if 0
	/* SPECS_PAGE: 39 */
	for (i = ALS4K_GCR91_DMA0_ADDR; i <= ALS4K_GCR96_DMA3_MODE_COUNT; ++i)
		snd_als4k_gcr_write(chip, i, 0);
#endif
	/* enable burst mode to prevent dropouts during high PCI bus usage */
	snd_als4k_gcr_write(chip, ALS4K_GCR99_DMA_EMULATION_CTRL,
		(snd_als4k_gcr_read(chip, ALS4K_GCR99_DMA_EMULATION_CTRL) & ~0x07) | 0x04);
	//snd_als4k_gcr_write(chip, ALS4K_GCR99_DMA_EMULATION_CTRL,
        //                    (snd_als4k_gcr_read(chip, ALS4K_GCR99_DMA_EMULATION_CTRL)) | 0x100); // from OSS driver
	spin_unlock_irq(&chip->reg_lock);

	als4000_mixer_init(chip);
}

int snd_als4000_playback_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	int result = 0;

	spin_lock(&chip->reg_lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		chip->mode |= SB_RATE_LOCK_PLAYBACK;
		snd_als4000_configure2(chip);
		snd_sbdsp_command(chip, playback_cmd(chip).dma_on);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		snd_sbdsp_command(chip, playback_cmd(chip).dma_off);
		chip->mode &= ~SB_RATE_LOCK_PLAYBACK;
		break;
	default:
		result = -EINVAL;
		break;
	}
	spin_unlock(&chip->reg_lock);
	return result;
}

#if 0
static snd_pcm_uframes_t snd_als4000_capture_pointer(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	unsigned int result;

	spin_lock(&chip->reg_lock);	
	result = snd_als4k_gcr_read(chip, ALS4K_GCRA4_FIFO2_CURRENT_ADDR);
	spin_unlock(&chip->reg_lock);
	result &= 0xffff;
	return bytes_to_frames( substream->runtime, result );
}
#endif

snd_pcm_uframes_t snd_als4000_playback_pointer(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	unsigned result;

	spin_lock(&chip->reg_lock);	
	result = snd_als4k_gcr_read(chip, ALS4K_GCRA0_FIFO1_CURRENT_ADDR);
	spin_unlock(&chip->reg_lock);
	//result &= 0xffff;
	//return bytes_to_frames( substream->runtime, result );
	return result;
}

/* FIXME: this IRQ routine doesn't really support IRQ sharing (we always
 * return IRQ_HANDLED no matter whether we actually had an IRQ flag or not).
 * ALS4000a.PDF writes that while ACKing IRQ in PCI block will *not* ACK
 * the IRQ in the SB core, ACKing IRQ in SB block *will* ACK the PCI IRQ
 * register (alt_port + ALS4K_IOB_0E_IRQTYPE_SB_CR1E_MPU). Probably something
 * could be optimized here to query/write one register only...
 * And even if both registers need to be queried, then there's still the
 * question of whether it's actually correct to ACK PCI IRQ before reading
 * SB IRQ like we do now, since ALS4000a.PDF mentions that PCI IRQ will *clear*
 * SB IRQ status.
 * (hmm, SPECS_PAGE: 38 mentions it the other way around!)
 * And do we *really* need the lock here for *reading* SB_DSP4_IRQSTATUS??
 * */
irqreturn_t snd_als4000_interrupt(int irq, void *dev_id)
{
	struct snd_sb *chip = dev_id;
	uint8_t pci_irqstatus;
	uint8_t sb_irqstatus;

	//snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | 0); // Mask INTA# output

	/* find out which bit of the ALS4000 PCI block produced the interrupt,
	   SPECS_PAGE: 38, 5 */
	pci_irqstatus = snd_als4k_iobase_readb(chip->alt_port,
				 ALS4K_IOB_0E_IRQTYPE_SB_CR1E_MPU);
        bool handled = false;
        if ((pci_irqstatus & ALS4K_IOB_0E_SB_DMA_IRQ)) handled = true;
#if 0
	if ((pci_irqstatus & ALS4K_IOB_0E_SB_DMA_IRQ)
	 && (chip->playback_substream)) /* playback */
		snd_pcm_period_elapsed(chip->playback_substream);
	if ((pci_irqstatus & ALS4K_IOB_0E_CR1E_IRQ)
	 && (chip->capture_substream)) /* capturing */
		snd_pcm_period_elapsed(chip->capture_substream);
	if ((pci_irqstatus & ALS4K_IOB_0E_MPU_IRQ)
	 && (chip->rmidi)) /* MPU401 interrupt */
		snd_mpu401_uart_interrupt(irq, chip->rmidi->private_data);
#endif
	if ((pci_irqstatus & ALS4K_IOB_0E_MPU_IRQ)) {
          uint8_t midi_in_data = snd_als4k_iobase_readb(chip->alt_port, ALS4K_IOB_30_MIDI_DATA);
          //printk("uartint %2.2X\n", midi_in_data);
          handled = true;
        }
	/* ACK the PCI block IRQ */ // -> Except for the SB IRQ since they are acknowledged below -> ???
        //if (pci_irqstatus & ~ALS4K_IOB_0E_SB_DMA_IRQ) snd_als4k_iobase_writeb(chip->alt_port, ALS4K_IOB_0E_IRQTYPE_SB_CR1E_MPU, pci_irqstatus & ~ALS4K_IOB_0E_SB_DMA_IRQ);
	snd_als4k_iobase_writeb(chip->alt_port, ALS4K_IOB_0E_IRQTYPE_SB_CR1E_MPU, pci_irqstatus);

        if (!handled) goto done;

	spin_lock(&chip->mixer_lock);
	/* SPECS_PAGE: 20 */
	sb_irqstatus = snd_sbmixer_read(chip, SB_DSP4_IRQSTATUS);
	spin_unlock(&chip->mixer_lock);

	if (sb_irqstatus & SB_IRQTYPE_8BIT) {
		snd_sb_ack_8bit(chip);
        }
	if (sb_irqstatus & SB_IRQTYPE_16BIT) {
		snd_sb_ack_16bit(chip);
        }
	if (sb_irqstatus & SB_IRQTYPE_MPUIN) {
          // chip->mpu_port is not used
          //inb(chip->mpu_port);
          uint8_t midi_in_data = snd_als4k_iobase_readb(chip->alt_port, ALS4K_IOB_30_MIDI_DATA);
          //printk("mpuin %2.2X\n", midi_in_data);
        }
	if (sb_irqstatus & ALS4K_IRQTYPE_CR1E_DMA) {
          //snd_sb_ack_CR1E(chip);
		snd_als4k_iobase_readb(chip->alt_port,
					ALS4K_IOB_16_ACK_FOR_CR1E);
        }

        //if (pci_irqstatus != 0x80 || sb_irqstatus != 0x02) printk("irq 0x%2.2X 0x%2.2X\n", pci_irqstatus, sb_irqstatus);
	/* dev_dbg(chip->card->dev, "als4000: irq 0x%04x 0x%04x\n",
					 pci_irqstatus, sb_irqstatus); */
#if 0
        bool handled =
	     (pci_irqstatus & (ALS4K_IOB_0E_SB_DMA_IRQ|ALS4K_IOB_0E_CR1E_IRQ|
				ALS4K_IOB_0E_MPU_IRQ))
	  || (sb_irqstatus & (SB_IRQTYPE_8BIT|SB_IRQTYPE_16BIT|
                              SB_IRQTYPE_MPUIN|ALS4K_IRQTYPE_CR1E_DMA));
#endif

 done:
	//snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | ALS4K_GCR8C_IRQ_MASK_CTRL_ENABLE); // Enable INTA# output again

	/* only ack the things we actually handled above */
	return IRQ_RETVAL(handled);
}

void als4000_interrupt_mask (void *dev_id, int mask)
{
  struct snd_sb *chip = dev_id;
  if (mask) {
    snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | 0); // Mask INTA# output
  } else {
    snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | ALS4K_GCR8C_IRQ_MASK_CTRL_ENABLE); // Enable INTA# output again
  }
}

/*****************************************************************/

static const struct snd_pcm_hardware snd_als4000_playback =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U8 |
				SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE,	/* formats */
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	65536,
	.period_bytes_min =	64,
	.period_bytes_max =	65536,
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0
};

#if 0
static const struct snd_pcm_hardware snd_als4000_capture =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U8 |
				SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE,	/* formats */
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	65536,
	.period_bytes_min =	64,
	.period_bytes_max =	65536,
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0
};
#endif

/*****************************************************************/

int snd_als4000_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	chip->playback_substream = substream;
	runtime->hw = snd_als4000_playback;
	return 0;
}

int snd_als4000_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);

	chip->playback_substream = NULL;
	return 0;
}

#if 0
static int snd_als4000_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	chip->capture_substream = substream;
	runtime->hw = snd_als4000_capture;
	return 0;
}

static int snd_als4000_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_sb *chip = snd_pcm_substream_chip(substream);

	chip->capture_substream = NULL;
	return 0;
}
#endif

/******************************************************************/

const struct snd_pcm_ops snd_als4000_playback_ops = {
	.open =		snd_als4000_playback_open,
	.close =	snd_als4000_playback_close,
	.prepare =	snd_als4000_playback_prepare,
	.trigger =	snd_als4000_playback_trigger,
	.pointer =	snd_als4000_playback_pointer
};

#if 0
static const struct snd_pcm_ops snd_als4000_capture_ops = {
	.open =		snd_als4000_capture_open,
	.close =	snd_als4000_capture_close,
	.prepare =	snd_als4000_capture_prepare,
	.trigger =	snd_als4000_capture_trigger,
	.pointer =	snd_als4000_capture_pointer
};

static int snd_als4000_pcm(struct snd_sb *chip, int device)
{
	struct snd_pcm *pcm;
	int err;

	err = snd_pcm_new(chip->card, "ALS4000 DSP", device, 1, 1, &pcm);
	if (err < 0)
		return err;
	pcm->private_data = chip;
	pcm->info_flags = SNDRV_PCM_INFO_JOINT_DUPLEX;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_als4000_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_als4000_capture_ops);

	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV,
				       &chip->pci->dev, 64*1024, 64*1024);

	chip->pcm = pcm;

	return 0;
}
#endif

/******************************************************************/

static void snd_als4000_set_addr(unsigned long iobase,
					unsigned int sb_io,
					unsigned int mpu_io,
					unsigned int opl_io,
					unsigned int game_io)
{
	u32 cfg1 = 0;
	u32 cfg2 = 0;

	if (mpu_io > 0)
		cfg2 |= (mpu_io | 1) << 16;
	if (sb_io > 0)
		cfg2 |= (sb_io | 1);
	if (game_io > 0)
		cfg1 |= (game_io | 1) << 16;
	if (opl_io > 0)
		cfg1 |= (opl_io | 1);
	snd_als4k_gcr_write_addr(iobase, ALS4K_GCRA8_LEGACY_CFG1, cfg1);
	snd_als4k_gcr_write_addr(iobase, ALS4K_GCRA9_LEGACY_CFG2, cfg2);
}

static void snd_als4000_configure(struct snd_sb *chip)
{
	u8 tmp;
	int i;

	/* do some more configuration */
	spin_lock_irq(&chip->mixer_lock);
	tmp = snd_als4_cr_read(chip, ALS4K_CR0_SB_CONFIG);
	snd_als4_cr_write(chip, ALS4K_CR0_SB_CONFIG,
				tmp|ALS4K_CR0_MX80_81_REG_WRITE_ENABLE);
	/* always select DMA channel 0, since we do not actually use DMA
	 * SPECS_PAGE: 19/20 */
	snd_sbmixer_write(chip, SB_DSP4_DMASETUP, SB_DMASETUP_DMA0);
	snd_als4_cr_write(chip, ALS4K_CR0_SB_CONFIG,
				 tmp & ~ALS4K_CR0_MX80_81_REG_WRITE_ENABLE);
	spin_unlock_irq(&chip->mixer_lock);
	
	spin_lock_irq(&chip->reg_lock);
	/* enable interrupts */
        // Version 3(ALS200/ALS110//ALS4000/ALS120) FIFO controlled continuous DMA mode
	snd_als4k_gcr_write(chip, ALS4K_GCR8C_MISC_CTRL, (3<<16) | ALS4K_GCR8C_IRQ_MASK_CTRL_ENABLE);

	/* SPECS_PAGE: 39 */
	for (i = ALS4K_GCR91_DMA0_ADDR; i <= ALS4K_GCR96_DMA3_MODE_COUNT; ++i)
		snd_als4k_gcr_write(chip, i, 0);
        for (i = 0x97; i < 0xa7; i++) // from OSS driver
          snd_als4k_gcr_write(chip, i, 0);
#if 0 // XXX try and disable this
	/* enable burst mode to prevent dropouts during high PCI bus usage */
	snd_als4k_gcr_write(chip, ALS4K_GCR99_DMA_EMULATION_CTRL,
		(snd_als4k_gcr_read(chip, ALS4K_GCR99_DMA_EMULATION_CTRL) & ~0x07) | 0x04);
#endif
	snd_als4k_gcr_write(chip, ALS4K_GCR99_DMA_EMULATION_CTRL,
                            (snd_als4k_gcr_read(chip, ALS4K_GCR99_DMA_EMULATION_CTRL)) | 0x100);
	spin_unlock_irq(&chip->reg_lock);
}

#ifdef SUPPORT_JOYSTICK
static int snd_als4000_create_gameport(struct snd_card_als4000 *acard, int dev)
{
	struct gameport *gp;
	struct resource *r;
	int io_port;

	if (joystick_port[dev] == 0)
		return -ENODEV;

	if (joystick_port[dev] == 1) { /* auto-detect */
		for (io_port = 0x200; io_port <= 0x218; io_port += 8) {
			r = devm_request_region(&acard->pci->dev, io_port, 8,
						"ALS4000 gameport");
			if (r)
				break;
		}
	} else {
		io_port = joystick_port[dev];
		r = devm_request_region(&acard->pci->dev, io_port, 8,
					"ALS4000 gameport");
	}

	if (!r) {
		dev_warn(&acard->pci->dev, "cannot reserve joystick ports\n");
		return -EBUSY;
	}

	acard->gameport = gp = gameport_allocate_port();
	if (!gp) {
		dev_err(&acard->pci->dev, "cannot allocate memory for gameport\n");
		return -ENOMEM;
	}

	gameport_set_name(gp, "ALS4000 Gameport");
	gameport_set_phys(gp, "pci%s/gameport0", pci_name(acard->pci));
	gameport_set_dev_parent(gp, &acard->pci->dev);
	gp->io = io_port;

	/* Enable legacy joystick port */
	snd_als4000_set_addr(acard->iobase, 0, 0, 0, 1);

	gameport_register_port(acard->gameport);

	return 0;
}

static void snd_als4000_free_gameport(struct snd_card_als4000 *acard)
{
	if (acard->gameport) {
		gameport_unregister_port(acard->gameport);
		acard->gameport = NULL;

		/* disable joystick */
		snd_als4000_set_addr(acard->iobase, 0, 0, 0, 0);
	}
}
#else
static inline int snd_als4000_create_gameport(struct snd_card_als4000 *acard, int dev) { return -ENOSYS; }
static inline void snd_als4000_free_gameport(struct snd_card_als4000 *acard) { }
#endif

#define OPL3_LEFT               0x0000
#define OPL3_RIGHT              0x0100
#define OPL3_REG_TEST                   0x01
#define   OPL3_ENABLE_WAVE_SELECT       0x20

#define OPL3_REG_TIMER1                 0x02
#define OPL3_REG_TIMER2                 0x03
#define OPL3_REG_TIMER_CONTROL          0x04    /* Left side */
#define   OPL3_IRQ_RESET                0x80
#define   OPL3_TIMER1_MASK              0x40
#define   OPL3_TIMER2_MASK              0x20
#define   OPL3_TIMER1_START             0x01
#define   OPL3_TIMER2_START             0x02

#define OPL3_REG_CONNECTION_SELECT      0x04    /* Right side */
#define   OPL3_LEFT_4OP_0               0x01
#define   OPL3_LEFT_4OP_1               0x02
#define   OPL3_LEFT_4OP_2               0x04
#define   OPL3_RIGHT_4OP_0              0x08
#define   OPL3_RIGHT_4OP_1              0x10
#define   OPL3_RIGHT_4OP_2              0x20

#define OPL3_REG_MODE                   0x05    /* Right side */
#define   OPL3_OPL3_ENABLE              0x01    /* OPL3 mode */
#define   OPL3_OPL4_ENABLE              0x02    /* OPL4 mode */

#define OPL3_REG_KBD_SPLIT              0x08    /* Left side */
#define   OPL3_COMPOSITE_SINE_WAVE_MODE 0x80    /* Don't use with OPL-3? */
#define   OPL3_KEYBOARD_SPLIT           0x40

#define OPL3_REG_PERCUSSION             0xbd    /* Left side only */
#define   OPL3_TREMOLO_DEPTH            0x80
#define   OPL3_VIBRATO_DEPTH            0x40
#define   OPL3_PERCUSSION_ENABLE        0x20
#define   OPL3_BASSDRUM_ON              0x10
#define   OPL3_SNAREDRUM_ON             0x08
#define   OPL3_TOMTOM_ON                0x04
#define   OPL3_CYMBAL_ON                0x02
#define   OPL3_HIHAT_ON                 0x01

static void opl3_command (uint16_t l_port, uint16_t r_port, unsigned short cmd, unsigned char val)
{
        unsigned long flags;

        /*
         * The OPL-3 survives with just two INBs
         * after writing to a register.
         */

        uint16_t port = (cmd & OPL3_RIGHT) ? r_port : l_port;

        spin_lock_irqsave(&opl3->reg_lock, flags);

        outb((unsigned char) cmd, port);
        inb(l_port);
        inb(l_port);

        outb((unsigned char) val, port + 1);
        inb(l_port);
        inb(l_port);

        spin_unlock_irqrestore(&opl3->reg_lock, flags);
}

static int opl3_detect (uint16_t l_port, uint16_t r_port)
{
        /*
         * This function returns 1 if the FM chip is present at the given I/O port
         * The detection algorithm plays with the timer built in the FM chip and
         * looks for a change in the status register.
         *
         * Note! The timers of the FM chip are not connected to AdLib (and compatible)
         * boards.
         *
         * Note2! The chip is initialized if detected.
         */

        unsigned char stat1, stat2, signature;

        /* Reset timers 1 and 2 */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER_CONTROL, OPL3_TIMER1_MASK | OPL3_TIMER2_MASK);
        /* Reset the IRQ of the FM chip */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER_CONTROL, OPL3_IRQ_RESET);
        signature = stat1 = inb(l_port);  /* Status register */
        if ((stat1 & 0xe0) != 0x00) {   /* Should be 0x00 */
                snd_printd("OPL3: stat1 = 0x%x\n", stat1);
                return -ENODEV;
        }
        /* Set timer1 to 0xff */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER1, 0xff);
        /* Unmask and start timer 1 */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER_CONTROL, OPL3_TIMER2_MASK | OPL3_TIMER1_START);
        /* Now we have to delay at least 80us */
        udelay(200);
        /* Read status after timers have expired */
        stat2 = inb(l_port);
        /* Stop the timers */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER_CONTROL, OPL3_TIMER1_MASK | OPL3_TIMER2_MASK);
        /* Reset the IRQ of the FM chip */
        opl3_command(l_port, r_port, OPL3_LEFT | OPL3_REG_TIMER_CONTROL, OPL3_IRQ_RESET);
        if ((stat2 & 0xe0) != 0xc0) {   /* There is no YM3812 */
                snd_printd("OPL3: stat2 = 0x%x\n", stat2);
                return -ENODEV;
        }
#if 0
        /* If the toplevel code knows exactly the type of chip, don't try
           to detect it. */
        if (opl3->hardware != OPL3_HW_AUTO)
                return 0;

        /* There is a FM chip on this address. Detect the type (OPL2 to OPL4) */
        if (signature == 0x06) {        /* OPL2 */
                opl3->hardware = OPL3_HW_OPL2;
        } else {
                /*
                 * If we had an OPL4 chip, opl3->hardware would have been set
                 * by the OPL4 driver; so we can assume OPL3 here.
                 */
                if (snd_BUG_ON(!opl3->r_port))
                        return -ENODEV;
                opl3->hardware = OPL3_HW_OPL3;
        }
#endif
        return 0;
}

static void snd_card_als4000_free( struct snd_card *card )
{
	struct snd_card_als4000 *acard = card->private_data;

	/* make sure that interrupts are disabled */
	snd_als4k_gcr_write_addr(acard->iobase, ALS4K_GCR8C_MISC_CTRL, 0);
	/* free resources */
	snd_als4000_free_gameport(acard);
}

int snd_card_als4000_create (struct snd_card *card,
                             struct pci_dev *pci,
                             struct snd_card_als4000 **racard)
{
	static int dev;
	struct snd_card_als4000 *acard;
	unsigned long iobase;
	struct snd_sb *chip;
	struct snd_opl3 *opl3;
	unsigned short word;
	int err;

	/* enable PCI device */
	err = pcim_enable_device(pci);
	if (err < 0)
		return err;

#if 0
	/* check, if we can restrict PCI DMA transfers to 24 bits */
	if (dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(24))) {
		dev_err(&pci->dev, "architecture does not support 24bit PCI busmaster DMA\n");
		return -ENXIO;
	}
#endif

	err = pci_request_regions(pci, "ALS4000");
	if (err < 0)
		return err;
	iobase = pci_resource_start(pci, 0);

	pci_read_config_word(pci, PCI_COMMAND, &word);
	//printk("PCI COMMAND: %X\n", word);
	pci_write_config_word(pci, PCI_COMMAND, word | PCI_COMMAND_IO);
	pci_set_master(pci);

#if 0
	err = snd_devm_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
				sizeof(*acard) /* private_data: acard */,
				&card);
	if (err < 0)
		return err;

	acard = card->private_data;
#else
        acard = kzalloc(sizeof(*acard), GFP_KERNEL);
        if (!acard)
          return -ENOMEM;
        card->private_data = acard;
#endif

	acard->pci = pci;
	acard->iobase = iobase;
	card->private_free = snd_card_als4000_free;

	// Reset
	outb(1, iobase + ALS4K_IOB_16_ESP_RESET);
	udelay(100);

	/* disable all legacy ISA stuff */
	snd_als4000_set_addr(acard->iobase, 0, 0, 0, 0);

	err = snd_sbdsp_create(card,
			       iobase + ALS4K_IOB_10_ADLIB_ADDR0,
			       pci->irq,
			       snd_als4000_interrupt,
			       -1,
			       -1,
			       SB_HW_ALS4000,
			       &chip);
	if (err < 0)
		return err;
	acard->chip = chip;

	chip->pci = pci;
	chip->alt_port = iobase;

	snd_als4000_configure(chip);

	strcpy(card->driver, "ALS4000");
	strcpy(card->shortname, "Avance Logic ALS4000");
	sprintf(card->longname, "%s at 0x%lx, irq %i",
		card->shortname, chip->alt_port, chip->irq);

#if 0
	err = snd_mpu401_uart_new(card, 0, MPU401_HW_ALS4000,
				  iobase + ALS4K_IOB_30_MIDI_DATA,
				  MPU401_INFO_INTEGRATED |
				  MPU401_INFO_IRQ_HOOK,
				  -1, &chip->rmidi);
	if (err < 0) {
		dev_err(&pci->dev, "no MPU-401 device at 0x%lx?\n",
				iobase + ALS4K_IOB_30_MIDI_DATA);
		return err;
	}
	/* FIXME: ALS4000 has interesting MPU401 configuration features
	 * at ALS4K_CR1A_MPU401_UART_MODE_CONTROL
	 * (pass-thru / UART switching, fast MIDI clock, etc.),
	 * however there doesn't seem to be an ALSA API for this...
	 * SPECS_PAGE: 21 */

	err = snd_als4000_pcm(chip, 0);
	if (err < 0)
		return err;
#endif

#if 0 // not required
	opl3_detect(iobase+ALS4K_IOB_10_ADLIB_ADDR0, iobase+ALS4K_IOB_12_ADLIB_ADDR2);
	opl3_command(iobase+ALS4K_IOB_10_ADLIB_ADDR0, iobase+ALS4K_IOB_12_ADLIB_ADDR2, OPL3_LEFT | OPL3_REG_TEST, OPL3_ENABLE_WAVE_SELECT);
	/* Melodic mode */
	opl3_command(iobase+ALS4K_IOB_10_ADLIB_ADDR0, iobase+ALS4K_IOB_12_ADLIB_ADDR2, OPL3_LEFT | OPL3_REG_PERCUSSION, 0x00);
	/* Enter OPL3 mode */
	opl3_command(iobase+ALS4K_IOB_10_ADLIB_ADDR0, iobase+ALS4K_IOB_12_ADLIB_ADDR2, OPL3_RIGHT | OPL3_REG_MODE, OPL3_OPL3_ENABLE);
#endif

	err = snd_sbmixer_new(chip);
	if (err < 0)
		return err;

#if 0
	if (snd_opl3_create(card,
				iobase + ALS4K_IOB_10_ADLIB_ADDR0,
				iobase + ALS4K_IOB_12_ADLIB_ADDR2,
			    OPL3_HW_AUTO, 1, &opl3) < 0) {
		dev_err(&pci->dev, "no OPL device at 0x%lx-0x%lx?\n",
			   iobase + ALS4K_IOB_10_ADLIB_ADDR0,
			   iobase + ALS4K_IOB_12_ADLIB_ADDR2);
	} else {
		err = snd_opl3_hwdep_new(opl3, 0, 1, NULL);
		if (err < 0)
			return err;
	}

	snd_als4000_create_gameport(acard, dev);

	err = snd_card_register(card);
	if (err < 0)
		return err;

	pci_set_drvdata(pci, card);
	dev++;
#endif
        *racard = acard;
	return 0;
}

#if 0
static int __snd_card_als4000_probe(struct pci_dev *pci,
				    const struct pci_device_id *pci_id)
{
	static int dev;
	struct snd_card *card;
	struct snd_card_als4000 *acard;
	unsigned long iobase;
	struct snd_sb *chip;
	struct snd_opl3 *opl3;
	unsigned short word;
	int err;

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}

	/* enable PCI device */
	err = pcim_enable_device(pci);
	if (err < 0)
		return err;

	/* check, if we can restrict PCI DMA transfers to 24 bits */
	if (dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(24))) {
		dev_err(&pci->dev, "architecture does not support 24bit PCI busmaster DMA\n");
		return -ENXIO;
	}

	err = pci_request_regions(pci, "ALS4000");
	if (err < 0)
		return err;
	iobase = pci_resource_start(pci, 0);

	pci_read_config_word(pci, PCI_COMMAND, &word);
	pci_write_config_word(pci, PCI_COMMAND, word | PCI_COMMAND_IO);
	pci_set_master(pci);
	
	err = snd_devm_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
				sizeof(*acard) /* private_data: acard */,
				&card);
	if (err < 0)
		return err;

	acard = card->private_data;
	acard->pci = pci;
	acard->iobase = iobase;
	card->private_free = snd_card_als4000_free;

	/* disable all legacy ISA stuff */
	snd_als4000_set_addr(acard->iobase, 0, 0, 0, 0);

	err = snd_sbdsp_create(card,
			       iobase + ALS4K_IOB_10_ADLIB_ADDR0,
			       pci->irq,
		/* internally registered as IRQF_SHARED in case of ALS4000 SB */
			       snd_als4000_interrupt,
			       -1,
			       -1,
			       SB_HW_ALS4000,
			       &chip);
	if (err < 0)
		return err;
	acard->chip = chip;

	chip->pci = pci;
	chip->alt_port = iobase;

	snd_als4000_configure(chip);

	strcpy(card->driver, "ALS4000");
	strcpy(card->shortname, "Avance Logic ALS4000");
	sprintf(card->longname, "%s at 0x%lx, irq %i",
		card->shortname, chip->alt_port, chip->irq);

	err = snd_mpu401_uart_new(card, 0, MPU401_HW_ALS4000,
				  iobase + ALS4K_IOB_30_MIDI_DATA,
				  MPU401_INFO_INTEGRATED |
				  MPU401_INFO_IRQ_HOOK,
				  -1, &chip->rmidi);
	if (err < 0) {
		dev_err(&pci->dev, "no MPU-401 device at 0x%lx?\n",
				iobase + ALS4K_IOB_30_MIDI_DATA);
		return err;
	}
	/* FIXME: ALS4000 has interesting MPU401 configuration features
	 * at ALS4K_CR1A_MPU401_UART_MODE_CONTROL
	 * (pass-thru / UART switching, fast MIDI clock, etc.),
	 * however there doesn't seem to be an ALSA API for this...
	 * SPECS_PAGE: 21 */

	err = snd_als4000_pcm(chip, 0);
	if (err < 0)
		return err;

	err = snd_sbmixer_new(chip);
	if (err < 0)
		return err;

	if (snd_opl3_create(card,
				iobase + ALS4K_IOB_10_ADLIB_ADDR0,
				iobase + ALS4K_IOB_12_ADLIB_ADDR2,
			    OPL3_HW_AUTO, 1, &opl3) < 0) {
		dev_err(&pci->dev, "no OPL device at 0x%lx-0x%lx?\n",
			   iobase + ALS4K_IOB_10_ADLIB_ADDR0,
			   iobase + ALS4K_IOB_12_ADLIB_ADDR2);
	} else {
		err = snd_opl3_hwdep_new(opl3, 0, 1, NULL);
		if (err < 0)
			return err;
	}

	snd_als4000_create_gameport(acard, dev);

	err = snd_card_register(card);
	if (err < 0)
		return err;

	pci_set_drvdata(pci, card);
	dev++;
	return 0;
}

static int snd_card_als4000_probe(struct pci_dev *pci,
				  const struct pci_device_id *pci_id)
{
	return snd_card_free_on_error(&pci->dev, __snd_card_als4000_probe(pci, pci_id));
}

#ifdef CONFIG_PM_SLEEP
static int snd_als4000_suspend(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct snd_card_als4000 *acard = card->private_data;
	struct snd_sb *chip = acard->chip;

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	
	snd_sbmixer_suspend(chip);
	return 0;
}

static int snd_als4000_resume(struct device *dev)
{
	struct snd_card *card = dev_get_drvdata(dev);
	struct snd_card_als4000 *acard = card->private_data;
	struct snd_sb *chip = acard->chip;

	snd_als4000_configure(chip);
	snd_sbdsp_reset(chip);
	snd_sbmixer_resume(chip);

#ifdef SUPPORT_JOYSTICK
	if (acard->gameport)
		snd_als4000_set_addr(acard->iobase, 0, 0, 0, 1);
#endif

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}

static SIMPLE_DEV_PM_OPS(snd_als4000_pm, snd_als4000_suspend, snd_als4000_resume);
#define SND_ALS4000_PM_OPS	&snd_als4000_pm
#else
#define SND_ALS4000_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static struct pci_driver als4000_driver = {
	.name = KBUILD_MODNAME,
	.id_table = snd_als4000_ids,
	.probe = snd_card_als4000_probe,
	.driver = {
		.pm = SND_ALS4000_PM_OPS,
	},
};

module_pci_driver(als4000_driver);
#endif
