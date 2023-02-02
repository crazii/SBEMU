#include "VDMA.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"

//registers
static uint16_t VDMA_Regs[16];
static uint8_t VDMA_PageRegs[16];

//internal datas
static uint8_t VDMA_VMask[8];
static int32_t VDMA_Index[16];
static int32_t VDMA_Counter[16];
static uint32_t VDMA_Addr[16];

static uint8_t VDMA_Complete[8];
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
        if((port&0x1) == 0)//addr
        {
            VDMA_Index[port>>1] = 0;
            VDMA_Addr[port>>1] = (VDMA_Addr[port>>1]&~0xFFFF) | VDMA_Regs[port];
        }
        else //counter
            VDMA_Counter[port>>1] = VDMA_Regs[port];
    }
    else
    {
        int channel = VDMA_PortChannelMap[port-0x80];
        if(channel != -1)
        {
            VDMA_PageRegs[channel] = byte;
            VDMA_Index[channel] = 0;
            VDMA_Addr[channel] &= 0xFFFF;
            VDMA_Addr[channel] |= byte<<16;
        }
    }

    UntrappedIO_OUT(port, byte);
}

uint8_t VDMA_Read(uint16_t port)
{
    _LOG("VDMA read: %x\n", port);
    int channel = port>>1;
    if(VDMA_VMask[channel])
    {
        if(port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER)
        {
            //int addr = VDMA_Regs[channel*2];
            //int counter = VDMA_Regs[channel*2+1];
            //int vaule = ((port&0x1) == 1) ? counter : addr;
            int value = VDMA_Regs[port];
            if(((VDMA_Regs[VDMA_REG_FLIPFLOP]++)&0x1) == 0)
                return value&0xFF;
            else
                return ((value>>8)&0xFF);
        }
        else if(port >= VDMA_REG_CH0_PAGEADDR && port <= VDMA_REG_CH3_PAGEADDR)
                return VDMA_PageRegs[channel];
    }

    uint8_t result = UntrappedIO_IN(port);

    if(port == VDMA_REG_STATUS_CMD)
    {
        for(int i = 0; i < 4; ++i)
        {
            if(VDMA_VMask[i])
            {
                result &= ~((1<<i) | (1<<(i+4)));
                result |= VDMA_Complete[i] ? (1<<i) : 0;
                //result |= !VDMA_Complete[i] && VDMA_Index[i] != -1 && VDMA_Index[i] < VDMA_GetCounter(i) ? (1<<(i+4)) : 0;
                VDMA_Complete[i] = 0; //clear on read
            }
        }
        _LOG("VDMA status: %02x\n", result);
    }
    return result;
}

void VDMA_Virtualize(int channel, int enable)
{
    if(channel >= 0 && channel <= 7)
        VDMA_VMask[channel] = enable ? 1: 0;
}

uint32_t VDMA_GetAddress(int channel)
{
    //return VDMA_Regs[channel*2] | (VDMA_PageAddr[channel]<<16L);
    return VDMA_Addr[channel];
}

uint32_t VDMA_GetCounter(int channel)
{
    return VDMA_Regs[channel*2+1] + 1;
}

int32_t VDMA_GetIndex(int channel)
{
    return VDMA_Index[channel];
}

int32_t VDMA_SetIndexCounter(int channel, int32_t index, int32_t counter)
{
    if(counter <= 0)
    {
        counter = 0x10000;
        VDMA_ToggleComplete(channel);
        if(VDMA_GetAuto())
        {
            index = 0;
            counter = VDMA_Counter[channel]+1;
        }
    }
    VDMA_Regs[(channel<<1)+1] = counter-1; //update counter reg
    VDMA_Regs[(channel<<1)] = VDMA_Addr[channel] + index; //update addr reg
    VDMA_PageRegs[channel] = (VDMA_Addr[channel] + index) >> 16;
    return VDMA_Index[channel] = index;
}

int VDMA_GetAuto()
{
    return VDMA_Regs[0x0B]&(1<<4);
}

void VDMA_ToggleComplete(int channel)
{
    VDMA_Complete[channel] = 1;
}