
#ifndef _VDISNEY_H_
#define _VDISNEY_H_
// Disney Sound Source / Covox Speech Thing emulation for SBEMU
// Based on DOSBox disney.cpp by the DOSBox Team (GPL v2+)
//
// Hardware: LPT1 parallel port DAC with 16-byte FIFO
//   LPT1 base = 0x378
//   Port 0x378 = Data (DAC sample byte)
//   Port 0x379 = Status (bit 6 = FIFO not full, bit 7 = pin9 mirror)
//   Port 0x37A = Control (bit 1 edge = stereo latch ch1, bit 0 edge = stereo latch ch0)
//
// Activation: games write data bytes to 0x378 and strobe the control port.
// Detection:  games read status port 0x379 and expect bit 6 clear (FIFO not full).

#include <stdint.h>
#include <sbemucfg.h>

#define SBEMU_DISNEY 1

#if SBEMU_DISNEY

#define VDISNEY_BASE    0x378   // LPT1
#define VDISNEY_FIFO    8192    // internal ring buffer per channel (MUST be power of 2)

// Initialize Disney emulation.
// output_freq: PCM rate of the card (e.g. 44100 or 22050).
void VDISNEY_Init(int output_freq);

// Shut down and free resources.
void VDISNEY_Shutdown(void);

// Returns 1 if Disney emulation is active (initialized).
int  VDISNEY_IsActive(void);

// Unified I/O handler for ports 0x378, 0x379, 0x37A.
// Signature matches SBEMU_IOTRAP_HANDLER: port, val, out(1=write/0=read).
uint32_t VDISNEY_IOHandler(uint32_t port, uint32_t val, uint32_t out);

// Mix Disney audio into pcm16 (stereo interleaved int16).
// domix=1: add to existing buffer; domix=0: overwrite.
// Called once per audio interrupt, same as VGUS_GenSamples.
void VDISNEY_GenSamples(int16_t *pcm16, int samples, int freq, int domix);

#else
#define VDISNEY_IsActive() 0
#endif // SBEMU_DISNEY

#endif // _VDISNEY_H_
