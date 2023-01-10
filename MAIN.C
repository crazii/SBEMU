#include <stdio.h>
#include <stdlib.h>
#include <MPXPLAY.H>

extern void TestSound();
mpxplay_audioout_info_s aui = {0};

int main()
{
    //aui.card_controlbits = AUINFOS_CARDCNTRLBIT_TESTCARD;
    AU_init(&aui);
    AU_ini_interrupts(&aui);
    AU_setmixer_init(&aui);
    //AU_setmixer_all(&aui);
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);

    TestSound();

    AU_del_interrupts(&aui);
    return 0;
}