#include "iotrap.h"
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#else
#include <pc.h>
#endif

void __attribute__((weak)) UntrappedIO_OUT(uint16_t port, uint8_t value)
{
    outp(port, value);
}

uint8_t __attribute__((weak)) UntrappedIO_IN(uint16_t port)
{
    return (uint8_t)inp(port);
}
