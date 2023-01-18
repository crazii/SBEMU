#include "OPL3EMU.H"
#include "dbopl.h"

#define OPL3EMU_PRIMARY 0
#define OPL3EMU_SECONDARY 1
//primary index read
#define OPL3EMU_TIMER_REG_INDEX 4
#define OPL3EMU_TIMER1_MASK 0xC0
#define OPL3EMU_TIMER2_MASK 0xA0
#define OPL3EMU_TIMER1_START 0x01
#define OPL3EMU_TIMER2_START 0x02
#define OPL3EMU_TIMER1_TIMEOUT OPL3EMU_TIMER1_MASK
#define OPL3EMU_TIMER2_TIMEOUT OPL3EMU_TIMER2_MASK
static uint32_t OPL3EMU_TimerCtrlReg[2]; //if start 1 and 2 seperately we will miss one, so use 2 cache
static uint32_t OPL3EMU_IndexReg[2];

//secondary index read (Adlib Gold). reference: AIL2.0 source code, dosbox
#define OPL3EMU_ADLG_IOBUSY 0x40UL
#define OPL3EMU_ADLG_VOLL_REG_INDEX 9 //left volume
#define OPL3EMU_ADLG_VOLR_REG_INDEX 10 //right volume
//data
#define KEY_ON 0x10 //channel on bit
static uint32_t OPL3EMU_ADLG_CtrlEnable = 0;    //seems not working for Miles Sound, don't use it
static uint32_t OPL3EMU_ADLG_Volume[2] = {0x08,0x08};

static DBOPL::Chip* OPL3EMU_Chip;

void OPL3EMU_Init(int samplerate)
{
    OPL3EMU_Chip = new DBOPL::Chip(true);
    OPL3EMU_Chip->Setup(samplerate);
}

int OPL3EMU_GetMode()
{
    return OPL3EMU_Chip->opl3Active;
}

int OPL3EMU_GenSamples(int16_t* pcm16, int count)
{
    return OPL3EMU_Chip->Generate(pcm16, count);
}

uint32_t OPL3EMU_PrimaryRead(uint32_t val)
{
    val &= ~0xFF;
    if ((OPL3EMU_TimerCtrlReg[0] & (OPL3EMU_TIMER1_MASK|OPL3EMU_TIMER1_START)) == OPL3EMU_TIMER1_START)
        val |= OPL3EMU_TIMER1_TIMEOUT;
    if ((OPL3EMU_TimerCtrlReg[1] & (OPL3EMU_TIMER2_MASK|OPL3EMU_TIMER2_START)) == OPL3EMU_TIMER2_START)
        val |= OPL3EMU_TIMER2_TIMEOUT;
    return val;
}

uint32_t OPL3EMU_PrimaryWriteIndex(uint32_t val)
{
    OPL3EMU_IndexReg[OPL3EMU_PRIMARY] = OPL3EMU_Chip->WriteAddr(0x388, val);
    return val;
}

uint32_t OPL3EMU_PrimaryWriteData(uint32_t val)
{
    if(OPL3EMU_IndexReg[OPL3EMU_PRIMARY] == OPL3EMU_TIMER_REG_INDEX)
    {
        if(val&(OPL3EMU_TIMER1_START|OPL3EMU_TIMER1_MASK))
            OPL3EMU_TimerCtrlReg[0] = val;
        if(val&(OPL3EMU_TIMER2_START|OPL3EMU_TIMER2_MASK))
            OPL3EMU_TimerCtrlReg[1] = val;
    }
    OPL3EMU_Chip->WriteReg(OPL3EMU_IndexReg[OPL3EMU_PRIMARY], val);
    return val;
}

uint32_t OPL3EMU_SecondaryRead(uint32_t val)
{
    if(OPL3EMU_IndexReg[OPL3EMU_SECONDARY] == 0x100+OPL3EMU_ADLG_VOLL_REG_INDEX || OPL3EMU_IndexReg[OPL3EMU_SECONDARY] == 0x100+OPL3EMU_ADLG_VOLR_REG_INDEX)
            return OPL3EMU_ADLG_Volume[OPL3EMU_IndexReg[OPL3EMU_SECONDARY]-OPL3EMU_ADLG_VOLL_REG_INDEX];
    return OPL3EMU_PrimaryRead(val);
}

uint32_t OPL3EMU_SecondaryWriteIndex(uint32_t val)
{
    if(val == 0xFF)
        OPL3EMU_ADLG_CtrlEnable = 1;
    else if(val == 0xFE)
        OPL3EMU_ADLG_CtrlEnable = 0;

    OPL3EMU_IndexReg[OPL3EMU_SECONDARY] = OPL3EMU_Chip->WriteAddr(0x38A, val);
    return val;
}

uint32_t OPL3EMU_SecondaryWriteData(uint32_t val)
{
    if(/*OPL3EMU_ADLG_CtrlEnable && */(OPL3EMU_IndexReg[OPL3EMU_SECONDARY] == 0x100+OPL3EMU_ADLG_VOLL_REG_INDEX || OPL3EMU_IndexReg[OPL3EMU_SECONDARY] == 0x100+OPL3EMU_ADLG_VOLR_REG_INDEX))
        OPL3EMU_ADLG_Volume[OPL3EMU_IndexReg[OPL3EMU_SECONDARY]-OPL3EMU_ADLG_VOLL_REG_INDEX] = val;
    OPL3EMU_Chip->WriteReg(OPL3EMU_IndexReg[OPL3EMU_SECONDARY], val);
    return val;
}
