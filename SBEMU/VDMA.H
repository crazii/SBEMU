#ifndef _VDMA_H_
#define _VDMA_H_
//ISA DMA virtualization
//https://wiki.osdev.org/ISA_DMA
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

//words
#define VDMA_REG_CH0_ADDR       0x00
#define VDMA_REG_CH0_COUNTER    0x01
#define VDMA_REG_CH1_ADDR       0x02
#define VDMA_REG_CH1_COUNTER    0x03
#define VDMA_REG_CH2_ADDR       0x04
#define VDMA_REG_CH2_COUNTER    0x05
#define VDMA_REG_CH3_ADDR       0x06
#define VDMA_REG_CH3_COUNTER    0x07
//bytes
#define VDMA_REG_STATUS_CMD     0x08 //status (R) / command (W)
#define VDMA_REG_REQUEST        0x09
#define VDMA_REG_SINGLEMASK     0x0A
#define VDMA_REG_MODE           0x0B
#define VDMA_REG_FLIPFLOP       0x0C
#define VDMA_REG_IMM_RESET      0x0D //immediate (R) / master reset (W)
#define VDMA_REG_MASK_RESET     0x0E
#define VDMA_REG_MULTIMASK      0x0F

#define VDMA_REG_CH0_PAGEADDR   0x87
#define VDMA_REG_CH1_PAGEADDR   0x83
#define VDMA_REG_CH2_PAGEADDR   0x81
#define VDMA_REG_CH3_PAGEADDR   0x82

void VDMA_Write(uint16_t port, uint8_t byte);
uint8_t VDMA_Read(uint16_t port);

void VDMA_Virtualize(int channel, int enable);
uint32_t VDMA_GetAddress(int channel);
uint32_t VDMA_GetCounter(int channel);
int32_t VDMA_GetIndex(int channel);
int32_t VDMA_SetIndexCounter(int channel, int32_t index, int32_t counter);
int VDMA_GetAuto(int channel);
void VDMA_ToggleComplete(int channel);

#ifdef __cplusplus
}
#endif

#endif//_VDMA_H_