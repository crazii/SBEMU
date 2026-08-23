#ifndef _IOTRAP_H_
#define _IOTRAP_H_
#include "platform.h"

typedef uint32_t (*SBEMU_IOTRAP_HANDLER)(uint32_t port, uint32_t val, uint32_t out);

typedef struct SBEMU_IODispatchTable
{
    uint32_t    port;
    SBEMU_IOTRAP_HANDLER handler;
}SBEMU_IODT;

typedef struct SBEMU_IOPorTrap
{
    uint32_t memory;
}SBEMU_IOPT;

//used internally
typedef struct SBEMU_IODT_LINK
{
    SBEMU_IODT* iodt; //owner
    BOOL* states; //owner
    int count;
    struct SBEMU_IODT_LINK* prev; //observer
    struct SBEMU_IODT_LINK* next; //observer
}SBEMU_IODT_LINK;

void UntrappedIO_OUT(uint16_t port, uint8_t value);
uint8_t UntrappedIO_IN(uint16_t port);

#endif