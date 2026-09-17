// SPDX-License-Identifier: GPL-2.0-only
// AMD CS5535/CS5536 Audio driver for SBEMU
// Based on Linux snd_cs5535audio (sound/pci/cs5535audio/) and the AMD CS5536 datasheet.
//
// Hardware summary:
//   - Single PCI BAR0 (I/O port space) for all ACC registers
//   - AC97 codec access via ACC_CODEC_CNTL / ACC_CODEC_STATUS (not a separate I/O range)
//   - Bus-master DMA uses a Physical Region Descriptor (PRD) table with a JMP-back entry
//     for circular, interrupt-per-period playback
//   - Position tracked via ACC_BM0_PNTR (current physical DMA byte address)

#include "au_cards.h"

#ifdef AU_CARDS_LINK_CS5535

#include <string.h>
#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"

// ACC register offsets from BAR0 I/O base
#define ACC_GPIO_STATUS   0x00  // reading ACKs codec/wakeup IRQs
#define ACC_CODEC_STATUS  0x08  // AC97 read-back result + status
#define ACC_CODEC_CNTL    0x0C  // AC97 command / control
#define ACC_IRQ_STATUS    0x12  // 16-bit interrupt status
#define ACC_BM0_CMD       0x20  // Bus Master 0 (playback) command byte
#define ACC_BM0_STATUS    0x21  // Bus Master 0 status byte
#define ACC_BM0_PRD       0x24  // BM0 PRD table address (physical, 32-bit)
#define ACC_BM0_PNTR      0x60  // BM0 current DMA byte pointer (physical, 32-bit)

// ACC_CODEC_CNTL bit fields
#define CS_CMD_NEW       0x00010000u  // set to issue command; HW clears when done
#define CS_CMD_MASK      0xFF00FFFFu  // keeps index (bits 31:24) and data (bits 15:0)
#define CS_CODEC_RD      0x80000000u  // bit 31 set = read
#define CS_LNK_WRM_RST   0x00020000u  // warm-reset the AC-link
#define CS_LNK_SHUTDOWN  0x00040000u  // shut down AC-link

// ACC_CODEC_STATUS bit fields
#define CS_STS_NEW       0x00020000u  // new read result available
#define CS_PRM_RDY       0x00800000u  // codec ready after warm reset

// IRQ status bits (ACC_IRQ_STATUS, 16-bit)
#define CS_IRQ_CODEC_STS  (1u << 0)
#define CS_IRQ_WU_STS     (1u << 1)
#define CS_IRQ_BM0_STS    (1u << 2)
#define CS_IRQ_BM1_STS    (1u << 3)

// BM0 command register values
#define CS_BM_EN          0x01
#define CS_BM_DIS         0x00
#define CS_BM_PAUSE       0x03

// BM0 status bits
#define CS_BM_EOP         0x01  // End-Of-Period interrupt
#define CS_BM_ERR         0x02  // DMA error

// PRD descriptor control flags (ctlreserved field)
#define CS_PRD_EOP        0x4000u  // generate interrupt when this entry finishes
#define CS_PRD_JMP        0x2000u  // addr field is next PRD table address (jump)
#define CS_PRD_EOT        0x8000u  // stop DMA after this entry

#define CS5535_PERIODS        8   // number of audio buffer periods
#define CS5535_PRD_COUNT      (CS5535_PERIODS + 1)  // +1 for the JMP entry
#define CS5535_PRD_TABLE_SIZE (CS5535_PRD_COUNT * sizeof(struct cs5535_prd))
// Must be >= PRD table size; doubled for safety; keep power-of-2 aligned
#define CS5535_BDL_ALIGN      256

#define CS5535_CODEC_TIMEOUT  500

struct cs5535_prd {
    uint32_t addr;         // physical address of audio buffer chunk
    uint16_t size;         // size in bytes of this chunk
    uint16_t ctlreserved;  // CS_PRD_* flags
} __attribute__((packed));

typedef struct cs5535_card_s {
    uint32_t             iobase;
    uint8_t              irq;
    struct pci_config_s *pci_dev;

    cardmem_t           *dm;
    struct cs5535_prd   *prd_table;    // virtual address of PRD table
    char                *pcmout_buffer;
    long                 pcmout_bufsize;

    unsigned int         period_size_bytes;
    unsigned char        vra;
} cs5535_card_s;

// ---------------------------------------------------------------------------
// Register access helpers

static inline void cs_writeb(cs5535_card_s *card, unsigned int reg, uint8_t v)  { outb(card->iobase + reg, v); }
static inline void cs_writew(cs5535_card_s *card, unsigned int reg, uint16_t v) { outw(card->iobase + reg, v); }
static inline void cs_writel(cs5535_card_s *card, unsigned int reg, uint32_t v) { outl(card->iobase + reg, v); }
static inline uint8_t  cs_readb(cs5535_card_s *card, unsigned int reg) { return inb(card->iobase + reg); }
static inline uint16_t cs_readw(cs5535_card_s *card, unsigned int reg) { return inw(card->iobase + reg); }
static inline uint32_t cs_readl(cs5535_card_s *card, unsigned int reg) { return inl(card->iobase + reg); }

// ---------------------------------------------------------------------------
// AC97 codec access (via ACC_CODEC_CNTL / ACC_CODEC_STATUS)

static void cs_codec_write(cs5535_card_s *card, uint8_t reg, uint16_t val)
{
    uint32_t cmd = ((uint32_t)reg << 24) | val;
    cmd &= CS_CMD_MASK;
    cmd |= CS_CMD_NEW;
    // bit 31 = 0 for write
    cs_writel(card, ACC_CODEC_CNTL, cmd);

    int i = CS5535_CODEC_TIMEOUT;
    while ((cs_readl(card, ACC_CODEC_CNTL) & CS_CMD_NEW) && --i)
        pds_delay_10us(1);
}

static uint16_t cs_codec_read(cs5535_card_s *card, uint8_t reg)
{
    uint32_t cmd = ((uint32_t)reg << 24) | CS_CODEC_RD | CS_CMD_NEW;
    cs_writel(card, ACC_CODEC_CNTL, cmd);

    // wait for write phase to complete
    int i = CS5535_CODEC_TIMEOUT;
    while ((cs_readl(card, ACC_CODEC_CNTL) & CS_CMD_NEW) && --i)
        pds_delay_10us(1);

    // wait for read result
    uint32_t status;
    i = CS5535_CODEC_TIMEOUT;
    do {
        status = cs_readl(card, ACC_CODEC_STATUS);
    } while ((!(status & CS_STS_NEW) || ((status >> 24) != reg)) && --i);

    return (uint16_t)status;
}

// ---------------------------------------------------------------------------
// AC97 initialisation

static void cs_ac97_init(cs5535_card_s *card, unsigned int freq_set)
{
    // unmute and set initial volumes
    cs_codec_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
    cs_codec_write(card, AC97_PCMOUT_VOL,        0x0202);
    cs_codec_write(card, AC97_HEADPHONE_VOL,     0x0202);

    // enable VRA if we need a non-48 kHz rate
    if (freq_set != 48000) {
        cs_codec_write(card, AC97_EXTENDED_STATUS, AC97_EA_VRA);
        if (cs_codec_read(card, AC97_EXTENDED_STATUS) & AC97_EA_VRA)
            card->vra = 1;
    }
}

// ---------------------------------------------------------------------------
// AC-link warm reset (ensures codec is alive after power-on)

static void cs_aclink_reset(cs5535_card_s *card)
{
    cs_writel(card, ACC_CODEC_CNTL, CS_LNK_WRM_RST | CS_CMD_NEW);
    int i = CS5535_CODEC_TIMEOUT;
    while ((cs_readl(card, ACC_CODEC_CNTL) & CS_CMD_NEW) && --i)
        pds_delay_10us(10);

    // wait for codec ready
    i = CS5535_CODEC_TIMEOUT;
    while (!(cs_readl(card, ACC_CODEC_STATUS) & CS_PRM_RDY) && --i)
        pds_delay_10us(10);
}

// ---------------------------------------------------------------------------
// DMA buffer allocation

static int cs_buffer_init(cs5535_card_s *card, struct mpxplay_audioout_info_s *aui)
{
    card->pcmout_bufsize = MDma_get_max_pcmoutbufsize(aui, 0, CS5535_BDL_ALIGN, 2, 0);

    card->dm = MDma_alloc_cardmem(CS5535_PRD_TABLE_SIZE + card->pcmout_bufsize);
    if (!card->dm)
        return 0;

    card->prd_table    = (struct cs5535_prd *)card->dm->linearptr;
    card->pcmout_buffer = card->dm->linearptr + CS5535_PRD_TABLE_SIZE;

    aui->card_DMABUFF = card->pcmout_buffer;
    memset(card->pcmout_buffer, 0, card->pcmout_bufsize);
    return 1;
}

// ---------------------------------------------------------------------------
// PRD table setup and playback configuration

static void cs_prepare_playback(cs5535_card_s *card, struct mpxplay_audioout_info_s *aui)
{
    // Disable DMA before reconfiguring
    cs_writeb(card, ACC_BM0_CMD, CS_BM_DIS);

    // Set sample rate via AC97
    if (card->vra)
        cs_codec_write(card, AC97_PCM_FRONT_DAC_RATE, (uint16_t)aui->freq_card);
    pds_delay_10us(1600);

    // Build PRD table
    uint32_t buf_phys = (uint32_t)pds_cardmem_physicalptr(card->dm, card->pcmout_buffer);
    uint32_t prd_phys = (uint32_t)pds_cardmem_physicalptr(card->dm, card->prd_table);

    for (int i = 0; i < CS5535_PERIODS; i++) {
        card->prd_table[i].addr = buf_phys + (uint32_t)(i * card->period_size_bytes);
        card->prd_table[i].size = (uint16_t)card->period_size_bytes;
        card->prd_table[i].ctlreserved = CS_PRD_EOP;
    }
    // JMP entry: loops back to the first descriptor
    card->prd_table[CS5535_PERIODS].addr = prd_phys;
    card->prd_table[CS5535_PERIODS].size = 0;
    card->prd_table[CS5535_PERIODS].ctlreserved = CS_PRD_JMP;

    // ACC_BM0_PRD receives the physical address of the JMP descriptor
    uint32_t jmp_phys = prd_phys + CS5535_PERIODS * sizeof(struct cs5535_prd);
    cs_writel(card, ACC_BM0_PRD, jmp_phys);

    aui->card_samples_per_int = (card->period_size_bytes / 2) / 2; // bytes/bytespersample/channels
}

// ---------------------------------------------------------------------------
// Driver API

static void CS5535_close(struct mpxplay_audioout_info_s *aui);

static void CS5535_card_info(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    char sout[100];
    sprintf(sout, "CS5535: AMD %s on port:%4.4X irq:%d",
            card->pci_dev->device_name, (unsigned)card->iobase, (int)card->irq);
    pds_textdisplay_printf(sout);
}

static pci_device_s cs5535_devices[] = {
    {"CS5535", 0x100B, 0x002E, 0},  // National Semi CS5535
    {"CS5536", 0x1022, 0x2093, 0},  // AMD CS5536
    {NULL, 0, 0, 0}
};

static int CS5535_adetect(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = (cs5535_card_s *)pds_calloc(1, sizeof(cs5535_card_s));
    if (!card)
        return 0;
    aui->card_private_data = card;

    card->pci_dev = (struct pci_config_s *)pds_calloc(1, sizeof(struct pci_config_s));
    if (!card->pci_dev)
        goto err_adetect;

    if (pcibios_search_devices(cs5535_devices, card->pci_dev) != PCI_SUCCESSFUL)
        goto err_adetect;

    pcibios_set_master(card->pci_dev);

    // BAR0 holds the ACC I/O port base (bits 2:0 are flags; mask to 0xFFF8 or 0xFFF0)
    card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR) & 0xFFF0;
    if (!card->iobase)
        goto err_adetect;

    aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
    aui->card_pci_dev = card->pci_dev;

    if (!cs_buffer_init(card, aui))
        goto err_adetect;

    cs_aclink_reset(card);
    cs_ac97_init(card, aui->freq_set);

    // signal FM and MPU-401 via standard ports (CS5535 does not have onboard OPL/MPU)
    aui->fm_port   = 0;
    aui->mpu401_port = 0;

    return 1;

err_adetect:
    CS5535_close(aui);
    return 0;
}

static void CS5535_close(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    if (card) {
        cs_writeb(card, ACC_BM0_CMD, CS_BM_DIS);
        MDma_free_cardmem(card->dm);
        if (card->pci_dev)
            pds_free(card->pci_dev);
        pds_free(card);
        aui->card_private_data = NULL;
    }
}

static void CS5535_setrate(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;

    aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE;
    aui->chan_card = 2;
    aui->bits_card = 16;

    if (!card->vra) {
        aui->freq_card = 48000;
    } else {
        if (aui->freq_card < 8000)  aui->freq_card = 8000;
        if (aui->freq_card > 48000) aui->freq_card = 48000;
    }

    unsigned int dmabufsize = MDma_init_pcmoutbuf(aui, card->pcmout_bufsize, CS5535_BDL_ALIGN, 0);
    card->period_size_bytes = dmabufsize / CS5535_PERIODS;

    cs_prepare_playback(card, aui);
}

static void CS5535_start(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    cs_writeb(card, ACC_BM0_CMD, CS_BM_EN);
}

static void CS5535_stop(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    cs_writeb(card, ACC_BM0_CMD, CS_BM_DIS);
}

static long CS5535_getbufpos(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    uint32_t buf_phys = (uint32_t)pds_cardmem_physicalptr(card->dm, card->pcmout_buffer);
    uint32_t pntr    = cs_readl(card, ACC_BM0_PNTR);
    int retry = 6;

    do {
        if (pntr < buf_phys || pntr >= buf_phys + (uint32_t)card->pcmout_bufsize)
            break;

        uint32_t offset = pntr - buf_phys;
        // Align to period boundary (same approach as ICH SBEMU mode)
        uint32_t period_idx = offset / card->period_size_bytes;
        if (period_idx >= (uint32_t)(CS5535_PERIODS - 1))
            break;

        uint32_t bufpos = period_idx * card->period_size_bytes;
        if (bufpos < (uint32_t)aui->card_dmasize) {
            aui->card_dma_lastgoodpos = bufpos;
            break;
        }
    } while (--retry);

    return aui->card_dma_lastgoodpos;
}

static int CS5535_IRQRoutine(struct mpxplay_audioout_info_s *aui)
{
    cs5535_card_s *card = aui->card_private_data;
    uint16_t irq_stat = cs_readw(card, ACC_IRQ_STATUS);

    if (!irq_stat)
        return 0;

    // ACK codec and wakeup interrupts by reading GPIO status
    if (irq_stat & (CS_IRQ_CODEC_STS | CS_IRQ_WU_STS))
        (void)cs_readl(card, ACC_GPIO_STATUS);

    // ACK BM0 (playback) interrupt by reading BM0 status
    if (irq_stat & CS_IRQ_BM0_STS)
        (void)cs_readb(card, ACC_BM0_STATUS);

    // ACK BM1 (capture) interrupt by reading BM1 status (0x29)
    if (irq_stat & CS_IRQ_BM1_STS)
        (void)inb(card->iobase + 0x29);

    return 1;
}

// Mixer: proxy to AC97 codec registers
static void CS5535_writeMIXER(struct mpxplay_audioout_info_s *aui, unsigned long reg, unsigned long val)
{
    cs5535_card_s *card = aui->card_private_data;
    cs_codec_write(card, (uint8_t)reg, (uint16_t)val);
}

static unsigned long CS5535_readMIXER(struct mpxplay_audioout_info_s *aui, unsigned long reg)
{
    cs5535_card_s *card = aui->card_private_data;
    return cs_codec_read(card, (uint8_t)reg);
}

// AC97 mixer channel descriptors (master + PCM, both stereo, reversed: 0 = max, 63 = mute)
static aucards_onemixerchan_s CS5535_master_vol = {
    AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER, AU_MIXCHANFUNC_VOLUME), 2,
    {{ AC97_MASTER_VOL_STEREO, 31, 8, SUBMIXCH_INFOBIT_REVERSEDVALUE },
     { AC97_MASTER_VOL_STEREO, 31, 0, SUBMIXCH_INFOBIT_REVERSEDVALUE }}
};

static aucards_onemixerchan_s CS5535_pcm_vol = {
    AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM, AU_MIXCHANFUNC_VOLUME), 2,
    {{ AC97_PCMOUT_VOL, 31, 8, SUBMIXCH_INFOBIT_REVERSEDVALUE },
     { AC97_PCMOUT_VOL, 31, 0, SUBMIXCH_INFOBIT_REVERSEDVALUE }}
};

static aucards_allmixerchan_s cs5535_mixerset[] = {
    &CS5535_master_vol,
    &CS5535_pcm_vol,
    NULL
};

one_sndcard_info CS5535_sndcard_info = {
    "CS5535/CS5536 AC97",
    SNDCARD_LOWLEVELHAND | SNDCARD_INT08_ALLOWED,

    NULL,             // card_config
    NULL,             // card_init
    &CS5535_adetect,
    &CS5535_card_info,
    &CS5535_start,
    &CS5535_stop,
    &CS5535_close,
    &CS5535_setrate,

    &MDma_writedata,
    &CS5535_getbufpos,
    &MDma_clearbuf,
    &MDma_interrupt_monitor,
    &CS5535_IRQRoutine,

    &CS5535_writeMIXER,
    &CS5535_readMIXER,
    &cs5535_mixerset[0],

    &ioport_fm_write,
    &ioport_fm_read,
    &ioport_mpu401_write_when_ready,
    &ioport_mpu401_read,
};

#endif // AU_CARDS_LINK_CS5535
