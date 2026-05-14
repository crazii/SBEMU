#ifndef _VMPU_H_
#define _VMPU_H_

#include <sbemucfg.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if SBEMU_VMPU

#define VMPU_DEF_SF2 "sbemusf.sf2"

BOOL VMPU_Init(int baseaddr, int* voices, int freq, const char* sf2);

BOOL VMPU_IsActive();

void VMPU_GenSamples(int16_t* pcm16, int samples, int freq, BOOL domix);

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out);
void VMPU_SBMidi_RawWrite( uint8_t value );

#else //SBEMU_VMPU

#define VMPU_IsActive() FALSE

#endif //SBEMU_VMPU

#ifdef __cplusplus
}
#endif

#endif//_VMPU_H_
