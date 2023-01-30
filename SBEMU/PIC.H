#ifndef _PIC_H_
#define _PIC_H_
//8259 Programmable Interrupt Controller

#include <PLATFORM.H>

//real mode vector mapping (also for DPMI interface)
//although DPMI server might have remapped the PIC, the real mode vector is still used as a compatible interface
//i.e. you can set DPMI interrupt vector (real mode or protected mode) using those number.
#define PIC_IRQ0_VEC 0x08   //PIC master
#define PIC_IRQ8_VEC 0x70   //PIC slave

#define PIC_IRQ2VEC(irq) ((uint8_t)((irq) < 8 ? PIC_IRQ0_VEC + (irq) : PIC_IRQ8_VEC + ((irq) - 8)))
#define PIC_VEC2IRQ(ivec) ((uint8_t)((ivec) < PIC_IRQ0_VEC + 8 ? (ivec) - 8 : (ivec) - PIC_IRQ8_VEC + 8))

#ifdef __cplusplus
extern "C"
{
#endif

//send end of interrupt. a little faster than PIC_SendEOI. MUST be called in interrupt handler
void PIC_SendEOIWithIRQ(uint8_t irq);

//send end of interrupt. MUST be called in interrupt handler
void PIC_SendEOI(void);

//get interrupting IRQ. MUST be called in interrupt handler
uint8_t PIC_GetIRQ(void);

//remap PIC, not used
void PIC_RemapMaster(uint8_t vector);

//remap PIC slave
void PIC_RemapSlave(uint8_t vector);

//mask an irq line
void PIC_MaskIRQ(uint8_t irq);

//unmask an irq line
void PIC_UnmaskIRQ(uint8_t irq);

uint16_t PIC_GetIRQMask(void);

void PIC_SetIRQMask(uint16_t mask);

#define PIC_IS_IRQ_MASKED(mask, irq) (((mask)&(1<<(irq))))
#define PIC_IRQ_MASK(mask, irq) (((mask)|(1<<(irq))))
#define PIC_IRQ_UNMASK(mask, irq) (((mask)&(~(1<<(irq)))))

#ifdef __cplusplus
}
#endif

#endif//_PIC_H_