//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//**************************************************************************
//function: Cirrus Logic CS4281 low level routines (e.g. Genius Sound Maker
//          Value 5.1, various "CS4281-CM" OEM cards). Has an on-board
//          standard AC'97 codec, so the existing ac97_def.c mixer table is
//          reused unchanged.
//
//Ported from a working, hardware-tested VSBHDA (github.com/Baron-von-
//Riedesel/VSBHDA) driver of the same name, based on register information
//from the ALSA driver sound/pci/cs4281.c (Jaroslav Kysela). Getting real
//audio out of a genuine CS4281 card (a Genius-branded "CS4281-CM" OEM
//board) needed several non-obvious fixes beyond what a straight ALSA-source
//reading suggests - see the comments at each one; they were all found by
//iterative hardware-in-the-loop debugging, not just spec reading.
//
//Unlike every other card in this codebase except sc_inthd.c (Intel HDA),
//the CS4281 is accessed through MEMORY-MAPPED I/O (PCI BAR0, "BA0", a 4KB
//register window) instead of port I/O - see sc_inthd.c for the mapping
//approach reused here (pds_dpmi_map_physical_memory(), see au_base.h).

#include "au_cards.h"

#ifdef AU_CARDS_LINK_CS4281

//#define MPXPLAY_USE_DEBUGF 1
//#define CS4281_DEBUG_OUTPUT stdout

#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"

#define CS4281_BA0_SIZE 0x1000  /* size of the BA0 MMIO register window */
#define CS4281_FIFO_SIZE 32     /* per-channel FIFO size in samples, full-duplex 2ch */
#define CS4281_DMABUF_PERIODS 32
#define CS4281_MAX_CHANNELS    2
#define CS4281_MAX_BYTES       4
#define CS4281_DMABUF_ALIGN (CS4281_DMABUF_PERIODS*CS4281_MAX_CHANNELS*CS4281_MAX_BYTES) // 256

//-------------------------------------------------------------------------
// BA0 register offsets/bits actually used by this (playback-only) driver.
// Full register set is much larger - see the ALSA driver for anything not
// listed here (MIDI, secondary/dual codec, joystick low-level access...).

#define BA0_HISR        0x0000  /* Host Interrupt Status (R/O, ack via HDSR/EOI) */
#define  BA0_HISR_DMA(c) (1UL<<(8+(c)))
/* Global/summary "a DMA event (half or end) happened" bit - separate from
 * the per-engine DMA(c) bits above. Confirmed (by testing) to be a
 * hierarchical gate: with only DMA(0) unmasked in HIMR and this bit left
 * masked, the per-engine interrupt never propagates to the PCI INTx# pin
 * at all - HISR_DMA(0) can be pending "beneath" it, but the top-level
 * interrupt is simply never asserted, so the IRQ routine is never called
 * (confirmed: zero calls logged across a full play session). ALSA's
 * snd_cs4281_chip_init() unmasks this alongside every per-engine DMA(c)
 * bit; see cs4281.c, BA0_HISR_DMAI. */
#define  BA0_HISR_DMAI   (1UL<<18)

#define BA0_HICR        0x0008  /* Host Interrupt Control */
#define  BA0_HICR_EOI    0x03   /* End Of Interrupt command */

#define BA0_HIMR        0x000c  /* Host Interrupt Mask (1=masked/disabled) */

#define BA0_HDSR0       0x00f0  /* Host DMA Engine 0 Status (playback) */
#define  BA0_HDSR_DHTC   (1<<17) /* DMA Half Terminal Count */
#define  BA0_HDSR_DTC    (1<<16) /* DMA Terminal Count */

#define BA0_DCA0        0x0110  /* Host DMA Engine 0 Current Address */
#define BA0_DCC0        0x0114  /* Host DMA Engine 0 Current Count (frames, counts down) */
#define BA0_DBA0        0x0118  /* Host DMA Engine 0 Base Address */
#define BA0_DBC0        0x011c  /* Host DMA Engine 0 Base Count (frames-1) */

/* per-engine register spacing, engines 1-3 (confirmed against ALSA's
 * snd_cs4281_create(): regDMR = BA0_DMR0 + engine*8, regDCR = BA0_DCR0 +
 * engine*8) - used only to defensively mask engines we don't otherwise
 * touch, see cs4281_chip_init(). */
#define BA0_DMR_N(n)    (0x0150 + (n)*8)
#define BA0_DCR_N(n)    (0x0154 + (n)*8)

#define BA0_DMR0        0x0150  /* Host DMA Engine 0 Mode */
#define  BA0_DMR_DMA     (1<<29) /* enable DMA mode */
#define  BA0_DMR_AUTO    (1<<4)  /* auto-initialize (loop) */
#define  BA0_DMR_TYPE_SINGLE (1<<6)
#define  BA0_DMR_TR_READ (2<<2)  /* "read transfer" = fetch FROM system memory (playback) */

#define BA0_DCR0        0x0154  /* Host DMA Engine 0 Command */
#define  BA0_DCR_TCIE    (1<<16) /* terminal count interrupt enable */
#define  BA0_DCR_HTCIE   (1<<17) /* half terminal count interrupt enable */
#define  BA0_DCR_MSK     (1<<0)  /* DMA mask (1 = channel masked/stopped) */

#define BA0_FCR0        0x0180  /* FIFO Control 0 (playback FIFO) */
#define  BA0_FCR_FEN     (1UL<<31) /* FIFO enable */
#define  BA0_FCR_RS(x)   (((x)&0x1f)<<24) /* right slot mapping */
#define  BA0_FCR_LS(x)   (((x)&0x1f)<<16) /* left slot mapping */
#define  BA0_FCR_SZ(x)   (((x)&0x7f)<<8)  /* FIFO size in samples */
#define  BA0_FCR_OF(x)   (((x)&0x7f)<<0)  /* FIFO starting offset in samples */

#define BA0_FSIC0       0x0210  /* FIFO Status/Interrupt Control 0 */

#define BA0_EPPMC       0x03e4  /* Extended PCI Power Management Control */
#define  BA0_EPPMC_FPDN  (1<<14) /* Full Power DowN - must be 0 or init fails */

#define BA0_CWPR        0x03e0  /* Configuration Write Protect */
#define BA0_SPMC        0x03ec  /* Serial Port Power Management Control */
#define  BA0_SPMC_RSTN   (1<<0)  /* Reset-Not: 0=AC97 ARST# asserted, 1=released */

#define BA0_CFLR        0x03f0  /* Configuration Load Register */
#define  BA0_CFLR_DEFAULT 0x00000001 /* must read back this value = AC97 link mode */

#define BA0_SERMC       0x0420  /* Serial Port Master Control */
#define  BA0_SERMC_PTC_AC97 (1<<1) /* port timing configuration = AC97 */
#define  BA0_SERMC_MSPE  (1<<0)  /* master serial port enable */

#define BA0_SERC1       0x0428  /* Serial Port Configuration 1 (R/O sanity check) */
#define  BA0_SERC1_AC97  (1<<1)
#define  BA0_SERC1_SO1EN (1<<0)

#define BA0_SERC2       0x042c  /* Serial Port Configuration 2 (R/O sanity check) */
#define  BA0_SERC2_AC97  (1<<1)
#define  BA0_SERC2_SI1EN (1<<0)

#define BA0_ACCTL       0x0460  /* AC'97 Control */
#define  BA0_ACCTL_CRW   (1<<4)  /* 0=write, 1=read command */
#define  BA0_ACCTL_DCV   (1<<3)  /* dynamic command valid (self-clears when done) */
#define  BA0_ACCTL_VFRM  (1<<2)  /* valid frame */
#define  BA0_ACCTL_ESYN  (1<<1)  /* enable sync generation */

#define BA0_ACSTS       0x0464  /* AC'97 Status */
#define  BA0_ACSTS_VSTS  (1<<1)  /* valid status (read data ready) */
#define  BA0_ACSTS_CRDY  (1<<0)  /* codec ready */

#define BA0_ACOSV       0x0468  /* AC'97 Output Slot Valid */
#define BA0_ACCAD       0x046c  /* AC'97 Command Address */
#define BA0_ACCDA       0x0470  /* AC'97 Command Data */
#define BA0_ACISV       0x0474  /* AC'97 Input Slot Valid */
#define  BA0_ACISV_SLV(x) (1UL<<((x)-3))
#define  BA0_ACOSV_SLV(x) (1UL<<((x)-3))
#define BA0_ACSDA       0x047c  /* AC'97 Status Data */

#define BA0_CLKCR1      0x0400  /* Clock Control Register 1 */
#define  BA0_CLKCR1_DLLRDY (1<<24) /* DLL ready (R/O) */
#define  BA0_CLKCR1_SWCE (1<<5)  /* software clock enable */
#define  BA0_CLKCR1_DLLP (1<<4)  /* DLL power up */

#define BA0_SSPM        0x0740  /* Sound System Power Management */
#define  BA0_SSPM_MIXEN  (1<<6)
#define  BA0_SSPM_CSRCEN (1<<5)
#define  BA0_SSPM_PSRCEN (1<<4)
#define  BA0_SSPM_JSEN   (1<<3)  /* joystick/gameport enable */
#define  BA0_SSPM_ACLEN  (1<<2)
#define  BA0_SSPM_FMEN   (1<<1)

#define BA0_DACSR       0x0744  /* DAC Sample Rate (playback SRC divisor) */

#define BA0_SRCSA       0x075c  /* SRC Slot Assignments */
#define BA0_PPLVC       0x0760  /* PCM Playback Left digital Volume Control */
#define BA0_PPRVC       0x0764  /* PCM Playback Right digital Volume Control */

//-------------------------------------------------------------------------
// generic PCI power management: force the device into D0. Required on the
// CS4281 - if left in a non-D0 state by the BIOS/a previous OS, every BA0
// register reads back as 0xFFFFFFFF and nothing else in this driver works.

#define PCIR_CAPPTR     0x34
#define PCI_CAP_ID_PM   0x01

static void cs4281_set_power_d0(struct pci_config_s *dev)
{
 uint16_t status = pcibios_ReadConfig_Word(dev, PCIR_STATUS);
 uint8_t ptr;
 int guard;

 if (!(status & (1<<4))) /* no capabilities list present */
  return;
 ptr = pcibios_ReadConfig_Byte(dev, PCIR_CAPPTR) & 0xfc;
 for (guard = 0; ptr && guard < 16; guard++) {
  uint8_t capid = pcibios_ReadConfig_Byte(dev, ptr);
  if (capid == PCI_CAP_ID_PM) {
   uint16_t pmcsr = pcibios_ReadConfig_Word(dev, ptr+4);
   mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_set_power_d0: PM cap at %2.2X, pmcsr=%4.4X",ptr,pmcsr);
   pcibios_WriteConfig_Word(dev, ptr+4, pmcsr & 0xfffc); /* power state bits = 00 = D0 */
   pds_delay_10us(1000); /* PCI PM spec: allow ~10ms for D3->D0 transition */
   return;
  }
  ptr = pcibios_ReadConfig_Byte(dev, ptr+1) & 0xfc;
 }
}

//-------------------------------------------------------------------------

typedef struct cs4281_card_s
{
 unsigned long ba0;    /* linear address of the mapped BA0 MMIO window */
 unsigned int  irq;
 struct pci_config_s *pci_dev;

 cardmem_t *dm;
 char *pcmout_buffer;
 long  pcmout_bufsize;
 unsigned int frag; /* half/full transfer toggle - see CS4281_IRQRoutine() */
}cs4281_card_s;

static unsigned long cs4281_peek(struct cs4281_card_s *card, unsigned long offs)
{
 return PDS_GETB_LE32((char *)(card->ba0 + offs));
}

static void cs4281_poke(struct cs4281_card_s *card, unsigned long offs, unsigned long val)
{
 PDS_PUTB_LE32((char *)(card->ba0 + offs), val);
}

//-------------------------------------------------------------------------
// AC'97 codec access - standard ACCAD/ACCDA/ACCTL/ACSTS/ACSDA protocol,
// same shape as sc_e1371.c's AC97 access, just through MMIO instead of I/O
// ports and with CS4281-specific register offsets/bit positions.

static void cs4281_ac97_write(struct cs4281_card_s *card, unsigned long reg, unsigned long val)
{
 unsigned int t;

 cs4281_poke(card, BA0_ACCAD, reg);
 cs4281_poke(card, BA0_ACCDA, val);
 cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_DCV | BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);
 for (t = 0; t < 2000; t++) {
  if (!(cs4281_peek(card, BA0_ACCTL) & BA0_ACCTL_DCV))
   return;
  pds_delay_10us(1);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_ac97_write: timeout, reg=%2.2X val=%4.4X",reg,val);
}

static unsigned long cs4281_ac97_read(struct cs4281_card_s *card, unsigned long reg)
{
 unsigned int t;

 cs4281_peek(card, BA0_ACSDA); /* discard stale state, as ALSA does */

 cs4281_poke(card, BA0_ACCAD, reg);
 cs4281_poke(card, BA0_ACCDA, 0);
 cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_DCV | BA0_ACCTL_CRW | BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);

 for (t = 0; t < 500; t++) {
  if (!(cs4281_peek(card, BA0_ACCTL) & BA0_ACCTL_DCV))
   goto dcv_ok;
  pds_delay_10us(1);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_ac97_read: DCV timeout, reg=%2.2X",reg);
 return 0xffff;

dcv_ok:
 for (t = 0; t < 100; t++) {
  if (cs4281_peek(card, BA0_ACSTS) & BA0_ACSTS_VSTS)
   return cs4281_peek(card, BA0_ACSDA);
  pds_delay_10us(1);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_ac97_read: VSTS timeout, reg=%2.2X",reg);
 return 0xffff;
}

//-------------------------------------------------------------------------

static unsigned int cs4281_rate_to_rv(unsigned int rate)
{
 /* a handful of rates map to fixed hardware indices; anything else uses
  * the direct divisor formula. Both forms are accepted by BA0_DACSR -
  * see ALSA's snd_cs4281_rate(). */
 switch (rate) {
  case 8000:  return 5;
  case 11025: return 4;
  case 16000: return 3;
  case 22050: return 2;
  case 44100: return 1;
  case 48000: return 0;
 }
 return 1536000UL / rate;
}

//-------------------------------------------------------------------------

static unsigned int cs4281_buffer_init(struct cs4281_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int bytes_per_sample=2; // 16 bit
 /* Unlike sc_e1370.c/sc_e1371.c, this chip has no per-period reload
  * register - it only ever raises two interrupts per DMA buffer cycle
  * (BA0_HDSR_DHTC at the halfway point, BA0_HDSR_DTC at the end), no
  * matter how large that buffer is. Those drivers get their fine
  * interrupt granularity (~256 bytes/period) by reloading the whole
  * buffer as many small periods; requesting the default (large, ~4KB)
  * buffer here instead would leave 8-16x fewer, much coarser refill
  * points, which is audibly different (worse real-time responsiveness)
  * even though nothing is wrong at the register level. Requesting a
  * small buffer up front (max_bufsize=2*ALIGN) keeps the half/full
  * interrupt cadence close to what sc_e1370.c/sc_e1371.c provide.
  */
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,2*CS4281_DMABUF_ALIGN,CS4281_DMABUF_ALIGN,bytes_per_sample,0);
 card->dm=MDma_alloc_cardmem(card->pcmout_bufsize);
 if(!card->dm)
  return 0;
 card->pcmout_buffer=(char *)card->dm->linearptr;
 aui->card_DMABUFF=card->pcmout_buffer;
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"buffer init: pcmoutbuf:%8.8X size:%d",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
 return 1;
}

/* full hardware bring-up sequence - see ALSA's snd_cs4281_chip_init() for
 * the reasoning behind each step; this is NOT a "just enable it" chip,
 * every one of these stages has been observed to be required in practice.
 */
static int cs4281_chip_init(struct cs4281_card_s *card)
{
 unsigned long tmp;
 unsigned int t,n;

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: enter");

 /* sanity check: if the chip wasn't successfully forced to D0, every
  * BA0 register reads back as all-ones. Bail out cleanly instead of
  * spinning through every wait-loop below for nothing. */
 if (cs4281_peek(card, BA0_HISR) == 0xffffffffUL) {
  mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: BA0 unreadable (0xFFFFFFFF) - PCI power state not D0?");
  return 0;
 }

 /* mask ALL FOUR DMA engines first thing, not just the one we use -
  * matches the vendor DOS driver's own very first init action
  * ("Writing DCRn with 0x1, Mask DMA engine", for n=0..3). This
  * matters because BA0_HISR_DMAI (unmasked further below, needed for
  * engine 0's interrupt to ever reach the PCI pin at all) is a
  * SUMMARY bit covering all four engines - if engines 1-3 were left
  * in a dirty/enabled state by a previous OS or driver, unmasking
  * DMAI without also masking them first can let spurious interrupts
  * from those unused engines through.
  */
 for (n = 0; n < 4; n++)
  cs4281_poke(card, BA0_DCR_N(n), BA0_DCR_MSK);

 tmp = cs4281_peek(card, BA0_EPPMC);
 if (tmp & BA0_EPPMC_FPDN)
  cs4281_poke(card, BA0_EPPMC, tmp & ~BA0_EPPMC_FPDN);

 tmp = cs4281_peek(card, BA0_CFLR);
 if (tmp != BA0_CFLR_DEFAULT) {
  cs4281_poke(card, BA0_CFLR, BA0_CFLR_DEFAULT);
  tmp = cs4281_peek(card, BA0_CFLR);
  if (tmp != BA0_CFLR_DEFAULT) {
   mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: CFLR setup failed (%8.8X)",tmp);
   return 0;
  }
 }

 /* allow the vendor-defined configuration space (E4h-FFh) to be written */
 cs4281_poke(card, BA0_CWPR, 0x4281);

 tmp = cs4281_peek(card, BA0_SERC1);
 if (tmp != (BA0_SERC1_SO1EN | BA0_SERC1_AC97)) {
  mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: SERC1 AC97 check failed (%8.8X)",tmp);
  return 0;
 }
 tmp = cs4281_peek(card, BA0_SERC2);
 if (tmp != (BA0_SERC2_SI1EN | BA0_SERC2_AC97)) {
  mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: SERC2 AC97 check failed (%8.8X)",tmp);
  return 0;
 }

 cs4281_poke(card, BA0_SSPM, BA0_SSPM_MIXEN | BA0_SSPM_CSRCEN | BA0_SSPM_PSRCEN |
                              BA0_SSPM_JSEN | BA0_SSPM_ACLEN | BA0_SSPM_FMEN);

 /* known state for the clock/serial-port logic before bringing it up */
 cs4281_poke(card, BA0_CLKCR1, 0);
 cs4281_poke(card, BA0_SERMC, 0);

 /* ESYN=0 turns off the AC97 sync pulse */
 cs4281_poke(card, BA0_ACCTL, 0);
 pds_delay_10us(5);

 /* pulse ARST# (AC97 reset) low then high, per the AC97 spec */
 cs4281_poke(card, BA0_SPMC, 0);
 pds_delay_10us(5);
 cs4281_poke(card, BA0_SPMC, BA0_SPMC_RSTN);
 pds_delay_10us(5000); /* 50ms */

 cs4281_poke(card, BA0_SERMC, (1<<16)/*TCID(1)*/ | BA0_SERMC_PTC_AC97 | BA0_SERMC_MSPE);

 /* start the DLL clock logic and wait for lock */
 cs4281_poke(card, BA0_CLKCR1, BA0_CLKCR1_DLLP);
 pds_delay_10us(5000); /* 50ms */
 cs4281_poke(card, BA0_CLKCR1, BA0_CLKCR1_SWCE | BA0_CLKCR1_DLLP);

 for (t = 0; t < 10000; t++) { /* up to ~1s */
  if (cs4281_peek(card, BA0_CLKCR1) & BA0_CLKCR1_DLLRDY)
   goto dllrdy_ok;
  pds_delay_10us(10);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: DLLRDY not seen");
 return 0;

dllrdy_ok:
 /* enable sync generation - once bit clock is seen, SYNC starts too */
 cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_ESYN);

 for (t = 0; t < 10000; t++) {
  if (cs4281_peek(card, BA0_ACSTS) & BA0_ACSTS_CRDY)
   goto crdy_ok;
  pds_delay_10us(10);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: codec ready (CRDY) not seen, status=%8.8X",cs4281_peek(card, BA0_ACSTS));
 return 0;

crdy_ok:
 /* start sending commands to the AC97 codec */
 cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);

 for (t = 0; t < 10000; t++) {
  if ((cs4281_peek(card, BA0_ACISV) & (BA0_ACISV_SLV(3)|BA0_ACISV_SLV(4))) == (BA0_ACISV_SLV(3)|BA0_ACISV_SLV(4)))
   goto isv_ok;
  pds_delay_10us(10);
 }
 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: ISV3/4 not seen");
 return 0;

isv_ok:
 /* commence digital audio transfer to the codec (slots 3+4 = PCM L/R) */
 cs4281_poke(card, BA0_ACOSV, BA0_ACOSV_SLV(3) | BA0_ACOSV_SLV(4));

 /* playback FIFO (DMA engine/FIFO index 0): left=slot0, right=slot1 */
 cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
                              BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
 cs4281_poke(card, BA0_SRCSA, (0<<0) | (1<<8) | (10<<16) | (11<<24));

 /* digital volume trim: 0 = unattenuated (all real volume control goes
  * through the AC97 mixer registers, same as every other card here) */
 cs4281_poke(card, BA0_PPLVC, 0);
 cs4281_poke(card, BA0_PPRVC, 0);

 /* CRITICAL: AC97 registers power up MUTED (bit15 set) with 0dB
  * attenuation. The generic mixer-init code only ever touches the
  * attenuation bits and leaves the mute bit exactly as found here -
  * so without this, these stay muted forever no matter what volume
  * is requested (confirmed by testing - see sc_e1371.c's own AC97
  * init, which does the same thing for Master/PCM).
  *
  * AC97_HEADPHONE_VOL is unmuted here too: cheap single-jack consumer
  * cards (such as this CS4281-based Genius card) commonly wire their
  * only physical output jack through the codec's headphone amplifier
  * rather than "Line Out" - confirmed by testing, this register
  * stayed muted and silent even with Master and PCM both unmuted.
  */
 cs4281_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0C0C);
 cs4281_ac97_write(card, AC97_PCMOUT_VOL,        0x0C0C);
 cs4281_ac97_write(card, AC97_HEADPHONE_VOL,     0x0C0C);

 /* CD-IN: unmute too - carries the ANALOG signal from the card's 4-pin
  * CD-audio header, independent of any digital playback this driver
  * does. Unlike the AK4531 (ES1370), a standard AC'97 codec doesn't
  * need a separate output-routing switch bit for this - unmuting the
  * volume register alone is enough, it feeds the output mix directly
  * once unmuted.
  */
 cs4281_ac97_write(card, AC97_CD_VOL, 0x0C0C);

 /* EAPD ("External Amplifier Power Down", reg 0x26 bit15) - on the
  * CS4297A/compatible codec family used on this card, this bit
  * disables an off-chip amplifier feeding the physical output jack.
  * Writing it explicitly costs nothing and closes off a well-
  * documented, common "everything reports fine, but silent" failure
  * mode for cards with an external amp.
  */
 cs4281_ac97_write(card, AC97_POWER_CONTROL, 0x0000);

 cs4281_poke(card, BA0_HICR, BA0_HICR_EOI);
 /* unmask DMA engine 0's interrupt (our playback channel) AND the
  * global DMAI summary bit - both are required, see BA0_HISR_DMAI
  * comment above. */
 cs4281_poke(card, BA0_HIMR, 0x7fffffffUL & ~(BA0_HISR_DMA(0) | BA0_HISR_DMAI));

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_chip_init: exit, OK");
 return 1;
}

static void cs4281_chip_close(struct cs4281_card_s *card)
{
 if(!card->ba0)
  return;
 cs4281_poke(card, BA0_HIMR, 0x7fffffffUL); /* mask all interrupts */
 cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK);
 cs4281_poke(card, BA0_DMR0, 0);
 cs4281_poke(card, BA0_CLKCR1, 0);
 cs4281_poke(card, BA0_SSPM, 0);
 cs4281_poke(card, BA0_SPMC, 0);
}

static void cs4281_prepare_playback(struct cs4281_card_s *card,struct mpxplay_audioout_info_s *aui)
{
 unsigned int rv;

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_prepare_playback: enter, dmasize=%d",aui->card_dmasize);

 cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK); /* stop/mask channel while reprogramming */
 cs4281_poke(card, BA0_DMR0, 0);

 cs4281_poke(card, BA0_DBA0, (unsigned long) pds_cardmem_physicalptr(card->dm, card->pcmout_buffer));
 cs4281_poke(card, BA0_DBC0, (aui->card_dmasize >> 2) - 1); /* frames-1, 16-bit stereo = 4 bytes/frame */

 cs4281_poke(card, BA0_SRCSA, (0<<0) | (1<<8) | (10<<16) | (11<<24));

 rv = cs4281_rate_to_rv(aui->freq_card);
 cs4281_poke(card, BA0_DACSR, rv);

 /* re-assert the playback FIFO's slot mapping */
 cs4281_poke(card, BA0_FCR0, cs4281_peek(card, BA0_FCR0) & ~BA0_FCR_FEN);
 cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
                              BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
 cs4281_poke(card, BA0_FSIC0, 0);

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"cs4281_prepare_playback: exit");
}

//-------------------------------------------------------------------------
static pci_device_s cs4281_devices[]={
 {"CS4281",0x1013,0x6005, 0},
 {NULL,0,0,0}
};

static void CS4281_close(struct mpxplay_audioout_info_s *aui);

static void CS4281_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"Cirrus Logic CS4281 : found on BA0:%8.8X irq:%d",card->ba0,card->irq);
 pds_textdisplay_printf(sout);
}

static int CS4281_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card;
 unsigned long physaddr;

 card=(struct cs4281_card_s *)pds_calloc(1,sizeof(struct cs4281_card_s));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(cs4281_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"CS4281_adetect: known card found, enable PCI io and busmaster");
 pcibios_set_master(card->pci_dev);
 cs4281_set_power_d0(card->pci_dev); /* required - see comment above */

 physaddr = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
 aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
#ifdef SBEMU
 aui->card_pci_dev = card->pci_dev;
#endif

 mpxplay_debugf(CS4281_DEBUG_OUTPUT,"CS4281_adetect: vend_id:%4.4X dev_id:%4.4X BA0:%8.8X irq:%d",
  card->pci_dev->vendor_id,card->pci_dev->device_id,physaddr,card->irq);

 if( physaddr & 0x1 ) { /* I/O space bit set? shouldn't happen, BA0 is memory-mapped */
  mpxplay_debugf(CS4281_DEBUG_OUTPUT,"CS4281_adetect: BA0 looks like an I/O BAR (%8.8X) - unexpected, aborting",physaddr);
  goto err_adetect;
 }
 physaddr &= 0xfffffff0UL;
 if(!physaddr)
  goto err_adetect;

 card->ba0 = pds_dpmi_map_physical_memory(physaddr, CS4281_BA0_SIZE);
 if(!card->ba0)
  goto err_adetect;

 if( !cs4281_chip_init(card) )
  goto err_adetect;
 if( !cs4281_buffer_init(card,aui) )
  goto err_adetect;

 return 1;

err_adetect:
 CS4281_close(aui);
 return 0;
}

static void CS4281_close(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;
 if(card){
  if(card->ba0){
   cs4281_chip_close(card);
   pds_dpmi_unmap_physycal_memory(card->ba0);
   card->ba0=0;
  }
  if(card->dm)
   MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void CS4281_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;

 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
 aui->chan_card=2;
 aui->bits_card=16;

 if(aui->freq_card<4000)
  aui->freq_card=4000;
 else if(aui->freq_card>48000)
  aui->freq_card=48000;

 MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,CS4281_DMABUF_ALIGN,0);

 /* frames between interrupts = half of the actual DMA buffer (see
  * BA0_HDSR_DHTC/DTC - this chip always interrupts at the half and
  * full points of whatever buffer size ended up allocated). Computed
  * from the real aui->card_dmasize rather than hardcoded, since
  * MDma_init_pcmoutbuf() may round it. Every other card driver in
  * this codebase sets this field (sc_e1371.c, sc_ich.c, sc_inthd.c,
  * ...) - leaving it unset here left it at its pds_calloc()-zeroed
  * default, and it is used as a divisor in main.c's direct-out
  * resample-ratio calculation. */
 aui->card_samples_per_int = (aui->card_dmasize/2) >> 2;

 cs4281_prepare_playback(card,aui);
}

static void CS4281_start(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;
 unsigned long dmr = BA0_DMR_TYPE_SINGLE | BA0_DMR_AUTO | BA0_DMR_TR_READ;

 card->frag = 0;
 /* Force a clean (re)start of the DMA engine: write DMR with the
  * DMA-enable bit still clear, THEN with it set (0->1 transition) -
  * a single write with the bit already set was not enough to get
  * audible output in testing. FIFO and unmask come last, matching
  * ALSA's snd_cs4281_trigger() write order exactly (DMR, then FCR,
  * then DCR - unmasking the channel is the very last step).
  */
 cs4281_poke(card, BA0_DMR0, dmr);
 cs4281_poke(card, BA0_DMR0, dmr | BA0_DMR_DMA);
 cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
                              BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
 cs4281_poke(card, BA0_DCR0, BA0_DCR_TCIE | BA0_DCR_HTCIE); /* MSK=0 last */
}

static void CS4281_stop(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;
 cs4281_poke(card, BA0_DMR0, 0); /* DMA=0 */
 cs4281_poke(card, BA0_FCR0, cs4281_peek(card, BA0_FCR0) & ~BA0_FCR_FEN);
 cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK); /* mask channel last */
}

//-------------------------------------------------------------------------

static long CS4281_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct cs4281_card_s *card=aui->card_private_data;
 unsigned long dmasize_frames = aui->card_dmasize >> 2;
 unsigned long dcc,bufpos_frames;

 if(cs4281_peek(card, BA0_DCR0) & BA0_DCR_MSK)
  return aui->card_dma_lastgoodpos;

 dcc = cs4281_peek(card, BA0_DCC0) & 0xffffff;
 bufpos_frames = dmasize_frames - (dcc + 1);
 aui->card_dma_lastgoodpos = bufpos_frames << 2;

 return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------

static void CS4281_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct cs4281_card_s *card=aui->card_private_data;
 cs4281_ac97_write(card,reg,val);
}

static unsigned long CS4281_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct cs4281_card_s *card=aui->card_private_data;
 return cs4281_ac97_read(card,reg);
}

#ifdef SBEMU
static int CS4281_IRQRoutine(mpxplay_audioout_info_s* aui)
{
 cs4281_card_s *card=aui->card_private_data;
 unsigned long status = cs4281_peek(card, BA0_HISR);
 int ours = 0;

 /* Do NOT mpxplay_debugf() in here, even with debug logging otherwise
  * enabled for this file - this runs in real hardware interrupt
  * context, and a debug build that logged from here reproducibly
  * crashed real DOS games once interrupts started actually firing
  * (once the DMAI fix above was in place). Matches sc_e1371.c's own
  * IRQ handling, which is likewise silent. */

 if (status & BA0_HISR_DMA(0)) {
  unsigned long hdsr = cs4281_peek(card, BA0_HDSR0); /* ack this DMA engine's pending status */
  /* CS4281 hardware quirk (documented in ALSA's cs4281.c): the chip
   * sometimes reports the SAME half/full transfer boundary twice in
   * a row instead of alternating. Track expected parity via
   * card->frag and ignore a duplicate report at the same boundary,
   * exactly as ALSA does - left unhandled, this is what caused the
   * interrupt to keep re-triggering hard enough to crash once DMAI
   * was unmasked.
   */
  card->frag++;
  if ((hdsr & BA0_HDSR_DHTC) && !(card->frag & 1))
   card->frag--;
  else if ((hdsr & BA0_HDSR_DTC) && (card->frag & 1))
   card->frag--;
  ours = 1;
 }
 cs4281_poke(card, BA0_HICR, BA0_HICR_EOI);
 return ours;
}
#endif

one_sndcard_info CS4281_sndcard_info={
 "Cirrus Logic CS4281",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                 // no init
 &CS4281_adetect,      // only autodetect
 &CS4281_card_info,
 &CS4281_start,
 &CS4281_stop,
 &CS4281_close,
 &CS4281_setrate,

 &MDma_writedata,
 &CS4281_getbufpos,
 &MDma_clearbuf,
 &MDma_interrupt_monitor,
 #ifdef SBEMU
 &CS4281_IRQRoutine,
 #else
 NULL,
 #endif

 &CS4281_writeMIXER,
 &CS4281_readMIXER,
 &mpxplay_aucards_ac97chan_mixerset[0]
};

#endif // AU_CARDS_LINK_CS4281
