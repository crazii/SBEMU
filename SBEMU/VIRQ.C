#include "VIRQ.H"
#include "PIC.H"
#include "DPMI/DPMI.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"
#include <dos.h>

static int VIRQ_Irq = -1;
static uint8_t VIRQ_ISR[2];
static uint8_t VIRQ_OCW[2];

#define VIRQ_IS_VIRTUALIZING() (VIRQ_Irq != -1)

void VIRQ_Write(uint16_t port, uint8_t value)
{
    //_LOG("VIRQW:%x,%x",port,value);
    if(VIRQ_IS_VIRTUALIZING())
    {
        _LOG("VIRQW:%x,%x",port,value);
        if(port&0x0F == 0x00)
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
    UntrappedIO_OUT(port, value);
}

uint8_t VIRQ_Read(uint16_t port)
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        _LOG("VIRQR:%x",port);
        if((port&0x0F) == 0x01)
        {
            int index = ((port==0x21) ? 0 : 1);
            return VIRQ_OCW[index] == 0x0B ? VIRQ_ISR[index] : UntrappedIO_IN(port);
        }
    }
    return UntrappedIO_IN(port);
}

void VIRQ_Invoke(uint8_t irq)
{
    _LOG("CALLINT %d\n", irq);
    CLIS();
    VIRQ_ISR[0] = VIRQ_ISR[1] = 0;
    if(irq < 8) //master
        VIRQ_ISR[0] = 1 << irq;
    else //slave
    {
        VIRQ_ISR[0] = 0x04; //cascade
        VIRQ_ISR[1] = 1 << (irq-8);
    }
    VIRQ_Irq = irq;
    DPMI_REG r = {0};
    DPMI_CallRealModeINT(PIC_IRQ2VEC(irq), &r);
    //int n = PIC_IRQ2VEC(irq);
    //r.w.ip = DPMI_LoadW(n*4);
    //r.w.cs = DPMI_LoadW(n*4+2);
    //DPMI_CallRealModeIRET(&r);
    //__dpmi_paddr pa;
    //__dpmi_get_protected_mode_interrupt_vector(PIC_IRQ2VEC(irq), &pa);
    //asm("pushfl \n\t lcall *%0" ::"m"(pa));
    //asm("int $0x0F");
    VIRQ_Irq = -1;
    STIL();
    _LOG("CALLINTEND", irq);
}
