#include "VDMA.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"

static uint16_t VDMA_Regs[16];
static int32_t VDMA_Index[16];
static uint8_t VDMA_PageAddr[16];
static uint8_t VDMA_VMask[8];
static const uint8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0,
    -1, 6, 7, 5, -1, -1, -1, 4,
};


void VDMA_Write(uint16_t port, uint8_t byte)
{
    _LOG("VDMA write: %x, %x %d\n", port, byte, UntrappedIO_PM);
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
        //if((port&0x1) == 0)//addr
            VDMA_Index[port>>1] = -1;
    }
    else
    {
        int channel = VDMA_PortChannelMap[port-0x80];
        if(channel != -1)
        {
            VDMA_PageAddr[channel] = byte;
            VDMA_Index[channel] = -1;
        }
    }

    UntrappedIO_OUT(port, byte);
}

uint8_t VDMA_Read(uint16_t port)
{
    _LOG("VDMA read: %x\n", port);
    if(port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER)
    {
        int channel = port>>1;
        if(VDMA_VMask[channel] && (port&0x1) == 1)//counter
        {
            int counter = VDMA_Index[channel] == -1 ? VDMA_GetCounter(channel)-1 : VDMA_GetCounter(channel)-1 - VDMA_Index[channel];
            if(((VDMA_Regs[VDMA_REG_FLIPFLOP]++)&0x1) == 0)
                return counter&0xFF;
            else
                return ((counter>>8)&0xFF);
        }
    }
    return UntrappedIO_IN(port);
}

void VDMA_Virtualize(int channel, int enable)
{
    if(channel >= 0 && channel <= 7)
        VDMA_VMask[channel] = enable ? 1: 0;
}

uint32_t VDMA_GetAddress(int channel)
{
    return VDMA_Regs[channel*2] | (VDMA_PageAddr[channel]<<16L);
}

uint32_t VDMA_GetCounter(int channel)
{
    return VDMA_Regs[channel*2+1] + 1;
}

int32_t VDMA_GetIndex(int channel)
{
    return VDMA_Index[channel];
}

int32_t VDMA_SetIndex(int channel, int32_t index)
{
    uint32_t counter = VDMA_GetCounter(channel);
    if(index >= counter)
    {
        if(DMA_GetAuto())
            index = 0;
        else
            index = -1;
    }
    return VDMA_Index[channel] = index;
}

int DMA_GetAuto()
{
    return VDMA_Regs[0x0B]&(1<<4);
}