#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <dpmi/dbgutil.h>
#include <sbemucfg.h>
#include <pic.h>
#include <opl3emu.h>
#include <vdma.h>
#include <virq.h>
#include <sbemu.h>
#include <untrapio.h>
#include "qemm.h"
#include "hdpmipt.h"

#include <mpxplay.h>
#include <au_mixer/mix_func.h>

#ifndef MAIN_SBEMU_VER
#define MAIN_SBEMU_VER "1.0 beta3"
#endif

#define MAIN_TRAP_PIC_ONDEMAND 1
#define MAIN_INSTALL_RM_ISR 1 //not needed. but to workaround some rm games' problem. need RAW_HOOk in dpmi_dj2.c
#define MAIN_DOUBLE_OPL_VOLUME 1 //hack: double the amplitude of OPL PCM. should be 1 or 0

#define MAIN_TSR_INT 0x2D   //AMIS multiplex. TODO: 0x2F?
#define MAIN_TSR_INTSTART_ID 0x01 //start id

mpxplay_audioout_info_s aui = {0};

#define MAIN_PCM_SAMPLESIZE 16384

static int16_t MAIN_OPLPCM[MAIN_PCM_SAMPLESIZE+256];
static int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE+256];

static DPMI_ISR_HANDLE MAIN_IntHandlePM;
static DPMI_ISR_HANDLE MAIN_IntHandleRM;
static DPMI_REG MAIN_IntREG;
static INTCONTEXT MAIN_IntContext;
static uint32_t MAIN_DMA_Addr = 0;
static uint32_t MAIN_DMA_Size = 0;
static uint32_t MAIN_DMA_MappedAddr = 0;
static uint8_t MAIN_QEMM_Present = 0;
static uint8_t MAIN_HDPMI_Present = 0;
static uint8_t MAIN_InINT;

SBEMU_EXTFUNS MAIN_SbemuExtFun;

static void MAIN_Interrupt();
static void MAIN_InterruptPM();
static void MAIN_InterruptRM();

static DPMI_ISR_HANDLE MAIN_TSRIntHandle;
static DPMI_REG MAIN_TSRREG;
static uint32_t MAIN_TSR_INT_FNO = MAIN_TSR_INTSTART_ID;
static uint32_t MAIN_ISR_DOSID;
static const char MAIN_ISR_DOSID_String[] = "Crazii  SBEMU   Sound Blaster emulation on AC97"; //8:8:asciiz
static void MAIN_TSR_InstallationCheck();
static void MAIN_TSR_Interrupt();

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

static QEMM_IODT MAIN_OPL3IODT[4] =
{
    0x388, &MAIN_OPL3_388,
    0x389, &MAIN_OPL3_389,
    0x38A, &MAIN_OPL3_38A,
    0x38B, &MAIN_OPL3_38B
};

static QEMM_IODT MAIN_VDMA_IODT[40] =
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

    0xC0, &MAIN_DMA,
    0xC2, &MAIN_DMA,
    0xC4, &MAIN_DMA,
    0xC6, &MAIN_DMA,
    0xC8, &MAIN_DMA,
    0xCA, &MAIN_DMA,
    0xCC, &MAIN_DMA,
    0xCE, &MAIN_DMA,
    0xD0, &MAIN_DMA,
    0xD2, &MAIN_DMA,
    0xD4, &MAIN_DMA,
    0xD6, &MAIN_DMA,
    0xD8, &MAIN_DMA,
    0xDA, &MAIN_DMA,
    0xDC, &MAIN_DMA,
    0xDE, &MAIN_DMA,
    0x89, &MAIN_DMA,
    0x8A, &MAIN_DMA,
    0x8B, &MAIN_DMA,
    0x8F, &MAIN_DMA,
};

static QEMM_IODT MAIN_VIRQ_IODT[4] =
{
    0x20, &MAIN_IRQ,
    0x21, &MAIN_IRQ,
    0xA0, &MAIN_IRQ,
    0xA1, &MAIN_IRQ,
};

static QEMM_IODT MAIN_SB_IODT[13] =
{ //MAIN_Options[OPT_ADDR].value will be added at runtime
    0x00, &MAIN_OPL3_388,
    0x01, &MAIN_OPL3_389,
    0x02, &MAIN_OPL3_38A,
    0x03, &MAIN_OPL3_38B,
    0x04, &MAIN_SB_MixerAddr,
    0x05, &MAIN_SB_MixerData,
    0x06, &MAIN_SB_DSP_Reset,
    0x08, &MAIN_OPL3_388,
    0x09, &MAIN_OPL3_389,
    0x0A, &MAIN_SB_DSP_Read,
    0x0C, &MAIN_SB_DSP_Write,
    0x0E, &MAIN_SB_DSP_ReadStatus,
    0x0F, &MAIN_SB_DSP_ReadINT16BitACK,
};

QEMM_IOPT OPL3IOPT;
QEMM_IOPT OPL3IOPT_PM;
QEMM_IOPT MAIN_VDMA_IOPT;
QEMM_IOPT MAIN_VIRQ_IOPT;
QEMM_IOPT MAIN_SB_IOPT;
QEMM_IOPT MAIN_VDMA_IOPT_PM1;
QEMM_IOPT MAIN_VDMA_IOPT_PM2;
QEMM_IOPT MAIN_VDMA_IOPT_PM3;
QEMM_IOPT MAIN_VHDMA_IOPT_PM1;
QEMM_IOPT MAIN_VHDMA_IOPT_PM2;
QEMM_IOPT MAIN_VHDMA_IOPT_PM3;
QEMM_IOPT MAIN_VIRQ_IOPT_PM1;
QEMM_IOPT MAIN_VIRQ_IOPT_PM2;
QEMM_IOPT MAIN_SB_IOPT_PM;

#define MAIN_SETCMD_SET 0x01 //set in command line
#define MAIN_SETCMD_HIDDEN 0x02 //hidden flag on report

struct MAIN_OPT
{
    const char* option;
    const char* desc;
    int value;
    int setcmd; //set by command line
}MAIN_Options[] =
{
    "/?", "Show help", FALSE, 0,
    "/A", "Specify IO address, valid value: 220,240", 0x220, 0,
    "/I", "Specify IRQ number, valud value: 5,7", 7, 0,
    "/D", "Specify DMA channel, valid value: 0,1,3", 1, 0,
    "/T", "Specify SB Type, valid value: 1-6", 5, 0,
    "/H", "Specify High DMA channel, valid value: 5,6,7", 5, 0,
    "/OPL", "Enable OPL3 emulation", TRUE, 0,
    "/PM", "Support protected mode games", TRUE, 0,
    "/RM", "Support real mode games", TRUE, 0,
    "/O", "Select output. 0: headphone, 1: speaker. Intel HDA only", 1, 0,
    "/VOL", "Set master volume (0-9)", 7, 0,
    "/K", "Set internal sample rate, valid value: 22050,44100", 0x22050, 0,
    "/SCL", "List installed sound cards", 0, MAIN_SETCMD_HIDDEN,
    "/SC", "Select sound card index in list (/SCL)", 0, MAIN_SETCMD_HIDDEN,
    "/R", "Reset sound card driver", 0, MAIN_SETCMD_HIDDEN,

    NULL, NULL, 0,
};
enum EOption
{
    OPT_Help,
    OPT_ADDR,
    OPT_IRQ,
    OPT_DMA,
    OPT_TYPE,
    OPT_HDMA,
    OPT_OPL,
    OPT_PM,
    OPT_RM,
    OPT_OUTPUT,
    OPT_VOL,
    OPT_RATE,
    OPT_SCLIST,
    OPT_SC,
    OPT_RESET,

    OPT_COUNT,
};

//T1~T6 maps
static const char* MAIN_SBTypeString[] =
{
    "0",
    "1.0/1.5",
    "Pro (old)",
    "2.0",
    "Pro",
    "Pro",
    "16",
};
static int MAIN_SB_DSPVersion[] =
{
    0,
    0x0100,
    0x0300,
    0x0202,
    0x0302,
    0x0302,
    0x0400,
};

static void MAIN_InvokeIRQ(uint8_t irq) //generate virtual IRQ
{
    #if MAIN_TRAP_PIC_ONDEMAND
    if(MAIN_Options[OPT_RM].value) QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
    if(MAIN_Options[OPT_PM].value)
    {
        HDPMIPT_Install_IOPortTrap(0x20, 0x21, MAIN_VIRQ_IODT, 2, &MAIN_VIRQ_IOPT_PM1);
        HDPMIPT_Install_IOPortTrap(0xA0, 0xA1, MAIN_VIRQ_IODT+2, 2, &MAIN_VIRQ_IOPT_PM2);
    }
    #endif
    VIRQ_Invoke(irq, &MAIN_IntContext.regs, MAIN_IntContext.EFLAGS&CPU_VMFLAG);
    #if MAIN_TRAP_PIC_ONDEMAND
    if(MAIN_Options[OPT_RM].value) QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
    if(MAIN_Options[OPT_PM].value)
    {
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM1);
        HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM2);
    }
    #endif
}

static void MAIN_SetBlasterEnv(struct MAIN_OPT* opt) //alter BLASTER env.
{
    char buf[256];
    if(opt[OPT_TYPE].value != 6)
        sprintf(buf, "A%x I%x D%x", opt[OPT_ADDR].value, opt[OPT_IRQ].value, opt[OPT_DMA].value);
    else
        sprintf(buf, "A%x I%x D%x T%x H%x", opt[OPT_ADDR].value, opt[OPT_IRQ].value, opt[OPT_DMA].value, opt[OPT_TYPE].value, opt[OPT_HDMA].value);
    #ifdef DJGPP //makes vscode happy
    setenv("BLASTER", buf, TRUE);
    #endif
}

int main(int argc, char* argv[])
{
    printf("SBEMU: Sound Blaster emulation on AC97. Version: %s", MAIN_SBEMU_VER);
    if((argc == 2 && stricmp(argv[1],"/?") == 0))
    {
        printf("Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option)
        {
            printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
            ++i;
        }
        printf("Note: SBEMU will read BLASTER environment variable and use it, "
        "\n if /A /I /D /T /H set, they will override the BLASTER values.\n");
        printf("Source code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }
    printf("\n");
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
                else if(c =='T')
                    MAIN_Options[OPT_TYPE].value = *(blaster++) - '0';
                else if(c =='H')
                    MAIN_Options[OPT_HDMA].value = *(blaster++) - '0';
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
                MAIN_Options[j].value = arglen == len ? 1 : strtol(&argv[i][len], NULL, 16);
                MAIN_Options[j].setcmd |= MAIN_SETCMD_SET;
                break;
            }
        }
    }

    if(MAIN_Options[OPT_ADDR].value != 0x220 && MAIN_Options[OPT_ADDR].value != 0x240)
    {
        printf("Error: Invalid IO port address: %x.\n", MAIN_Options[OPT_ADDR].value);
        return 1;
    }
    if(MAIN_Options[OPT_IRQ].value != 0x5 && MAIN_Options[OPT_IRQ].value != 0x7)
    {
        printf("Error: invalid IRQ: %d.\n", MAIN_Options[OPT_IRQ].value);
        return 1;
    }
    if(MAIN_Options[OPT_DMA].value != 0x0 && MAIN_Options[OPT_DMA].value != 0x1 && MAIN_Options[OPT_DMA].value != 0x3)
    {
        printf("Error: invalid DMA channel.\n");
        return 1;
    }
    if(MAIN_Options[OPT_TYPE].value <= 0 || MAIN_Options[OPT_TYPE].value > 6)
    {
        printf("Error: invalid SB Type.\n");
        return 1;
    }
    if(MAIN_Options[OPT_OUTPUT].value != 0 && MAIN_Options[OPT_OUTPUT].value != 1)
    {
        printf("Error: Invalid Output.\n");
        return 1;
    }
    if(MAIN_Options[OPT_VOL].value < 0 || MAIN_Options[OPT_VOL].value > 9)
    {
        printf("Error: Invalid Volume.\n");
        return 1;
    }
    if(MAIN_Options[OPT_RATE].value != 0x22050 && MAIN_Options[OPT_RATE].value != 0x44100)
    {
        printf("Error: Invalid Sample rate.\n");
        return 1;
    }
    if(MAIN_Options[OPT_TYPE].value != 6)
        MAIN_Options[OPT_HDMA].value = MAIN_Options[OPT_DMA].value; //16 bit transfer through 8 bit dma

    DPMI_Init();

    MAIN_QEMM_Present = TRUE;
    if(MAIN_Options[OPT_RM].value)
    {
        MAIN_Options[OPT_RM].value = TRUE; //set to known value for compare
        int bcd = QEMM_GetVersion();
        _LOG("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
        if(bcd < 0x703)
        {
            printf("QEMM not installed, or version bellow 7.03: %x.%02x, disable real mode support.\n", bcd>>8, bcd&0xFF);
            MAIN_Options[OPT_RM].value = FALSE;
            MAIN_QEMM_Present = FALSE;
        }
    }
    MAIN_HDPMI_Present = FALSE;
    if(MAIN_Options[OPT_PM].value)
    {
        MAIN_Options[OPT_PM].value = TRUE; //set to known value for compare
        BOOL hasHDPMI = HDPMIPT_Detect(); //another DPMI host used other than HDPMI
        if(!hasHDPMI)
            printf("HDPMI not installed, disable protected mode support.\n");
        MAIN_Options[OPT_PM].value = hasHDPMI;
        MAIN_HDPMI_Present = hasHDPMI;
    }
    MAIN_Options[OPT_OPL].value = (MAIN_Options[OPT_OPL].value) ? TRUE : FALSE;

    //TSR installation check: update parameter & exit if already installed
    MAIN_TSR_InstallationCheck();

    MAIN_SetBlasterEnv(MAIN_Options);

    BOOL enablePM = MAIN_Options[OPT_PM].value;
    BOOL enableRM = MAIN_Options[OPT_RM].value;
    if(!enablePM && !enableRM)
    {
        printf("Both real mode & protected mode supprted are disabled, exiting.\n");
        return 1;
    }

    if(MAIN_Options[OPT_SCLIST].value)
        aui.card_controlbits |= AUINFOS_CARDCNTRLBIT_TESTCARD; //note: this bit will make aui.card_handler == NULL and quit.
    if(MAIN_Options[OPT_SC].value)
        aui.card_select_index = MAIN_Options[OPT_SC].value; //TODO: this is a HEX in commandline, it's OK to not inform user since there's might not be over 10 (0xA) sound cards installed
    aui.card_select_config = MAIN_Options[OPT_OUTPUT].value;
    AU_init(&aui);
    if(!aui.card_handler)
        return 1;
    if(aui.card_irq == MAIN_Options[OPT_IRQ].value)
    {
        printf("Sound card IRQ conflict, abort.\n");
        printf("Please try use /i5 or /i7 switch, or disable some onboard devices in the BIOS settings to release IRQs.\n");
        return 1;
    }
    
    printf("Real mode support: %s.\n", enableRM ? "enabled" : "disabled");
    printf("Protected mode support: %s.\n", enablePM ? "enabled" : "disabled");
    if(enableRM)
    {
        UntrappedIO_OUT_Handler = &QEMM_UntrappedIO_Write;
        UntrappedIO_IN_Handler = &QEMM_UntrappedIO_Read;
    }
    else
    {
        UntrappedIO_OUT_Handler = &HDPMIPT_UntrappedIO_Write;
        UntrappedIO_IN_Handler = &HDPMIPT_UntrappedIO_Read;
    }

    if(MAIN_Options[OPT_OPL].value)
    {
        if(enableRM && !QEMM_Install_IOPortTrap(MAIN_OPL3IODT, 4, &OPL3IOPT))
        {
            printf("Error: Failed installing IO port trap for QEMM.\n");
            return 1;
        }
        if(enablePM && !HDPMIPT_Install_IOPortTrap(0x388, 0x38B, MAIN_OPL3IODT, 4, &OPL3IOPT_PM))
        {
            printf("Error: Failed installing IO port trap for HDPMI.\n");
            if(enableRM) QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            return 1;          
        }

        //OPL3EMU_Init(aui.freq_card);
        printf("OPL3 emulation enabled at port 388h.\n");
    }
    
    MAIN_SbemuExtFun.StartPlayback = NULL; //not used
    MAIN_SbemuExtFun.RaiseIRQ = NULL;
    MAIN_SbemuExtFun.DMA_Size = &VDMA_GetCounter;
    MAIN_SbemuExtFun.DMA_Write = &VDMA_WriteData;

    SBEMU_Init(MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, MAIN_Options[OPT_HDMA].value, MAIN_SB_DSPVersion[MAIN_Options[OPT_TYPE].value], &MAIN_SbemuExtFun);
    VDMA_Virtualize(MAIN_Options[OPT_DMA].value, TRUE);
    if(MAIN_Options[OPT_TYPE].value == 6)
        VDMA_Virtualize(MAIN_Options[OPT_HDMA].value, TRUE);
    for(int i = 0; i < countof(MAIN_SB_IODT); ++i)
        MAIN_SB_IODT[i].port += MAIN_Options[OPT_ADDR].value;
    QEMM_IODT* SB_Iodt = MAIN_Options[OPT_OPL].value ? MAIN_SB_IODT : MAIN_SB_IODT+4;
    int SB_IodtCount = MAIN_Options[OPT_OPL].value ? countof(MAIN_SB_IODT) : countof(MAIN_SB_IODT)-4;

    printf("Sound Blaster %s emulation enabled at Adress: %x, IRQ: %x, DMA: %x\n", MAIN_SBTypeString[MAIN_Options[OPT_TYPE].value], MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value);

    BOOL QEMMInstalledVDMA = !enableRM || QEMM_Install_IOPortTrap(MAIN_VDMA_IODT, countof(MAIN_VDMA_IODT), &MAIN_VDMA_IOPT);
    #if MAIN_TRAP_PIC_ONDEMAND//will crash with VIRQ installed, do it temporarily. TODO: figure out why
    BOOL QEMMInstalledVIRQ = TRUE;
    #else
    BOOL QEMMInstalledVIRQ = !enableRM || QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
    #endif
    BOOL QEMMInstalledSB = !enableRM || QEMM_Install_IOPortTrap(SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT);

    BOOL HDPMIInstalledVDMA1 = !enablePM || HDPMIPT_Install_IOPortTrap(0x0, 0xF, MAIN_VDMA_IODT, 16, &MAIN_VDMA_IOPT_PM1);
    BOOL HDPMIInstalledVDMA2 = !enablePM || HDPMIPT_Install_IOPortTrap(0x81, 0x83, MAIN_VDMA_IODT+16, 3, &MAIN_VDMA_IOPT_PM2);
    BOOL HDPMIInstalledVDMA3 = !enablePM || HDPMIPT_Install_IOPortTrap(0x87, 0x87, MAIN_VDMA_IODT+19, 1, &MAIN_VDMA_IOPT_PM3);
    BOOL HDPMIInstalledVHDMA1 = !enablePM || HDPMIPT_Install_IOPortTrap(0xC0, 0xDE, MAIN_VDMA_IODT+20, 16, &MAIN_VHDMA_IOPT_PM1);
    BOOL HDPMIInstalledVHDMA2 = !enablePM || HDPMIPT_Install_IOPortTrap(0x89, 0x8B, MAIN_VDMA_IODT+36, 3, &MAIN_VHDMA_IOPT_PM2);
    BOOL HDPMIInstalledVHDMA3 = !enablePM || HDPMIPT_Install_IOPortTrap(0x8F, 0x8F, MAIN_VDMA_IODT+39, 1, &MAIN_VHDMA_IOPT_PM3);
    #if MAIN_TRAP_PIC_ONDEMAND
    BOOL HDPMIInstalledVIRQ1 = TRUE;
    BOOL HDPMIInstalledVIRQ2 = TRUE;
    #else
    BOOL HDPMIInstalledVIRQ1 = !enablePM || HDPMIPT_Install_IOPortTrap(0x20, 0x21, MAIN_VIRQ_IODT, 2, &MAIN_VIRQ_IOPT_PM1);
    BOOL HDPMIInstalledVIRQ2 = !enablePM || HDPMIPT_Install_IOPortTrap(0xA0, 0xA1, MAIN_VIRQ_IODT+2, 2, &MAIN_VIRQ_IOPT_PM2);
    #endif
    BOOL HDPMIInstalledSB = !enablePM || HDPMIPT_Install_IOPortTrap(MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_ADDR].value+0x0F, SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT_PM);

    BOOL TSR_ISR = FALSE;
    for(int i = MAIN_TSR_INTSTART_ID; i <= 0xFF; ++i)
    {
        DPMI_REG r = {0};
        r.h.ah = i;
        //DBG_DumpREG(&r);
        DPMI_CallRealModeINT(MAIN_TSR_INT, &r);
        if(r.h.al != 0) //find free multiplex number
            continue;
        MAIN_TSR_INT_FNO = i;
        TSR_ISR = DPMI_InstallRealModeISR(MAIN_TSR_INT, MAIN_TSR_Interrupt, &MAIN_TSRREG, &MAIN_TSRIntHandle) == 0;
        if(!TSR_ISR)
            break;
        MAIN_ISR_DOSID = DPMI_HighMalloc((sizeof(MAIN_ISR_DOSID_String)+15)>>4, TRUE);
        //printf("DOSID:%x\n", DPMI_SEGOFF2L(MAIN_ISR_DOSID,0));
        DPMI_CopyLinear(DPMI_SEGOFF2L(MAIN_ISR_DOSID,0), DPMI_PTR2L((char*)MAIN_ISR_DOSID_String), sizeof(MAIN_ISR_DOSID_String));
        break;
    }

    _LOG("sound card IRQ: %d\n", aui.card_irq);
    PIC_MaskIRQ(aui.card_irq);
    AU_ini_interrupts(&aui);
    int samplerate = (MAIN_Options[OPT_RATE].value == 0x22050) ? 22050 : 44100;
    mpxplay_audio_decoder_info_s adi = {NULL, 0, 1, samplerate, SBEMU_CHANNELS, SBEMU_CHANNELS, NULL, SBEMU_BITS, SBEMU_BITS/8, 0};
    AU_setrate(&aui, &adi);
    AU_setmixer_init(&aui);
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
    //set volume
    AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, MAIN_Options[OPT_VOL].value*100/9);
    if(MAIN_Options[OPT_OPL].value)
        OPL3EMU_Init(aui.freq_card); //aui.freq_card available after AU_setrate

    BOOL PM_ISR = DPMI_InstallISR(PIC_IRQ2VEC(aui.card_irq), MAIN_InterruptPM, &MAIN_IntHandlePM) == 0;
    //set default ACK, to skip recursion of DOS/4GW
    {
        DPMI_ISR_HANDLE h;
        for(int i = 0; i < 15; ++i)
        {
            DPMI_GetISR(PIC_IRQ2VEC(i), &h);
            HDPMIPT_InstallIRQACKHandler(i, h.old_cs, h.old_offset);
        }
    }
    HDPMIPT_InstallIRQACKHandler(aui.card_irq, MAIN_IntHandlePM.wrapper_cs, MAIN_IntHandlePM.wrapper_offset);
    #if MAIN_INSTALL_RM_ISR
    BOOL RM_ISR = DPMI_InstallRealModeISR(PIC_IRQ2VEC(aui.card_irq), MAIN_InterruptRM, &MAIN_IntREG, &MAIN_IntHandleRM) == 0;
    #else
    BOOL RM_ISR = TRUE;
    #endif
    PIC_UnmaskIRQ(aui.card_irq);

    AU_prestart(&aui);
    AU_start(&aui);

    BOOL TSR = TRUE;
    if(!PM_ISR || !RM_ISR || !TSR_ISR
    || !QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB
    || !HDPMIInstalledVDMA1 || !HDPMIInstalledVDMA2 || !HDPMIInstalledVDMA3 || !HDPMIInstalledVHDMA1 || !HDPMIInstalledVHDMA2 || !HDPMIInstalledVHDMA3 
    || !HDPMIInstalledVIRQ1 || !HDPMIInstalledVIRQ2 || !HDPMIInstalledSB
    || !(TSR=DPMI_TSR()))
    {
        if(MAIN_Options[OPT_OPL].value)
        {
            if(enableRM) QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
            if(enablePM) HDPMIPT_Uninstall_IOPortTrap(&OPL3IOPT_PM);
        }

        if(!QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB)
            printf("Error: Failed installing IO port trap for QEMM.\n");
        if(enableRM && QEMMInstalledVDMA) QEMM_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT);
        #if !MAIN_TRAP_PIC_ONDEMAND
        if(enableRM && QEMMInstalledVIRQ) QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
        #endif
        if(enableRM && QEMMInstalledSB) QEMM_Uninstall_IOPortTrap(&MAIN_SB_IOPT);

        if(!HDPMIInstalledVDMA1 || !HDPMIInstalledVDMA2 || !HDPMIInstalledVDMA3 || !HDPMIInstalledVHDMA1 || !HDPMIInstalledVHDMA2 || !HDPMIInstalledVHDMA3 || !HDPMIInstalledVIRQ1 || !HDPMIInstalledVIRQ2 || !HDPMIInstalledSB)
            printf("Error: Failed installing IO port trap for HDPMI.\n");
        if(enablePM && HDPMIInstalledVDMA1) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM1);
        if(enablePM && HDPMIInstalledVDMA2) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM2);
        if(enablePM && HDPMIInstalledVDMA3) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM3);
        if(enablePM && HDPMIInstalledVHDMA1) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM1);
        if(enablePM && HDPMIInstalledVHDMA2) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM2);
        if(enablePM && HDPMIInstalledVHDMA3) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM3);
        #if !MAIN_TRAP_PIC_ONDEMAND
        if(enablePM && HDPMIInstalledVIRQ1) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM1);
        if(enablePM && HDPMIInstalledVIRQ2) HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM2);
        #endif
        if(enablePM && HDPMIInstalledSB) HDPMIPT_Uninstall_IOPortTrap(&MAIN_SB_IOPT_PM);

        if(!PM_ISR)
            printf("Error: Failed installing sound card ISR.\n");
        if(!RM_ISR)
            printf("Error: Failed installing sound card ISR.\n");
        if(PM_ISR) DPMI_UninstallISR(&MAIN_IntHandlePM);
        #if MAIN_INSTALL_RM_ISR
        if(RM_ISR) DPMI_UninstallISR(&MAIN_IntHandleRM);
        #endif
        if(!TSR_ISR)
            printf("Error: Failed installing TSR interrupt.\n");
        if(TSR_ISR) DPMI_UninstallISR(&MAIN_TSRIntHandle);

        if(!TSR)
            printf("Error: Failed installing TSR.\n");
    }
    return 1;
}

static void MAIN_InterruptPM()
{
    HDPMIPT_GetInterrupContext(&MAIN_IntContext);
    if(!MAIN_InINT && aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    {
        MAIN_Interrupt();
        PIC_SendEOIWithIRQ(aui.card_irq);
    }
    else
    {
        BOOL InInt = MAIN_InINT;
        MAIN_InINT = TRUE;
        if(MAIN_IntContext.EFLAGS&CPU_VMFLAG)
            DPMI_CallOldISR(&MAIN_IntHandlePM);
        else
            DPMI_CallOldISRWithContext(&MAIN_IntHandlePM, &MAIN_IntContext.regs);
        PIC_UnmaskIRQ(aui.card_irq);
        MAIN_InINT = InInt;
    }
}

static void MAIN_InterruptRM()
{
    if(!MAIN_InINT && aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    {
        MAIN_IntContext.regs = MAIN_IntREG;
        MAIN_IntContext.EFLAGS = MAIN_IntREG.w.flags | CPU_VMFLAG;
        MAIN_Interrupt();
        PIC_SendEOIWithIRQ(aui.card_irq);
    }
    else
    {
        BOOL InInt = MAIN_InINT;
        MAIN_InINT = TRUE;
        DPMI_CallRealModeOldISR(&MAIN_IntHandleRM, &MAIN_IntREG);
        PIC_UnmaskIRQ(aui.card_irq);
        MAIN_InINT = InInt;
    }
}

static void MAIN_Interrupt()
{
    #if 0
    aui.card_outbytes = aui.card_dmasize;
    int space = AU_cardbuf_space(&aui)+2048;
    //_LOG("int space: %d\n", space);
    int samples = space / sizeof(int16_t) / 2 * 2;
    //int samples = 22050/18*2;
    _LOG("samples: %d %d\n", 22050/18*2, space/4*2);
    static int cur = 0;
    aui.samplenum = min(samples, TEST_SampleLen-cur);
    aui.pcm_sample = TEST_Sample + cur;
    //_LOG("cur: %d %d\n",cur,aui.samplenum);
    cur += aui.samplenum;
    cur -= AU_writedata(&aui);
    #else
    if(!(aui.card_infobits&AUINFOS_CARDINFOBIT_PLAYING))
        return;
        
    if(SBEMU_IRQTriggered())
    {
        MAIN_InvokeIRQ(SBEMU_GetIRQ());
        SBEMU_SetIRQTriggered(FALSE);
    }
    int32_t vol;
    int32_t voicevol;
    int32_t midivol;
    if(MAIN_Options[OPT_TYPE].value == 1 || MAIN_Options[OPT_TYPE].value == 3) //SB2.0 and before
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERVOL) >> 1)*256/7;
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICEVOL) >> 1)*256/3;
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDIVOL) >> 1)*256/7;
    }
    else if(MAIN_Options[OPT_TYPE].value == 6) //SB16
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERSTEREO)>>4)*256/15; //4:4
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICESTEREO)>>4)*256/15; //4:4
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDISTEREO)>>4)*256/15; //4:4
        //_LOG("vol: %d, voicevol: %d, midivol: %d\n", vol, voicevol, midivol);
    }
    else //SBPro
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERSTEREO)>>5)*256/7; //3:1:3:1 stereo usually the same for both channel for games?;
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICESTEREO)>>5)*256/7; //3:1:3:1
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDISTEREO)>>5)*256/7;
        //_LOG("vol: %d, voicevol: %d, midivol: %d\n", vol, voicevol, midivol);
    }

    aui.card_outbytes = aui.card_dmasize;
    int samples = AU_cardbuf_space(&aui) / sizeof(int16_t) / 2; //16 bit, 2 channels
    //_LOG("samples:%d\n",samples);
    if(samples == 0)
        return;
    
    BOOL digital = SBEMU_HasStarted();
    int dma = (SBEMU_GetBits() <= 8 || MAIN_Options[OPT_TYPE].value < 6) ? SBEMU_GetDMA() : SBEMU_GetHDMA();
    int32_t DMA_Count = VDMA_GetCounter(dma); //count in bytes
    if(digital)//&& DMA_Count != 0x10000) //-1(0xFFFF)+1=0
    {
        uint32_t DMA_Addr = VDMA_GetAddress(dma);
        int32_t DMA_Index = VDMA_GetIndex(dma);
        uint32_t SB_Bytes = SBEMU_GetSampleBytes();
        uint32_t SB_Pos = SBEMU_GetPos();
        uint32_t SB_Rate = SBEMU_GetSampleRate();
        int samplesize = max(1, SBEMU_GetBits()/8); //sample size in bytes 1 for 8bit. 2 for 16bit
        int channels = SBEMU_GetChannels();
        _LOG("sample rate: %d %d\n", SB_Rate, aui.freq_card);
        _LOG("channels: %d, size:%d\n", channels, samplesize);
        //_LOG("DMA index: %x\n", DMA_Index);
        //_LOG("digital start\n");
        int pos = 0;
        do {
            if(MAIN_DMA_MappedAddr != 0
             && !(DMA_Addr >= MAIN_DMA_Addr && DMA_Addr+DMA_Index+DMA_Count <= MAIN_DMA_Addr+MAIN_DMA_Size))
            {
                if(MAIN_DMA_MappedAddr > 1024*1024)
                    DPMI_UnmappMemory(MAIN_DMA_MappedAddr);
                MAIN_DMA_MappedAddr = 0;
            }
            if(MAIN_DMA_MappedAddr == 0)
            {
                MAIN_DMA_Addr = DMA_Addr&~0xFFF;
                MAIN_DMA_Size = align(max(DMA_Addr-MAIN_DMA_Addr+DMA_Index+DMA_Count, 64*1024*2), 4096);
                MAIN_DMA_MappedAddr = (DMA_Addr+DMA_Index+DMA_Count <= 1024*1024) ? (DMA_Addr&~0xFFF) : DPMI_MapMemory(MAIN_DMA_Addr, MAIN_DMA_Size);
            }
            //_LOG("DMA_ADDR:%x, %x, %x\n",DMA_Addr, MAIN_DMA_Addr, MAIN_DMA_MappedAddr);

            int count = samples-pos;
            BOOL resample = TRUE; //don't resample if sample rates are close
            if(SB_Rate < aui.freq_card-50)
                count = max(1, count*SB_Rate/aui.freq_card);
            else if(SB_Rate > aui.freq_card+50)
                count = count*SB_Rate/aui.freq_card;
            else
                resample = FALSE;
            count = min(count, max(1,(DMA_Count)/samplesize/channels)); //max for stereo initial 1 byte
            count = min(count, max(1,(SB_Bytes-SB_Pos)/samplesize/channels)); //max for stereo initial 1 byte. 1/2channel = 0, make it 1
            if(SBEMU_GetBits()<8) //ADPCM 8bit
                count = max(1, count / (9 / SBEMU_GetBits()));
            _LOG("samples:%d %d %d, %d %d, %d %d\n", samples, pos+count, count, DMA_Count, DMA_Index, SB_Bytes, SB_Pos);
            int bytes = count * samplesize * channels;

            if(MAIN_DMA_MappedAddr == 0) //map failed?
                memset(MAIN_PCM+pos*2, 0, bytes);
            else
                DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM+pos*2), MAIN_DMA_MappedAddr+(DMA_Addr-MAIN_DMA_Addr)+DMA_Index, bytes);
            if(SBEMU_GetBits()<8) //ADPCM  8bit
                count = SBEMU_DecodeADPCM((uint8_t*)(MAIN_PCM+pos*2), bytes);
            if(samplesize != 2)
                cv_bits_n_to_m(MAIN_PCM+pos*2, count*channels, samplesize, 2);
            if(resample/*SB_Rate != aui.freq_card*/)
                count = mixer_speed_lq(MAIN_PCM+pos*2, count*channels, channels, SB_Rate, aui.freq_card)/channels;
            if(channels == 1) //should be the last step
                cv_channels_1_to_n(MAIN_PCM+pos*2, count, 2, 2);
            pos += count;
            //_LOG("samples:%d %d %d\n", count, pos, samples);
            DMA_Index = VDMA_SetIndexCounter(dma, DMA_Index+bytes, DMA_Count-bytes);
            DMA_Count = VDMA_GetCounter(dma);
            SB_Pos = SBEMU_SetPos(SB_Pos+bytes);
            //_LOG("SB bytes: %d %d\n", SB_Pos, SB_Bytes);
            if(SB_Pos >= SB_Bytes)
            {
                //_LOG("INT:%d,%d,%d,%d\n",MAIN_SBBytes,SBEMU_GetSampleBytes(),MAIN_DMAIndex,DMA_Count);
                //_LOG("SBEMU: Auto: %d\n",SBEMU_GetAuto());
                if(!SBEMU_GetAuto())
                    SBEMU_Stop();
                SB_Pos = SBEMU_SetPos(0);
                
                MAIN_InvokeIRQ(SBEMU_GetIRQ());
                if(SB_Bytes <= 32) //detection routine?
                {
                    int c = SBEMU_GetDetectionCounter();
                    if(++c >= 256) //Miles Sound will "freeze" or crash when we continually send virtual interrupt to it, it seems it processes slow and virtual interrupt keeps happening until crash.
                        SBEMU_Stop(); //fix problem when Miles Sound using SB driver on SBPro emulation
                    SBEMU_SetDetectionCounter(c);
                    break; //fix crash in virtualbox.
                }
                
                SB_Bytes = SBEMU_GetSampleBytes();
                SB_Pos = SBEMU_GetPos();
                SB_Rate = SBEMU_GetSampleRate();
                //incase IRQ handler re-programs DMA
                DMA_Index = VDMA_GetIndex(dma);
                DMA_Count = VDMA_GetCounter(dma);
                DMA_Addr = VDMA_GetAddress(dma);
                //_LOG("DMACount: %d, DMAIndex:%d, DMA_Addr:%x\n",DMA_Count, DMA_Index, DMA_Addr);
            }
        } while(VDMA_GetAuto(dma) && (pos < samples) && SBEMU_HasStarted());
        //_LOG("digital end %d %d\n", samples, pos);
        //for(int i = pos; i < samples; ++i)
        //    MAIN_PCM[i*2+1] = MAIN_PCM[i*2] = 0;
        samples = min(samples, pos);
    }
    else if(SBEMU_GetDirectCount()>=3)
    {
        samples = SBEMU_GetDirectCount();
        _LOG("direct out:%d %d\n",samples,aui.card_samples_per_int);
        memcpy(MAIN_PCM, SBEMU_GetDirectPCM8(), samples);
        SBEMU_ResetDirect();
        #if 1 //fix noise for some games
        int zeros = TRUE;
        for(int i = 0; i < samples && zeros; ++i)
        {
            if(((uint8_t*)MAIN_PCM)[i] != 0)
                zeros = FALSE;
        }
        if(zeros)
        {
            for(int i = 0; i < samples; ++i)
                ((uint8_t*)MAIN_PCM)[i] = 128;
        }
        #endif
        //for(int i = 0; i < samples; ++i) _LOG("%d ",((uint8_t*)MAIN_PCM)[i]); _LOG("\n");
        cv_bits_n_to_m(MAIN_PCM, samples, 1, 2);
        //for(int i = 0; i < samples; ++i) _LOG("%d ",MAIN_PCM[i]); _LOG("\n");
        const int interrupt_frequency = aui.freq_card/aui.card_samples_per_int;
        samples = mixer_speed_lq(MAIN_PCM, samples, 1, (samples-1)*interrupt_frequency, aui.freq_card);
        //for(int i = 0; i < samples; ++i) _LOG("%d ",MAIN_PCM[i]); _LOG("\n");
        cv_channels_1_to_n(MAIN_PCM, samples, 2, 2);
        digital = TRUE;
    }
    else if(!MAIN_Options[OPT_OPL].value)
        memset(MAIN_PCM, 0, samples*sizeof(int16_t)*2); //output muted samples.

    if(MAIN_Options[OPT_OPL].value)
    {
        int16_t* pcm = digital ? MAIN_OPLPCM : MAIN_PCM;
        OPL3EMU_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        int channels = OPL3EMU_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(pcm, samples, 2, SBEMU_BITS/8);

        if(digital)
        {
            for(int i = 0; i < samples*2; ++i)
            {
                #if 1
                // https://stackoverflow.com/questions/12089662/mixing-16-bit-linear-pcm-streams-and-avoiding-clipping-overflow
                int a = (int)(MAIN_PCM[i] * voicevol/256) + 32768;
                int b = (int)(MAIN_OPLPCM[i] * midivol/256 * (MAIN_DOUBLE_OPL_VOLUME+1)) + 32768;
                int mixed = (a < 32768 || b < 32768) ? (a*b/32768) : ((a+b)*2 - a*b/32768 - 65536);
                if(mixed == 65536) mixed = 65535;
                MAIN_PCM[i] = (mixed - 32768) * vol/256;
                #else //simple average: sounds the same as DOSBox
                int a = (int)(MAIN_PCM[i] * voicevol/256);
                int b = (int)(MAIN_OPLPCM[i] * midivol/256);
                MAIN_PCM[i] = (a+b)/2 * vol/256;
                #endif
            }
        }
        else for(int i = 0; i < samples*2; ++i)
            MAIN_PCM[i] = MAIN_PCM[i] * midivol/256 * vol/256;
    }
    else if(digital)
        for(int i = 0; i < samples*2; ++i)
            MAIN_PCM[i] = MAIN_PCM[i] * voicevol/256 * vol/256;
    samples *= 2; //to stereo

    aui.samplenum = samples;
    aui.pcm_sample = MAIN_PCM;
    AU_writedata(&aui);

    //_LOG("MAIN INT END\n");
    #endif
}

void MAIN_TSR_InstallationCheck()
{
    for(int i = MAIN_TSR_INTSTART_ID; i <= 0xFF; ++i)
    { //detect TSR existence
        DPMI_REG r = {0};
        r.h.ah = i;
        DPMI_CallRealModeINT(MAIN_TSR_INT, &r);
        if(r.h.al == 0)
            continue;
        //printf("DOSID:%x\n", DPMI_SEGOFF2L(r.w.dx, r.w.di));
        if(DPMI_CompareLinear(DPMI_SEGOFF2L(r.w.dx, r.w.di), DPMI_PTR2L((char*)MAIN_ISR_DOSID_String), 16) == 0)
        {
            printf("SBEMU is active.\n");

            r.h.ah = i;
            r.h.al = 0x01; //get current settings
            DPMI_CallRealModeINT(MAIN_TSR_INT, &r);

            struct MAIN_OPT* opt = (struct MAIN_OPT*)malloc(sizeof(MAIN_Options));
            DPMI_CopyLinear(DPMI_PTR2L(opt), r.d.ebx, sizeof(MAIN_Options)); //note: copy array won't copy string pointers in the array, don't reference them, use string in MAIN_Options.

            for(int j = 0; j < OPT_COUNT; ++j)
            {
                if((MAIN_Options[j].setcmd==MAIN_SETCMD_SET) && MAIN_Options[j].value != opt[j].value)
                {
                    printf("%s changed from %x to %x\n", MAIN_Options[j].option, opt[j].value, MAIN_Options[j].value);
                    opt[j].value = MAIN_Options[j].value;
                }
            }
            if(MAIN_Options[OPT_RESET].value)
                printf("Resetting sound card driver...\n");
            printf("\n");

            if((MAIN_Options[OPT_ADDR].setcmd&MAIN_SETCMD_SET) || (MAIN_Options[OPT_IRQ].setcmd&MAIN_SETCMD_SET) || (MAIN_Options[OPT_DMA].setcmd&MAIN_SETCMD_SET) || (MAIN_Options[OPT_TYPE].setcmd&MAIN_SETCMD_SET) || (MAIN_Options[OPT_HDMA].setcmd&MAIN_SETCMD_SET))
                MAIN_SetBlasterEnv(opt);

            r.h.ah = i;
            r.h.al = 0x02; //set new settings
            r.d.ebx = DPMI_PTR2L(opt);
            DPMI_CallRealModeINT(MAIN_TSR_INT, &r);

            r.h.ah = i;
            r.h.al = 0x01; //read back to confirm
            DPMI_CallRealModeINT(MAIN_TSR_INT, &r);
            DPMI_CopyLinear(DPMI_PTR2L(opt), r.d.ebx, sizeof(MAIN_Options));
            printf("Current settings:\n");
            for(int i = OPT_Help+1; i < OPT_COUNT; ++i)
            {
                if(!(MAIN_Options[i].setcmd&MAIN_SETCMD_HIDDEN))
                    printf("%-8s: %x\n", MAIN_Options[i].option, opt[i].value);
            }
            free(opt);
            exit(0);
        }
    }
}

static void MAIN_TSR_Interrupt()
{
    if(MAIN_TSRREG.h.ah != MAIN_TSR_INT_FNO)
    {
        //DBG_Log("chain %x ",MAIN_TSRREG.d.eax);
        //DBG_DumpREG(&MAIN_TSRREG);
        DPMI_CallRealModeOldISR(&MAIN_TSRIntHandle, &MAIN_TSRREG);
        return;
    }
    //DBG_Log("call tsr\n");
    switch(MAIN_TSRREG.h.al)
    {
        case 0x00:  //AMIS installation check
            MAIN_TSRREG.h.al = 0xFF;
            MAIN_TSRREG.w.cx = 0x0100; //bcd version
            MAIN_TSRREG.w.dx = (MAIN_ISR_DOSID&0xFFFF); //a real mode string buffer of identification: segment
            MAIN_TSRREG.w.di = 0; //offset
        return;
        case 0x01: //query
        {
            MAIN_TSRREG.d.ebx = DPMI_PTR2L(MAIN_Options);
        }
        return;
        case 0x02: //set
        {
            struct MAIN_OPT* opt = (struct MAIN_OPT*)malloc(sizeof(MAIN_Options));
            DPMI_CopyLinear(DPMI_PTR2L(opt), MAIN_TSRREG.d.ebx, sizeof(MAIN_Options));

            char* fpustate = (char*)malloc(108);
            #ifdef DJGPP //make vscode happy
            asm("fsave %0\n\t finit":"=m"(*fpustate));
            #endif
            int irq = aui.card_irq;
            PIC_MaskIRQ(irq);
            if(MAIN_Options[OPT_OUTPUT].value != opt[OPT_OUTPUT].value || MAIN_Options[OPT_RATE].value != opt[OPT_RATE].value || opt[OPT_RESET].value)
            {
                if(opt[OPT_OUTPUT].value != MAIN_Options[OPT_OUTPUT].value || opt[OPT_RESET].value)
                {
                    _LOG("Reset\n");
                    AU_close(&aui);
                    memset(&aui, 0, sizeof(aui));
                    aui.card_select_config = MAIN_Options[OPT_OUTPUT].value = opt[OPT_OUTPUT].value;
                    aui.card_select_index =  MAIN_Options[OPT_SC].value;
                    aui.card_controlbits |= AUINFOS_CARDCNTRLBIT_SILENT; //don't print anything in interrupt
                    AU_init(&aui);
                    AU_ini_interrupts(&aui);
                    AU_setmixer_init(&aui);
                    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
                    MAIN_Options[OPT_VOL].value = ~opt[OPT_VOL].value; //mark volume dirty
                }
                _LOG("Change sample rate\n");
                _LOG("FLAGS:%x\n",CPU_FLAGS());

                int samplerate = (opt[OPT_RATE].value == 0x22050) ? 22050 : 44100;
                mpxplay_audio_decoder_info_s adi = {NULL, 0, 1, samplerate, SBEMU_CHANNELS, SBEMU_CHANNELS, NULL, SBEMU_BITS, SBEMU_BITS/8, 0};
                AU_setrate(&aui, &adi);
                if(MAIN_Options[OPT_RATE].value != opt[OPT_RATE].value)
                    OPL3EMU_Init(aui.freq_card);
                AU_prestart(&aui); //setsamplerate/reset will do stop
                AU_start(&aui);
                MAIN_Options[OPT_RATE].value = opt[OPT_RATE].value;
            }
            if(MAIN_Options[OPT_VOL].value != opt[OPT_VOL].value)
            {
                _LOG("Reset volume\n");
                MAIN_Options[OPT_VOL].value = opt[OPT_VOL].value;
                AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, MAIN_Options[OPT_VOL].value*100/9);
            }
            #ifdef DJGPP //make vscode happy
            asm("frstor %0" ::"m"(*fpustate));
            #endif
            free(fpustate);
            PIC_UnmaskIRQ(irq);

            if(MAIN_Options[OPT_DMA].value != opt[OPT_DMA].value)
            {
                _LOG("Change DMA\n");
                VDMA_Virtualize(MAIN_Options[OPT_DMA].value, FALSE);
                VDMA_Virtualize(opt[OPT_DMA].value, TRUE);
            }
            if(MAIN_Options[OPT_HDMA].value != opt[OPT_HDMA].value)
            {
                _LOG("Change HDMA\n");
                VDMA_Virtualize(MAIN_Options[OPT_HDMA].value, FALSE);
                VDMA_Virtualize(opt[OPT_HDMA].value, TRUE);
            }
            if(MAIN_Options[OPT_DMA].value != opt[OPT_DMA].value || MAIN_Options[OPT_HDMA].value != opt[OPT_HDMA].value || MAIN_Options[OPT_IRQ].value != opt[OPT_IRQ].value || opt[OPT_TYPE].value != MAIN_Options[OPT_TYPE].value)
            {
                _LOG("Reinit SBEMU\n");
                MAIN_Options[OPT_DMA].value = opt[OPT_DMA].value;
                MAIN_Options[OPT_HDMA].value = opt[OPT_HDMA].value;
                MAIN_Options[OPT_IRQ].value = opt[OPT_IRQ].value;
                MAIN_Options[OPT_TYPE].value = opt[OPT_TYPE].value;
                SBEMU_Init(MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, MAIN_Options[OPT_HDMA].value, MAIN_SB_DSPVersion[MAIN_Options[OPT_TYPE].value], &MAIN_SbemuExtFun);
            }

            if(MAIN_Options[OPT_OPL].value == opt[OPT_OPL].value && MAIN_Options[OPT_ADDR].value == opt[OPT_ADDR].value && MAIN_Options[OPT_PM].value == opt[OPT_PM].value && MAIN_Options[OPT_RM].value == opt[OPT_RM].value)
            {
                free(opt);
                return;
            }
            
            //re-install all
            if(MAIN_Options[OPT_RM].value)
            {
                _LOG("uninstall qemm\n");
                if(MAIN_Options[OPT_OPL].value) QEMM_Uninstall_IOPortTrap(&OPL3IOPT);
                QEMM_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT);
                #if !MAIN_TRAP_PIC_ONDEMAND
                QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT);
                #endif
                QEMM_Uninstall_IOPortTrap(&MAIN_SB_IOPT);
            }
            if(MAIN_Options[OPT_PM].value)
            {
                _LOG("uninstall hdpmi\n");
                if(MAIN_Options[OPT_OPL].value) HDPMIPT_Uninstall_IOPortTrap(&OPL3IOPT_PM);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM1);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM2);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VDMA_IOPT_PM3);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM1);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM2);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VHDMA_IOPT_PM3);
                #if !MAIN_TRAP_PIC_ONDEMAND
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM1);
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT_PM2);
                #endif
                HDPMIPT_Uninstall_IOPortTrap(&MAIN_SB_IOPT_PM);
            }
            opt[OPT_PM].value = opt[OPT_PM].value && MAIN_HDPMI_Present;
            opt[OPT_RM].value = opt[OPT_RM].value && MAIN_QEMM_Present;

            if(opt[OPT_OPL].value)
            {
                _LOG("install opl\n");
                if(opt[OPT_RM].value) QEMM_Install_IOPortTrap(MAIN_OPL3IODT, 4, &OPL3IOPT);
                if(opt[OPT_PM].value) HDPMIPT_Install_IOPortTrap(0x388, 0x38B, MAIN_OPL3IODT, 4, &OPL3IOPT_PM);
            }

            QEMM_IODT* SB_Iodt = opt[OPT_OPL].value ? MAIN_SB_IODT : MAIN_SB_IODT+4;
            int SB_IodtCount = opt[OPT_OPL].value ? countof(MAIN_SB_IODT) : countof(MAIN_SB_IODT)-4;
            if(opt[OPT_ADDR].value != MAIN_Options[OPT_ADDR].value)
            {
                for(int i = 0; i < countof(MAIN_SB_IODT); ++i)
                    MAIN_SB_IODT[i].port = MAIN_SB_IODT[i].port - MAIN_Options[OPT_ADDR].value + opt[OPT_ADDR].value;
                MAIN_Options[OPT_ADDR].value = opt[OPT_ADDR].value;
            }

            if(opt[OPT_RM].value)
            {
                _LOG("install qemm\n");
                QEMM_Install_IOPortTrap(MAIN_VDMA_IODT, countof(MAIN_VDMA_IODT), &MAIN_VDMA_IOPT);
                QEMM_Install_IOPortTrap(SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT);
                #if !MAIN_TRAP_PIC_ONDEMAND
                QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT);
                #endif
            }

            if(opt[OPT_PM].value)
            {
                _LOG("install hdpmi\n");
                HDPMIPT_Install_IOPortTrap(0x0, 0xF, MAIN_VDMA_IODT, 16, &MAIN_VDMA_IOPT_PM1);
                HDPMIPT_Install_IOPortTrap(0x81, 0x83, MAIN_VDMA_IODT+16, 3, &MAIN_VDMA_IOPT_PM2);
                HDPMIPT_Install_IOPortTrap(0x87, 0x87, MAIN_VDMA_IODT+19, 1, &MAIN_VDMA_IOPT_PM3);
                HDPMIPT_Install_IOPortTrap(0xC0, 0xDE, MAIN_VDMA_IODT+20, 16, &MAIN_VHDMA_IOPT_PM1);
                HDPMIPT_Install_IOPortTrap(0x89, 0x8B, MAIN_VDMA_IODT+36, 3, &MAIN_VHDMA_IOPT_PM2);
                HDPMIPT_Install_IOPortTrap(0x8F, 0x8F, MAIN_VDMA_IODT+39, 1, &MAIN_VHDMA_IOPT_PM3);
                #if !MAIN_TRAP_PIC_ONDEMAND
                HDPMIPT_Install_IOPortTrap(0x20, 0x21, MAIN_VIRQ_IODT, 2, &MAIN_VIRQ_IOPT_PM1);
                HDPMIPT_Install_IOPortTrap(0xA0, 0xA1, MAIN_VIRQ_IODT+2, 2, &MAIN_VIRQ_IOPT_PM2);
                #endif
                HDPMIPT_Install_IOPortTrap(MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_ADDR].value+0x0F, SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT_PM);
            }

            if(opt[OPT_RM].value)
            {
                UntrappedIO_OUT_Handler = &QEMM_UntrappedIO_Write;
                UntrappedIO_IN_Handler = &QEMM_UntrappedIO_Read;
            }
            else
            {
                UntrappedIO_OUT_Handler = &HDPMIPT_UntrappedIO_Write;
                UntrappedIO_IN_Handler = &HDPMIPT_UntrappedIO_Read;
            }
            MAIN_Options[OPT_PM].value = opt[OPT_PM].value;
            MAIN_Options[OPT_RM].value = opt[OPT_RM].value;
            MAIN_Options[OPT_OPL].value = opt[OPT_OPL].value;

            free(opt);
        }
        return;
    }
}
