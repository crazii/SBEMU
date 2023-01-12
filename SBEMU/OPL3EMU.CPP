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

//secondary index read (Adlib Gold). reference: AIL2.0 source code, dosbox
#define OPL3EMU_ADLG_IOBUSY 0x40UL
#define OPL3EMU_ADLG_VOLL_REG_INDEX 9 //left volume
#define OPL3EMU_ADLG_VOLR_REG_INDEX 10 //right volume

//data
#define KEY_ON 0x10 //channel on bit
static uint32_t ADLG_CtrlEnable = 0;    //seems not working for Miles Sound, don't use it
static uint32_t ADLG_Volume[2] = {0x08,0x08};

static DBOPL::Chip* chip;

static uint32_t MAIN_OPLTimerCtrlReg[2]; //if start 1 and 2 seperately we will miss one, so use 2 cache
static uint8_t MAIN_OPLIndexReg[2];

void OPL3EMU_Init(int freq)
{
    chip = new DBOPL::Chip(true);
    chip->Setup(freq);
}

void OPL3EMU_GenSamples(int32_t* pcm32, int count)
{
    chip->Generate(pcm32, count);
}

int OPL3EMU_PrimaryRead(int val)
{
    val &= ~0xFF;
    if ((MAIN_OPLTimerCtrlReg[0] & (OPL3EMU_TIMER1_MASK|OPL3EMU_TIMER1_START)) == OPL3EMU_TIMER1_START)
        val |= OPL3EMU_TIMER1_TIMEOUT;
    if ((MAIN_OPLTimerCtrlReg[1] & (OPL3EMU_TIMER2_MASK|OPL3EMU_TIMER2_START)) == OPL3EMU_TIMER2_START)
        val |= OPL3EMU_TIMER2_TIMEOUT;
    return val;
}

int OPL3EMU_PrimaryWriteIndex(int val)
{
    MAIN_OPLIndexReg[OPL3EMU_PRIMARY] = val;
    return chip->WriteAddr(0x388, val);
}

int OPL3EMU_PrimaryWriteData(int val)
{
    if(MAIN_OPLIndexReg[OPL3EMU_PRIMARY] == OPL3EMU_TIMER_REG_INDEX)
    {
        if(val&(OPL3EMU_TIMER1_START|OPL3EMU_TIMER1_MASK))
            MAIN_OPLTimerCtrlReg[0] = val;
        if(val&(OPL3EMU_TIMER2_START|OPL3EMU_TIMER2_MASK))
            MAIN_OPLTimerCtrlReg[1] = val;
    }
    chip->WriteReg(MAIN_OPLIndexReg[OPL3EMU_PRIMARY], val);
    return val;
}

int OPL3EMU_SecondaryRead(int val)
{
    if(MAIN_OPLIndexReg[OPL3EMU_SECONDARY] == OPL3EMU_ADLG_VOLL_REG_INDEX || MAIN_OPLIndexReg[OPL3EMU_SECONDARY] == OPL3EMU_ADLG_VOLR_REG_INDEX)
            return ADLG_Volume[MAIN_OPLIndexReg[OPL3EMU_SECONDARY]-OPL3EMU_ADLG_VOLL_REG_INDEX];
    return OPL3EMU_PrimaryRead(val);
}

int OPL3EMU_SecondaryWriteIndex(int val)
{
    if(val == 0xFF)
        ADLG_CtrlEnable = 1;
    else if(val == 0xFE)
        ADLG_CtrlEnable = 0;

    MAIN_OPLIndexReg[OPL3EMU_SECONDARY] = (uint8_t)val;
    return chip->WriteAddr(0x38A, val);
}

int OPL3EMU_SecondaryWriteData(int val)
{
    if(/*ADLG_CtrlEnable && */(MAIN_OPLIndexReg[OPL3EMU_SECONDARY] == OPL3EMU_ADLG_VOLL_REG_INDEX || MAIN_OPLIndexReg[OPL3EMU_SECONDARY] == OPL3EMU_ADLG_VOLR_REG_INDEX))
        ADLG_Volume[MAIN_OPLIndexReg[OPL3EMU_SECONDARY]-OPL3EMU_ADLG_VOLL_REG_INDEX] = val;
    chip->WriteReg(MAIN_OPLIndexReg[OPL3EMU_SECONDARY], val);
    return val;
}
