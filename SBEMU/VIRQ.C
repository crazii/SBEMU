#include "VIRQ.H"
#include "PIC.H"
#include "DPMI/DPMI.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"

static uint8_t VIRQ_ISR[2];
static uint8_t VIRQ_OCW[2];
static int VIRQ_Irq = -1;

#define VIRQ_IS_VIRTUALIZING() (VIRQ_Irq != -1)

void VIRQ_Write(uint16_t port, uint8_t value)
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        if(port&0x0F == 0x0)
        {
            int index = ((port==0x20) ? 0 : 1);
            VIRQ_OCW[index] = value;

            if(value == 0x20) //EOI: clear ISR
            {
                VIRQ_ISR[index] = 0;
                return; //don't send to real PIC. it's virtualized
            }

            if(value == 0x0B) //read ISR
                return; //don't send to real PIC, will be handled in VIRQ_Read.
        }
    }
    UnTrappedIO_Write(port, value);
}

uint8_t VIRQ_Read(uint16_t port)
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        int index = ((port==0x21) ? 0 : 1);
        return VIRQ_OCW[index] == 0x0B ? VIRQ_ISR[index] : UnTrappedIO_Read(port);
    }
    return UnTrappedIO_Read(port);
}

void VIRQ_Invoke(uint8_t irq)
{
    VIRQ_Irq = irq;

    VIRQ_ISR[0] = VIRQ_ISR[1] = 0;
    if(irq < 8) //master
        VIRQ_ISR[0] = 1 << irq;
    else //slave
    {
        VIRQ_ISR[0] = 0x04; //cascade
        VIRQ_ISR[1] = 1 << (irq-8);
    }

    DPMI_REG r = {0};
    DPMI_CallRealModeINT(PIC_IRQ2VEC(irq), &r);
    VIRQ_Irq = -1;
}
