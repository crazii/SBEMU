#ifndef __SOUND_EMU10K1_H
#define __SOUND_EMU10K1_H

/*
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>,
 *		     Creative Labs, Inc.
 *  Definitions for EMU10K1 (SB Live!) chips
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef PCI_VENDOR_ID_CREATIVE
#define PCI_VENDOR_ID_CREATIVE		0x1102
#endif
#ifndef PCI_DEVICE_ID_CREATIVE_EMU10K1
#define PCI_DEVICE_ID_CREATIVE_EMU10K1	0x0002
#endif

#define PCI_DEVICE_ID_CREATIVE_AUDIGY  0x0004

/* ------------------- DEFINES -------------------- */

#define EMUPAGESIZE     4096
#define MAXREQVOICES    8
#define MAXPAGES        8192
#define RESERVED        0
#define NUM_MIDI        16
#define NUM_G           64              /* use all channels */
#define NUM_FXSENDS     4
#define NUM_EFX_PLAYBACK    16

/* FIXME? - according to the OSS driver the EMU10K1 needs a 29 bit DMA mask */
#define EMU10K1_DMA_MASK	0x7fffffffUL	/* 31bit */
#define AUDIGY_DMA_MASK		0xffffffffUL	/* 32bit */

#define TMEMSIZE        256*1024
#define TMEMSIZEREG     4

#define IP_TO_CP(ip) ((ip == 0) ? 0 : (((0x00001000uL | (ip & 0x00000FFFL)) << (((ip >> 12) & 0x000FL) + 4)) & 0xFFFF0000uL))

// Audigy specify registers are prefixed with 'A_'

/************************************************************************************************/
/* PCI function 0 registers, address = <val> + PCIBASE0						*/
/************************************************************************************************/

#define PTR			0x00		/* Indexed register set pointer register	*/
						/* NOTE: The CHANNELNUM and ADDRESS words can	*/
						/* be modified independently of each other.	*/
#define PTR_CHANNELNUM_MASK	0x0000003f	/* For each per-channel register, indicates the	*/
						/* channel number of the register to be		*/
						/* accessed.  For non per-channel registers the	*/
						/* value should be set to zero.			*/
#define PTR_ADDRESS_MASK	0x07ff0000	/* Register index				*/
#define A_PTR_ADDRESS_MASK	0x0fff0000

#define DATA			0x04		/* Indexed register set data register		*/

#define IPR			0x08		/* Global interrupt pending register		*/
						/* Clear pending interrupts by writing a 1 to	*/
						/* the relevant bits and zero to the other bits	*/

#define IPR_GPIOMSG		0x20000000	/* GPIO message interrupt (RE'd, still not sure 
						   which INTE bits enable it)			*/

/* The next two interrupts are for the midi port on the Audigy Drive (A_MPU1)			*/
#define IPR_A_MIDITRANSBUFEMPTY2 0x10000000	/* MIDI UART transmit buffer empty		*/
#define IPR_A_MIDIRECVBUFEMPTY2	0x08000000	/* MIDI UART receive buffer empty		*/

#define IPR_SPDIFBUFFULL	0x04000000	/* SPDIF capture related, 10k2 only? (RE)	*/
#define IPR_SPDIFBUFHALFFULL	0x02000000	/* SPDIF capture related? (RE)			*/

#define IPR_SAMPLERATETRACKER	0x01000000	/* Sample rate tracker lock status change	*/
#define IPR_FXDSP		0x00800000	/* Enable FX DSP interrupts			*/
#define IPR_FORCEINT		0x00400000	/* Force Sound Blaster interrupt		*/
#define IPR_PCIERROR		0x00200000	/* PCI bus error				*/
#define IPR_VOLINCR		0x00100000	/* Volume increment button pressed		*/
#define IPR_VOLDECR		0x00080000	/* Volume decrement button pressed		*/
#define IPR_MUTE		0x00040000	/* Mute button pressed				*/
#define IPR_MICBUFFULL		0x00020000	/* Microphone buffer full			*/
#define IPR_MICBUFHALFFULL	0x00010000	/* Microphone buffer half full			*/
#define IPR_ADCBUFFULL		0x00008000	/* ADC buffer full				*/
#define IPR_ADCBUFHALFFULL	0x00004000	/* ADC buffer half full				*/
#define IPR_EFXBUFFULL		0x00002000	/* Effects buffer full				*/
#define IPR_EFXBUFHALFFULL	0x00001000	/* Effects buffer half full			*/
#define IPR_GPSPDIFSTATUSCHANGE	0x00000800	/* GPSPDIF channel status change		*/
#define IPR_CDROMSTATUSCHANGE	0x00000400	/* CD-ROM channel status change			*/
#define IPR_INTERVALTIMER	0x00000200	/* Interval timer terminal count		*/
#define IPR_MIDITRANSBUFEMPTY	0x00000100	/* MIDI UART transmit buffer empty		*/
#define IPR_MIDIRECVBUFEMPTY	0x00000080	/* MIDI UART receive buffer empty		*/
#define IPR_CHANNELLOOP		0x00000040	/* Channel (half) loop interrupt(s) pending	*/
#define IPR_CHANNELNUMBERMASK	0x0000003f	/* When IPR_CHANNELLOOP is set, indicates the	*/
						/* highest set channel in CLIPL, CLIPH, HLIPL,  */
						/* or HLIPH.  When IP is written with CL set,	*/
						/* the bit in H/CLIPL or H/CLIPH corresponding	*/
						/* to the CIN value written will be cleared.	*/

#define INTE			0x0c		/* Interrupt enable register			*/
#define INTE_VIRTUALSB_MASK	0xc0000000	/* Virtual Soundblaster I/O port capture	*/
#define INTE_VIRTUALSB_220	0x00000000	/* Capture at I/O base address 0x220-0x22f	*/
#define INTE_VIRTUALSB_240	0x40000000	/* Capture at I/O base address 0x240		*/
#define INTE_VIRTUALSB_260	0x80000000	/* Capture at I/O base address 0x260		*/
#define INTE_VIRTUALSB_280	0xc0000000	/* Capture at I/O base address 0x280		*/
#define INTE_VIRTUALMPU_MASK	0x30000000	/* Virtual MPU I/O port capture			*/
#define INTE_VIRTUALMPU_300	0x00000000	/* Capture at I/O base address 0x300-0x301	*/
#define INTE_VIRTUALMPU_310	0x10000000	/* Capture at I/O base address 0x310		*/
#define INTE_VIRTUALMPU_320	0x20000000	/* Capture at I/O base address 0x320		*/
#define INTE_VIRTUALMPU_330	0x30000000	/* Capture at I/O base address 0x330		*/
#define INTE_MASTERDMAENABLE	0x08000000	/* Master DMA emulation at 0x000-0x00f		*/
#define INTE_SLAVEDMAENABLE	0x04000000	/* Slave DMA emulation at 0x0c0-0x0df		*/
#define INTE_MASTERPICENABLE	0x02000000	/* Master PIC emulation at 0x020-0x021		*/
#define INTE_SLAVEPICENABLE	0x01000000	/* Slave PIC emulation at 0x0a0-0x0a1		*/
#define INTE_VSBENABLE		0x00800000	/* Enable virtual Soundblaster			*/
#define INTE_ADLIBENABLE	0x00400000	/* Enable AdLib emulation at 0x388-0x38b	*/
#define INTE_MPUENABLE		0x00200000	/* Enable virtual MPU				*/
#define INTE_FORCEINT		0x00100000	/* Continuously assert INTAN			*/

#define INTE_MRHANDENABLE	0x00080000	/* Enable the "Mr. Hand" logic			*/
						/* NOTE: There is no reason to use this under	*/
						/* Linux, and it will cause odd hardware 	*/
						/* behavior and possibly random segfaults and	*/
						/* lockups if enabled.				*/

/* The next two interrupts are for the midi port on the Audigy Drive (A_MPU1)			*/
#define INTE_A_MIDITXENABLE2	0x00020000	/* Enable MIDI transmit-buffer-empty interrupts	*/
#define INTE_A_MIDIRXENABLE2	0x00010000	/* Enable MIDI receive-buffer-empty interrupts	*/


#define INTE_SAMPLERATETRACKER	0x00002000	/* Enable sample rate tracker interrupts	*/
						/* NOTE: This bit must always be enabled       	*/
#define INTE_FXDSPENABLE	0x00001000	/* Enable FX DSP interrupts			*/
#define INTE_PCIERRORENABLE	0x00000800	/* Enable PCI bus error interrupts		*/
#define INTE_VOLINCRENABLE	0x00000400	/* Enable volume increment button interrupts	*/
#define INTE_VOLDECRENABLE	0x00000200	/* Enable volume decrement button interrupts	*/
#define INTE_MUTEENABLE		0x00000100	/* Enable mute button interrupts		*/
#define INTE_MICBUFENABLE	0x00000080	/* Enable microphone buffer interrupts		*/
#define INTE_ADCBUFENABLE	0x00000040	/* Enable ADC buffer interrupts			*/
#define INTE_EFXBUFENABLE	0x00000020	/* Enable Effects buffer interrupts		*/
#define INTE_GPSPDIFENABLE	0x00000010	/* Enable GPSPDIF status interrupts		*/
#define INTE_CDSPDIFENABLE	0x00000008	/* Enable CDSPDIF status interrupts		*/
#define INTE_INTERVALTIMERENB	0x00000004	/* Enable interval timer interrupts		*/
#define INTE_MIDITXENABLE	0x00000002	/* Enable MIDI transmit-buffer-empty interrupts	*/
#define INTE_MIDIRXENABLE	0x00000001	/* Enable MIDI receive-buffer-empty interrupts	*/

#define WC			0x10		/* Wall Clock register				*/
#define WC_SAMPLECOUNTER_MASK	0x03FFFFC0	/* Sample periods elapsed since reset		*/
#define WC_SAMPLECOUNTER	0x14060010
#define WC_CURRENTCHANNEL	0x0000003F	/* Channel [0..63] currently being serviced	*/
						/* NOTE: Each channel takes 1/64th of a sample	*/
						/* period to be serviced.			*/

#define HCFG			0x14		/* Hardware config register			*/
						/* NOTE: There is no reason to use the legacy	*/
						/* SoundBlaster emulation stuff described below	*/
						/* under Linux, and all kinds of weird hardware	*/
						/* behavior can result if you try.  Don't.	*/
#define HCFG_LEGACYFUNC_MASK	0xe0000000	/* Legacy function number 			*/
#define HCFG_LEGACYFUNC_MPU	0x00000000	/* Legacy MPU	 				*/
#define HCFG_LEGACYFUNC_SB	0x40000000	/* Legacy SB					*/
#define HCFG_LEGACYFUNC_AD	0x60000000	/* Legacy AD					*/
#define HCFG_LEGACYFUNC_MPIC	0x80000000	/* Legacy MPIC					*/
#define HCFG_LEGACYFUNC_MDMA	0xa0000000	/* Legacy MDMA					*/
#define HCFG_LEGACYFUNC_SPCI	0xc0000000	/* Legacy SPCI					*/
#define HCFG_LEGACYFUNC_SDMA	0xe0000000	/* Legacy SDMA					*/
#define HCFG_IOCAPTUREADDR	0x1f000000	/* The 4 LSBs of the captured I/O address.	*/
#define HCFG_LEGACYWRITE	0x00800000	/* 1 = write, 0 = read 				*/
#define HCFG_LEGACYWORD		0x00400000	/* 1 = word, 0 = byte 				*/
#define HCFG_LEGACYINT		0x00200000	/* 1 = legacy event captured. Write 1 to clear.	*/
						/* NOTE: The rest of the bits in this register	*/
						/* _are_ relevant under Linux.			*/
#define HCFG_CODECFORMAT_MASK	0x00070000	/* CODEC format					*/
#define HCFG_CODECFORMAT_AC97	0x00000000	/* AC97 CODEC format -- Primary Output		*/
#define HCFG_CODECFORMAT_I2S	0x00010000	/* I2S CODEC format -- Secondary (Rear) Output	*/
#define HCFG_GPINPUT0		0x00004000	/* External pin112				*/
#define HCFG_GPINPUT1		0x00002000	/* External pin110				*/
#define HCFG_GPOUTPUT_MASK	0x00001c00	/* External pins which may be controlled	*/
#define HCFG_GPOUT0		0x00001000	/* External pin? (spdif enable on 5.1)		*/
#define HCFG_GPOUT1		0x00000800	/* External pin? (IR)				*/
#define HCFG_GPOUT2		0x00000400	/* External pin? (IR)				*/
#define HCFG_JOYENABLE      	0x00000200	/* Internal joystick enable    			*/
#define HCFG_PHASETRACKENABLE	0x00000100	/* Phase tracking enable			*/
						/* 1 = Force all 3 async digital inputs to use	*/
						/* the same async sample rate tracker (ZVIDEO)	*/
#define HCFG_AC3ENABLE_MASK	0x000000e0	/* AC3 async input control - Not implemented	*/
#define HCFG_AC3ENABLE_ZVIDEO	0x00000080	/* Channels 0 and 1 replace ZVIDEO		*/
#define HCFG_AC3ENABLE_CDSPDIF	0x00000040	/* Channels 0 and 1 replace CDSPDIF		*/
#define HCFG_AC3ENABLE_GPSPDIF  0x00000020      /* Channels 0 and 1 replace GPSPDIF             */
#define HCFG_AUTOMUTE		0x00000010	/* When set, the async sample rate convertors	*/
						/* will automatically mute their output when	*/
						/* they are not rate-locked to the external	*/
						/* async audio source  				*/
#define HCFG_LOCKSOUNDCACHE	0x00000008	/* 1 = Cancel bustmaster accesses to soundcache */
						/* NOTE: This should generally never be used.  	*/
#define HCFG_LOCKTANKCACHE_MASK	0x00000004	/* 1 = Cancel bustmaster accesses to tankcache	*/
						/* NOTE: This should generally never be used.  	*/
#define HCFG_LOCKTANKCACHE	0x01020014
#define HCFG_MUTEBUTTONENABLE	0x00000002	/* 1 = Master mute button sets AUDIOENABLE = 0.	*/
						/* NOTE: This is a 'cheap' way to implement a	*/
						/* master mute function on the mute button, and	*/
						/* in general should not be used unless a more	*/
						/* sophisticated master mute function has not	*/
						/* been written.       				*/
#define HCFG_AUDIOENABLE	0x00000001	/* 0 = CODECs transmit zero-valued samples	*/
						/* Should be set to 1 when the EMU10K1 is	*/
						/* completely initialized.			*/

//For Audigy, MPU port move to 0x70-0x74 ptr register

#define MUDATA			0x18		/* MPU401 data register (8 bits)       		*/

#define MUCMD			0x19		/* MPU401 command register (8 bits)    		*/
#define MUCMD_RESET		0xff		/* RESET command				*/
#define MUCMD_ENTERUARTMODE	0x3f		/* Enter_UART_mode command			*/
						/* NOTE: All other commands are ignored		*/

#define MUSTAT			MUCMD		/* MPU401 status register (8 bits)     		*/
#define MUSTAT_IRDYN		0x80		/* 0 = MIDI data or command ACK			*/
#define MUSTAT_ORDYN		0x40		/* 0 = MUDATA can accept a command or data	*/

#define A_IOCFG			0x18		/* GPIO on Audigy card (16bits)			*/
#define A_GPINPUT_MASK		0xff00
#define A_GPOUTPUT_MASK		0x00ff

// Audigy output/GPIO stuff taken from the kX drivers
#define A_IOCFG_GPOUT0		0x0044		/* analog/digital				*/
#define A_IOCFG_DISABLE_ANALOG	0x0040		/* = 'enable' for Audigy2 (chiprev=4)		*/
#define A_IOCFG_ENABLE_DIGITAL	0x0004
#define A_IOCFG_UNKNOWN_20      0x0020
#define A_IOCFG_DISABLE_AC97_FRONT      0x0080  /* turn off ac97 front -> front (10k2.1)	*/
#define A_IOCFG_GPOUT1		0x0002		/* IR? drive's internal bypass (?)		*/
#define A_IOCFG_GPOUT2		0x0001		/* IR */
#define A_IOCFG_MULTIPURPOSE_JACK	0x2000  /* center+lfe+rear_center (a2/a2ex)		*/
                                                /* + digital for generic 10k2			*/
#define A_IOCFG_DIGITAL_JACK    0x1000          /* digital for a2 platinum			*/
#define A_IOCFG_FRONT_JACK      0x4000
#define A_IOCFG_REAR_JACK       0x8000
#define A_IOCFG_PHONES_JACK     0x0100          /* LiveDrive					*/

/* outputs:
 *	for audigy2 platinum:	0xa00
 *	for a2 platinum ex:	0x1c00
 *	for a1 platinum:	0x0
 */

#define TIMER			0x1a		/* Timer terminal count register		*/
						/* NOTE: After the rate is changed, a maximum	*/
						/* of 1024 sample periods should be allowed	*/
						/* before the new rate is guaranteed accurate.	*/
#define TIMER_RATE_MASK		0x000003ff	/* Timer interrupt rate in sample periods	*/
						/* 0 == 1024 periods, [1..4] are not useful	*/
#define TIMER_RATE		0x0a00001a

#define AC97DATA		0x1c		/* AC97 register set data register (16 bit)	*/

#define AC97ADDRESS		0x1e		/* AC97 register set address register (8 bit)	*/
#define AC97ADDRESS_READY	0x80		/* Read-only bit, reflects CODEC READY signal	*/
#define AC97ADDRESS_ADDRESS	0x7f		/* Address of indexed AC97 register		*/

/* Available on the Audigy 2 and Audigy 4 only. This is the P16V chip. */
#define PTR2			0x20		/* Indexed register set pointer register	*/
#define DATA2			0x24		/* Indexed register set data register		*/
#define IPR2			0x28		/* P16V interrupt pending register		*/
#define IPR2_PLAYBACK_CH_0_LOOP      0x00001000 /* Playback Channel 0 loop                               */
#define IPR2_PLAYBACK_CH_0_HALF_LOOP 0x00000100 /* Playback Channel 0 half loop                          */
#define IPR2_CAPTURE_CH_0_LOOP       0x00100000 /* Capture Channel 0 loop                               */
#define IPR2_CAPTURE_CH_0_HALF_LOOP  0x00010000 /* Capture Channel 0 half loop                          */
						/* 0x00000100 Playback. Only in once per period.
						 * 0x00110000 Capture. Int on half buffer.
						 */
#define INTE2			0x2c		/* P16V Interrupt enable register. 	*/
#define INTE2_PLAYBACK_CH_0_LOOP      0x00001000 /* Playback Channel 0 loop                               */
#define INTE2_PLAYBACK_CH_0_HALF_LOOP 0x00000100 /* Playback Channel 0 half loop                          */
#define INTE2_PLAYBACK_CH_1_LOOP      0x00002000 /* Playback Channel 1 loop                               */
#define INTE2_PLAYBACK_CH_1_HALF_LOOP 0x00000200 /* Playback Channel 1 half loop                          */
#define INTE2_PLAYBACK_CH_2_LOOP      0x00004000 /* Playback Channel 2 loop                               */
#define INTE2_PLAYBACK_CH_2_HALF_LOOP 0x00000400 /* Playback Channel 2 half loop                          */
#define INTE2_PLAYBACK_CH_3_LOOP      0x00008000 /* Playback Channel 3 loop                               */
#define INTE2_PLAYBACK_CH_3_HALF_LOOP 0x00000800 /* Playback Channel 3 half loop                          */
#define INTE2_CAPTURE_CH_0_LOOP       0x00100000 /* Capture Channel 0 loop                               */
#define INTE2_CAPTURE_CH_0_HALF_LOOP  0x00010000 /* Caputre Channel 0 half loop                          */
#define HCFG2			0x34		/* Defaults: 0, win2000 sets it to 00004201 */
						/* 0x00000000 2-channel output. */
						/* 0x00000200 8-channel output. */
						/* 0x00000004 pauses stream/irq fail. */
						/* Rest of bits no nothing to sound output */
						/* bit 0: Enable P16V audio.
						 * bit 1: Lock P16V record memory cache.
						 * bit 2: Lock P16V playback memory cache.
						 * bit 3: Dummy record insert zero samples.
						 * bit 8: Record 8-channel in phase.
						 * bit 9: Playback 8-channel in phase.
						 * bit 11-12: Playback mixer attenuation: 0=0dB, 1=-6dB, 2=-12dB, 3=Mute.
						 * bit 13: Playback mixer enable.
						 * bit 14: Route SRC48 mixer output to fx engine.
						 * bit 15: Enable IEEE 1394 chip.
						 */
#define IPR3			0x38		/* Cdif interrupt pending register		*/
#define INTE3			0x3c		/* Cdif interrupt enable register. 	*/
/************************************************************************************************/
/* PCI function 1 registers, address = <val> + PCIBASE1						*/
/************************************************************************************************/

#define JOYSTICK1		0x00		/* Analog joystick port register		*/
#define JOYSTICK2		0x01		/* Analog joystick port register		*/
#define JOYSTICK3		0x02		/* Analog joystick port register		*/
#define JOYSTICK4		0x03		/* Analog joystick port register		*/
#define JOYSTICK5		0x04		/* Analog joystick port register		*/
#define JOYSTICK6		0x05		/* Analog joystick port register		*/
#define JOYSTICK7		0x06		/* Analog joystick port register		*/
#define JOYSTICK8		0x07		/* Analog joystick port register		*/

/* When writing, any write causes JOYSTICK_COMPARATOR output enable to be pulsed on write.	*/
/* When reading, use these bitfields: */
#define JOYSTICK_BUTTONS	0x0f		/* Joystick button data				*/
#define JOYSTICK_COMPARATOR	0xf0		/* Joystick comparator data			*/


/********************************************************************************************************/
/* Emu10k1 pointer-offset register set, accessed through the PTR and DATA registers			*/
/********************************************************************************************************/

#define CPF			0x00		/* Current pitch and fraction register			*/
#define CPF_CURRENTPITCH_MASK	0xffff0000	/* Current pitch (linear, 0x4000 == unity pitch shift) 	*/
#define CPF_CURRENTPITCH	0x10100000
#define CPF_STEREO_MASK		0x00008000	/* 1 = Even channel interleave, odd channel locked	*/
#define CPF_STOP_MASK		0x00004000	/* 1 = Current pitch forced to 0			*/
#define CPF_FRACADDRESS_MASK	0x00003fff	/* Linear fractional address of the current channel	*/

#define PTRX			0x01		/* Pitch target and send A/B amounts register		*/
#define PTRX_PITCHTARGET_MASK	0xffff0000	/* Pitch target of specified channel			*/
#define PTRX_PITCHTARGET	0x10100001
#define PTRX_FXSENDAMOUNT_A_MASK 0x0000ff00	/* Linear level of channel output sent to FX send bus A	*/
#define PTRX_FXSENDAMOUNT_A	0x08080001
#define PTRX_FXSENDAMOUNT_B_MASK 0x000000ff	/* Linear level of channel output sent to FX send bus B	*/
#define PTRX_FXSENDAMOUNT_B	0x08000001

#define CVCF			0x02		/* Current volume and filter cutoff register		*/
#define CVCF_CURRENTVOL_MASK	0xffff0000	/* Current linear volume of specified channel		*/
#define CVCF_CURRENTVOL		0x10100002
#define CVCF_CURRENTFILTER_MASK	0x0000ffff	/* Current filter cutoff frequency of specified channel	*/
#define CVCF_CURRENTFILTER	0x10000002

#define VTFT			0x03		/* Volume target and filter cutoff target register	*/
#define VTFT_VOLUMETARGET_MASK	0xffff0000	/* Volume target of specified channel			*/
#define VTFT_VOLUMETARGET	0x10100003
#define VTFT_FILTERTARGET_MASK	0x0000ffff	/* Filter cutoff target of specified channel		*/
#define VTFT_FILTERTARGET	0x10000003

#define Z1			0x05		/* Filter delay memory 1 register			*/

#define Z2			0x04		/* Filter delay memory 2 register			*/

#define PSST			0x06		/* Send C amount and loop start address register	*/
#define PSST_FXSENDAMOUNT_C_MASK 0xff000000	/* Linear level of channel output sent to FX send bus C	*/

#define PSST_FXSENDAMOUNT_C	0x08180006

#define PSST_LOOPSTARTADDR_MASK	0x00ffffff	/* Loop start address of the specified channel		*/
#define PSST_LOOPSTARTADDR	0x18000006

#define DSL			0x07		/* Send D amount and loop start address register	*/
#define DSL_FXSENDAMOUNT_D_MASK	0xff000000	/* Linear level of channel output sent to FX send bus D	*/

#define DSL_FXSENDAMOUNT_D	0x08180007

#define DSL_LOOPENDADDR_MASK	0x00ffffff	/* Loop end address of the specified channel		*/
#define DSL_LOOPENDADDR		0x18000007

#define CCCA			0x08		/* Filter Q, interp. ROM, byte size, cur. addr register */
#define CCCA_RESONANCE		0xf0000000	/* Lowpass filter resonance (Q) height			*/
#define CCCA_INTERPROMMASK	0x0e000000	/* Selects passband of interpolation ROM		*/
						/* 1 == full band, 7 == lowpass				*/
						/* ROM 0 is used when pitch shifting downward or less	*/
						/* then 3 semitones upward.  Increasingly higher ROM	*/
						/* numbers are used, typically in steps of 3 semitones,	*/
						/* as upward pitch shifting is performed.		*/
#define CCCA_INTERPROM_0	0x00000000	/* Select interpolation ROM 0				*/
#define CCCA_INTERPROM_1	0x02000000	/* Select interpolation ROM 1				*/
#define CCCA_INTERPROM_2	0x04000000	/* Select interpolation ROM 2				*/
#define CCCA_INTERPROM_3	0x06000000	/* Select interpolation ROM 3				*/
#define CCCA_INTERPROM_4	0x08000000	/* Select interpolation ROM 4				*/
#define CCCA_INTERPROM_5	0x0a000000	/* Select interpolation ROM 5				*/
#define CCCA_INTERPROM_6	0x0c000000	/* Select interpolation ROM 6				*/
#define CCCA_INTERPROM_7	0x0e000000	/* Select interpolation ROM 7				*/
#define CCCA_8BITSELECT		0x01000000	/* 1 = Sound memory for this channel uses 8-bit samples	*/
#define CCCA_CURRADDR_MASK	0x00ffffff	/* Current address of the selected channel		*/
#define CCCA_CURRADDR		0x18000008

#define CCR			0x09		/* Cache control register				*/
#define CCR_CACHEINVALIDSIZE	0x07190009
#define CCR_CACHEINVALIDSIZE_MASK	0xfe000000	/* Number of invalid samples cache for this channel    	*/
#define CCR_CACHELOOPFLAG	0x01000000	/* 1 = Cache has a loop service pending			*/
#define CCR_INTERLEAVEDSAMPLES	0x00800000	/* 1 = A cache service will fetch interleaved samples	*/
#define CCR_WORDSIZEDSAMPLES	0x00400000	/* 1 = A cache service will fetch word sized samples	*/
#define CCR_READADDRESS		0x06100009
#define CCR_READADDRESS_MASK	0x003f0000	/* Location of cache just beyond current cache service	*/
#define CCR_LOOPINVALSIZE	0x0000fe00	/* Number of invalid samples in cache prior to loop	*/
						/* NOTE: This is valid only if CACHELOOPFLAG is set	*/
#define CCR_LOOPFLAG		0x00000100	/* Set for a single sample period when a loop occurs	*/
#define CCR_CACHELOOPADDRHI	0x000000ff	/* DSL_LOOPSTARTADDR's hi byte if CACHELOOPFLAG is set	*/

#define CLP			0x0a		/* Cache loop register (valid if CCR_CACHELOOPFLAG = 1) */
						/* NOTE: This register is normally not used		*/
#define CLP_CACHELOOPADDR	0x0000ffff	/* Cache loop address (DSL_LOOPSTARTADDR [0..15])	*/

#define FXRT			0x0b		/* Effects send routing register			*/
						/* NOTE: It is illegal to assign the same routing to	*/
						/* two effects sends.					*/
#define FXRT_CHANNELA		0x000f0000	/* Effects send bus number for channel's effects send A	*/
#define FXRT_CHANNELB		0x00f00000	/* Effects send bus number for channel's effects send B	*/
#define FXRT_CHANNELC		0x0f000000	/* Effects send bus number for channel's effects send C	*/
#define FXRT_CHANNELD		0xf0000000	/* Effects send bus number for channel's effects send D	*/

#define MAPA			0x0c		/* Cache map A						*/

#define MAPB			0x0d		/* Cache map B						*/

#define MAP_PTE_MASK		0xffffe000	/* The 19 MSBs of the PTE indexed by the PTI		*/
#define MAP_PTI_MASK		0x00001fff	/* The 13 bit index to one of the 8192 PTE dwords      	*/

#define ENVVOL			0x10		/* Volume envelope register				*/
#define ENVVOL_MASK		0x0000ffff	/* Current value of volume envelope state variable	*/  
						/* 0x8000-n == 666*n usec delay	       			*/

#define ATKHLDV 		0x11		/* Volume envelope hold and attack register		*/
#define ATKHLDV_PHASE0		0x00008000	/* 0 = Begin attack phase				*/
#define ATKHLDV_HOLDTIME_MASK	0x00007f00	/* Envelope hold time (127-n == n*88.2msec)		*/
#define ATKHLDV_ATTACKTIME_MASK	0x0000007f	/* Envelope attack time, log encoded			*/
						/* 0 = infinite, 1 = 10.9msec, ... 0x7f = 5.5msec	*/

#define DCYSUSV 		0x12		/* Volume envelope sustain and decay register		*/
#define DCYSUSV_PHASE1_MASK	0x00008000	/* 0 = Begin attack phase, 1 = begin release phase	*/
#define DCYSUSV_SUSTAINLEVEL_MASK 0x00007f00	/* 127 = full, 0 = off, 0.75dB increments		*/
#define DCYSUSV_CHANNELENABLE_MASK 0x00000080	/* 1 = Inhibit envelope engine from writing values in	*/
						/* this channel and from writing to pitch, filter and	*/
						/* volume targets.					*/
#define DCYSUSV_DECAYTIME_MASK	0x0000007f	/* Volume envelope decay time, log encoded     		*/
						/* 0 = 43.7msec, 1 = 21.8msec, 0x7f = 22msec		*/

#define LFOVAL1 		0x13		/* Modulation LFO value					*/
#define LFOVAL_MASK		0x0000ffff	/* Current value of modulation LFO state variable	*/
						/* 0x8000-n == 666*n usec delay				*/

#define ENVVAL			0x14		/* Modulation envelope register				*/
#define ENVVAL_MASK		0x0000ffff	/* Current value of modulation envelope state variable 	*/
						/* 0x8000-n == 666*n usec delay				*/

#define ATKHLDM			0x15		/* Modulation envelope hold and attack register		*/
#define ATKHLDM_PHASE0		0x00008000	/* 0 = Begin attack phase				*/
#define ATKHLDM_HOLDTIME	0x00007f00	/* Envelope hold time (127-n == n*42msec)		*/
#define ATKHLDM_ATTACKTIME	0x0000007f	/* Envelope attack time, log encoded			*/
						/* 0 = infinite, 1 = 11msec, ... 0x7f = 5.5msec		*/

#define DCYSUSM			0x16		/* Modulation envelope decay and sustain register	*/
#define DCYSUSM_PHASE1_MASK	0x00008000	/* 0 = Begin attack phase, 1 = begin release phase	*/
#define DCYSUSM_SUSTAINLEVEL_MASK 0x00007f00	/* 127 = full, 0 = off, 0.75dB increments		*/
#define DCYSUSM_DECAYTIME_MASK	0x0000007f	/* Envelope decay time, log encoded			*/
						/* 0 = 43.7msec, 1 = 21.8msec, 0x7f = 22msec		*/

#define LFOVAL2 		0x17		/* Vibrato LFO register					*/
#define LFOVAL2_MASK		0x0000ffff	/* Current value of vibrato LFO state variable 		*/
						/* 0x8000-n == 666*n usec delay				*/

#define IP			0x18		/* Initial pitch register				*/
#define IP_MASK			0x0000ffff	/* Exponential initial pitch shift			*/
						/* 4 bits of octave, 12 bits of fractional octave	*/
#define IP_UNITY		0x0000e000	/* Unity pitch shift					*/

#define IFATN			0x19		/* Initial filter cutoff and attenuation register	*/
#define IFATN_FILTERCUTOFF_MASK	0x0000ff00	/* Initial filter cutoff frequency in exponential units	*/
						/* 6 most significant bits are semitones		*/
						/* 2 least significant bits are fractions		*/
#define IFATN_FILTERCUTOFF	0x08080019
#define IFATN_ATTENUATION_MASK	0x000000ff	/* Initial attenuation in 0.375dB steps			*/
#define IFATN_ATTENUATION	0x08000019


#define PEFE			0x1a		/* Pitch envelope and filter envelope amount register	*/
#define PEFE_PITCHAMOUNT_MASK	0x0000ff00	/* Pitch envlope amount					*/
						/* Signed 2's complement, +/- one octave peak extremes	*/
#define PEFE_PITCHAMOUNT	0x0808001a
#define PEFE_FILTERAMOUNT_MASK	0x000000ff	/* Filter envlope amount				*/
						/* Signed 2's complement, +/- six octaves peak extremes */
#define PEFE_FILTERAMOUNT	0x0800001a
#define FMMOD			0x1b		/* Vibrato/filter modulation from LFO register		*/
#define FMMOD_MODVIBRATO	0x0000ff00	/* Vibrato LFO modulation depth				*/
						/* Signed 2's complement, +/- one octave extremes	*/
#define FMMOD_MOFILTER		0x000000ff	/* Filter LFO modulation depth				*/
						/* Signed 2's complement, +/- three octave extremes	*/


#define TREMFRQ 		0x1c		/* Tremolo amount and modulation LFO frequency register	*/
#define TREMFRQ_DEPTH		0x0000ff00	/* Tremolo depth					*/
						/* Signed 2's complement, with +/- 12dB extremes	*/

#define TREMFRQ_FREQUENCY	0x000000ff	/* Tremolo LFO frequency				*/
						/* ??Hz steps, maximum of ?? Hz.			*/
#define FM2FRQ2 		0x1d		/* Vibrato amount and vibrato LFO frequency register	*/
#define FM2FRQ2_DEPTH		0x0000ff00	/* Vibrato LFO vibrato depth				*/
						/* Signed 2's complement, +/- one octave extremes	*/
#define FM2FRQ2_FREQUENCY	0x000000ff	/* Vibrato LFO frequency				*/
						/* 0.039Hz steps, maximum of 9.85 Hz.			*/

#define TEMPENV 		0x1e		/* Tempory envelope register				*/
#define TEMPENV_MASK		0x0000ffff	/* 16-bit value						*/
						/* NOTE: All channels contain internal variables; do	*/
						/* not write to these locations.			*/

/* 1f something */

#define CD0			0x20		/* Cache data 0 register				*/
#define CD1			0x21		/* Cache data 1 register				*/
#define CD2			0x22		/* Cache data 2 register				*/
#define CD3			0x23		/* Cache data 3 register				*/
#define CD4			0x24		/* Cache data 4 register				*/
#define CD5			0x25		/* Cache data 5 register				*/
#define CD6			0x26		/* Cache data 6 register				*/
#define CD7			0x27		/* Cache data 7 register				*/
#define CD8			0x28		/* Cache data 8 register				*/
#define CD9			0x29		/* Cache data 9 register				*/
#define CDA			0x2a		/* Cache data A register				*/
#define CDB			0x2b		/* Cache data B register				*/
#define CDC			0x2c		/* Cache data C register				*/
#define CDD			0x2d		/* Cache data D register				*/
#define CDE			0x2e		/* Cache data E register				*/
#define CDF			0x2f		/* Cache data F register				*/

/* 0x30-3f seem to be the same as 0x20-2f */

#define PTB			0x40		/* Page table base register				*/
#define PTB_MASK		0xfffff000	/* Physical address of the page table in host memory	*/

#define TCB			0x41		/* Tank cache base register    				*/
#define TCB_MASK		0xfffff000	/* Physical address of the bottom of host based TRAM	*/

#define ADCCR			0x42		/* ADC sample rate/stereo control register		*/
#define ADCCR_RCHANENABLE	0x00000010	/* Enables right channel for writing to the host       	*/
#define ADCCR_LCHANENABLE	0x00000008	/* Enables left channel for writing to the host		*/
						/* NOTE: To guarantee phase coherency, both channels	*/
						/* must be disabled prior to enabling both channels.	*/
#define A_ADCCR_RCHANENABLE	0x00000020
#define A_ADCCR_LCHANENABLE	0x00000010

#define A_ADCCR_SAMPLERATE_MASK 0x0000000F      /* Audigy sample rate convertor output rate		*/
#define ADCCR_SAMPLERATE_MASK	0x00000007	/* Sample rate convertor output rate			*/
#define ADCCR_SAMPLERATE_48	0x00000000	/* 48kHz sample rate					*/
#define ADCCR_SAMPLERATE_44	0x00000001	/* 44.1kHz sample rate					*/
#define ADCCR_SAMPLERATE_32	0x00000002	/* 32kHz sample rate					*/
#define ADCCR_SAMPLERATE_24	0x00000003	/* 24kHz sample rate					*/
#define ADCCR_SAMPLERATE_22	0x00000004	/* 22.05kHz sample rate					*/
#define ADCCR_SAMPLERATE_16	0x00000005	/* 16kHz sample rate					*/
#define ADCCR_SAMPLERATE_11	0x00000006	/* 11.025kHz sample rate				*/
#define ADCCR_SAMPLERATE_8	0x00000007	/* 8kHz sample rate					*/
#define A_ADCCR_SAMPLERATE_12	0x00000006	/* 12kHz sample rate					*/
#define A_ADCCR_SAMPLERATE_11	0x00000007	/* 11.025kHz sample rate				*/
#define A_ADCCR_SAMPLERATE_8	0x00000008	/* 8kHz sample rate					*/

#define FXWC			0x43		/* FX output write channels register			*/
						/* When set, each bit enables the writing of the	*/
						/* corresponding FX output channel (internal registers  */
						/* 0x20-0x3f) to host memory.  This mode of recording   */
						/* is 16bit, 48KHz only. All 32 channels can be enabled */
						/* simultaneously.					*/

#define FXWC_DEFAULTROUTE_C     (1<<0)		/* left emu out? */
#define FXWC_DEFAULTROUTE_B     (1<<1)		/* right emu out? */
#define FXWC_DEFAULTROUTE_A     (1<<12)
#define FXWC_DEFAULTROUTE_D     (1<<13)
#define FXWC_ADCLEFT            (1<<18)
#define FXWC_CDROMSPDIFLEFT     (1<<18)
#define FXWC_ADCRIGHT           (1<<19)
#define FXWC_CDROMSPDIFRIGHT    (1<<19)
#define FXWC_MIC                (1<<20)
#define FXWC_ZOOMLEFT           (1<<20)
#define FXWC_ZOOMRIGHT          (1<<21)
#define FXWC_SPDIFLEFT          (1<<22)		/* 0x00400000 */
#define FXWC_SPDIFRIGHT         (1<<23)		/* 0x00800000 */

#define TCBS			0x44		/* Tank cache buffer size register			*/
#define TCBS_MASK		0x00000007	/* Tank cache buffer size field				*/
#define TCBS_BUFFSIZE_16K	0x00000000
#define TCBS_BUFFSIZE_32K	0x00000001
#define TCBS_BUFFSIZE_64K	0x00000002
#define TCBS_BUFFSIZE_128K	0x00000003
#define TCBS_BUFFSIZE_256K	0x00000004
#define TCBS_BUFFSIZE_512K	0x00000005
#define TCBS_BUFFSIZE_1024K	0x00000006
#define TCBS_BUFFSIZE_2048K	0x00000007

#define MICBA			0x45		/* AC97 microphone buffer address register		*/
#define MICBA_MASK		0xfffff000	/* 20 bit base address					*/

#define ADCBA			0x46		/* ADC buffer address register				*/
#define ADCBA_MASK		0xfffff000	/* 20 bit base address					*/

#define FXBA			0x47		/* FX Buffer Address */
#define FXBA_MASK		0xfffff000	/* 20 bit base address					*/

/* 0x48 something - word access, defaults to 3f */

#define MICBS			0x49		/* Microphone buffer size register			*/

#define ADCBS			0x4a		/* ADC buffer size register				*/

#define FXBS			0x4b		/* FX buffer size register				*/

/* register: 0x4c..4f: ffff-ffff current amounts, per-channel */

/* The following mask values define the size of the ADC, MIX and FX buffers in bytes */
#define ADCBS_BUFSIZE_NONE	0x00000000
#define ADCBS_BUFSIZE_384	0x00000001
#define ADCBS_BUFSIZE_448	0x00000002
#define ADCBS_BUFSIZE_512	0x00000003
#define ADCBS_BUFSIZE_640	0x00000004
#define ADCBS_BUFSIZE_768	0x00000005
#define ADCBS_BUFSIZE_896	0x00000006
#define ADCBS_BUFSIZE_1024	0x00000007
#define ADCBS_BUFSIZE_1280	0x00000008
#define ADCBS_BUFSIZE_1536	0x00000009
#define ADCBS_BUFSIZE_1792	0x0000000a
#define ADCBS_BUFSIZE_2048	0x0000000b
#define ADCBS_BUFSIZE_2560	0x0000000c
#define ADCBS_BUFSIZE_3072	0x0000000d
#define ADCBS_BUFSIZE_3584	0x0000000e
#define ADCBS_BUFSIZE_4096	0x0000000f
#define ADCBS_BUFSIZE_5120	0x00000010
#define ADCBS_BUFSIZE_6144	0x00000011
#define ADCBS_BUFSIZE_7168	0x00000012
#define ADCBS_BUFSIZE_8192	0x00000013
#define ADCBS_BUFSIZE_10240	0x00000014
#define ADCBS_BUFSIZE_12288	0x00000015
#define ADCBS_BUFSIZE_14366	0x00000016
#define ADCBS_BUFSIZE_16384	0x00000017
#define ADCBS_BUFSIZE_20480	0x00000018
#define ADCBS_BUFSIZE_24576	0x00000019
#define ADCBS_BUFSIZE_28672	0x0000001a
#define ADCBS_BUFSIZE_32768	0x0000001b
#define ADCBS_BUFSIZE_40960	0x0000001c
#define ADCBS_BUFSIZE_49152	0x0000001d
#define ADCBS_BUFSIZE_57344	0x0000001e
#define ADCBS_BUFSIZE_65536	0x0000001f


#define CDCS			0x50		/* CD-ROM digital channel status register	*/

#define GPSCS			0x51		/* General Purpose SPDIF channel status register*/

#define DBG			0x52		/* DO NOT PROGRAM THIS REGISTER!!! MAY DESTROY CHIP */

#define REG53			0x53		/* DO NOT PROGRAM THIS REGISTER!!! MAY DESTROY CHIP */

#define A_DBG			 0x53
#define A_DBG_SINGLE_STEP	 0x00020000	/* Set to zero to start dsp */
#define A_DBG_ZC		 0x40000000	/* zero tram counter */
#define A_DBG_STEP_ADDR		 0x000003ff
#define A_DBG_SATURATION_OCCURED 0x20000000
#define A_DBG_SATURATION_ADDR	 0x0ffc0000

// NOTE: 0x54,55,56: 64-bit
#define SPCS0			0x54		/* SPDIF output Channel Status 0 register	*/

#define SPCS1			0x55		/* SPDIF output Channel Status 1 register	*/

#define SPCS2			0x56		/* SPDIF output Channel Status 2 register	*/

#define SPCS_CLKACCYMASK	0x30000000	/* Clock accuracy				*/
#define SPCS_CLKACCY_1000PPM	0x00000000	/* 1000 parts per million			*/
#define SPCS_CLKACCY_50PPM	0x10000000	/* 50 parts per million				*/
#define SPCS_CLKACCY_VARIABLE	0x20000000	/* Variable accuracy				*/
#define SPCS_SAMPLERATEMASK	0x0f000000	/* Sample rate					*/
#define SPCS_SAMPLERATE_44	0x00000000	/* 44.1kHz sample rate				*/
#define SPCS_SAMPLERATE_48	0x02000000	/* 48kHz sample rate				*/
#define SPCS_SAMPLERATE_32	0x03000000	/* 32kHz sample rate				*/
#define SPCS_CHANNELNUMMASK	0x00f00000	/* Channel number				*/
#define SPCS_CHANNELNUM_UNSPEC	0x00000000	/* Unspecified channel number			*/
#define SPCS_CHANNELNUM_LEFT	0x00100000	/* Left channel					*/
#define SPCS_CHANNELNUM_RIGHT	0x00200000	/* Right channel				*/
#define SPCS_SOURCENUMMASK	0x000f0000	/* Source number				*/
#define SPCS_SOURCENUM_UNSPEC	0x00000000	/* Unspecified source number			*/
#define SPCS_GENERATIONSTATUS	0x00008000	/* Originality flag (see IEC-958 spec)		*/
#define SPCS_CATEGORYCODEMASK	0x00007f00	/* Category code (see IEC-958 spec)		*/
#define SPCS_MODEMASK		0x000000c0	/* Mode (see IEC-958 spec)			*/
#define SPCS_EMPHASISMASK	0x00000038	/* Emphasis					*/
#define SPCS_EMPHASIS_NONE	0x00000000	/* No emphasis					*/
#define SPCS_EMPHASIS_50_15	0x00000008	/* 50/15 usec 2 channel				*/
#define SPCS_COPYRIGHT		0x00000004	/* Copyright asserted flag -- do not modify	*/
#define SPCS_NOTAUDIODATA	0x00000002	/* 0 = Digital audio, 1 = not audio		*/
#define SPCS_PROFESSIONAL	0x00000001	/* 0 = Consumer (IEC-958), 1 = pro (AES3-1992)	*/

/* The 32-bit CLIx and SOLx registers all have one bit per channel control/status      		*/
#define CLIEL			0x58		/* Channel loop interrupt enable low register	*/

#define CLIEH			0x59		/* Channel loop interrupt enable high register	*/

#define CLIPL			0x5a		/* Channel loop interrupt pending low register	*/

#define CLIPH			0x5b		/* Channel loop interrupt pending high register	*/

#define SOLEL			0x5c		/* Stop on loop enable low register		*/

#define SOLEH			0x5d		/* Stop on loop enable high register		*/

#define SPBYPASS		0x5e		/* SPDIF BYPASS mode register			*/
#define SPBYPASS_SPDIF0_MASK	0x00000003	/* SPDIF 0 bypass mode				*/
#define SPBYPASS_SPDIF1_MASK	0x0000000c	/* SPDIF 1 bypass mode				*/
/* bypass mode: 0 - DSP; 1 - SPDIF A, 2 - SPDIF B, 3 - SPDIF C					*/
#define SPBYPASS_FORMAT		0x00000f00      /* If 1, SPDIF XX uses 24 bit, if 0 - 20 bit	*/

#define AC97SLOT		0x5f            /* additional AC97 slots enable bits		*/
#define AC97SLOT_REAR_RIGHT	0x01		/* Rear left */
#define AC97SLOT_REAR_LEFT	0x02		/* Rear right */
#define AC97SLOT_CNTR		0x10            /* Center enable */
#define AC97SLOT_LFE		0x20            /* LFE enable */

// NOTE: 0x60,61,62: 64-bit
#define CDSRCS			0x60		/* CD-ROM Sample Rate Converter status register	*/

#define GPSRCS			0x61		/* General Purpose SPDIF sample rate cvt status */

#define ZVSRCS			0x62		/* ZVideo sample rate converter status		*/
						/* NOTE: This one has no SPDIFLOCKED field	*/
						/* Assumes sample lock				*/

/* These three bitfields apply to CDSRCS, GPSRCS, and (except as noted) ZVSRCS.			*/
#define SRCS_SPDIFLOCKED	0x02000000	/* SPDIF stream locked				*/
#define SRCS_RATELOCKED		0x01000000	/* Sample rate locked				*/
#define SRCS_ESTSAMPLERATE	0x0007ffff	/* Do not modify this field.			*/

/* Note that these values can vary +/- by a small amount                                        */
#define SRCS_SPDIFRATE_44	0x0003acd9
#define SRCS_SPDIFRATE_48	0x00040000
#define SRCS_SPDIFRATE_96	0x00080000

#define MICIDX                  0x63            /* Microphone recording buffer index register   */
#define MICIDX_MASK             0x0000ffff      /* 16-bit value                                 */
#define MICIDX_IDX		0x10000063

#define ADCIDX			0x64		/* ADC recording buffer index register		*/
#define ADCIDX_MASK		0x0000ffff	/* 16 bit index field				*/
#define ADCIDX_IDX		0x10000064

#define A_ADCIDX		0x63
#define A_ADCIDX_IDX		0x10000063

#define A_MICIDX		0x64
#define A_MICIDX_IDX		0x10000064

#define FXIDX			0x65		/* FX recording buffer index register		*/
#define FXIDX_MASK		0x0000ffff	/* 16-bit value					*/
#define FXIDX_IDX		0x10000065

/* The 32-bit HLIx and HLIPx registers all have one bit per channel control/status      		*/
#define HLIEL			0x66		/* Channel half loop interrupt enable low register	*/

#define HLIEH			0x67		/* Channel half loop interrupt enable high register	*/

#define HLIPL			0x68		/* Channel half loop interrupt pending low register	*/

#define HLIPH			0x69		/* Channel half loop interrupt pending high register	*/

// 0x6a,6b,6c used for some recording
// 0x6d unused
// 0x6e,6f - tanktable base / offset

/* This is the MPU port on the card (via the game port)						*/
#define A_MUDATA1		0x70
#define A_MUCMD1		0x71
#define A_MUSTAT1		A_MUCMD1

/* This is the MPU port on the Audigy Drive 							*/
#define A_MUDATA2		0x72
#define A_MUCMD2		0x73
#define A_MUSTAT2		A_MUCMD2	

/* The next two are the Audigy equivalent of FXWC						*/
/* the Audigy can record any output (16bit, 48kHz, up to 64 channel simultaneously) 		*/
/* Each bit selects a channel for recording */
#define A_FXWC1			0x74            /* Selects 0x7f-0x60 for FX recording           */
#define A_FXWC2			0x75		/* Selects 0x9f-0x80 for FX recording           */

#define A_SPDIF_SAMPLERATE	0x76		/* Set the sample rate of SPDIF output		*/
//#define A_SPDIF_RATE_MASK	0x000000c0
//#define A_SPDIF_48000		0x00000000
//#define A_SPDIF_44100		0x00000080
//#define A_SPDIF_96000		0x00000040
//#define A_SPDIF_192000	0x00000020
#define A_SPDIF_RATE_MASK	0x0000e0e0
#define A_SPDIF_48000		0x00000000
#define A_SPDIF_44100		0x00008080
#define A_SPDIF_96000		0x00004040
#define A_SPDIF_192000		0x00002020

/* 0x77,0x78,0x79 "something i2s-related" - default to 0x01080000 on my audigy 2 ZS --rlrevell	*/
/* 0x7a, 0x7b - lookup tables */

#define A_FXRT2			0x7c
#define A_FXRT_CHANNELE		0x0000003f	/* Effects send bus number for channel's effects send E	*/
#define A_FXRT_CHANNELF		0x00003f00	/* Effects send bus number for channel's effects send F	*/
#define A_FXRT_CHANNELG		0x003f0000	/* Effects send bus number for channel's effects send G	*/
#define A_FXRT_CHANNELH		0x3f000000	/* Effects send bus number for channel's effects send H	*/

#define A_SENDAMOUNTS		0x7d
#define A_FXSENDAMOUNT_E_MASK	0xFF000000
#define A_FXSENDAMOUNT_F_MASK	0x00FF0000
#define A_FXSENDAMOUNT_G_MASK	0x0000FF00
#define A_FXSENDAMOUNT_H_MASK	0x000000FF
/* 0x7c, 0x7e "high bit is used for filtering" */
 
/* The send amounts for this one are the same as used with the emu10k1 */
#define A_FXRT1			0x7e
#define A_FXRT_CHANNELA		0x0000003f
#define A_FXRT_CHANNELB		0x00003f00
#define A_FXRT_CHANNELC		0x003f0000
#define A_FXRT_CHANNELD		0x3f000000


/* Each FX general purpose register is 32 bits in length, all bits are used			*/
#define FXGPREGBASE		0x100		/* FX general purpose registers base       	*/
#define A_FXGPREGBASE		0x400		/* Audigy GPRs, 0x400 to 0x5ff			*/

#define A_TANKMEMCTLREGBASE	0x100		/* Tank memory control registers base - only for Audigy */
#define A_TANKMEMCTLREG_MASK	0x1f		/* only 5 bits used - only for Audigy */

/* Tank audio data is logarithmically compressed down to 16 bits before writing to TRAM and is	*/
/* decompressed back to 20 bits on a read.  There are a total of 160 locations, the last 32	*/
/* locations are for external TRAM. 								*/
#define TANKMEMDATAREGBASE	0x200		/* Tank memory data registers base     		*/
#define TANKMEMDATAREG_MASK	0x000fffff	/* 20 bit tank audio data field			*/

/* Combined address field and memory opcode or flag field.  160 locations, last 32 are external	*/
#define TANKMEMADDRREGBASE	0x300		/* Tank memory address registers base		*/
#define TANKMEMADDRREG_ADDR_MASK 0x000fffff	/* 20 bit tank address field			*/
#define TANKMEMADDRREG_CLEAR	0x00800000	/* Clear tank memory				*/
#define TANKMEMADDRREG_ALIGN	0x00400000	/* Align read or write relative to tank access	*/
#define TANKMEMADDRREG_WRITE	0x00200000	/* Write to tank memory				*/
#define TANKMEMADDRREG_READ	0x00100000	/* Read from tank memory			*/

#define MICROCODEBASE		0x400		/* Microcode data base address			*/

/* Each DSP microcode instruction is mapped into 2 doublewords 					*/
/* NOTE: When writing, always write the LO doubleword first.  Reads can be in either order.	*/
#define LOWORD_OPX_MASK		0x000ffc00	/* Instruction operand X			*/
#define LOWORD_OPY_MASK		0x000003ff	/* Instruction operand Y			*/
#define HIWORD_OPCODE_MASK	0x00f00000	/* Instruction opcode				*/
#define HIWORD_RESULT_MASK	0x000ffc00	/* Instruction result				*/
#define HIWORD_OPA_MASK		0x000003ff	/* Instruction operand A			*/


/* Audigy Soundcard have a different instruction format */
#define A_MICROCODEBASE		0x600
#define A_LOWORD_OPY_MASK	0x000007ff
#define A_LOWORD_OPX_MASK	0x007ff000
#define A_HIWORD_OPCODE_MASK	0x0f000000
#define A_HIWORD_RESULT_MASK	0x007ff000
#define A_HIWORD_OPA_MASK	0x000007ff


#define snd_emu10k1_compose_send_routing(route) \
((route[0] | (route[1] << 4) | (route[2] << 8) | (route[3] << 12)) << 16)

#define snd_emu10k1_compose_audigy_fxrt1(route) \
((unsigned int)route[0] | ((unsigned int)route[1] << 8) | ((unsigned int)route[2] << 16) | ((unsigned int)route[3] << 24))

#define snd_emu10k1_compose_audigy_fxrt2(route) \
((unsigned int)route[4] | ((unsigned int)route[5] << 8) | ((unsigned int)route[6] << 16) | ((unsigned int)route[7] << 24))

#define EMU10K1_MAX_TRAM_BLOCKS_PER_CODE	16

#define emu10k1_gpr_ctl(n) list_entry(n, snd_emu10k1_fx8010_ctl_t, list)

/*
 * ---- FX8010 ----
 */

#define EMU10K1_CARD_CREATIVE			0x00000000
#define EMU10K1_CARD_EMUAPS			0x00000001

#define EMU10K1_FX8010_PCM_COUNT		8

/* instruction set */
#define iMAC0	 0x00	/* R = A + (X * Y >> 31)   ; saturation */
#define iMAC1	 0x01	/* R = A + (-X * Y >> 31)  ; saturation */
#define iMAC2	 0x02	/* R = A + (X * Y >> 31)   ; wraparound */
#define iMAC3	 0x03	/* R = A + (-X * Y >> 31)  ; wraparound */
#define iMACINT0 0x04	/* R = A + X * Y	   ; saturation */
#define iMACINT1 0x05	/* R = A + X * Y	   ; wraparound (31-bit) */
#define iACC3	 0x06	/* R = A + X + Y	   ; saturation */
#define iMACMV   0x07	/* R = A, acc += X * Y >> 31 */
#define iANDXOR  0x08	/* R = (A & X) ^ Y */
#define iTSTNEG  0x09	/* R = (A >= Y) ? X : ~X */
#define iLIMITGE 0x0a	/* R = (A >= Y) ? X : Y */
#define iLIMITLT 0x0b	/* R = (A < Y) ? X : Y */
#define iLOG	 0x0c	/* R = linear_data, A (log_data), X (max_exp), Y (format_word) */
#define iEXP	 0x0d	/* R = log_data, A (linear_data), X (max_exp), Y (format_word) */
#define iINTERP  0x0e	/* R = A + (X * (Y - A) >> 31)  ; saturation */
#define iSKIP    0x0f	/* R = A (cc_reg), X (count), Y (cc_test) */

/* GPRs */
#define FXBUS(x)	(0x00 + (x))	/* x = 0x00 - 0x0f */
#define EXTIN(x)	(0x10 + (x))	/* x = 0x00 - 0x0f */
#define EXTOUT(x)	(0x20 + (x))	/* x = 0x00 - 0x0f physical outs -> FXWC low 16 bits */
#define FXBUS2(x)	(0x30 + (x))	/* x = 0x00 - 0x0f copies of fx buses for capture -> FXWC high 16 bits */
					/* NB: 0x31 and 0x32 are shared with Center/LFE on SB live 5.1 */

#define C_00000000	0x40
#define C_00000001	0x41
#define C_00000002	0x42
#define C_00000003	0x43
#define C_00000004	0x44
#define C_00000008	0x45
#define C_00000010	0x46
#define C_00000020	0x47
#define C_00000100	0x48
#define C_00010000	0x49
#define C_00080000	0x4a
#define C_10000000	0x4b
#define C_20000000	0x4c
#define C_40000000	0x4d
#define C_80000000	0x4e
#define C_7fffffff	0x4f
#define C_ffffffff	0x50
#define C_fffffffe	0x51
#define C_c0000000	0x52
#define C_4f1bbcdc	0x53
#define C_5a7ef9db	0x54
#define C_00100000	0x55		/* ?? */
#define GPR_ACCU	0x56		/* ACCUM, accumulator */
#define GPR_COND	0x57		/* CCR, condition register */
#define GPR_NOISE0	0x58		/* noise source */
#define GPR_NOISE1	0x59		/* noise source */
#define GPR_IRQ		0x5a		/* IRQ register */
#define GPR_DBAC	0x5b		/* TRAM Delay Base Address Counter */
#define GPR(x)		(FXGPREGBASE + (x)) /* free GPRs: x = 0x00 - 0xff */
#define ITRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x00 + (x)) /* x = 0x00 - 0x7f */
#define ETRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x80 + (x)) /* x = 0x00 - 0x1f */
#define ITRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x00 + (x)) /* x = 0x00 - 0x7f */
#define ETRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x80 + (x)) /* x = 0x00 - 0x1f */

#define A_ITRAM_DATA(x)	(TANKMEMDATAREGBASE + 0x00 + (x)) /* x = 0x00 - 0xbf */
#define A_ETRAM_DATA(x)	(TANKMEMDATAREGBASE + 0xc0 + (x)) /* x = 0x00 - 0x3f */
#define A_ITRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0x00 + (x)) /* x = 0x00 - 0xbf */
#define A_ETRAM_ADDR(x)	(TANKMEMADDRREGBASE + 0xc0 + (x)) /* x = 0x00 - 0x3f */
#define A_ITRAM_CTL(x)	(A_TANKMEMCTLREGBASE + 0x00 + (x)) /* x = 0x00 - 0xbf */
#define A_ETRAM_CTL(x)	(A_TANKMEMCTLREGBASE + 0xc0 + (x)) /* x = 0x00 - 0x3f */

#define A_FXBUS(x)	(0x00 + (x))	/* x = 0x00 - 0x3f FX buses */
#define A_EXTIN(x)	(0x40 + (x))	/* x = 0x00 - 0x0f physical ins */
#define A_P16VIN(x)	(0x50 + (x))	/* x = 0x00 - 0x0f p16v ins (A2 only) "EMU32 inputs" */
#define A_EXTOUT(x)	(0x60 + (x))	/* x = 0x00 - 0x1f physical outs -> A_FXWC1 0x79-7f unknown   */
#define A_FXBUS2(x)	(0x80 + (x))	/* x = 0x00 - 0x1f extra outs used for EFX capture -> A_FXWC2 */
#define A_EMU32OUTH(x)	(0xa0 + (x))	/* x = 0x00 - 0x0f "EMU32_OUT_10 - _1F" - ??? */
#define A_EMU32OUTL(x)	(0xb0 + (x))	/* x = 0x00 - 0x0f "EMU32_OUT_1 - _F" - ??? */
#define A_GPR(x)	(A_FXGPREGBASE + (x))

/* cc_reg constants */
#define CC_REG_NORMALIZED C_00000001
#define CC_REG_BORROW	C_00000002
#define CC_REG_MINUS	C_00000004
#define CC_REG_ZERO	C_00000008
#define CC_REG_SATURATE	C_00000010
#define CC_REG_NONZERO	C_00000100

/* FX buses */
#define FXBUS_PCM_LEFT		0x00
#define FXBUS_PCM_RIGHT		0x01
#define FXBUS_PCM_LEFT_REAR	0x02
#define FXBUS_PCM_RIGHT_REAR	0x03
#define FXBUS_MIDI_LEFT		0x04
#define FXBUS_MIDI_RIGHT	0x05
#define FXBUS_PCM_CENTER	0x06
#define FXBUS_PCM_LFE		0x07
#define FXBUS_PCM_LEFT_FRONT	0x08
#define FXBUS_PCM_RIGHT_FRONT	0x09
#define FXBUS_MIDI_REVERB	0x0c
#define FXBUS_MIDI_CHORUS	0x0d
#define FXBUS_PCM_LEFT_SIDE	0x0e
#define FXBUS_PCM_RIGHT_SIDE	0x0f
#define FXBUS_PT_LEFT		0x14
#define FXBUS_PT_RIGHT		0x15

/* Inputs */
#define EXTIN_AC97_L	   0x00	/* AC'97 capture channel - left */
#define EXTIN_AC97_R	   0x01	/* AC'97 capture channel - right */
#define EXTIN_SPDIF_CD_L   0x02	/* internal S/PDIF CD - onboard - left */
#define EXTIN_SPDIF_CD_R   0x03	/* internal S/PDIF CD - onboard - right */
#define EXTIN_ZOOM_L	   0x04	/* Zoom Video I2S - left */
#define EXTIN_ZOOM_R	   0x05	/* Zoom Video I2S - right */
#define EXTIN_TOSLINK_L	   0x06	/* LiveDrive - TOSLink Optical - left */
#define EXTIN_TOSLINK_R    0x07	/* LiveDrive - TOSLink Optical - right */
#define EXTIN_LINE1_L	   0x08	/* LiveDrive - Line/Mic 1 - left */
#define EXTIN_LINE1_R	   0x09	/* LiveDrive - Line/Mic 1 - right */
#define EXTIN_COAX_SPDIF_L 0x0a	/* LiveDrive - Coaxial S/PDIF - left */
#define EXTIN_COAX_SPDIF_R 0x0b /* LiveDrive - Coaxial S/PDIF - right */
#define EXTIN_LINE2_L	   0x0c	/* LiveDrive - Line/Mic 2 - left */
#define EXTIN_LINE2_R	   0x0d	/* LiveDrive - Line/Mic 2 - right */

/* Outputs */
#define EXTOUT_AC97_L	   0x00	/* AC'97 playback channel - left */
#define EXTOUT_AC97_R	   0x01	/* AC'97 playback channel - right */
#define EXTOUT_TOSLINK_L   0x02	/* LiveDrive - TOSLink Optical - left */
#define EXTOUT_TOSLINK_R   0x03	/* LiveDrive - TOSLink Optical - right */
#define EXTOUT_AC97_CENTER 0x04	/* SB Live 5.1 - center */
#define EXTOUT_AC97_LFE	   0x05 /* SB Live 5.1 - LFE */
#define EXTOUT_HEADPHONE_L 0x06	/* LiveDrive - Headphone - left */
#define EXTOUT_HEADPHONE_R 0x07	/* LiveDrive - Headphone - right */
#define EXTOUT_REAR_L	   0x08	/* Rear channel - left */
#define EXTOUT_REAR_R	   0x09	/* Rear channel - right */
#define EXTOUT_ADC_CAP_L   0x0a	/* ADC Capture buffer - left */
#define EXTOUT_ADC_CAP_R   0x0b	/* ADC Capture buffer - right */
#define EXTOUT_MIC_CAP	   0x0c	/* MIC Capture buffer */
#define EXTOUT_AC97_REAR_L 0x0d	/* SB Live 5.1 (c) 2003 - Rear Left */
#define EXTOUT_AC97_REAR_R 0x0e	/* SB Live 5.1 (c) 2003 - Rear Right */
#define EXTOUT_ACENTER	   0x11 /* Analog Center */
#define EXTOUT_ALFE	   0x12 /* Analog LFE */

/* Audigy Inputs */
#define A_EXTIN_AC97_L		0x00	/* AC'97 capture channel - left */
#define A_EXTIN_AC97_R		0x01	/* AC'97 capture channel - right */
#define A_EXTIN_SPDIF_CD_L	0x02	/* digital CD left */
#define A_EXTIN_SPDIF_CD_R	0x03	/* digital CD left */
#define A_EXTIN_OPT_SPDIF_L     0x04    /* audigy drive Optical SPDIF - left */
#define A_EXTIN_OPT_SPDIF_R     0x05    /*                              right */ 
#define A_EXTIN_LINE2_L		0x08	/* audigy drive line2/mic2 - left */
#define A_EXTIN_LINE2_R		0x09	/*                           right */
#define A_EXTIN_ADC_L		0x0a    /* Philips ADC - left */
#define A_EXTIN_ADC_R		0x0b    /*               right */
#define A_EXTIN_AUX2_L		0x0c	/* audigy drive aux2 - left */
#define A_EXTIN_AUX2_R		0x0d	/*                   - right */

/* Audigiy Outputs */
#define A_EXTOUT_FRONT_L	0x00	/* digital front left */
#define A_EXTOUT_FRONT_R	0x01	/*               right */
#define A_EXTOUT_CENTER		0x02	/* digital front center */
#define A_EXTOUT_LFE		0x03	/* digital front lfe */
#define A_EXTOUT_HEADPHONE_L	0x04	/* headphone audigy drive left */
#define A_EXTOUT_HEADPHONE_R	0x05	/*                        right */
#define A_EXTOUT_REAR_L		0x06	/* digital rear left */
#define A_EXTOUT_REAR_R		0x07	/*              right */
#define A_EXTOUT_AFRONT_L	0x08	/* analog front left */
#define A_EXTOUT_AFRONT_R	0x09	/*              right */
#define A_EXTOUT_ACENTER	0x0a	/* analog center */
#define A_EXTOUT_ALFE		0x0b	/* analog LFE */
#define A_EXTOUT_ASIDE_L	0x0c	/* analog side left  - Audigy 2 ZS */
#define A_EXTOUT_ASIDE_R	0x0d	/*             right - Audigy 2 ZS */
#define A_EXTOUT_AREAR_L	0x0e	/* analog rear left */
#define A_EXTOUT_AREAR_R	0x0f	/*             right */
#define A_EXTOUT_AC97_L		0x10	/* AC97 left (front) */
#define A_EXTOUT_AC97_R		0x11	/*      right */
#define A_EXTOUT_ADC_CAP_L	0x16	/* ADC capture buffer left */
#define A_EXTOUT_ADC_CAP_R	0x17	/*                    right */
#define A_EXTOUT_MIC_CAP	0x18	/* Mic capture buffer */

/* Audigy constants */
#define A_C_00000000	0xc0
#define A_C_00000001	0xc1
#define A_C_00000002	0xc2
#define A_C_00000003	0xc3
#define A_C_00000004	0xc4
#define A_C_00000008	0xc5
#define A_C_00000010	0xc6
#define A_C_00000020	0xc7
#define A_C_00000100	0xc8
#define A_C_00010000	0xc9
#define A_C_00000800	0xca
#define A_C_10000000	0xcb
#define A_C_20000000	0xcc
#define A_C_40000000	0xcd
#define A_C_80000000	0xce
#define A_C_7fffffff	0xcf
#define A_C_ffffffff	0xd0
#define A_C_fffffffe	0xd1
#define A_C_c0000000	0xd2
#define A_C_4f1bbcdc	0xd3
#define A_C_5a7ef9db	0xd4
#define A_C_00100000	0xd5
#define A_GPR_ACCU	0xd6		/* ACCUM, accumulator */
#define A_GPR_COND	0xd7		/* CCR, condition register */
#define A_GPR_NOISE0	0xd8		/* noise source */
#define A_GPR_NOISE1	0xd9		/* noise source */
#define A_GPR_IRQ	0xda		/* IRQ register */
#define A_GPR_DBAC	0xdb		/* TRAM Delay Base Address Counter - internal */
#define A_GPR_DBACE	0xde		/* TRAM Delay Base Address Counter - external */

/* definitions for debug register */
#define EMU10K1_DBG_ZC			0x80000000	/* zero tram counter */
#define EMU10K1_DBG_SATURATION_OCCURED	0x02000000	/* saturation control */
#define EMU10K1_DBG_SATURATION_ADDR	0x01ff0000	/* saturation address */
#define EMU10K1_DBG_SINGLE_STEP		0x00008000	/* single step mode */
#define EMU10K1_DBG_STEP		0x00004000	/* start single step */
#define EMU10K1_DBG_CONDITION_CODE	0x00003e00	/* condition code */
#define EMU10K1_DBG_SINGLE_STEP_ADDR	0x000001ff	/* single step address */

/* tank memory address line */
#ifndef __KERNEL__
#define TANKMEMADDRREG_ADDR_MASK 0x000fffff	/* 20 bit tank address field			*/
#define TANKMEMADDRREG_CLEAR	 0x00800000	/* Clear tank memory				*/
#define TANKMEMADDRREG_ALIGN	 0x00400000	/* Align read or write relative to tank access	*/
#define TANKMEMADDRREG_WRITE	 0x00200000	/* Write to tank memory				*/
#define TANKMEMADDRREG_READ	 0x00100000	/* Read from tank memory			*/
#endif

#define EMU10K1_GPR_TRANSLATION_NONE		0
#define EMU10K1_GPR_TRANSLATION_TABLE100	1
#define EMU10K1_GPR_TRANSLATION_BASS		2
#define EMU10K1_GPR_TRANSLATION_TREBLE		3
#define EMU10K1_GPR_TRANSLATION_ONOFF		4

#define SNDRV_EMU10K1_IOCTL_INFO	_IOR ('H', 0x10, emu10k1_fx8010_info_t)
#define SNDRV_EMU10K1_IOCTL_CODE_POKE	_IOW ('H', 0x11, emu10k1_fx8010_code_t)
#define SNDRV_EMU10K1_IOCTL_CODE_PEEK	_IOWR('H', 0x12, emu10k1_fx8010_code_t)
#define SNDRV_EMU10K1_IOCTL_TRAM_SETUP	_IOW ('H', 0x20, int)
#define SNDRV_EMU10K1_IOCTL_TRAM_POKE	_IOW ('H', 0x21, emu10k1_fx8010_tram_t)
#define SNDRV_EMU10K1_IOCTL_TRAM_PEEK	_IOWR('H', 0x22, emu10k1_fx8010_tram_t)
#define SNDRV_EMU10K1_IOCTL_PCM_POKE	_IOW ('H', 0x30, emu10k1_fx8010_pcm_t)
#define SNDRV_EMU10K1_IOCTL_PCM_PEEK	_IOWR('H', 0x31, emu10k1_fx8010_pcm_t)
#define SNDRV_EMU10K1_IOCTL_STOP	_IO  ('H', 0x80)
#define SNDRV_EMU10K1_IOCTL_CONTINUE	_IO  ('H', 0x81)
#define SNDRV_EMU10K1_IOCTL_ZERO_TRAM_COUNTER _IO ('H', 0x82)
#define SNDRV_EMU10K1_IOCTL_SINGLE_STEP	_IOW ('H', 0x83, int)
#define SNDRV_EMU10K1_IOCTL_DBG_READ	_IOR ('H', 0x84, int)

#endif	/* __SOUND_EMU10K1_H */


//-------------------------------------------------------------------------

#ifndef _EFXMGR_H
#define _EFXMGR_H

#define VOL_8BIT 0x100
#define VOL_7BIT 0x80
#define VOL_6BIT 0x40
#define VOL_5BIT 0x20
#define VOL_4BIT 0x10

#define A_WRITE_EFX(a, b, c) emu10k1_writeptr((a), A_MICROCODEBASE + (b), 0, (c))

#define A_OP(op, z, w, x, y) \
{ \
 A_WRITE_EFX(card, (pc) * 2, ((x) << 12) | (y)); \
 A_WRITE_EFX(card, (pc) * 2 + 1, ((op) << 24 ) | ((z) << 12) | (w)); \
 ++pc; \
}

#define L_WRITE_EFX(a, b, c) emu10k1_writeptr((a), MICROCODEBASE + (b), 0, (c))

#define L_OP(op, z, w, x, y) \
{ \
 L_WRITE_EFX(card, (pc) * 2, ((x) << 10) | (y)); \
 L_WRITE_EFX(card, (pc) * 2 + 1, ((op) << 20) | ((z) << 10) | (w)); \
 ++pc; \
}

#endif // _EFXMGR_H

//------------------------------------------------------------------------

#ifndef P16V_H
#define P16V_H

/*
 *  Copyright (c) by James Courtier-Dutton <James@superbug.demon.co.uk>
 *  Driver p16v chips
 *  Version: 0.21
 *
 *  FEATURES currently supported:
 *    Output fixed at S32_LE, 2 channel to hw:0,0
 *    Rates: 44.1, 48, 96, 192.
 *
 *  TODO:
 *    SPDIF out.
 *    Find out how to change capture sample rates. E.g. To record SPDIF at 48000Hz.
 *    Currently capture fixed at 48000Hz.
 *
 *    --
 *  GENERAL INFO:
 *    Model: SB0240
 *    P16V Chip: CA0151-DBS
 *    Audigy 2 Chip: CA0102-IAT
 *    AC97 Codec: STAC 9721
 *    ADC: Philips 1361T (Stereo 24bit)
 *    DAC: CS4382-K (8-channel, 24bit, 192Khz)
 *
 *  This code was initally based on code from ALSA's emu10k1x.c which is:
 *  Copyright (c) by Francisco Moraes <fmoraes@nc.rr.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/********************************************************************************************************/
/* Audigy2 P16V pointer-offset register set, accessed through the PTR2 and DATA2 registers                     */
/********************************************************************************************************/
                                                                                                                           
/* The sample rate of the SPDIF outputs is set by modifying a register in the EMU10K2 PTR register A_SPDIF_SAMPLERATE.
 * The sample rate is also controlled by the same registers that control the rate of the EMU10K2 sample rate converters.
 */

/* Initally all registers from 0x00 to 0x3f have zero contents. */
#define PLAYBACK_LIST_ADDR	0x00		/* Base DMA address of a list of pointers to each period/size */
						/* One list entry: 4 bytes for DMA address, 
						 * 4 bytes for period_size << 16.
						 * One list entry is 8 bytes long.
						 * One list entry for each period in the buffer.
						 */
#define PLAYBACK_LIST_SIZE	0x01		/* Size of list in bytes << 16. E.g. 8 periods -> 0x00380000  */
#define PLAYBACK_LIST_PTR	0x02		/* Pointer to the current period being played */
#define PLAYBACK_UNKNOWN3	0x03		/* Not used */
#define PLAYBACK_DMA_ADDR	0x04		/* Playback DMA addresss */
#define PLAYBACK_PERIOD_SIZE	0x05		/* Playback period size. win2000 uses 0x04000000 */
#define PLAYBACK_POINTER	0x06		/* Playback period pointer. Used with PLAYBACK_LIST_PTR to determine buffer position currently in DAC */
#define PLAYBACK_FIFO_END_ADDRESS	0x07		/* Playback FIFO end address */
#define PLAYBACK_FIFO_POINTER	0x08		/* Playback FIFO pointer and number of valid sound samples in cache */
#define PLAYBACK_UNKNOWN9	0x09		/* Not used */
#define CAPTURE_DMA_ADDR	0x10		/* Capture DMA address */
#define CAPTURE_BUFFER_SIZE	0x11		/* Capture buffer size */
#define CAPTURE_POINTER		0x12		/* Capture buffer pointer. Sample currently in ADC */
#define CAPTURE_FIFO_POINTER	0x13		/* Capture FIFO pointer and number of valid sound samples in cache */
#define CAPTURE_P16V_VOLUME1	0x14		/* Low: Capture volume 0xXXXX3030 */
#define CAPTURE_P16V_VOLUME2	0x15		/* High:Has no effect on capture volume */
#define CAPTURE_P16V_SOURCE     0x16            /* P16V source select. Set to 0x0700E4E5 for AC97 CAPTURE */
						/* [0:1] Capture input 0 channel select. 0 = Capture output 0.
						 *                                       1 = Capture output 1.
						 *                                       2 = Capture output 2.
						 *                                       3 = Capture output 3.
						 * [3:2] Capture input 1 channel select. 0 = Capture output 0.
						 *                                       1 = Capture output 1.
						 *                                       2 = Capture output 2.
						 *                                       3 = Capture output 3.
						 * [5:4] Capture input 2 channel select. 0 = Capture output 0.
						 *                                       1 = Capture output 1.
						 *                                       2 = Capture output 2.
						 *                                       3 = Capture output 3.
						 * [7:6] Capture input 3 channel select. 0 = Capture output 0.
						 *                                       1 = Capture output 1.
						 *                                       2 = Capture output 2.
						 *                                       3 = Capture output 3.
						 * [9:8] Playback input 0 channel select. 0 = Play output 0.
						 *                                        1 = Play output 1.
						 *                                        2 = Play output 2.
						 *                                        3 = Play output 3.
						 * [11:10] Playback input 1 channel select. 0 = Play output 0.
						 *                                          1 = Play output 1.
						 *                                          2 = Play output 2.
						 *                                          3 = Play output 3.
						 * [13:12] Playback input 2 channel select. 0 = Play output 0.
						 *                                          1 = Play output 1.
						 *                                          2 = Play output 2.
						 *                                          3 = Play output 3.
						 * [15:14] Playback input 3 channel select. 0 = Play output 0.
						 *                                          1 = Play output 1.
						 *                                          2 = Play output 2.
						 *                                          3 = Play output 3.
						 * [19:16] Playback mixer output enable. 1 bit per channel.
						 * [23:20] Capture mixer output enable. 1 bit per channel.
						 * [26:24] FX engine channel capture 0 = 0x60-0x67.
						 *                                   1 = 0x68-0x6f.
						 *                                   2 = 0x70-0x77.
						 *                                   3 = 0x78-0x7f.
						 *                                   4 = 0x80-0x87.
						 *                                   5 = 0x88-0x8f.
						 *                                   6 = 0x90-0x97.
						 *                                   7 = 0x98-0x9f.
						 * [31:27] Not used.
						 */

						/* 0x1 = capture on.
						 * 0x100 = capture off.
						 * 0x200 = capture off.
						 * 0x1000 = capture off.
						 */
#define CAPTURE_RATE_STATUS		0x17		/* Capture sample rate. Read only */
						/* [15:0] Not used.
						 * [18:16] Channel 0 Detected sample rate. 0 - 44.1khz
						 *                               1 - 48 khz
						 *                               2 - 96 khz
						 *                               3 - 192 khz
						 *                               7 - undefined rate.
						 * [19] Channel 0. 1 - Valid, 0 - Not Valid.
						 * [22:20] Channel 1 Detected sample rate. 
						 * [23] Channel 1. 1 - Valid, 0 - Not Valid.
						 * [26:24] Channel 2 Detected sample rate. 
						 * [27] Channel 2. 1 - Valid, 0 - Not Valid.
						 * [30:28] Channel 3 Detected sample rate. 
						 * [31] Channel 3. 1 - Valid, 0 - Not Valid.
						 */
/* 0x18 - 0x1f unused */
#define PLAYBACK_LAST_SAMPLE    0x20		/* The sample currently being played. Read only */
/* 0x21 - 0x3f unused */
#define BASIC_INTERRUPT         0x40		/* Used by both playback and capture interrupt handler */
						/* Playback (0x1<<channel_id) Don't touch high 16bits. */
						/* Capture  (0x100<<channel_id). not tested */
						/* Start Playback [3:0] (one bit per channel)
						 * Start Capture [11:8] (one bit per channel)
						 * Record source select for channel 0 [18:16]
						 * Record source select for channel 1 [22:20]
						 * Record source select for channel 2 [26:24]
						 * Record source select for channel 3 [30:28]
						 * 0 - SPDIF channel.
						 * 1 - I2S channel.
						 * 2 - SRC48 channel.
						 * 3 - SRCMulti_SPDIF channel.
						 * 4 - SRCMulti_I2S channel.
						 * 5 - SPDIF channel.
						 * 6 - fxengine capture.
						 * 7 - AC97 capture.
						 */
						/* Default 41110000.
						 * Writing 0xffffffff hangs the PC.
						 * Writing 0xffff0000 -> 77770000 so it must be some sort of route.
						 * bit 0x1 starts DMA playback on channel_id 0
						 */
/* 0x41,42 take values from 0 - 0xffffffff, but have no effect on playback */
/* 0x43,0x48 do not remember settings */
/* 0x41-45 unused */
#define WATERMARK            0x46		/* Test bit to indicate cache level usage */
						/* Values it can have while playing on channel 0. 
						 * 0000f000, 0000f004, 0000f008, 0000f00c.
						 * Readonly.
						 */
/* 0x47-0x4f unused */
/* 0x50-0x5f Capture cache data */
#define SRCSel			0x60            /* SRCSel. Default 0x4. Bypass P16V 0x14 */
						/* [0] 0 = 10K2 audio, 1 = SRC48 mixer output.
						 * [2] 0 = 10K2 audio, 1 = SRCMulti SPDIF mixer output.
						 * [4] 0 = 10K2 audio, 1 = SRCMulti I2S mixer output.
						 */
						/* SRC48 converts samples rates 44.1, 48, 96, 192 to 48 khz. */
						/* SRCMulti converts 48khz samples rates to 44.1, 48, 96, 192 to 48. */
						/* SRC48 and SRCMULTI sample rate select and output select. */
						/* 0xffffffff -> 0xC0000015
						 * 0xXXXXXXX4 = Enable Front Left/Right
						 * Enable PCMs
						 */

/* 0x61 -> 0x6c are Volume controls */
#define PLAYBACK_VOLUME_MIXER1  0x61		/* SRC48 Low to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER2  0x62		/* SRC48 High to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER3  0x63		/* SRCMULTI SPDIF Low to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER4  0x64		/* SRCMULTI SPDIF High to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER5  0x65		/* SRCMULTI I2S Low to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER6  0x66		/* SRCMULTI I2S High to mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER7  0x67		/* P16V Low to SRCMULTI SPDIF mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER8  0x68		/* P16V High to SRCMULTI SPDIF mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER9  0x69		/* P16V Low to SRCMULTI I2S mixer input volume control. */
						/* 0xXXXX3030 = PCM0 Volume (Front).
						 * 0x3030XXXX = PCM1 Volume (Center)
						 */
#define PLAYBACK_VOLUME_MIXER10  0x6a		/* P16V High to SRCMULTI I2S mixer input volume control. */
						/* 0x3030XXXX = PCM3 Volume (Rear). */
#define PLAYBACK_VOLUME_MIXER11  0x6b		/* E10K2 Low to SRC48 mixer input volume control. */
#define PLAYBACK_VOLUME_MIXER12 0x6c		/* E10K2 High to SRC48 mixer input volume control. */

#define SRC48_ENABLE            0x6d            /* SRC48 input audio enable */
						/* SRC48 converts samples rates 44.1, 48, 96, 192 to 48 khz. */
						/* [23:16] The corresponding P16V channel to SRC48 enabled if == 1.
						 * [31:24] The corresponding E10K2 channel to SRC48 enabled.
						 */
#define SRCMULTI_ENABLE         0x6e            /* SRCMulti input audio enable. Default 0xffffffff */
						/* SRCMulti converts 48khz samples rates to 44.1, 48, 96, 192 to 48. */
						/* [7:0] The corresponding P16V channel to SRCMulti_I2S enabled if == 1.
						 * [15:8] The corresponding E10K2 channel to SRCMulti I2S enabled.
						 * [23:16] The corresponding P16V channel to SRCMulti SPDIF enabled.
						 * [31:24] The corresponding E10K2 channel to SRCMulti SPDIF enabled.
						 */
						/* Bypass P16V 0xff00ff00 
						 * Bitmap. 0 = Off, 1 = On.
						 * P16V playback outputs:
						 * 0xXXXXXXX1 = PCM0 Left. (Front)
						 * 0xXXXXXXX2 = PCM0 Right.
						 * 0xXXXXXXX4 = PCM1 Left. (Center/LFE)
						 * 0xXXXXXXX8 = PCM1 Right.
						 * 0xXXXXXX1X = PCM2 Left. (Unknown)
						 * 0xXXXXXX2X = PCM2 Right.
						 * 0xXXXXXX4X = PCM3 Left. (Rear)
						 * 0xXXXXXX8X = PCM3 Right.
						 */
#define AUDIO_OUT_ENABLE        0x6f            /* Default: 000100FF */
						/* [3:0] Does something, but not documented. Probably capture enable.
						 * [7:4] Playback channels enable. not documented.
						 * [16] AC97 output enable if == 1
						 * [30] 0 = SRCMulti_I2S input from fxengine 0x68-0x6f.
						 *      1 = SRCMulti_I2S input from SRC48 output.
						 * [31] 0 = SRCMulti_SPDIF input from fxengine 0x60-0x67.
						 *      1 = SRCMulti_SPDIF input from SRC48 output.
						 */
						/* 0xffffffff -> C00100FF */
						/* 0 -> Not playback sound, irq still running */
						/* 0xXXXXXX10 = PCM0 Left/Right On. (Front)
						 * 0xXXXXXX20 = PCM1 Left/Right On. (Center/LFE)
						 * 0xXXXXXX40 = PCM2 Left/Right On. (Unknown)
						 * 0xXXXXXX80 = PCM3 Left/Right On. (Rear)
						 */
#define PLAYBACK_SPDIF_SELECT     0x70          /* Default: 12030F00 */
						/* 0xffffffff -> 3FF30FFF */
						/* 0x00000001 pauses stream/irq fail. */
						/* All other bits do not effect playback */
#define PLAYBACK_SPDIF_SRC_SELECT 0x71          /* Default: 0000E4E4 */
						/* 0xffffffff -> F33FFFFF */
						/* All bits do not effect playback */
#define PLAYBACK_SPDIF_USER_DATA0 0x72		/* SPDIF out user data 0 */
#define PLAYBACK_SPDIF_USER_DATA1 0x73		/* SPDIF out user data 1 */
/* 0x74-0x75 unknown */
#define CAPTURE_SPDIF_CONTROL	0x76		/* SPDIF in control setting */
#define CAPTURE_SPDIF_STATUS	0x77		/* SPDIF in status */
#define CAPURE_SPDIF_USER_DATA0 0x78		/* SPDIF in user data 0 */
#define CAPURE_SPDIF_USER_DATA1 0x79		/* SPDIF in user data 1 */
#define CAPURE_SPDIF_USER_DATA2 0x7a		/* SPDIF in user data 2 */

#endif // P16V_H
