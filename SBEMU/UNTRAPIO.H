#ifndef _UNTRAPIO_H_
#define _UNTRAPIO_H_
//Untrapped IO wrapper function
//in PM mode, HDPMI will call the trap handler with IOPL0 so direct in/out works, but in RM, QEMM doesn't do that,
//so QEMM's untrapped function need to be called, this works for both PM & RM mode

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

//external handler
void (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value);
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port);

//generic use
void UntrappedIO_OUT(uint16_t port, uint8_t value);
uint8_t UntrappedIO_IN(uint16_t port);

#ifdef __cplusplus
}
#endif

#endif//_UNTRAPIO_H_