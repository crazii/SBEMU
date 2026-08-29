
#ifndef _VPCSPEAKER_H_
#define _VPCSPEAKER_H_
// PC Speaker (Internal Beeper) emulation for SBEMU
// Based on DOSBox pcspeaker.cpp
//
// Emulates the Intel 8253/8254 PIT (Programmable Interval Timer) Channel 2
// and the System Control Port (0x61) to generate standard PC Speaker sound.

#include <stdint.h>
#include <sbemucfg.h>

#define SBEMU_PCSPEAKER 1

#if SBEMU_PCSPEAKER

// Initialize PC Speaker emulation
// output_freq: PCM rate of the card
void VPCSPEAKER_Init(int output_freq);

// Shut down and free resources
void VPCSPEAKER_Shutdown(void);

// Returns 1 if PC Speaker emulation is active
int  VPCSPEAKER_IsActive(void);

// Unified I/O handler for ports 0x42, 0x43 (PIT) and 0x61 (SysCtrl)
uint32_t VPCSPEAKER_IOHandler(uint32_t port, uint32_t val, uint32_t out);

// Generate/Mix PC Speaker audio into pcm16 (stereo interleaved int16).
// Called once per audio interrupt.
void VPCSPEAKER_GenSamples(int16_t *pcm16, int samples, int freq, int domix);

#else
#define VPCSPEAKER_IsActive() 0
#endif // SBEMU_PCSPEAKER

#endif // _VPCSPEAKER_H_
