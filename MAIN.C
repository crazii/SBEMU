#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <DPMI/DBGUTIL.H>
#include <SBEMUCFG.H>
#include <PIC.H>
#include <OPL3EMU.H>
#include "QEMM.H"
#include "HDPMIPT.H"

#include <MPXPLAY.H>
#include <AU_MIXER/MIX_FUNC.H>

extern void TestSound(BOOL play);
extern int16_t* TEST_Sample;
extern unsigned long TEST_SampleLen;

mpxplay_audioout_info_s aui = {0};

#define MAIN_USE_INT70 1
#define MAIN_INT70_FREQ 1024
#define MAIN_PCM_SAMPLESIZE 32768
static DPMI_ISR_HANDLE MAIN_TimerIntHandle;
static int MAIN_Int70Counter = 0;
int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE];
int MAIN_PCMStart;
int MAIN_PCMEnd;
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

struct 
{
    const char* option;
    const char* desc;
    int value;
}MAIN_Options[] =
{
    "/?", "Show help", FALSE,
    "/A", "Specify Memory address", 0x220,
    "/I", "Specify IRQ number", 5,
    "/D", "Specify DMA channel", 1,
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
        //TODO: parse BLASTER env first.

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
    }

    if(MAIN_Options[OPT_TEST].value) //test
    {
        AU_init(&aui);
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
        static QEMM_IODT OPL3IODT[4] =
        {
            0x388, 0, &MAIN_OPL3_388,
            0x389, 0, &MAIN_OPL3_389,
            0x38A, 0, &MAIN_OPL3_38A,
            0x38B, 0, &MAIN_OPL3_38B
        };

        if(!QEMM_Install_IOPortTrap(0x388, 0x38B, OPL3IODT, 4, &OPL3IOPT))
        {
            printf("Error: Failed installing IO port trap for QEMM.\n");
            return 1;
        }
        if(!HDPMIPT_Install_IOPortTrap(0x388, 0x38B, OPL3IODT, 4, &OPL3IOPT_PM))
        {
            printf("Error: Failed installing IO port trap for HDPMI.\n");
            QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            return 1;          
        }

        OPL3EMU_Init(aui.freq_card);
        TestSound(FALSE);
    }

    if(DPMI_InstallISR(MAIN_USE_INT70 ? 0x70 : 0x08, MAIN_TimerInterruptPM, /*MAIN_TimerInterruptRM not needed for HDPMI*/NULL, &MAIN_TimerIntReg, &MAIN_TimerIntHandle) != 0)
    {
        printf("Error: Failed installing timer.\n");
        return 1;
    }
    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif

    if(!DPMI_TSR())
    {
        if(MAIN_Options[OPT_OPL].value)
        {
            QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            HDPMIPT_Uninstall_IOPortTrap(&OPL3IOPT_PM);
        }
        DPMI_UninstallISR(&MAIN_TimerIntHandle);
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

    #if MAIN_USE_INT70
    MAIN_EnableInt70();
    #endif
}

static void MAIN_TimerInterrupt()
{
    if(MAIN_Options[OPT_OPL].value)
    {
        const int freq = MAIN_USE_INT70 ? MAIN_INT70_FREQ : 18;
        #if 0
        const int samples = (SBEMU_SAMPLERATE/**SBEMU_CHANNELS*/+freq-1) / freq;
        int16_t* buffer = TEST_Sample;
        static int cur = 0;
        aui.samplenum = min(samples, TEST_SampleLen-cur);
        aui.pcm_sample = TEST_Sample + cur;
        cur += aui.samplenum;
        AU_writedata(&aui);
        #else
        const int SAMPLES = SBEMU_SAMPLERATE / freq;
        int channels = OPL3EMU_GetMode() ? 2 : 1;
        int samples = SAMPLES * channels;
        if(MAIN_PCMEnd - MAIN_PCMStart >= samples)
        {
            aui.samplenum = samples;
            aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
            MAIN_PCMStart += samples - AU_writedata(&aui);
            return;
        }

        if(MAIN_PCMEnd + samples*4 >= MAIN_PCM_SAMPLESIZE)
        {
            memcpy(MAIN_PCM, MAIN_PCM + MAIN_PCMStart, (MAIN_PCMEnd - MAIN_PCMStart)*sizeof(int16_t));
            MAIN_PCMEnd = MAIN_PCMEnd - MAIN_PCMStart;
            MAIN_PCMStart = 0;
        }

        OPL3EMU_GenSamples(MAIN_PCM + MAIN_PCMEnd, samples);
        //always use 2 channels
        if(channels == 1)
            cv_channels_1_to_n(MAIN_PCM + MAIN_PCMEnd, samples, 2, SBEMU_BITS/8);
        samples *= 2;
        MAIN_PCMEnd += samples;

        aui.samplenum = samples;
        aui.pcm_sample = MAIN_PCM + MAIN_PCMStart;
        MAIN_PCMStart += samples;
        MAIN_PCMStart -= AU_writedata(&aui);
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