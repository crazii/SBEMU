//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: NULL device low level routines

#include "mpxplay.h"

static unsigned long beginhundred;
extern unsigned int displaymode;

static int NUL_init(struct mpxplay_audioout_info_s *aui)
{
 aui->card_port=aui->card_isa_dma=aui->card_irq=aui->card_isa_hidma=aui->card_type=0;
 return 1;
}

static int NUL_detect(struct mpxplay_audioout_info_s *aui)
{
 aui->card_port=aui->card_isa_dma=aui->card_irq=aui->card_isa_hidma=aui->card_type=0;
 return 1;
}

static void NUL_card_info(struct mpxplay_audioout_info_s *aui)
{
 pds_textdisplay_printf("NUL : NULL device output with speed test (no sound)");
}

static void speedtestresults(struct mpxplay_audioout_info_s *aui)
{
 unsigned long runtimeh,framenum;
 float index;
 char sout[200];

 //if(beginhundred){
 if(displaymode){
  runtimeh=pds_gettimeh()-beginhundred;
  framenum=aui->mvp->fr_primary->frameNum;
  if(runtimeh && framenum)
   index=(float)(framenum)/(float)(runtimeh)*6000.0f/22.96875f;
  else
   index=100.0f;

  sprintf(sout,"Runtime: %1.1f sec, decoded frames: %lu, speed: %.1fx (%.1f%%) (play:%.1f%%) ",
     (float)runtimeh/100.0f,framenum,index/100.0f,
     index,10000.0f/index);
  pds_textdisplay_printf(sout);
 }
}

static void NUL_close(struct mpxplay_audioout_info_s *aui)
{
 speedtestresults(aui);
}

static void NUL_setrate(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
  funcbit_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_BITSTREAMOUT);
 beginhundred=pds_gettimeh();
}

static void NUL_writedata(struct mpxplay_audioout_info_s *aui,char *pcm_sample,unsigned long outbytenum)
{

}

one_sndcard_info NON_sndcard_info={  // OUTMODE_TYPE_NONE
 "NON",
 SNDCARD_SELECT_ONLY|SNDCARD_IGNORE_STARTUP,
 NULL,             // card_config
 &NUL_init,        // card_init
 &NUL_detect,      // card_detect
 NULL,             // card_info
 NULL,             // card_start
 NULL,             // card_stop
 NULL,             // card_close
 NULL,             // card_setrate
 NULL,             // cardbuf_writedata
 NULL,             // cardbuf_pos
 NULL,             // cardbuf_clear
 NULL,             // cardbuf_int_monitor
 NULL,             // irq_routine
 NULL,             // card_writemixer
 NULL,             // card_readmixer
 NULL              // card_mixerchans
};

one_sndcard_info TST_sndcard_info={  // OUTMODE_TYPE_TEST
 "TST",
 SNDCARD_SELECT_ONLY|SNDCARD_SETRATE|SNDCARD_IGNORE_STARTUP,
 NULL,             // card_config
 &NUL_init,        // card_init
 &NUL_detect,      // card_detect
 &NUL_card_info,   // card_info
 NULL,             // card_start
 NULL,             // card_stop
 &NUL_close,       // card_close
 &NUL_setrate,     // card_setrate
 &NUL_writedata,   // cardbuf_writedata
 NULL,             // cardbuf_pos
 NULL,             // cardbuf_clear
 NULL,             // cardbuf_int_monitor
 NULL,             // irq_routine
 NULL,             // card_writemixer
 NULL,             // card_readmixer
 NULL              // card_mixerchans
};

one_sndcard_info NUL_sndcard_info={  // OUTMODE_TYPE_NULL
 "NUL",
 SNDCARD_SELECT_ONLY|SNDCARD_INT08_ALLOWED,
 NULL,             // card_config
 &NUL_init,        // card_init
 &NUL_detect,      // card_detect
 &NUL_card_info,   // card_info
 NULL,             // card_start
 NULL,             // card_stop
 &NUL_close,       // card_close
 &NUL_setrate,     // card_setrate
 &NUL_writedata,   // cardbuf_writedata
 NULL,             // cardbuf_pos
 NULL,             // cardbuf_clear
 NULL,             // cardbuf_int_monitor
 NULL,             // irq_routine
 NULL,             // card_writemixer
 NULL,             // card_readmixer
 NULL              // card_mixerchans
};
