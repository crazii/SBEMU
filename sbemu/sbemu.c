#include "platform.h"
#include "dpmi/dbgutil.h"
#include "sbemu.h"
#include "ctadpcm.h"
#include <string.h>

typedef struct 
{
    int step;
    uint8_t ref;
    uint8_t useRef;
}ADPCM_STATE;

//internal cmds
#define SBEMU_DSPCMD_INVALID -1
#define SBEMU_DSPCMD_SKIP1 -2
#define SBEMU_DSPCMD_SKIP2 -3
#define SBEMU_DSPCMD_DIRECT -4 //to resolve conflict of SBEMU_CMD_8BIT_DIRECT / SBEMU_CMD_MODE_PCM16_MONO

#define SBEMU_RESET_START 0
#define SBEMU_RESET_END 1
#define SBEMU_RESET_POLL 2

#define SBEMU_DELAY_FOR_IRQ for(volatile int i = 0; i < 0xFFFFFFF; ++i) NOP()

SBEMU_EXTFUNS* SBEMU_ExtFuns;
static int SBEMU_ResetState = SBEMU_RESET_END;
static int SBEMU_Started = 0;
static int SBEMU_IRQ = 5;
static int SBEMU_DMA = 1;
static int SBEMU_HDMA = 5;
static int SBEMU_DACSpeaker = 1;
static int SBEMU_Bits = 8;
static int SBEMU_SampleRate = 22050;
static int SBEMU_Samples = 0;
static int SBEMU_Auto = 0;
static int SBEMU_HighSpeed = 0;
static int SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
static int SBEMU_DSPCMD_Subindex = 0;
static int SBEMU_DSPDATA_Subindex = 0;
static int SBEMU_TriggerIRQ = 0;
static int SBEMU_Pos = 0;
static int SBEMU_DetectionCounter = 0;
static int SBEMU_DirectCount = 0;
static int SBEMU_UseTimeConst = 0;
static uint8_t SBEMU_IRQMap[4] = {2,5,7,10};
static uint8_t SBEMU_MixerRegIndex = 0;
static uint8_t SBEMU_idbyte;
static uint8_t SBEMU_WS;
static uint8_t SBEMU_RS = 0x2A;
static uint8_t SBEMU_TestReg;
static uint8_t SBEMU_DMAID_A;
static uint8_t SBEMU_DMAID_X;
static uint16_t SBEMU_DSPVER = 0x0302;
static ADPCM_STATE SBEMU_ADPCM;

static int SBEMU_TimeConstantMapMono[][2] =
{
    0xA5, 11025,
    0xD2, 22050,
    0xE9, 44100,
};
static uint8_t SBEMU_Copyright[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

static uint8_t SBEMU_MixerRegs[256];
#define SBEMU_DIRECT_BUFFER_SIZE 1024
static uint8_t SBEMU_DirectBuffer[SBEMU_DIRECT_BUFFER_SIZE];

static int SBEMU_Indexof(uint8_t* array, int count, uint8_t  val)
{
    for(int i = 0; i < count; ++i)
    {
        if(array[i] == val)
            return i;
    }
    return -1;
}


void SBEMU_Mixer_WriteAddr(int16_t port, uint8_t value)
{
    _LOG("SBEMU: mixer wirte addr: %x %x\n", port, value);
    SBEMU_MixerRegIndex = value;
}

void SBEMU_Mixer_Write(uint16_t port, uint8_t value)
{
    _LOG("SBEMU: mixer wirte: %x\n", value);
    SBEMU_MixerRegs[SBEMU_MixerRegIndex] = value;
    if(SBEMU_MixerRegIndex == SBEMU_MIXERREG_RESET)
    {
        // Mixer Registers are documented here:
        // https://pdos.csail.mit.edu/6.828/2018/readings/hardware/SoundBlaster.pdf

        // bits: 0000VVV0 (3 bits volume, LSB is zero)
        static const uint8_t MONO_3BIT_FULL = (7 << 1);

        SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERVOL] = MONO_3BIT_FULL; // default 4
        SBEMU_MixerRegs[SBEMU_MIXERREG_MIDIVOL] = MONO_3BIT_FULL; // default 4
        SBEMU_MixerRegs[SBEMU_MIXERREG_CDVOL] = MONO_3BIT_FULL; // default 0

        // bits: 00000VV0 (2 bits volume, LSB is zero)
        static const uint8_t MONO_2BIT_FULL = (3 << 1);

        SBEMU_MixerRegs[SBEMU_MIXERREG_VOICEVOL] = MONO_2BIT_FULL; // default 0

        if(SBEMU_DSPVER < 0x0400) //before SB16
        {
            // bits: LLL0RRR0 (3 bits per channel, LSB in each nibble is zero)
            static const uint8_t STEREO_3BIT_FULL = ((7 << 1) << 4) | (7 << 1);

            SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] = STEREO_3BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] = STEREO_3BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] = STEREO_3BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] = STEREO_3BIT_FULL;
        }
        else //SB16
        {
            // bits: LLLLRRRR (4 bits per channel)
            static const uint8_t STEREO_4BIT_FULL = (0xF << 4) | 0xF;

            SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] = STEREO_4BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] = STEREO_4BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] = STEREO_4BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] = STEREO_4BIT_FULL;

            // bits: VVVVV000 (5 MSB volume, 3 LSB are zero)
            static const uint8_t SINGLE_5BIT_FULL = 0x1F << 3;

            SBEMU_MixerRegs[SBEMU_MIXRREG_MASTERL] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_MASTERR] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_VOICEL] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_VOICER] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_MIDIL] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_MIDIR] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_CDL] = SINGLE_5BIT_FULL;
            SBEMU_MixerRegs[SBEMU_MIXRREG_CDR] = SINGLE_5BIT_FULL;
        }
    }
    if(SBEMU_DSPVER >= 0x0400) //SB16
    {
        if(SBEMU_MixerRegIndex >= SBEMU_MIXRREG_MASTERL && SBEMU_MixerRegIndex <= SBEMU_MIXRREG_CDR)
        {
            //5bits, drop 1 bit
            value = (value >> 4)&0xF;
            switch(SBEMU_MixerRegIndex)
            {
                case SBEMU_MIXRREG_MASTERL: SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] |= (value<<4); break;
                case SBEMU_MIXRREG_MASTERR: SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] |= value; break;
                case SBEMU_MIXRREG_VOICEL: SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] |= (value<<4); break;
                case SBEMU_MIXRREG_VOICER: SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] |= value; break;
                case SBEMU_MIXRREG_MIDIL: SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] |= (value<<4); break;
                case SBEMU_MIXRREG_MIDIR: SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] |= value; break;
                case SBEMU_MIXRREG_CDL: SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] |= (value<<4); break;
                case SBEMU_MIXRREG_CDR: SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_CDSTEREO] |= value; break;
            }
        }
    }
    if(SBEMU_MixerRegIndex == SBEMU_MIXERREG_MODEFILTER && SBEMU_UseTimeConst)
    {
        //divide channels: channels might be set later than time const, order opposite to the SB programming guide. (Game: Epic Pinball)
        SBEMU_SampleRate = SBEMU_SampleRate * SBEMU_UseTimeConst / SBEMU_GetChannels();
        SBEMU_UseTimeConst = SBEMU_GetChannels();
    }
}

uint8_t SBEMU_Mixer_Read(uint16_t port)
{
    _LOG("SBEMU: mixer read: %x\n", SBEMU_MixerRegs[SBEMU_MixerRegIndex]);
    return SBEMU_MixerRegs[SBEMU_MixerRegIndex];
}

void SBEMU_DSP_Reset(uint16_t port, uint8_t value)
{
    _LOG("SBEMU: DSP reset: %d\n",value);
    if(value == 1)
    {
        SBEMU_ResetState = SBEMU_RESET_START;
        SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP] = 1<<SBEMU_Indexof(SBEMU_IRQMap,countof(SBEMU_IRQMap),SBEMU_IRQ);
        SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] = ((1<<SBEMU_DMA)|(SBEMU_HDMA?(1<<SBEMU_HDMA):0))&0xEB;
        SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] = 0xFD; //mask out stereo
        SBEMU_MixerRegIndex = 0;
        SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
        SBEMU_DSPCMD_Subindex = 0;
        SBEMU_DSPDATA_Subindex = 0;
        SBEMU_Started = 0;
        SBEMU_Samples = 0;
        SBEMU_Auto = 0;
        SBEMU_Bits = 8;
        SBEMU_Pos = 0;
        SBEMU_HighSpeed = 0;
        SBEMU_TriggerIRQ = 0;
        SBEMU_DetectionCounter = 0;
        SBEMU_DirectCount = 0;
        SBEMU_DirectBuffer[0] = 0;
        SBEMU_DMAID_A = 0xAA;
        SBEMU_DMAID_X = 0x96;
        SBEMU_UseTimeConst = 0;

        //SBEMU_Mixer_WriteAddr(0, SBEMU_MIXERREG_RESET);
        //SBEMU_Mixer_Write(0, 1);
    }
    if(value == 0 && SBEMU_ResetState == SBEMU_RESET_START)
        SBEMU_ResetState = SBEMU_RESET_POLL;
}

void SBEMU_DSP_Write(uint16_t port, uint8_t value)
{
    _LOG("SBEMU: DSP write %02x, original: %02x\n", value, SBEMU_DSPCMD);
    if(SBEMU_HighSpeed) //highspeed won't accept further commands, need reset
        return;
    int OldStarted = SBEMU_Started;
    if(SBEMU_DSPCMD == -1)
    {
        SBEMU_DSPCMD = value;
        SBEMU_DSPCMD_Subindex = 0;
        switch(SBEMU_DSPCMD)
        {
            case SBEMU_CMD_TRIGGER_IRQ:
            case SBEMU_CMD_TRIGGER_IRQ16:
            {
                SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] |= SBEMU_DSPCMD == SBEMU_CMD_TRIGGER_IRQ ? 0x1 : 0x2;
                SBEMU_TriggerIRQ = 1;
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
                //SBEMU_ExtFuns->RaiseIRQ(SBEMU_GetIRQ());
                SBEMU_DELAY_FOR_IRQ; //hack: add CPU delay so that sound interrupt raises virtual IRQ when games handler is installed (timing issue)
            }
            break;
            case SBEMU_CMD_DAC_SPEAKER_ON:
            case SBEMU_CMD_DAC_SPEAKER_OFF:
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
                break;
            case SBEMU_CMD_HALT_DMA:
            case SBEMU_CMD_CONTINUE_DMA:
            case SBEMU_CMD_CONTINUE_AUTO:
            {
                SBEMU_Started = SBEMU_DSPCMD == SBEMU_CMD_CONTINUE_DMA || SBEMU_DSPCMD == SBEMU_CMD_CONTINUE_AUTO;
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
            }
            break;
            case SBEMU_CMD_CONTINUE_DMA16:
            case SBEMU_CMD_HALT_DMA16:
            {
                SBEMU_Started = SBEMU_DSPCMD == SBEMU_CMD_CONTINUE_DMA16;
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID; 
            }
            break;
            case SBEMU_CMD_8BIT_OUT_AUTO_HS:
            case SBEMU_CMD_8BIT_OUT_AUTO:
            case SBEMU_CMD_8BIT_OUT_1_HS:
            {
                SBEMU_Auto = SBEMU_DSPCMD==SBEMU_CMD_8BIT_OUT_AUTO_HS || SBEMU_DSPCMD==SBEMU_CMD_8BIT_OUT_AUTO;
                SBEMU_Bits = 8;
                SBEMU_HighSpeed = (SBEMU_DSPCMD==SBEMU_CMD_8BIT_OUT_AUTO_HS || SBEMU_DSPCMD==SBEMU_CMD_8BIT_OUT_1_HS);
                SBEMU_Started = TRUE; //start transfer
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
                SBEMU_Pos = 0;
            }
            break;
            case SBEMU_CMD_2BIT_OUT_AUTO:
            case SBEMU_CMD_3BIT_OUT_AUTO:
            case SBEMU_CMD_4BIT_OUT_AUTO:
            {
                SBEMU_Auto = TRUE;
                SBEMU_ADPCM.useRef = TRUE;
                SBEMU_ADPCM.step = 0;
                SBEMU_Bits = (SBEMU_DSPCMD<=SBEMU_CMD_2BIT_OUT_1_NREF) ? 2 : (SBEMU_DSPCMD>=SBEMU_CMD_3BIT_OUT_1_NREF) ? 3 : 4;
                SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] &= ~0x2;
                SBEMU_Started = TRUE; //start transfer here
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
                SBEMU_Pos = 0;
            }
            break;
            case SBEMU_CMD_EXIT_16BIT_AUTO:
            case SBEMU_CMD_EXIT_8BIT_AUTO:
            {
                if(SBEMU_Auto)
                {
                    SBEMU_Auto = FALSE;
                    SBEMU_Started = FALSE;
                }
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
            }
            break;
            case SBEMU_CMD_8BIT_DIRECT: //unsupported
            {
                SBEMU_DSPCMD = SBEMU_DSPCMD_DIRECT;
            }
            break;
            case 0x2A: //unknown commands
            {
                SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
            }
            break;
        }
    }
    else
    {
        switch(SBEMU_DSPCMD)
        {
            case SBEMU_CMD_SET_TIMECONST:
            {
                SBEMU_SampleRate = 0;
                for(int i = 0; i < 3; ++i)
                {
                    if(value >= SBEMU_TimeConstantMapMono[i][0]-3 && value <= SBEMU_TimeConstantMapMono[i][0]+3)
                    {
                        SBEMU_SampleRate = SBEMU_TimeConstantMapMono[i][1] / SBEMU_GetChannels();
                        break;
                    }
                }
                if(SBEMU_SampleRate == 0)
                    SBEMU_SampleRate = 256000000/(65536-(value<<8)) / SBEMU_GetChannels();
                SBEMU_DSPCMD_Subindex = 2; //only 1byte
                SBEMU_UseTimeConst = SBEMU_GetChannels(); // 1 or 2
                //_LOG("SBEMU: set sampling rate: %d", SBEMU_SampleRate);
            }
            break;
            case SBEMU_CMD_SET_SIZE: //used for auto command
            case SBEMU_CMD_8BIT_OUT_1:
            {
              if(SBEMU_DSPCMD_Subindex++ == 0)
                    SBEMU_Samples = value;
                else
                {
                    SBEMU_Samples |= value<<8;
                    SBEMU_Started = SBEMU_DSPCMD==SBEMU_CMD_8BIT_OUT_1; //start transfer
                    SBEMU_HighSpeed = FALSE;
                    SBEMU_Auto = FALSE;
                    if(SBEMU_Started)
                    {
                        SBEMU_Bits = 8;
                        SBEMU_Pos = 0;
                    }
                }
            }
            break;
            case SBEMU_CMD_2BIT_OUT_1:
            case SBEMU_CMD_2BIT_OUT_1_NREF:
            case SBEMU_CMD_3BIT_OUT_1:
            case SBEMU_CMD_3BIT_OUT_1_NREF:
            case SBEMU_CMD_4BIT_OUT_1:
            case SBEMU_CMD_4BIT_OUT_1_NREF:
            {
                if(SBEMU_DSPCMD_Subindex++ == 0)
                    SBEMU_Samples = value;
                else
                {
                    SBEMU_Samples |= value<<8;
                    SBEMU_Auto = FALSE;
                    SBEMU_ADPCM.useRef = (SBEMU_DSPCMD==SBEMU_CMD_2BIT_OUT_1 || SBEMU_DSPCMD==SBEMU_CMD_3BIT_OUT_1 || SBEMU_DSPCMD==SBEMU_CMD_4BIT_OUT_1);
                    SBEMU_ADPCM.step = 0;
                    SBEMU_Bits = (SBEMU_DSPCMD<=SBEMU_CMD_2BIT_OUT_1_NREF) ? 2 : (SBEMU_DSPCMD>=SBEMU_CMD_3BIT_OUT_1_NREF) ? 3 : 4;
                    SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] &= ~0x2;
                    SBEMU_Started = TRUE; //start transfer here
                    SBEMU_Pos = 0;
                }
            }
            break;
            case SBEMU_CMD_SET_SAMPLERATE_I:
                SBEMU_DSPCMD_Subindex++;
            break;
            case SBEMU_CMD_SET_SAMPLERATE: //command start: sample rate, next command: mode
            {
                if(SBEMU_DSPCMD_Subindex++ == 0)
                    SBEMU_SampleRate = value<<8;
                else
                {
                    SBEMU_SampleRate &= ~0xFF;
                    SBEMU_SampleRate |= value;
                    SBEMU_UseTimeConst = 0;
                }
            }
            break;
            case SBEMU_CMD_8OR16_8_OUT_1:
            case SBEMU_CMD_8OR16_8_OUT_AUTO:
            case SBEMU_CMD_8OR16_8_OUT_AUTO_NOFIFO:
            case SBEMU_CMD_8OR16_16_OUT_1:
            case SBEMU_CMD_8OR16_16_OUT_AUTO:
            case SBEMU_CMD_8OR16_16_OUT_AUTO_NOFIFO:
            {
                SBEMU_Auto = (SBEMU_DSPCMD==SBEMU_CMD_8OR16_8_OUT_AUTO || SBEMU_DSPCMD==SBEMU_CMD_8OR16_16_OUT_AUTO || SBEMU_DSPCMD==SBEMU_CMD_8OR16_8_OUT_AUTO_NOFIFO);
                SBEMU_DSPCMD = value; //set next command: mode
                SBEMU_DSPCMD_Subindex = 0;
            }
            break;
            case SBEMU_CMD_MODE_PCM8_MONO:
            case SBEMU_CMD_MODE_PCM8_STEREO:
            case SBEMU_CMD_MODE_PCM16_MONO:
            case SBEMU_CMD_MODE_PCM16_STEREO:
            {
                if(SBEMU_DSPCMD_Subindex++ == 0)
                    SBEMU_Samples = value;
                else
                {
                    SBEMU_Samples |= value<<8;
                    SBEMU_Bits = (SBEMU_DSPCMD==SBEMU_CMD_MODE_PCM8_MONO || SBEMU_DSPCMD==SBEMU_CMD_MODE_PCM8_STEREO) ? 8 : 16;
                    SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] &= ~0x2;
                    SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] |= (SBEMU_DSPCMD==SBEMU_CMD_MODE_PCM8_STEREO || SBEMU_DSPCMD==SBEMU_CMD_MODE_PCM16_STEREO) ? 0x2 : 0;
                    SBEMU_Started = TRUE; //start transfer here
                    SBEMU_Pos = 0;
                }
            }
            break;
            case SBEMU_CMD_DSP_ID:
            {
                SBEMU_idbyte = value;
            }
            break;
            case SBEMU_DSPCMD_DIRECT:
            {
                SBEMU_DirectBuffer[(SBEMU_DirectCount++)%SBEMU_DIRECT_BUFFER_SIZE] = value;
                SBEMU_DSPCMD_Subindex = 2;
            }
            break;
            case SBEMU_CMD_DSP_WRITE_TESTREG:
            {
                SBEMU_TestReg = value;
                SBEMU_DSPCMD_Subindex = 2;
            }
            break;
            case SBEMU_CMD_DSP_DMA_ID:
            {
                SBEMU_DMAID_A += value ^ SBEMU_DMAID_X;
                SBEMU_DMAID_X = (SBEMU_DMAID_X >> 2u) | (SBEMU_DMAID_X << 6u);
                SBEMU_DSPCMD_Subindex = 2;

                SBEMU_ExtFuns->DMA_Write(SBEMU_DMA, SBEMU_DMAID_A);
            }
            break;
            case SBEMU_DSPCMD_SKIP1:
            {
                SBEMU_DSPCMD_Subindex = 2;
            }
            break;
            case SBEMU_DSPCMD_SKIP2:
            {
                ++SBEMU_DSPCMD_Subindex;
            }
            break;
        }
        if(SBEMU_DSPCMD_Subindex >= 2)
            SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
    }
    if(SBEMU_Started && !OldStarted)//handle driver detection
    {
        /*if(SBEMU_StartCB)
        {
            CLIS();
            SBEMU_StartCB(); //if don't do this, need always output 0 PCM to keep interrupt alive for now
            STIL();
        }*/

        if(SBEMU_GetSampleBytes() <= 32 && SBEMU_ExtFuns->DMA_Size(SBEMU_GetDMA()) <= 32) //small buffer, probably a detection routine
        {
            //SBEMU_ExtFuns->RaiseIRQ(SBEMU_GetIRQ());
            //SBEMU_Started = FALSE;
            SBEMU_DELAY_FOR_IRQ; //hack: add CPU delay so that sound interrupt raises virtual IRQ when games handler is installed (timing issue)
        }
    }
}

uint8_t SBEMU_DSP_Read(uint16_t port)
{
    _LOG("SBEMU: DSP read.\n");
    if(SBEMU_ResetState == SBEMU_RESET_POLL || SBEMU_ResetState == SBEMU_RESET_START)
    {
        SBEMU_ResetState = SBEMU_RESET_END;
        //_LOG("SBEMU: DSP reset read.\n");
        return 0xAA; //reset ready
    }
    if(SBEMU_DSPCMD == SBEMU_CMD_DSP_GETVER)
    {
        //https://github.com/joncampbell123/dosbox-x/wiki/Hardware:Sound-Blaster:DSP-commands:0xE1
        if(SBEMU_DSPDATA_Subindex++ == 0)
            return SBEMU_DSPVER>>8;
        else
        {
            SBEMU_DSPDATA_Subindex = 0;
            SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
            //_LOG("SBEMU: DSP get version.\n");
            return SBEMU_DSPVER&0xFF;
        }
    }
    else if(SBEMU_DSPCMD == SBEMU_CMD_DSP_READ_TESTREG)
    {
        SBEMU_DSPDATA_Subindex = 0;
        SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
        return SBEMU_TestReg;
    }
    else if(SBEMU_DSPCMD == SBEMU_CMD_DSP_ID)
    {
        SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
        SBEMU_DSPDATA_Subindex = 0;
        return SBEMU_idbyte^0xFF;
    }
    else if(SBEMU_DSPCMD == SBEMU_CMD_DSP_COPYRIGHT)
    {
        if(SBEMU_DSPDATA_Subindex == sizeof(SBEMU_Copyright)-1)
        {
            SBEMU_DSPCMD = SBEMU_DSPCMD_INVALID;
            SBEMU_DSPDATA_Subindex = 0;
            return 0xFF;
        }
        return SBEMU_Copyright[SBEMU_DSPDATA_Subindex++];
    }
    return 0xFF;
}

uint8_t SBEMU_DSP_WriteStatus(uint16_t port)
{
    _LOG("SBEMU: DSP WS\n");
    //return 0; //ready for write (bit7 clear)
    SBEMU_WS += 0x80; //some games will wait on busy first
    return SBEMU_WS;
}

uint8_t SBEMU_DSP_ReadStatus(uint16_t port)
{
    _LOG("SBEMU: DSP RS\n");
    SBEMU_RS += 0x80;
    SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] &= ~0x1;
    return SBEMU_RS;
}

uint8_t SBEMU_DSP_INT16ACK(uint16_t port)
{
    SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] &= ~0x2;
    return 0xFF;
}

void SBEMU_Init(int irq, int dma, int hdma, int DSPVer, SBEMU_EXTFUNS* extfuns)
{
    SBEMU_IRQ = irq;
    SBEMU_DMA = dma;
    SBEMU_HDMA = hdma;
    SBEMU_DSPVER = DSPVer;
    SBEMU_ExtFuns = extfuns;

    SBEMU_Mixer_WriteAddr(0, SBEMU_MIXERREG_RESET);
    SBEMU_Mixer_Write(0, 1);
}

uint8_t SBEMU_GetIRQ()
{
    if(SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP] == 0)
        return 0xFF;
    int bit = BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP]);
    if(bit >= 4)
        return 0xFF;
    return SBEMU_IRQMap[bit];
}

uint8_t SBEMU_GetDMA()
{
    if(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] == 0)
        return 0xFF;
    int bit = BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP]);
    return bit;
}

uint8_t SBEMU_GetHDMA()
{
    if(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] == 0)
        return 0xFF;
    int bit = BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP]>>4) + 4;
    return bit;
}

int SBEMU_HasStarted()
{
    return SBEMU_Started;
}

void SBEMU_Stop()
{
    SBEMU_Started = FALSE;
    SBEMU_HighSpeed = FALSE;
    SBEMU_Pos = 0;
}

int SBEMU_GetDACSpeaker()
{
    return SBEMU_DACSpeaker;
}

int SBEMU_GetBits()
{
    return SBEMU_Bits;
}

int SBEMU_GetChannels()
{
    return (SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER]&0x2) ? 2 : 1;
}

int SBEMU_GetSampleRate()
{
    return SBEMU_SampleRate;
}

int SBEMU_GetSampleBytes()
{
    return (SBEMU_Samples + 1)*max(8, SBEMU_Bits)/8;
}

int SBEMU_GetAuto()
{
    return SBEMU_Auto;
}

int SBEMU_GetPos()
{
    return SBEMU_Pos;
}

int SBEMU_SetPos(int pos)
{
    if(pos >= SBEMU_GetSampleBytes())
        SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] |= SBEMU_GetBits() <= 8 ? 0x01 : 0x02;
    return SBEMU_Pos = pos;
}

int SBEMU_IRQTriggered()
{
    return SBEMU_TriggerIRQ;
}
void SBEMU_SetIRQTriggered(int triggered)
{
    SBEMU_TriggerIRQ = triggered;
}

uint8_t SBEMU_GetMixerReg(uint8_t index)
{
    return SBEMU_MixerRegs[index];
}

int SBEMU_DecodeADPCM(uint8_t* adpcm, int bytes)
{
    int start = 0;
    if(SBEMU_ADPCM.useRef)
    {
        SBEMU_ADPCM.useRef = FALSE;
        SBEMU_ADPCM.ref = *adpcm;
        SBEMU_ADPCM.step = 0;
        ++start;
    }

    int outbytes = bytes * (9/SBEMU_Bits);
    uint8_t* pcm = (uint8_t*)malloc(outbytes);
    int outcount = 0;

    for(int i = start; i < bytes; ++i)
    {
        if(SBEMU_Bits == 2)
        {
            pcm[outcount++]=decode_ADPCM_2_sample((adpcm[i] >> 6) & 0x3,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_2_sample((adpcm[i] >> 4) & 0x3,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_2_sample((adpcm[i] >> 2) & 0x3,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_2_sample((adpcm[i] >> 0) & 0x3,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
        }
        else if(SBEMU_Bits == 3)
        {
            pcm[outcount++]=decode_ADPCM_3_sample((adpcm[i] >> 5) & 0x7,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_3_sample((adpcm[i] >> 2) & 0x7,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_3_sample((adpcm[i] & 0x3) << 1,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
        }
        else if(SBEMU_Bits == 4)
        {
            pcm[outcount++]=decode_ADPCM_4_sample(adpcm[i] >> 4,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
            pcm[outcount++]=decode_ADPCM_4_sample(adpcm[i]& 0xf,&SBEMU_ADPCM.ref,&SBEMU_ADPCM.step);
        }
    }
    //assert(outcount <= outbytes);
    _LOG("SBEMU: adpcm decode: %d %d", outcount, outbytes);
    memcpy(adpcm, pcm, outcount);
    free(pcm);
    return outcount;
}

int SBEMU_GetDirectCount()
{
    return SBEMU_DirectCount;
}

void SBEMU_ResetDirect()
{
    SBEMU_DirectBuffer[0] = SBEMU_DirectCount > 1 ? SBEMU_DirectBuffer[SBEMU_DirectCount-1] : 0; //leave one sample for next interpolation
    SBEMU_DirectCount = 1;
    //SBEMU_DirectCount = 0;
}

const uint8_t* SBEMU_GetDirectPCM8()
{
    return SBEMU_DirectBuffer;
}


int SBEMU_GetDetectionCounter()
{
    return SBEMU_DetectionCounter; 
}

void SBEMU_SetDetectionCounter(int c)
{
    SBEMU_DetectionCounter = c;
}
