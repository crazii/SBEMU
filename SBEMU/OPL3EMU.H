#ifndef _OPL3EMU_H_
#define _OPL3EMU_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void OPL3EMU_Init(int samplerate);
int OPL3EMU_GenSamples(int32_t* pcm32, int count, int* channel);

uint32_t OPL3EMU_PrimaryRead(uint32_t val);
uint32_t OPL3EMU_PrimaryWriteIndex(uint32_t val);
uint32_t OPL3EMU_PrimaryWriteData(uint32_t val);

uint32_t OPL3EMU_SecondaryRead(uint32_t val);
uint32_t OPL3EMU_SecondaryWriteIndex(uint32_t val);
uint32_t OPL3EMU_SecondaryWriteData(uint32_t val);

#ifdef __cplusplus
}
#endif


#endif//_OPL3EMU_H_