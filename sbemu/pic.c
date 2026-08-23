//reference: https://wiki.osdev.org/PIC
#include <dos.h>
#include "pic.h"
#include "iotrap.h"

//master PIC
#define PIC_PORT1 0x20
#define PIC_DATA1 0x21
//slave PIC
#define PIC_PORT2 0xA0
#define PIC_DATA2 0xA1

#define PIC_READISR 0x0B    //read interrupte service register (current interrupting IRQ)

#undef CLIS
#undef STIL
#define CLIS()
#define STIL()

void PIC_SendEOIWithIRQ(uint8_t irq)
{
    if(irq == 7 || irq == 15) //check spurious irq
        return PIC_SendEOI();
    CLIS();
    if(irq >= 8)
        outp(PIC_PORT2, 0x20);
    outp(PIC_PORT1, 0x20);
    STIL();
}

void PIC_SendEOI(void)
{
    CLIS();
    //get irq mask
    outp(PIC_PORT1, PIC_READISR);
    uint16_t mask = inp(PIC_PORT1);
    if(mask&0x4)
    {
        outp(PIC_PORT2, PIC_READISR);
        if(inp(PIC_PORT2))
            outp(PIC_PORT2, 0x20);
    }
    if(mask)
        outp(PIC_PORT1, 0x20);
    STIL();
}

uint8_t PIC_GetIRQ(void)
{
    CLIS();
    //get irq mask
    outp(PIC_PORT1, PIC_READISR);
    uint16_t mask = inp(PIC_PORT1);
    if(mask&0x4)
    {
        outp(PIC_PORT2, PIC_READISR);
        mask &= ~0x04;
        mask = (uint16_t)(inp(PIC_PORT2)<<8) | mask;
    }
    STIL();
    if(mask == 0)
        return 0xFF;
    return (uint8_t)BSF(mask);
}

//use untrapped io for IRQ masking
//while for IRQ/EOI, use trapped io because VHDPMI virtualized it
#define inp UntrappedIO_IN
#define outp UntrappedIO_OUT

void PIC_MaskIRQ(uint8_t irq)
{
    uint16_t port = PIC_DATA1;
    if(irq >= 8)
    {
        port = PIC_DATA2;
        irq = (uint8_t)(irq - 8);
    }
    CLIS();
    outp(port, (uint8_t)(inp(port)|(1<<irq)));
    STIL();
}

void PIC_UnmaskIRQ(uint8_t irq)
{
    uint16_t port = PIC_DATA1;
    CLIS();
    if(irq >= 8)
    {
        uint8_t master = inp(port);
        if(master&0x4)
            outp(port, (uint8_t)(master&~0x4)); //unmask slave
        port = PIC_DATA2;
        irq = (uint8_t)(irq - 8);
    }
    outp(port, (uint8_t)(inp(port)&~(1<<irq)));
    STIL();
}

uint16_t PIC_GetIRQMask(void)
{
    CLIS();
    uint16_t mask = (uint16_t)((inp(PIC_DATA2)<<8) | inp(PIC_DATA1));
    STIL();
    return mask;
}

void PIC_SetIRQMask(uint16_t mask)
{
    CLIS();
    outp(PIC_DATA1, (uint8_t)mask);
    outp(PIC_DATA2, (uint8_t)(mask>>8));
    STIL();
}
