//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: common AC97 definitions (for SB Live, ES1371, ICH)

#ifdef SBEMU
#include <mpxplay.h>
#endif
#include "au_cards.h"
#include "ac97_def.h"

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_MASTER_VOL_STEREO,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // left
  {AC97_MASTER_VOL_STEREO,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // right
  {AC97_SURROUND_MASTER,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_SURROUND_MASTER,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_pcm_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_PCMOUT_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_PCMOUT_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_headphone_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_HEADPHONE,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_HEADPHONE_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_HEADPHONE_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_micin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MICIN,AU_MIXCHANFUNC_VOLUME),1,
 {{AC97_MIC_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_linein_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_LINEIN_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_LINEIN_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_cdin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_CD_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_CD_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ac97chan_auxin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_AUXIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AC97_AUX_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_AUX_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

aucards_allmixerchan_s mpxplay_aucards_ac97chan_mixerset[]={
 &mpxplay_aucards_ac97chan_master_vol,
 &mpxplay_aucards_ac97chan_pcm_vol,
 &mpxplay_aucards_ac97chan_headphone_vol,
 &mpxplay_aucards_ac97chan_micin_vol,
 &mpxplay_aucards_ac97chan_linein_vol,
 &mpxplay_aucards_ac97chan_cdin_vol,
 &mpxplay_aucards_ac97chan_auxin_vol,
 NULL
};
