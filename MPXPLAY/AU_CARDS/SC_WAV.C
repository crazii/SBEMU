//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                 http://mpxplay.sourceforge.net                         *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: WAV-file and bitstream output

#include "mpxplay.h"
#include "control\control.h"
#include <fcntl.h>

extern unsigned int outmode;

static int outfile_hand;
static unsigned long outfile_count,outfile_bytecount;
static char outfile_path[MAX_PATHNAMELEN],prev_path[MAX_PATHNAMELEN];

static void wavout_open(struct mpxplay_audioout_info_s *aui)
{
 struct playlist_entry_info *pei=aui->pei;
 unsigned int len;
 char fullname[MAX_PATHNAMELEN],strtmp[MAX_PATHNAMELEN];

 if(!pei)
  return;

 if(!outfile_path[0]){
  if(freeopts[OPT_OUTPUTFILE]){
   mpxplay_playlist_startfile_fullpath(outfile_path,freeopts[OPT_OUTPUTFILE]);
   if(!pds_dir_exists(outfile_path)){ // argument is not a dir
    pds_strcpy(fullname,outfile_path);
    outfile_path[0]=0;
    goto outfile_open;
   }
   len=pds_strlen(outfile_path);
  }else
   len=pds_strcpy(outfile_path,mpxplay_playlist_startdir());

  if(outfile_path[len-1]!=PDS_DIRECTORY_SEPARATOR_CHAR)
   pds_strcpy(&outfile_path[len],PDS_DIRECTORY_SEPARATOR_STR);
 }

 pds_strcpy(fullname,outfile_path);

 if((aui->card_controlbits&AUINFOS_CARDCNTRLBIT_AUTOTAGLFN) && (pei->id3info[I3I_ARTIST] || pei->id3info[I3I_TITLE])){
  if(pds_stricmp(outfile_path,prev_path)!=0){ // reset file counter at new subdir
   pds_strcpy(prev_path,outfile_path);
   outfile_count=1;
  }
  sprintf(strtmp,"%2.2d. %.60s%s%.60s",outfile_count++, // create filename from id3info
    ((pei->id3info[I3I_ARTIST])? pei->id3info[I3I_ARTIST]:""),
    ((pei->id3info[I3I_ARTIST] && pei->id3info[I3I_TITLE])? " - ":""),
    ((pei->id3info[I3I_TITLE])? pei->id3info[I3I_TITLE]:""));

  pds_filename_conv_forbidden_chars(strtmp);
  pds_strcutspc(strtmp);
#ifndef MPXPLAY_UTF8
  mpxplay_playlist_textconv_by_texttypes(
   MPXPLAY_TEXTCONV_TYPES_PUT(MPXPLAY_TEXTCONV_TYPE_MPXPLAY,MPXPLAY_TEXTCONV_TYPE_MPXNATIVE),
   strtmp,-1,strtmp,sizeof(strtmp));
#endif
 }else
  pds_getfilename_noext_from_fullname(strtmp,pei->filename);

 if((aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT) && aui->card_wave_name){
  pds_strcat(strtmp,".");
  pds_strcat(strtmp,aui->card_wave_name);
 }else
  pds_strcat(strtmp,".WAV");

 pds_strcat(fullname,strtmp);

outfile_open:
 pds_fullpath(fullname,fullname);
 pds_fullpath(strtmp,pei->filename);

 if(pds_stricmp(fullname,strtmp)!=0) // input and output filename must be different
  outfile_hand=pds_open_create(fullname,O_RDWR|O_BINARY);
 else
  outfile_hand=0;
 if(!outfile_hand && (outmode&OUTMODE_TYPE_FILE))
  mpxplay_close_program(MPXERROR_CANTWRITEFILE);
}

static void wavout_write_header(struct mpxplay_audioout_info_s *aui)
{
 struct RIFF{
  unsigned long riffID;
  unsigned long rLen;
 }riff;
 struct WAVE{
  unsigned long waveID;
 }wave;
 struct FORMAT{
  unsigned long fmtID;
  unsigned long fLen;
  unsigned short wTag;
  unsigned short wChannel;
  unsigned long nSample;
  unsigned long nByte;
  unsigned short align;
  unsigned short sample;
 }fmt;
 struct DATA{
  unsigned long dataID;
  unsigned long dLen;
 }data;

 unsigned long filelen=pds_filelength(outfile_hand);
 unsigned int bytespersample;

 if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT){
  bytespersample=4;
  aui->bits_card=1; // output scale (-1.0 - +1.0)
 }else
  bytespersample=(aui->bits_card+7)/8;

 outfile_bytecount=filelen;

 if(filelen<44)
  filelen=44;

 riff.riffID=PDS_GET4C_LE32('R','I','F','F');
 riff.rLen  =filelen-sizeof(struct RIFF);

 wave.waveID=PDS_GET4C_LE32('W','A','V','E');

 fmt.fmtID   =PDS_GET4C_LE32('f','m','t',' ');
 fmt.fLen    =sizeof(struct FORMAT)-8;
 fmt.wTag    =aui->card_wave_id;
 fmt.wChannel=aui->chan_card;
 fmt.nSample =aui->freq_card;
 fmt.nByte   =aui->freq_card*aui->chan_card*bytespersample;
 fmt.align   =aui->chan_card*bytespersample;
 if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT)
  fmt.sample =32;
 else
  fmt.sample =aui->bits_card;

 data.dataID=PDS_GET4C_LE32('d','a','t','a');
 data.dLen  =riff.rLen-(sizeof(struct WAVE)+sizeof(struct FORMAT)+sizeof(struct DATA));

 pds_dos_write(outfile_hand,(void *)&riff,sizeof(struct RIFF));
 pds_dos_write(outfile_hand,(void *)&wave,sizeof(struct WAVE));
 pds_dos_write(outfile_hand,(void *)&fmt ,sizeof(struct FORMAT));
 pds_dos_write(outfile_hand,(void *)&data,sizeof(struct DATA));
}

//------------------------------------------------------------------------
#define ADPCM_MAX_CHANNELS 8

extern mpxp_uint8_t adpcm_channel_matrix[ADPCM_MAX_CHANNELS-2][ADPCM_MAX_CHANNELS];

static int WAV_init(struct mpxplay_audioout_info_s *aui)
{
 aui->card_port=aui->card_isa_dma=aui->card_irq=aui->card_isa_hidma=aui->card_type=0;
 return 1;
}

static void WAV_card_info(struct mpxplay_audioout_info_s *aui)
{
 pds_textdisplay_printf("WAV : pcm wave file or bitstream output (disk writer)");
}

static void WAV_stop(struct mpxplay_audioout_info_s *aui)
{
 if(outfile_hand && !(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT)){
  long filepos=pds_tell(outfile_hand);
  pds_lseek(outfile_hand,0,SEEK_SET);
  wavout_write_header(aui);
  pds_lseek(outfile_hand,filepos,SEEK_SET);
 }
}

static void WAV_close(struct mpxplay_audioout_info_s *aui)
{
 if(outfile_hand){
  pds_close(outfile_hand);
  outfile_hand=0;
 }
}

static void WAV_setrate(struct mpxplay_audioout_info_s *aui) // new file
{
 WAV_close(aui);
 aui->card_channelmap=NULL;
 if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
  funcbit_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_BITSTREAMOUT);
 wavout_open(aui);
 if(outfile_hand && !(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT)){
  wavout_write_header(aui);
  if((aui->chan_card>2) && (aui->chan_card<=ADPCM_MAX_CHANNELS))
   aui->card_channelmap=&adpcm_channel_matrix[aui->chan_card-3][0];
 }
}

static void WAV_writedata(struct mpxplay_audioout_info_s *aui,char *pcm_sample,unsigned long outbytes)
{
 if(outfile_hand){
  if(outfile_bytecount<(0x7fffffffUL-outbytes)){
   if(pds_dos_write(outfile_hand,pcm_sample,outbytes)!=outbytes)
    mpxplay_close_program(MPXERROR_CANTWRITEFILE);
   outfile_bytecount+=outbytes;
  }else{
   WAV_stop(aui);
   WAV_close(aui);
  }
 }
}

one_sndcard_info WAV_sndcard_info={
 "WAV",
 SNDCARD_FLAGS_DISKWRITER|SNDCARD_IGNORE_STARTUP,

 NULL,            // card_config
 &WAV_init,       // card_init
 &WAV_init,       // card_detect
 &WAV_card_info,  // card_info
 NULL,            // card_start
 &WAV_stop,       // card_stop
 &WAV_close,      // card_close
 &WAV_setrate,    // card_setrate

 &WAV_writedata,  // cardbuf_writedata
 NULL,            // cardbuf_pos
 NULL,            // cardbuf_clear
 NULL,            // cardbuf_int_monitor
 NULL,            // irq_routine

 NULL,            // card_writemixer
 NULL,            // card_readmixer
 NULL             // card_mixerchans
};
