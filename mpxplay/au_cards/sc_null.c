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

#include "au_cards.h"

static int NUL_init(struct mpxplay_audioout_info_s *aui)
{
 return 1;
}

static int NUL_detect(struct mpxplay_audioout_info_s *aui)
{
 return 1;
}

one_sndcard_info NON_sndcard_info={  // OUTMODE_TYPE_NONE
 "NUL",
 //SNDCARD_SELECT_ONLY|
 SNDCARD_IGNORE_STARTUP,
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
