#include "VIRQ.H"
#include "PIC.H"
#include "DPMI/DPMI.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"
#include <dos.h>
#include <string.h>

static int VIRQ_Irq = -1;
static uint8_t VIRQ_ISR[2];
static uint8_t VIRQ_OCW[2];

#define VIRQ_IS_VIRTUALIZING() (VIRQ_Irq != -1)

void VIRQ_Write(uint16_t port, uint8_t value)
{
    //_LOG("VIRQW:%x,%x\n",port,value);
    if(VIRQ_IS_VIRTUALIZING())
    {
        _LOG("VIRQW:%x,%x\n",port,value);
        if((port&0x0F) == 0x00)
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
        return;
    }
    UntrappedIO_OUT(port, value);
}

uint8_t VIRQ_Read(uint16_t port)
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        //_LOG("VIRQR:%x\n",port);
        if((port&0x0F) == 0x00)
        {
            int index = ((port==0x20) ? 0 : 1);
            if(VIRQ_OCW[index] == 0x0B)//ISR
            {
                _LOG("VIRQR: %x %x\n",port, VIRQ_ISR[index]);
                return VIRQ_ISR[index];
            }
            //return VIRQ_OCW[index] == 0x0B ? VIRQ_ISR[index] : UntrappedIO_IN(port);
        }
        _LOG("VIRQR: %x 0\n", port);
        return 0;
    }
    return UntrappedIO_IN(port);
}

void VIRQ_Invoke(uint8_t irq, DPMI_REG* reg, BOOL VM)
{
    _LOG("CALLINT %d\n", irq);
    //CLIS();
    int mask = PIC_GetIRQMask();
    PIC_SetIRQMask(0xFFFF);
    VIRQ_ISR[0] = VIRQ_ISR[1] = 0;
    if(irq < 8) //master
        VIRQ_ISR[0] = 1 << irq;
    else //slave
    {
        VIRQ_ISR[0] = 0x04; //cascade
        VIRQ_ISR[1] = 1 << (irq-8);
    }
    
    VIRQ_Irq = irq;
    if(VM || 1) //pm/rm int method not working good yet (Miles Sound)
    {
        #if 1
        DPMI_REG r = {0};
        int n = PIC_IRQ2VEC(irq);
        r.w.ip = DPMI_LoadW(n*4);
        r.w.cs = DPMI_LoadW(n*4+2);
        DPMI_CallRealModeIRET(&r);
        #else
        DPMI_REG r = *reg;
        r.w.ss = r.w.sp = 0;
        DPMI_CallRealModeINT(PIC_IRQ2VEC(irq), &r); //now this works with new HDPMI
        #endif
    }
    else
    {
        if(irq == 5)
            DPMI_CallProtectedModeINT(0x0D, *reg);
        else //7
            DPMI_CallProtectedModeINT(0x0F, *reg);
    }
    VIRQ_Irq = -1;

    //_LOG("CPU FLAGS: %x\n", CPU_FLAGS());
    PIC_SetIRQMask(mask);
    //STIL();
    _LOG("CALLINTEND\n");
}
