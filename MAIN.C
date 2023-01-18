#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <DPMI/DBGUTIL.H>
#include <SBEMUCFG.H>
#include <PIC.H>
#include <OPL3EMU.H>
#include <VDMA.H>
#include <VIRQ.H>
#include <SBEMU.H>
#include <UNTRAPIO.H>
#include "QEMM.H"
#include "HDPMIPT.H"

#include <MPXPLAY.H>
#include <AU_MIXER/MIX_FUNC.H>

extern void TestSound(BOOL play);
extern int16_t* TEST_Sample;
extern unsigned long TEST_SampleLen;

mpxplay_audioout_info_s aui = {0};

#define MAIN_USE_INT70 0
#define MAIN_INT70_FREQ 100 //1024 max
#define MAIN_PCM_SAMPLESIZE 32768
#define MAIN_FREQ (MAIN_USE_INT70 ? MAIN_INT70_FREQ : 18) //because some programs will assume default frequency on running/uninstalling its own timer, so we don't change the default
#define MAIN_SAMPLES (SBEMU_SAMPLERATE / MAIN_FREQ)
static const int MAIN_PCM_COUNT = (MAIN_PCM_SAMPLESIZE / (MAIN_SAMPLES*SBEMU_CHANNELS) * MAIN_SAMPLES*SBEMU_CHANNELS);

static DPMI_ISR_HANDLE MAIN_TimerIntHandle;
static int MAIN_Int70Counter = 0;
static uint32_t MAIN_DMAIndex = -1;
static uint32_t MAIN_DMA_Addr = 0;
static uint32_t MAIN_DMA_Size = 0;
static uint32_t MAIN_DMA_MappedAddr = 0;
static uint32_t MAIN_SBBytes = 0;
static int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE+128];
static int16_t MAIN_OPLPCM[MAIN_SAMPLES*8+128];
static int MAIN_PCMStart;
static int MAIN_PCMEnd;
static int MAIN_PCMEndTag = MAIN_PCM_COUNT;
static void MAIN_TimerInterrupt();
static void MAIN_TimerInterruptPM();
static void MAIN_EnableInt70();

static uint32_t MAIN_OPL3_388(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? OPL3EMU_PrimaryWriteIndex(val) : OPL3EMU_PrimaryRead(val);
}
static uint32_t MAIN_OPL3_389(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? OPL3EMU_PrimaryWriteData(val) : OPL3EMU_PrimaryRead(val);
}
static uint32_t MAIN_OPL3_38A(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? OPL3EMU_SecondaryWriteIndex(val) : OPL3EMU_SecondaryRead(val);
}
static uint32_t MAIN_OPL3_38B(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? OPL3EMU_SecondaryWriteData(val) : OPL3EMU_SecondaryRead(val);
}

static uint32_t MAIN_DMA(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (VDMA_Write(port, val), val) : (val &=~0xFF, val |= VDMA_Read(port));
}

static uint32_t MAIN_IRQ(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (VIRQ_Write(port, val), val) : (val &=~0xFF, val |= VIRQ_Read(port));
}

static uint32_t MAIN_SB_MixerAddr(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (SBEMU_Mixer_WriteAddr(port, val), val) : val;
}
static uint32_t MAIN_SB_MixerData(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (SBEMU_Mixer_Write(port, val), val) : (val &=~0xFF, val |= SBEMU_Mixer_Read(port));
}
static uint32_t MAIN_SB_DSP_Reset(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (SBEMU_DSP_Reset(port, val), val) : val;
}
static uint32_t MAIN_SB_DSP_Read(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_Read(port));
}
static uint32_t MAIN_SB_DSP_Write(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (SBEMU_DSP_Write(port, val), val) : SBEMU_DSP_WriteStatus(port);
}
static uint32_t MAIN_SB_DSP_ReadStatus(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_ReadStatus(port));
}
static uint32_t MAIN_SB_DSP_ReadINT16BitACK(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_INT16ACK(port));
}
static uint32_t MAIN_SB_Delay(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? (UntrappedIO_OUT(port, val),val) : (delay(1), val &=~0xFF, val |=UntrappedIO_IN(port));
}

static QEMM_IODT MAIN_OPL3IODT[4] =
{
    0x388, &MAIN_OPL3_388,
    0x389, &MAIN_OPL3_389,
    0x38A, &MAIN_OPL3_38A,
    0x38B, &MAIN_OPL3_38B
};

static QEMM_IODT MAIN_VDMA_IODT[20] =
{
    0x00, &MAIN_DMA,
    0x01, &MAIN_DMA,
    0x02, &MAIN_DMA,
    0x03, &MAIN_DMA,
    0x04, &MAIN_DMA,
    0x05, &MAIN_DMA,
    0x06, &MAIN_DMA,
    0x07, &MAIN_DMA,
    0x08, &MAIN_DMA,
    0x09, &MAIN_DMA,
    0x0A, &MAIN_DMA,
    0x0B, &MAIN_DMA,
    0x0C, &MAIN_DMA,
    0x0D, &MAIN_DMA,
    0x0E, &MAIN_DMA,
    0x0F, &MAIN_DMA,
    0x81, &MAIN_DMA,
    0x82, &MAIN_DMA,
    0x83, &MAIN_DMA,
    0x87, &MAIN_DMA,
};

static QEMM_IODT MAIN_VIRQ_IODT[4] =
{
    0x20, &MAIN_IRQ,
    0x21, &MAIN_IRQ,
    0xA0, &MAIN_IRQ,
    0xA1, &MAIN_IRQ,
};

static QEMM_IODT MAIN_SB_IODT[12] =
{ //MAIN_Options[OPT_ADDR].value will be added at runtime
    0x00, &MAIN_OPL3_388,
    0x01, &MAIN_OPL3_389,
    0x02, &MAIN_OPL3_38A,
    0x03, &MAIN_OPL3_38B,
    0x04, &MAIN_SB_MixerAddr,
    0x05, &MAIN_SB_MixerData,
    0x06, &MAIN_SB_DSP_Reset,
    0x0A, &MAIN_SB_DSP_Read,
    0x0C, &MAIN_SB_DSP_Write,
    0x0E, &MAIN_SB_DSP_ReadStatus,
    0x0F, &MAIN_SB_DSP_ReadINT16BitACK,
    //0x00, &MAIN_SB_Delay
};

QEMM_IOPT MAIN_VDMA_IOPT;
QEMM_IOPT MAIN_VIRQ_IOPT;
QEMM_IOPT MAIN_SB_IOPT;
QEMM_IOPT MAIN_VDMA_IOPT_PM1;
QEMM_IOPT MAIN_VDMA_IOPT_PM2;
QEMM_IOPT MAIN_VDMA_IOPT_PM3;
QEMM_IOPT MAIN_VIRQ_IOPT_PM1;
QEMM_IOPT MAIN_VIRQ_IOPT_PM2;
QEMM_IOPT MAIN_SB_IOPT_PM;

struct 
{
    const char* option;
    const char* desc;
    int value;
}MAIN_Options[] =
{
    "/?", "Show help", FALSE,
    "/A", "Specify IO address, valid value: 220,240", 0x220,
    "/I", "Specify IRQ number, valud value: 5,7", 7,
    "/D", "Specify DMA channel, valid value: 0,1,3", 1,
    "/OPL", "Enable OPL3 emulation", FALSE,
    
    "/test", "Test sound and exit", FALSE,
    NULL, NULL, 0,
};
enum EOption
{
    OPT_Help,
    OPT_ADDR,
    OPT_IRQ,
    OPT_DMA,
    OPT_OPL,

    OPT_TEST,
    OPT_COUNT,
};

int main(int argc, char* argv[])
{
    if(/*argc == 1 || */(argc == 2 && stricmp(argv[1],"/?") == 0))
    {
        printf("SBEMU: Sound Blaster emulator for AC97. Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option)
        {
            printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
            ++i;
        }
        printf("\nNote: SBEMU will read BLASTER environment variable and use it, "
        "\n if /A /I /D set, they will override the BLASTER values.\n");
        printf("\nSource code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }
    else
    {
        //parse BLASTER env first.
        {
            char* blaster = getenv("BLASTER");
            if(blaster != NULL)
            {
                char c;
                while((c=toupper(*(blaster++))))
                {
                    if(c == 'I')
                        MAIN_Options[OPT_IRQ].value = *(blaster++) - '0';
                    else if(c == 'D')
                        MAIN_Options[OPT_DMA].value = *(blaster++) - '0';
                    else if(c == 'A')
                        MAIN_Options[OPT_ADDR].value = strtol(blaster, &blaster, 16);
                }
            }
        }

        for(int i = 1; i < argc; ++i)
        {
            for(int j = 0; j < OPT_COUNT; ++j)
            {
                int len = strlen(MAIN_Options[j].option);
                if(memicmp(argv[i], MAIN_Options[j].option, len) == 0)
                {
                    int arglen = strlen(argv[i]);
                    MAIN_Options[j].value = arglen == len ? 1 : strtol(&argv[i][arglen], NULL, 16);
                    break;
                }
            }
        }

        if(MAIN_Options[OPT_ADDR].value != 0x220 && MAIN_Options[OPT_ADDR].value != 0x240)
        {
            printf("Error: invalid IO port address.\n");
            return 1;
        }
        if(MAIN_Options[OPT_IRQ].value != 0x5 && MAIN_Options[OPT_IRQ].value != 0x7)
        {
            printf("Error: invalid IRQ.\n");
            return 1;
        }
        if(MAIN_Options[OPT_DMA].value != 0x0 && MAIN_Options[OPT_DMA].value != 0x1 && MAIN_Options[OPT_DMA].value != 0x3)
        {
            printf("Error: invalid DMA channel.\n");
            return 1;
        }
        //TODO: alter BLASTER env?
    }

    if(MAIN_Options[OPT_TEST].value) //test
    {
        AU_init(&aui);
        if(!aui.card_handler)
            return 0;
        AU_ini_interrupts(&aui);
        AU_setmixer_init(&aui);
        AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
        TestSound(TRUE);
        AU_del_interrupts(&aui);
        return 0;
    }

    DPMI_Init();
    int bcd = QEMM_GetVersion();
    _LOG("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
    if(bcd < 0x703)
    {
        printf("Error: QEMM not installed, or version bellow 7.03: %x.%02x\n", bcd>>8, bcd&0xFF);
        return 1;
    }
    AU_init(&aui);
    if(!aui.card_handler)
        return 0;
    AU_ini_interrupts(&aui);
    AU_setmixer_init(&aui);
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
    //use fixed rate
    mpxplay_audio_decoder_info_s adi = {NULL, 0, 1, SBEMU_SAMPLERATE, SBEMU_CHANNELS, SBEMU_CHANNELS, NULL, SBEMU_BITS, SBEMU_BITS/8, 0};
    AU_setrate(&aui, &adi);
    AU_prestart(&aui);
    AU_start(&aui);

    QEMM_IOPT OPL3IOPT;
    QEMM_IOPT OPL3IOPT_PM;
    if(MAIN_Options[OPT_OPL].value)
    {
        if(!QEMM_Install_IOPortTrap(MAIN_OPL3IODT, 4, &OPL3IOPT))
        {
            printf("Error: Failed installing IO port trap for QEMM.\n");
            return 1;
        }
        if(!HDPMIPT_Install_IOPortTrap(0x388, 0x38B, MAIN_OPL3IODT, 4, &OPL3IOPT_PM))
        {
            printf("Error: Failed installing IO port trap for HDPMI.\n");
            QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            return 1;          
        }

        OPL3EMU_Init(aui.freq_card);
        //TestSound(FALSE);
    }

    SBEMU_Init(MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value);
    for(int i = 0; i < countof(MAIN_SB_IODT); ++i)
        MAIN_SB_IODT[i].port += MAIN_Options[OPT_ADDR].value;
    QEMM_IODT* SB_Iodt = MAIN_Options[OPT_OPL].value ? MAIN_SB_IODT : MAIN_SB_IODT+4;
    int SB_IodtCount = MAIN_Options[OPT_OPL].value ? countof(MAIN_SB_IODT) : countof(MAIN_SB_IODT)-4;
    //MAIN_SB_IODT[countof(MAIN_SB_IODT)-1].port = DPMI_LoadW(DPMI_SEGOFF2L(0x40,0x63)) + 6;

    BOOL QEMMInstalledVDMA = QEMM_Install_IOPortTrap(MAIN_VDMA_IODT, countof(MAIN_VDMA_IODT), &MAIN_VDMA_IOPT);
    #if MAIN_USE_INT70 //will crash with VIRQ installed, do it temporarily. TODO: figure out why
    BOOL QEMMInstalledVIRQ = TRUE;
    #else
    BOOL QEMMInstalledVIRQ = QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
    #endif
    BOOL QEMMInstalledSB = QEMM_Install_IOPortTrap(SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT);
    if(!QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB)
    {
        printf("Error: Failed installing IO port trap for QEMM.\n");
        if(QEMMInstalledVDMA) QEMM_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT);
        #if !MAIN_USE_INT70
        if(QEMMInstalledVIRQ) QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
        #endif
        if(QEMMInstalledSB) QEMM_Uninstall_IOPortTrap(&MAIN_SB_IOPT);
        return 1;
    }

    BOOL HDPMIInstalledVDMA1 = HDPMIPT_Install_IOPortTrap(0x0, 0xF, MAIN_VDMA_IODT, 16, &MAIN_VDMA_IOPT_PM1);
    BOOL HDPMIInstalledVDMA2 = HDPMIPT_Install_IOPortTrap(0x81, 0x83, MAIN_VDMA_IODT+16, 3, &MAIN_VDMA_IOPT_PM2);
    BOOL HDPMIInstalledVDMA3 = HDPMIPT_Install_IOPortTrap(0x87, 0x87, MAIN_VDMA_IODT+19, 1, &MAIN_VDMA_IOPT_PM3);
    BOOL HDPMIInstalledVIRQ1 = HDPMIPT_Install_IOPortTrap(0x20, 0x21, MAIN_VIRQ_IODT, 2, &MAIN_VIRQ_IOPT_PM1);
    BOOL HDPMIInstalledVIRQ2 = HDPMIPT_Install_IOPortTrap(0xA0, 0xA1, MAIN_VIRQ_IODT+2, 2, &MAIN_VIRQ_IOPT_PM2);
    BOOL HDPMIInstalledSB = HDPMIPT_Install_IOPortTrap(MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_ADDR].value+0x0F, SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT_PM);
    if(!HDPMIInstalledVDMA1 || !HDPMIInstalledVDMA2 || !HDPMIInstalledVDMA3 || !HDPMIInstalledVIRQ1 || !HDPMIInstalledVIRQ2 || !HDPMIInstalledSB)
    {
        printf("Error: Failed installing IO port trap for HDPMI.\n");
        if(HDPMIInstalledVDMA1) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM1);
        if(HDPMIInstalledVDMA2) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM2);
        if(HDPMIInstalledVDMA3) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM3);
        if(HDPMIInstalledVIRQ1) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM1);
        if(HDPMIInstalledVIRQ2) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM2);
        if(HDPMIInstalledSB) HDPMIPT_Uninstall_IOPortTrap(&MAIN_SB_IOPT_PM);
        return 1;
    }

    if(DPMI_InstallISR(MAIN_USE_INT70 ? 0x70 : 0x08, MAIN_TimerInterruptPM, &MAIN_TimerIntHandle) != 0)
    {
        printf("Error: Failed installing timer.\n");
        return 1;
    }
    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif
    
    if(!DPMI_TSR())
    {
        DPMI_UninstallISR(&MAIN_TimerIntHandle);
        if(MAIN_Options[OPT_OPL].value)
        {
            QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            HDPMIPT_Uninstall_IOPortTrap(&OPL3IOPT_PM);
        }

        QEMM_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT);
        QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
        QEMM_Uninstall_IOPortTrap(&MAIN_SB_IOPT);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM1);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM2);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM3);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM1);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM2);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_SB_IOPT_PM);
        printf("Error: Failed installing TSR.\n");
    }
    return 0;
}

static void MAIN_TimerInterruptPM()
{
    #if MAIN_USE_INT70
    if(++MAIN_Int70Counter >= 1024/MAIN_INT70_FREQ)
    #endif
    {
        MAIN_TimerInterrupt();
        MAIN_Int70Counter = 0;
    }
    DPMI_CallOldISR(&MAIN_TimerIntHandle);
    //DPMI_CallRealModeOldISR(&MAIN_TimerIntHandle);
    
    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif
}

static void MAIN_TimerInterrupt()
{
    #if 0
    const int samples = MAIN_SAMPLES*2;
    int16_t* buffer = TEST_Sample;
    static int cur = 0;
    aui.samplenum = min(samples, TEST_SampleLen-cur);
    aui.pcm_sample = TEST_Sample + cur;
    cur += aui.samplenum;
    AU_writedata(&aui);
    #else
    int samples = MAIN_SAMPLES;
    if((MAIN_PCMEnd - MAIN_PCMStart + MAIN_PCMEndTag)%MAIN_PCMEndTag >= MAIN_PCMEndTag-MAIN_SAMPLES*4) //buffer full
    {
        aui.samplenum = MAIN_PCMEnd >= MAIN_PCMStart ? MAIN_PCMEnd - MAIN_PCMStart : MAIN_PCMEndTag - MAIN_PCMStart;
        aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
        MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
        if(MAIN_PCMStart >= MAIN_PCMEndTag)
        {
            MAIN_PCMStart = 0;
            aui.samplenum = MAIN_PCMEnd - MAIN_PCMStart;
            aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
            MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
        }
        return;
    }
    if(MAIN_PCMEnd > MAIN_PCMStart && MAIN_PCMEnd - MAIN_PCMStart < MAIN_SAMPLES*2) //(almost) empty
        samples += samples/4; //extra samples to avoid underrun due to timer interrupt precision

    BOOL digital = SBEMU_HasStarted();
    if(digital)
    {
        digital = TRUE;
        uint32_t DMA_Addr = VDMA_GetAddress(SBEMU_GetDMA());
        uint32_t DMA_Count = VDMA_GetCounter(SBEMU_GetDMA()); //count in bytes (8bit dma)
        if(MAIN_DMAIndex = -1)
        {
            MAIN_DMAIndex = 0;
            #if 0
            if(MAIN_DMA_MappedAddr != 0)
            {
                if(!(DMA_Addr == MAIN_DMA_Addr && DMA_Count <= MAIN_DMA_Size))
                {
                    if(MAIN_DMA_MappedAddr > 640*1024)
                        DPMI_UnmappMemory(MAIN_DMA_MappedAddr);
                    MAIN_DMA_MappedAddr = 0;
                }
            }
            if(MAIN_DMA_MappedAddr == 0)
                MAIN_DMA_MappedAddr = DMA_Addr <= 640*1024 ? DMA_Addr : DPMI_MapMemory(DMA_Addr, DMA_Count);
                #endif
        }
        int count = min(samples, (DMA_Count-MAIN_DMAIndex)/(SBEMU_GetBits()/8)/SBEMU_GetChannels());
        int bytes = count * (SBEMU_GetBits()/8) * SBEMU_GetChannels();
        #if 0
        DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM+MAIN_PCMEnd), MAIN_DMA_MappedAddr+MAIN_DMAIndex, bytes);
        if(SBEMU_GetBits() != 16)
            cv_bits_n_to_m(MAIN_PCM+MAIN_PCMEnd, samples, SBEMU_GetBits()/8, 2);
        if(SBEMU_GetSampleRate() != aui.freq_card)
            samples = mixer_speed_lq(MAIN_PCM+MAIN_PCMEnd, count, SBEMU_GetChannels(), SBEMU_GetSampleRate(), aui.freq_card);
        if(SBEMU_GetChannels() == 1)
            cv_channels_1_to_n(MAIN_PCM+MAIN_PCMEnd, samples, 2, 2);
        #endif
        MAIN_DMAIndex += bytes;
        if(MAIN_DMAIndex == DMA_Count)
        {
            if(DMA_GetAuto())
                MAIN_DMAIndex = 0;
            else
                MAIN_DMAIndex = -1;
        }
        MAIN_SBBytes += bytes;
        if(MAIN_SBBytes >= SBEMU_GetSampleBytes())
        {
            //_LOG("INT:%d,%d,%d\n",MAIN_SBBytes,SBEMU_GetSampleBytes(),DMA_Count);
            #if MAIN_USE_INT70
            QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
            #endif
            //VIRQ_Invoke(SBEMU_GetIRQ());
            #if MAIN_USE_INT70
            QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
            #endif
            if(!SBEMU_GetAuto())
                SBEMU_Stop();
            MAIN_SBBytes = 0;
        }
    }
    else
    {
        if(!MAIN_Options[OPT_OPL].value)
            return;
        else
            memset(MAIN_PCM+MAIN_PCMEnd, 0, samples*2*sizeof(int16_t));
    }

    if(MAIN_Options[OPT_OPL].value)
    {
        int16_t* pcm = digital ? MAIN_OPLPCM : MAIN_PCM + MAIN_PCMEnd;
        OPL3EMU_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        int channels = OPL3EMU_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(pcm, samples, 2, SBEMU_BITS/8);

        if(digital)
        {
            for(int i = 0; i < samples*2; ++i)
            {
                int a = (int)MAIN_PCM[MAIN_PCMEnd+i] + 32768;
                int b = (int)MAIN_OPLPCM[i] + 32768;
                int mixed = (a < 32768 || b < 32768) ? (a*b/32768) : ((a+b)*2 - a*b/32768 - 65536);
                if(mixed == 65536) mixed = 65535;
                MAIN_PCM[MAIN_PCMEnd+i] = mixed - 32768;
            }
        }
    }        
    samples *= 2; //to stereo
    MAIN_PCMEnd += samples;

    if(MAIN_PCMEnd >= MAIN_PCM_COUNT - MAIN_SAMPLES*4)
    {
        MAIN_PCMEndTag = MAIN_PCMEnd;
        MAIN_PCMEnd = 0;
    }

    aui.samplenum = MAIN_PCMEnd >= MAIN_PCMStart ? MAIN_PCMEnd - MAIN_PCMStart : MAIN_PCMEndTag - MAIN_PCMStart;
    aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
    MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
    if(MAIN_PCMStart >= MAIN_PCMEndTag)
    {
        MAIN_PCMStart = 0;
        aui.samplenum = MAIN_PCMEnd - MAIN_PCMStart;
        aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
        MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
    }
    #endif
}

static void MAIN_EnableInt70()
{
    CLIS();
    PIC_UnmaskIRQ(8);

    outp(0x70, 0x0A);
    int freq = inp(0x71);
    if(freq != 0x26) //1024
    {
        outp(0x70, 0x0A);
        outp(0x71, 0x26);
    }

    outp(0x70, 0x0B);
    int mask = inp(0x71);
    if(!(mask&0x40)) //enable mask
    {
        outp(0x70, 0x0B);
        outp(0x71, mask|0x40);
    }

    outp(0x70, 0x0C);
    inp(0x71);
    outp(0x70, 0x0D);
    inp(0x71);
    STIL();
}