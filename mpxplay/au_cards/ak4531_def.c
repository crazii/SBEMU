//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//**************************************************************************
//function: AK4531 mixer definitions (for ES1370-based SB PCI64/128 cards)
//
//Unlike ac97_def.c, each AK4531 channel uses TWO separate 8-bit registers
//for left/right (not one register with a left/right bitfield), and the
//codec cannot be read back from hardware - card_readmixer() (see
//SC_E1370.C) returns a value cached in software instead.

#include "au_cards.h"
#include "ak4531_def.h"

static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,
 {{AK4531_LMASTER,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AK4531_RMASTER,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

/* the "PCM"/voice channel is the DAC output level - this is the one that
 * actually controls how loud the digital audio played by SBEMU is.
 */
static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_pcm_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),2,
 {{AK4531_LVOICE,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AK4531_RVOICE,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_micin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MICIN,AU_MIXCHANFUNC_VOLUME),1,
 {{AK4531_MIC,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_linein_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AK4531_LLINE,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AK4531_RLINE,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_cdin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AK4531_LCD,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AK4531_RCD,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

static aucards_onemixerchan_s mpxplay_aucards_ak4531chan_auxin_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_AUXIN,AU_MIXCHANFUNC_VOLUME),2,
 {{AK4531_LAUXA,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AK4531_RAUXA,0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}}
};

/* note: AK4531 has no dedicated headphone or S/PDIF output, so
 * AU_MIXCHAN_HEADPHONE / AU_MIXCHAN_SPDIFOUT are intentionally not listed
 * here - a mixchan absent from this table is silently skipped by the
 * generic mixer-setting code, so this is safe.
 */
aucards_allmixerchan_s mpxplay_aucards_ak4531chan_mixerset[]={
 &mpxplay_aucards_ak4531chan_master_vol,
 &mpxplay_aucards_ak4531chan_pcm_vol,
 &mpxplay_aucards_ak4531chan_micin_vol,
 &mpxplay_aucards_ak4531chan_linein_vol,
 &mpxplay_aucards_ak4531chan_cdin_vol,
 &mpxplay_aucards_ak4531chan_auxin_vol,
 NULL
};
