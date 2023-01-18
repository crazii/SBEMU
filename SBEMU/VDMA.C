#include "VDMA.H"
#include "dos.h"
#include "UNTRAPIO.H"

static uint16_t VDMA_Regs[16];
static uint8_t VDMA_PageAddr[16];
static const uint8_t VDMA_ChannelMap[8] = 
{
    0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A,
};

void VDMA_Write(uint16_t port, uint8_t byte)
{
    if(port >= VDMA_REG_STATUS_CMD && port <= VDMA_REG_MULTIMASK)
    {
        if(port == VDMA_REG_FLIPFLOP)
            VDMA_Regs[port] = 0;
        else
            VDMA_Regs[port] = byte;
    }
    else if(port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER)
    {
        if(((VDMA_Regs[VDMA_REG_FLIPFLOP]++)&0x1) == 0)
            VDMA_Regs[port] = (VDMA_Regs[port]&~0xFF) | byte;
        else
            VDMA_Regs[port] = (VDMA_Regs[port]&~0xFF00) | (byte<<8);
    }
    else
        VDMA_PageAddr[port-0x80] = byte;

    UntrappedIO_OUT(port, byte);
}

uint8_t VDMA_Read(uint16_t port)
{
    return UntrappedIO_IN(port);
}

uint32_t VDMA_GetAddress(int channel)
{
    return VDMA_Regs[channel*2] | (VDMA_PageAddr[VDMA_ChannelMap[channel]-80]<<16);
}

uint32_t VDMA_GetCounter(int channel)
{
    return VDMA_Regs[channel*2+1] + 1;
}

int DMA_GetAuto()
{
    return VDMA_Regs[0x0B]&(1<<4);
}