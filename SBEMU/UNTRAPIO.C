#include <dos.h>
#include "UNTRAPIO.H"

int UntrappedIO_PM = 0;

static void UntrappedIO_OUT_Default(uint16_t port, uint8_t value)
{
    outp(port, value);
}

static uint8_t UntrappedIO_IN_Default(uint16_t port)
{
    return inp(port);
}

void (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = &UntrappedIO_OUT_Default;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = &UntrappedIO_IN_Default;

void UntrappedIO_OUT(uint16_t port, uint8_t value)
{
    if(UntrappedIO_PM)
        outp(port, value);
    else
        UntrappedIO_OUT_Handler(port, value);
}

uint8_t UntrappedIO_IN(uint16_t port)
{
    if(UntrappedIO_PM)
        return inp(port);
    else
        return UntrappedIO_IN_Handler(port);
}

