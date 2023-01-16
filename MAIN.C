#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
#define MAIN_INT70_FREQ 100
#define MAIN_PCM_SAMPLESIZE 32768
#define MAIN_FREQ (MAIN_USE_INT70 ? MAIN_INT70_FREQ : 18) //because some programs will assume default frequency on running/uninstalling its own timer, so we don't change the default
static const int MAIN_SAMPLES = (SBEMU_SAMPLERATE / MAIN_FREQ);
static const int MAIN_PCM_COUNT = (MAIN_PCM_SAMPLESIZE / (MAIN_SAMPLES*SBEMU_CHANNELS) * MAIN_SAMPLES*SBEMU_CHANNELS);

static DPMI_ISR_HANDLE MAIN_TimerIntHandle;
static int MAIN_Int70Counter = 0;
int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE+128];
int MAIN_PCMStart;
int MAIN_PCMEnd;
int MAIN_PCMEndTag = MAIN_PCM_COUNT;
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
    return out ? (SBEMU_DSP_Write(port, val), val) : val;
}
static uint32_t MAIN_SB_DSP_ReadStatus(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_ReadStatus(port));
}
static uint32_t MAIN_SB_DSP_ReadINT16BitACK(uint32_t port, uint32_t val, uint32_t out)
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_INT16ACK(port));
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

static QEMM_IODT MAIN_SB_IODT[11] =
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
    "/I", "Specify IRQ number, valud value: 5,7", 5,
    "/D", "Specify DMA channel, valid value: 0,1,3", 1,
    "/OPL", "Enable OPL3 emulation", TRUE,
    
    "/test", "tests sound", FALSE,
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
    if(argc == 1 || (argc == 2 && stricmp(argv[1],"/?") == 0))
    {
        printf("SBEMU: Sound Blaster emulator for AC97. Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option)
        {
            printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
            ++i;
        }
        printf("\nNote: SBEMU will read BLASTER environment variable and use it, "
        "\n if set, they will override the BLASTER values.\n");
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
                        MAIN_Options[OPT_IRQ].value = *(blaster) - '0';
                    else if(c == 'D')
                        MAIN_Options[OPT_DMA].value = *(blaster) - '0';
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

    if(DPMI_InstallISR(MAIN_USE_INT70 ? 0x70 : 0x08, MAIN_TimerInterruptPM, &MAIN_TimerIntHandle) != 0)
    {
        printf("Error: Failed installing timer.\n");
        return 1;
    }
    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif

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
        TestSound(FALSE);
    }

    for(int i = 0; i < countof(MAIN_SB_IODT); ++i)
        MAIN_SB_IODT[i].port += MAIN_Options[OPT_ADDR].value;

    BOOL QEMMInstalledVDMA = QEMM_Install_IOPortTrap(MAIN_VDMA_IODT, countof(MAIN_VDMA_IODT), &MAIN_VDMA_IOPT);
    BOOL QEMMInstalledVIRQ = QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
    BOOL QEMMInstalledSB = QEMM_Install_IOPortTrap(MAIN_SB_IODT, countof(MAIN_SB_IODT), &MAIN_SB_IOPT);
    if(!QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB)
    {
        printf("Error: Failed installing IO port trap for QEMM.\n");
        if(QEMMInstalledVDMA) QEMM_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT);
        if(QEMMInstalledVIRQ) QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
        if(QEMMInstalledSB) QEMM_Uninstall_IOPortTrap(&MAIN_SB_IOPT);
        return 1;
    }

    BOOL HDPMIInstalledVDMA1 = HDPMIPT_Install_IOPortTrap(0x0, 0xF, MAIN_VDMA_IODT, 16, &MAIN_VDMA_IOPT_PM1);
    BOOL HDPMIInstalledVDMA2 = HDPMIPT_Install_IOPortTrap(0x81, 0x83, MAIN_VDMA_IODT+16, 3, &MAIN_VDMA_IOPT_PM2);
    BOOL HDPMIInstalledVDMA3 = HDPMIPT_Install_IOPortTrap(0x87, 0x87, MAIN_VDMA_IODT+19, 1, &MAIN_VDMA_IOPT_PM3);
    BOOL HDPMIInstalledVIRQ1 = HDPMIPT_Install_IOPortTrap(0x20, 0x21, MAIN_VIRQ_IODT, 2, &MAIN_VIRQ_IOPT_PM1);
    BOOL HDPMIInstalledVIRQ2 = HDPMIPT_Install_IOPortTrap(0xA0, 0xA1, MAIN_VIRQ_IODT+2, 2, &MAIN_VIRQ_IOPT_PM2);
    BOOL HDPMIInstalledSB = HDPMIPT_Install_IOPortTrap(MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_ADDR].value+0x0F, MAIN_SB_IODT, countof(MAIN_SB_IODT), &MAIN_SB_IOPT_PM);
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

    if(!DPMI_TSR())
    {
        if(MAIN_Options[OPT_OPL].value)
        {
            QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            HDPMIPT_Uninstall_IOPortTrap(&OPL3IOPT_PM);
        }
        DPMI_UninstallISR(&MAIN_TimerIntHandle);

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
    DPMI_CallRealModeOldISR(&MAIN_TimerIntHandle);

    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif
}

static void MAIN_TimerInterrupt()
{
    if(MAIN_Options[OPT_OPL].value)
    {
        #if 0
        const int samples = (SBEMU_SAMPLERATE/**SBEMU_CHANNELS*/+MAIN_FREQ-1) / MAIN_FREQ;
        int16_t* buffer = TEST_Sample;
        static int cur = 0;
        aui.samplenum = min(samples, TEST_SampleLen-cur);
        aui.pcm_sample = TEST_Sample + cur;
        cur += aui.samplenum;
        AU_writedata(&aui);
        #else
        int samples = MAIN_SAMPLES;
        if((MAIN_PCMEnd - MAIN_PCMStart + MAIN_PCMEndTag)%MAIN_PCMEndTag >= MAIN_PCMEndTag-MAIN_SAMPLES*2) //buffer full
        {
            aui.samplenum = MAIN_PCMEndTag;
            aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
            MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
            if(MAIN_PCMStart >= MAIN_PCMEndTag)
                MAIN_PCMStart = 0;
            return;
        }
        if(MAIN_PCMEnd - MAIN_PCMStart < MAIN_SAMPLES) //(almost) empty
            samples += samples/4; //extra samples to avoid underrun due to timer interrupt precision

        OPL3EMU_GenSamples(MAIN_PCM + MAIN_PCMEnd, samples);
        //always use 2 channels
        int channels = OPL3EMU_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(MAIN_PCM + MAIN_PCMEnd, samples, 2, SBEMU_BITS/8);
        samples *= 2;
        MAIN_PCMEnd += samples;
        if(MAIN_PCMEnd >= MAIN_PCM_COUNT - MAIN_SAMPLES*4)
        {
            MAIN_PCMEndTag = MAIN_PCMEnd;
            MAIN_PCMEnd = 0;
        }

        aui.samplenum = (MAIN_PCMEnd - MAIN_PCMStart + MAIN_PCMEndTag)%MAIN_PCMEndTag;
        aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
        MAIN_PCMStart += aui.samplenum - AU_writedata(&aui);
        if(MAIN_PCMStart >= MAIN_PCMEndTag)
            MAIN_PCMStart = 0;
        #endif
    }
}

static void MAIN_EnableInt70()
{
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
}