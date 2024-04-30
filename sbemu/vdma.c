#include "vdma.h"
#include "untrapio.h"
#include "dpmi/dbgutil.h"

//registers
static uint16_t VDMA_Regs[32];
static uint8_t VDMA_PageRegs[8];
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
static const uint8_t VDMA_PortChannelMap[16] = //0x8x map
{
    -1, 2, 3, 1, -1, -1, -1, 0,
    -1, 6, 7, 5, -1, -1, -1, 4,
};

#define VMDA_IS_CHANNEL_VIRTUALIZED(channel) (channel != -1 && VDMA_VMask[channel])

void VDMA_Write(uint16_t port, uint8_t byte)
{
    _LOG("VDMA write: %x, %x\n", port, byte);
    if((port >= VDMA_REG_STATUS_CMD && port <= VDMA_REG_MULTIMASK) || (port >= VDMA_REG_STATUS_CMD16 && port <= VDMA_REG_MULTIMASK16))
    {
        int base = 0;
        int channelbase = 0;
        if((port >= VDMA_REG_STATUS_CMD16 && port <= VDMA_REG_MULTIMASK16))
        {
            port = (port - VDMA_REG_STATUS_CMD16)/2 + 8; //map to 0x8~0xF
            base = 16;
            channelbase = 4;
        }

        if(port == VDMA_REG_FLIPFLOP)
            VDMA_Regs[base+port] = 0;
        else if(port == VDMA_REG_MODE)
        {
            int channel = byte&0x3; //0~3
            VDMA_Modes[channelbase+channel] = byte&~0x3;
        }
        else
            VDMA_Regs[base+port] = byte;
    }
    else if((port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER) || (port >= VDMA_REG_CH4_ADDR && port <= VDMA_REG_CH7_COUNTER))
    {
        int channel = (port>>1);
        int base = 0;

        if((port >= VDMA_REG_CH4_ADDR && port <= VDMA_REG_CH7_COUNTER))
        {
            port = (port - VDMA_REG_CH4_ADDR)/2; //map to 0x0~0x7
            base = 16;
            channel = (port>>1) + 4;
        }
        //_LOG("base:%d port:%d\n", base, port);

        if(((VDMA_Regs[base+VDMA_REG_FLIPFLOP]++)&0x1) == 0)
        {
            VDMA_InIO[channel] = TRUE;
            VDMA_Regs[base+port] = (VDMA_Regs[base+port]&~0xFF) | byte;
        }
        else
        {
            VDMA_Regs[base+port] = (VDMA_Regs[base+port]&~0xFF00) | (byte<<8);
            VDMA_InIO[channel] = FALSE;
            VDMA_DelayUpdate[channel] = FALSE;
        }

        if((port&0x1) == 0)//addr
        {
            VDMA_Index[channel] = 0;
            VDMA_Addr[channel] = (VDMA_Addr[channel]&~0xFFFF) | VDMA_Regs[base+port];
        }
        else //counter
            VDMA_CurCounter[channel] = VDMA_Counter[channel] = VDMA_Regs[base+port] + 1;
    }
    else if(port >= 0x80 && port <= 0x8F)//page registers 0x87~0x8F
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
    int channel = -1;
    if(port <= VDMA_REG_CH3_COUNTER) channel = (port>>1);
    else if(port >= VDMA_REG_CH4_ADDR && port <= VDMA_REG_CH7_COUNTER) channel = ((port-VDMA_REG_CH4_ADDR)/4)+4;
    else if(port>=0x80 && port <= 0x8F) channel = VDMA_PortChannelMap[port-0x80];

    if(VMDA_IS_CHANNEL_VIRTUALIZED(channel))
    {
        if((port >= VDMA_REG_CH0_ADDR && port <= VDMA_REG_CH3_COUNTER) || (port >= VDMA_REG_CH4_ADDR && port <= VDMA_REG_CH7_COUNTER) )
        {
            int base = 0;
            if((port >= VDMA_REG_CH4_ADDR && port <= VDMA_REG_CH7_COUNTER))
            {
                port = (port-VDMA_REG_CH4_ADDR)/2;
                base = 16;
            }
            //_LOG("base:%d port:%d\n", base, port);

            int value = VDMA_Regs[base+port];
            _LOG("VDMA %s: %d\n", ((port&0x1) == 1) ? "counter" : "addr", value);
            if(((VDMA_Regs[base+VDMA_REG_FLIPFLOP]++)&0x1) == 0)
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
                    int base2 = channel <= 3 ? (channel<<1) : 16+((channel-4)<<1);
                    VDMA_Regs[base2+1] = VDMA_CurCounter[channel]-1; //update counter reg
                    VDMA_Regs[base2] = VDMA_Addr[channel] + VDMA_Index[channel]; //update addr reg
                    VDMA_PageRegs[channel] = (VDMA_Addr[channel] + VDMA_Index[channel]) >> 16;
                }
                VDMA_InIO[channel] = FALSE;
                VDMA_DelayUpdate[channel] = FALSE;
                return ret;
            }
        }
        else
            return VDMA_PageRegs[channel];
    }

    uint8_t result = UntrappedIO_IN(port);

    if(port == VDMA_REG_STATUS_CMD)
    {
        for(int i = 0; i < 8; ++i)
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
    {
        _LOG("VDMA channel: %d %s\n", channel, enable ? "enabled" : "disabled");
        VDMA_VMask[channel] = enable ? 1: 0;
    }
}

uint32_t VDMA_GetAddress(int channel)
{
    int size = channel <= 3 ? 1 : 2;
    uint32_t page = VDMA_Addr[channel]&0xFF0000; //page not /2
    return (page | (VDMA_Addr[channel]&0xFFFF)*size); //addr reg for 16 bit is real addr/2.
}

uint32_t VDMA_GetCounter(int channel)
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_CurCounter[channel] * size; //counter for 16 bit is in words
}

int32_t VDMA_GetIndex(int channel)
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_Index[channel] * size;
}

int32_t VDMA_SetIndexCounter(int channel, int32_t index, int32_t counter)
{
    const int size = channel <= 3 ? 1 : 2;
    int base = channel <= 3 ? (channel<<1) : 16+((channel-4)<<1);

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
    //_LOG("counter: %d, index: %d, size: %d\n", counter, index, size);
    VDMA_CurCounter[channel] = counter / size;
    VDMA_Index[channel] = index / size;
    
    if(!VDMA_InIO[channel])
    {
        //_LOG("VDMA channel: %d, addr: %x\n", channel, VDMA_Addr[channel]);
        VDMA_Regs[base+1] = VDMA_CurCounter[channel]-1; //update counter reg
        VDMA_Regs[base] = VDMA_Addr[channel] + VDMA_Index[channel]; //update addr reg
        VDMA_PageRegs[channel] = (VDMA_Addr[channel] + VDMA_Index[channel]) >> 16;
    }
    else
        VDMA_DelayUpdate[channel] = TRUE;
    //_LOG("cur counter: %d\n", counter);
    return VDMA_Index[channel] * size;
}

int VDMA_GetAuto(int channel)
{
    return VDMA_Modes[channel]&(1<<4);
}

int VDMA_GetWriteMode(int channel)
{
    return (VDMA_Modes[channel]&0x0C) == 0x04;
}

void VDMA_ToggleComplete(int channel)
{
    VDMA_Complete[channel] = 1;
}

void VDMA_WriteData(int channel, uint8_t data)
{
    if(VDMA_GetWriteMode(channel))
    {
        uint32_t addr = VDMA_GetAddress(channel);
        int32_t index = VDMA_GetIndex(channel);

        uint32_t laddr = addr;        
        if(addr>1024*1024)
            laddr = DPMI_MapMemory(addr, 65536);

        _LOG("dmaw: %x, %d\n", addr+index, data);
        DPMI_CopyLinear(laddr+index, DPMI_PTR2L(&data), 1);

        if(addr>1024*1024)
            DPMI_UnmappMemory(laddr);
        VDMA_SetIndexCounter(channel, index+1, VDMA_GetCounter(channel)-1);
    }
}
