#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MPXPLAY.H>

extern void TestSound();
mpxplay_audioout_info_s aui = {0};

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
        printf("SBEMU: Sound Blaster emulation for DOS. Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option)
        {
            printf(" %-8s: %s. default:%x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
            ++i;
        }
    }
    else
    {
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

    //test
    if(MAIN_Options[OPT_TEST].value)
    {
        AU_init(&aui);
        AU_ini_interrupts(&aui);
        AU_setmixer_init(&aui);
        AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
        TestSound();
        AU_del_interrupts(&aui);
        return 0;
    }


    return 0;
}