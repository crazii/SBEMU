#ifndef AK4531_DEF_H
#define AK4531_DEF_H

//**************************************************************************
//function: register definitions for the Asahi Kasei AK4531 codec, as used
//on Ensoniq/Creative ES1370 (SB PCI64/128, CT4700, CT5803, ...) cards.
//
//The AK4531 is NOT an AC'97 codec: it's a simple, write-only serial-control
//audio mixer chip. There is no way to read a register back from the codec
//itself - values must be cached in software (see SC_E1370.C, card->regs[]).
//
//Register format (all registers except 0x10-0x19):
// bit  7   : MUTE (1=channel muted)
// bits 6-5 : unused
// bits 4-0 : ATTEN, 5-bit attenuation; 0x00=0dB (loudest), 0x1F=mute level
//(higher register value = more attenuation = quieter; same polarity as the
// AC'97 volume registers already used elsewhere in this project)
//
//based on ALSA's include/sound/ak4531_codec.h (Jaroslav Kysela)
//**************************************************************************

#define AK4531_LMASTER   0x00  /* master volume left            */
#define AK4531_RMASTER   0x01  /* master volume right           */
#define AK4531_LVOICE    0x02  /* PCM (DAC/"voice") volume left  */
#define AK4531_RVOICE    0x03  /* PCM (DAC/"voice") volume right */
#define AK4531_LFM       0x04  /* FM volume left                */
#define AK4531_RFM       0x05  /* FM volume right               */
#define AK4531_LCD       0x06  /* CD volume left                */
#define AK4531_RCD       0x07  /* CD volume right               */
#define AK4531_LLINE     0x08  /* LINE volume left              */
#define AK4531_RLINE     0x09  /* LINE volume right             */
#define AK4531_LAUXA     0x0a  /* AUX volume left               */
#define AK4531_RAUXA     0x0b  /* AUX volume right              */
#define AK4531_MONO1     0x0c  /* MONO1 volume                  */
#define AK4531_MONO2     0x0d  /* MONO2 volume                  */
#define AK4531_MIC       0x0e  /* MIC volume                    */
#define AK4531_MONO_OUT  0x0f  /* Mono-out volume               */
#define AK4531_OUT_SW1   0x10  /* Output mixer switch 1         */
#define AK4531_OUT_SW2   0x11  /* Output mixer switch 2         */
#define AK4531_LIN_SW1   0x12  /* Input left mixer switch 1     */
#define AK4531_RIN_SW1   0x13  /* Input right mixer switch 1    */
#define AK4531_LIN_SW2   0x14  /* Input left mixer switch 2     */
#define AK4531_RIN_SW2   0x15  /* Input right mixer switch 2    */
#define AK4531_RESET     0x16  /* Reset & power down            */
#define AK4531_CLOCK     0x17  /* Clock select                  */
#define AK4531_AD_IN     0x18  /* AD input select               */
#define AK4531_MIC_GAIN  0x19  /* MIC amplifier gain            */

#define AK4531_MUTE      0x80  /* mute bit, present in most registers */
#define AK4531_ATTNMASK  0x1f  /* 5-bit attenuation mask */

/* OUT_SW2 bits that route the PCM ("voice"/DAC) channel into the
 * analog output mix - without this, DAC audio is silent even if
 * LVOICE/RVOICE/LMASTER/RMASTER are all unmuted with 0 attenuation.
 */
#define AK4531_OUTSW2_PCM_L (1<<3)
#define AK4531_OUTSW2_PCM_R (1<<2)

/* AK4531_RESET (reg 0x16) bit meaning:
 * bit0 = /RST (1 = not in reset, 0 = reset asserted)
 * bit1 = /PD  (1 = powered on,   0 = powered down)
 * The codec powers up already held in reset AND powered down (0x00) -
 * it MUST be brought out of that state before any other register write
 * has any audible effect. See snd_es1370_ak4531_init() in SC_E1370.C.
 */
#define AK4531_RESET_POWERDOWN 0x01 /* not in reset, but powered down */
#define AK4531_RESET_NORMAL    0x03 /* not in reset, powered on (normal operation) */

extern aucards_allmixerchan_s mpxplay_aucards_ak4531chan_mixerset[];

#endif // AK4531_DEF_H
