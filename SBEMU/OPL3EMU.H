#ifndef _OPL3EMU_H_
#define _OPL3EMU_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern void OPL3EMU_Init(int freq);
extern void OPL3EMU_GenSamples(int32_t* pcm32, int count);

extern int OPL3EMU_PrimaryRead(int val);
extern int OPL3EMU_PrimaryWriteIndex(int val);
extern int OPL3EMU_PrimaryWriteData(int val);

extern int OPL3EMU_SecondaryRead(int val);
extern int OPL3EMU_SecondaryWriteIndex(int val);
extern int OPL3EMU_SecondaryWriteData(int val);

#ifdef __cplusplus
}
#endif


#endif//_OPL3EMU_H_