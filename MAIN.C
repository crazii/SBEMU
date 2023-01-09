#include <stdio.h>
#include <stdlib.h>
#include <MPXPLAY.H>

int main()
{
    struct mpxplay_audioout_info_s aui = {0};
    //aui.card_controlbits = AUINFOS_CARDCNTRLBIT_TESTCARD;
    AU_init(&aui);
    return 0;
}