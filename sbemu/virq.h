#ifndef _VIRQ_H_
#define _VIRQ_H_
//IRQ virtualization
//https://wiki.osdev.org/8259_PIC
#include <stdint.h>
#include <dpmi/dpmi.h>

#ifdef __cplusplus
extern "C"
{
#endif

void VIRQ_Init();

void VIRQ_Write(uint16_t port, uint8_t value);
uint8_t VIRQ_Read(uint16_t port);

void VIRQ_Invoke(uint8_t irq, DPMI_REG* reg, BOOL VM);

#ifdef __cplusplus
}
#endif

#endif//_VIRQ_H_
