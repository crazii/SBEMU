#include "VDMA.H"
#include "UNTRAPIO.H"
#include "DPMI/DBGUTIL.H"

//registers
static uint16_t VDMA_Regs[16];
static uint8_t VDMA_PageRegs[16];
static uint8_t VDMA_Modes[8];

//internal datas
static uint8_t VDMA_VMask[8];
static uint32_t VDMA_Addr[8];   //initial addr
static int32_t VDMA_Index[8]; //current addr offset
static int32_t VDMA_Counter[8]; //initial counter
static int32_t VDMA_CurCounter[8]; //current counter

static uint32_t VDMA_InIO[8]; //in the middle of reading counter/addr
static uint8_t VDMA_DelayUpdate[8];

static uint8_t VDMA_Complete[8];
static const uint8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0,
    -1, 6, 7, 5, -1, -1, -1, 4,
};

#define VMDA_IS_CHANNEL_VIRTUALIZED(channel) (VDMA_VMask[channel])

void VDMA_Write(uint16_t port, uint8_t byte)
{
    _LOG("VDMA write: %x, %x\n", port, byte);
    if(port >= VDMA_REG_STATUS_CMD && port <= VDMA_REG_MULTIMASK)
    {
        if(port == VDMA_REG_FLIPFLOP)
            VDMA_Regs[port] = 0;
        else if(port == VDMA_REG_MODE)
        {
            int channel = byte&0x3; //0~3
            VDMA_Modes[channel] = byte&~0x3;
        }
        else
            VDMA_Regs[port] = byte;
    }
    else if(port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER)
    {
        if(((VDMA_Regs[VDMA_REG_FLIPFLOP]++)&0x1) == 0)
        {
            VDMA_InIO[port>>1] = TRUE;        
            VDMA_Regs[port] = (VDMA_Regs[port]&~0xFF) | byte;
        }
        else
        {
            VDMA_Regs[port] = (VDMA_Regs[port]&~0xFF00) | (byte<<8);
            VDMA_InIO[port>>1] = FALSE;
            VDMA_DelayUpdate[port>>1] = FALSE;
        }
        if((port&0x1) == 0)//addr
        {
            VDMA_Index[port>>1] = 0;
            VDMA_Addr[port>>1] = (VDMA_Addr[port>>1]&~0xFFFF) | VDMA_Regs[port];
        }
        else //counter
            VDMA_CurCounter[port>>1] = VDMA_Counter[port>>1] = VDMA_Regs[port] + 1;
    }
    else //page registers 0x87~0x8A
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
    if(VMDA_IS_CHANNEL_VIRTUALIZED(channel))
    {
        if(port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER)
        {
            int value = VDMA_Regs[port];
            _LOG("VDMA %s: %d\n", ((port&0x1) == 1) ? "counter" : "addr", value);
            if(((VDMA_Regs[VDMA_REG_FLIPFLOP]++)&0x1) == 0)
            {
                VDMA_InIO[channel] = TRUE;
                return value&0xFF;
            }
            else
            {
                uint8_t ret = ((value>>8)&0xFF);
                if(VDMA_DelayUpdate[channel])
                {
                    int size = channel <= 3 ? 1 : 2;
                    VDMA_Regs[(channel<<1)+1] = VDMA_CurCounter[channel]-1; //update counter reg
                    VDMA_Regs[(channel<<1)] = VDMA_Addr[channel] + VDMA_Index[channel]*size; //update addr reg
                }
                VDMA_InIO[channel] = FALSE;
                VDMA_DelayUpdate[channel] = FALSE;
                return ret;
            }
        }
        else if(port >= VDMA_REG_CH0_PAGEADDR && port <= VDMA_REG_CH3_PAGEADDR)
                return VDMA_PageRegs[channel];
    }

    uint8_t result = UntrappedIO_IN(port);

    if(port == VDMA_REG_STATUS_CMD)
    {
        for(int i = 0; i < 4; ++i)
        {
            if(VMDA_IS_CHANNEL_VIRTUALIZED(i))
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
    return VDMA_Addr[channel];
}

uint32_t VDMA_GetCounter(int channel)
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_CurCounter[channel] * size;
}

int32_t VDMA_GetIndex(int channel)
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_Index[channel] * size;
}

int32_t VDMA_SetIndexCounter(int channel, int32_t index, int32_t counter)
{
    int size = channel <= 3 ? 1 : 2;

    if(counter <= 0)
    {
        counter = 0x10000*size;
        VDMA_ToggleComplete(channel);
        if(VDMA_GetAuto(channel))
        {
            index = 0;
            counter = VDMA_Counter[channel]*size;
        }
    }
    if(!VDMA_InIO[channel])
    {
        VDMA_Regs[(channel<<1)+1] = counter/size-1; //update counter reg
        VDMA_Regs[(channel<<1)] = VDMA_Addr[channel] + index; //update addr reg
    }
    else
        VDMA_DelayUpdate[channel] = TRUE;
    VDMA_PageRegs[channel] = (VDMA_Addr[channel] + index) >> 16;
    //_LOG("cur counter: %d\n", counter);
    VDMA_CurCounter[channel] = counter / size;
    return (VDMA_Index[channel] = (index / size)) * size;
}

int VDMA_GetAuto(int channel)
{
    return VDMA_Modes[channel]&(1<<4);
}

void VDMA_ToggleComplete(int channel)
{
    VDMA_Complete[channel] = 1;
}