#ifndef SBEMU_ALS4000_H
#define SBEMU_ALS4000_H

#include "sound/sb.h"

struct snd_card_als4000 {
	/* most frequent access first */
	unsigned long iobase;
	struct pci_dev *pci;
	struct snd_sb *chip;
#ifdef SUPPORT_JOYSTICK
	struct gameport *gameport;
#endif
};

enum als4k_iobase_t {
	/* IOx: B == Byte, W = Word, D = DWord; SPECS_PAGE: 37 */
	ALS4K_IOD_00_AC97_ACCESS = 0x00,
	ALS4K_IOW_04_AC97_READ = 0x04,
	ALS4K_IOB_06_AC97_STATUS = 0x06,
	ALS4K_IOB_07_IRQSTATUS = 0x07,
	ALS4K_IOD_08_GCR_DATA = 0x08,
	ALS4K_IOB_0C_GCR_INDEX = 0x0c,
	ALS4K_IOB_0E_IRQTYPE_SB_CR1E_MPU = 0x0e,
	ALS4K_IOB_10_ADLIB_ADDR0 = 0x10,
	ALS4K_IOB_11_ADLIB_ADDR1 = 0x11,
	ALS4K_IOB_12_ADLIB_ADDR2 = 0x12,
	ALS4K_IOB_13_ADLIB_ADDR3 = 0x13,
	ALS4K_IOB_14_MIXER_INDEX = 0x14,
	ALS4K_IOB_15_MIXER_DATA = 0x15,
	ALS4K_IOB_16_ESP_RESET = 0x16,
	ALS4K_IOB_16_ACK_FOR_CR1E = 0x16, /* 2nd function */
	ALS4K_IOB_18_OPL_ADDR0 = 0x18,
	ALS4K_IOB_19_OPL_ADDR1 = 0x19,
	ALS4K_IOB_1A_ESP_RD_DATA = 0x1a,
	ALS4K_IOB_1C_ESP_CMD_DATA = 0x1c,
	ALS4K_IOB_1C_ESP_WR_STATUS = 0x1c, /* 2nd function */
	ALS4K_IOB_1E_ESP_RD_STATUS8 = 0x1e,
	ALS4K_IOB_1F_ESP_RD_STATUS16 = 0x1f,
	ALS4K_IOB_20_ESP_GAMEPORT_200 = 0x20,
	ALS4K_IOB_21_ESP_GAMEPORT_201 = 0x21,
	ALS4K_IOB_30_MIDI_DATA = 0x30,
	ALS4K_IOB_31_MIDI_STATUS = 0x31,
	ALS4K_IOB_31_MIDI_COMMAND = 0x31, /* 2nd function */
};

enum als4k_iobase_0e_t {
	ALS4K_IOB_0E_MPU_IRQ = 0x10,
	ALS4K_IOB_0E_CR1E_IRQ = 0x40,
	ALS4K_IOB_0E_SB_DMA_IRQ = 0x80,
};

enum als4k_gcr_t { /* all registers 32bit wide; SPECS_PAGE: 38 to 42 */
	ALS4K_GCR8C_MISC_CTRL = 0x8c,
	ALS4K_GCR90_TEST_MODE_REG = 0x90,
	ALS4K_GCR91_DMA0_ADDR = 0x91,
	ALS4K_GCR92_DMA0_MODE_COUNT = 0x92,
	ALS4K_GCR93_DMA1_ADDR = 0x93,
	ALS4K_GCR94_DMA1_MODE_COUNT = 0x94,
	ALS4K_GCR95_DMA3_ADDR = 0x95,
	ALS4K_GCR96_DMA3_MODE_COUNT = 0x96,
	ALS4K_GCR99_DMA_EMULATION_CTRL = 0x99,
	ALS4K_GCRA0_FIFO1_CURRENT_ADDR = 0xa0,
	ALS4K_GCRA1_FIFO1_STATUS_BYTECOUNT = 0xa1,
	ALS4K_GCRA2_FIFO2_PCIADDR = 0xa2,
	ALS4K_GCRA3_FIFO2_COUNT = 0xa3,
	ALS4K_GCRA4_FIFO2_CURRENT_ADDR = 0xa4,
	ALS4K_GCRA5_FIFO1_STATUS_BYTECOUNT = 0xa5,
	ALS4K_GCRA6_PM_CTRL = 0xa6,
	ALS4K_GCRA7_PCI_ACCESS_STORAGE = 0xa7,
	ALS4K_GCRA8_LEGACY_CFG1 = 0xa8,
	ALS4K_GCRA9_LEGACY_CFG2 = 0xa9,
	ALS4K_GCRFF_DUMMY_SCRATCH = 0xff,
};

enum als4k_gcr8c_t {
	ALS4K_GCR8C_IRQ_MASK_CTRL_ENABLE = 0x8000,
	ALS4K_GCR8C_CHIP_REV_MASK = 0xf0000
};

static inline void snd_als4k_iobase_writeb(unsigned long iobase,
						enum als4k_iobase_t reg,
						u8 val)
{
	outb(val, iobase + reg);
}

static inline void snd_als4k_iobase_writel(unsigned long iobase,
						enum als4k_iobase_t reg,
						u32 val)
{
	outl(val, iobase + reg);
}

static inline u8 snd_als4k_iobase_readb(unsigned long iobase,
						enum als4k_iobase_t reg)
{
	return inb(iobase + reg);
}

static inline u32 snd_als4k_iobase_readl(unsigned long iobase,
						enum als4k_iobase_t reg)
{
	return inl(iobase + reg);
}

static inline u32 snd_als4k_gcr_read_addr(unsigned long iobase,
						 enum als4k_gcr_t reg)
{
	/* SPECS_PAGE: 37/38 */
	snd_als4k_iobase_writeb(iobase, ALS4K_IOB_0C_GCR_INDEX, reg);
	return snd_als4k_iobase_readl(iobase, ALS4K_IOD_08_GCR_DATA);
}

static inline u32 snd_als4k_gcr_read(struct snd_sb *sb, enum als4k_gcr_t reg)
{
	return snd_als4k_gcr_read_addr(sb->alt_port, reg);
}

static inline void snd_als4k_gcr_write_addr(unsigned long iobase,
						 enum als4k_gcr_t reg,
						 u32 val)
{
	snd_als4k_iobase_writeb(iobase, ALS4K_IOB_0C_GCR_INDEX, reg);
	snd_als4k_iobase_writel(iobase, ALS4K_IOD_08_GCR_DATA, val);
	//printk("als wr addr %X : %X\n", reg, val);
	//printk("als %X wrote %X read %X\n", reg, val, snd_als4k_gcr_read_addr(iobase, reg));
}

static inline void snd_als4k_gcr_write(struct snd_sb *sb,
					 enum als4k_gcr_t reg,
					 u32 val)
{
	snd_als4k_gcr_write_addr(sb->alt_port, reg, val);
}	

enum als4k_cr_t { /* all registers 8bit wide; SPECS_PAGE: 20 to 23 */
	ALS4K_CR0_SB_CONFIG = 0x00,
	ALS4K_CR2_MISC_CONTROL = 0x02,
	ALS4K_CR3_CONFIGURATION = 0x03,
	ALS4K_CR17_FIFO_STATUS = 0x17,
	ALS4K_CR18_ESP_MAJOR_VERSION = 0x18,
	ALS4K_CR19_ESP_MINOR_VERSION = 0x19,
	ALS4K_CR1A_MPU401_UART_MODE_CONTROL = 0x1a,
	ALS4K_CR1C_FIFO2_BLOCK_LENGTH_LO = 0x1c,
	ALS4K_CR1D_FIFO2_BLOCK_LENGTH_HI = 0x1d,
	ALS4K_CR1E_FIFO2_CONTROL = 0x1e, /* secondary PCM FIFO (recording) */
	ALS4K_CR3A_MISC_CONTROL = 0x3a,
	ALS4K_CR3B_CRC32_BYTE0 = 0x3b, /* for testing, activate via CR3A */
	ALS4K_CR3C_CRC32_BYTE1 = 0x3c,
	ALS4K_CR3D_CRC32_BYTE2 = 0x3d,
	ALS4K_CR3E_CRC32_BYTE3 = 0x3e,
};

enum als4k_cr0_t {
	ALS4K_CR0_DMA_CONTIN_MODE_CTRL = 0x02, /* IRQ/FIFO controlled for 0/1 */
	ALS4K_CR0_DMA_90H_MODE_CTRL = 0x04, /* IRQ/FIFO controlled for 0/1 */
	ALS4K_CR0_MX80_81_REG_WRITE_ENABLE = 0x80,
};

static inline void snd_als4_cr_write(struct snd_sb *chip,
					enum als4k_cr_t reg,
					u8 data)
{
	/* Control Register is reg | 0xc0 (bit 7, 6 set) on sbmixer_index
	 * NOTE: assumes chip->mixer_lock to be locked externally already!
	 * SPECS_PAGE: 6 */
	snd_sbmixer_write(chip, reg | 0xc0, data);
}

static inline u8 snd_als4_cr_read(struct snd_sb *chip,
					enum als4k_cr_t reg)
{
	/* NOTE: assumes chip->mixer_lock to be locked externally already! */
	return snd_sbmixer_read(chip, reg | 0xc0);
}

#endif
