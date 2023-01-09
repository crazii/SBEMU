//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2015 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: sofware-mixer routines - main

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_API NULL // stdout
#define MPXPLAY_DEBUG_CF NULL // stdout
#define MPXPLAY_DEBUG_OUTPUT stdout
#define MPXPLAY_DEBUG_CHKVAR stdout
#define MPXPLAY_DEBUG_INIT NULL // stdout
//#define MPXPLAY_USE_DEBUGMSG 1

#include "mpxplay.h"
#include "mix_func.h"
#include "display\display.h"
#include <float.h>

extern unsigned int displaymode,crossfadepart;
extern unsigned int SOUNDLIMITvol;
#if defined(MPXPLAY_LINK_INFILE_FF_MPEG) && defined(MPXPLAY_GUI_QT)
extern unsigned int mpxplay_config_video_audiovisualization_type;
#endif

extern struct one_mixerfunc_info MIXER_FUNCINFO_crossfader;
extern struct one_mixerfunc_info MIXER_FUNCINFO_swapchan;
extern struct one_mixerfunc_info MIXER_FUNCINFO_mute;
extern struct one_mixerfunc_info MIXER_FUNCINFO_balance;
extern struct one_mixerfunc_info MIXER_FUNCINFO_surround;
extern struct one_mixerfunc_info MIXER_FUNCINFO_tone_bass;
extern struct one_mixerfunc_info MIXER_FUNCINFO_tone_treble;
extern struct one_mixerfunc_info MIXER_FUNCINFO_tone_loudness;
extern struct one_mixerfunc_info MIXER_FUNCINFO_seekspeed;
extern struct one_mixerfunc_info MIXER_FUNCINFO_speed;
extern struct one_mixerfunc_info MIXER_FUNCINFO_autovolume;
extern struct one_mixerfunc_info MIXER_FUNCINFO_limiter;
extern struct one_mixerfunc_info MIXER_FUNCINFO_volume;

static struct one_mixerfunc_info *all_mixerfunc_info[MIXER_MAX_FUNCTIONS+1]={
 &MIXER_FUNCINFO_speed, // recommended to keep it on 1st place (to limit the buffer of other mixer functions, especially tone)
 &MIXER_FUNCINFO_tone_bass,
 &MIXER_FUNCINFO_crossfader, // last preprocess
 &MIXER_FUNCINFO_swapchan,
 &MIXER_FUNCINFO_surround,
 &MIXER_FUNCINFO_tone_loudness,
 &MIXER_FUNCINFO_tone_treble,// parallel dep. : loudness
 &MIXER_FUNCINFO_seekspeed,
 &MIXER_FUNCINFO_autovolume,
 &MIXER_FUNCINFO_mute,
 &MIXER_FUNCINFO_balance,
 &MIXER_FUNCINFO_limiter,    // last-1 (parallel dep.:surround,tone,(volume))
 &MIXER_FUNCINFO_volume,     // must be the last (parallel dep.:mute,balance,limiter)
 NULL
}; // !!! MIXER_BUILTIN_ in au_mixer.h

static void aumixer_reinit_independent_preprocesses(struct mpxp_aumixer_passinfo_s *mpi);
static void aumixer_checkallfunc_dependencies_internal_allflags(struct mpxp_aumixer_passinfo_s *mpi);

unsigned long MIXER_controlbits;
static unsigned int MIXER_nb_mixerfuncs=MIXER_BUILTIN_FUNCTIONS;

static int aumixer_pcmoutbuf_realloc(struct mpxplay_audioout_info_s *aui,
  struct mpxpframe_s *frp,long blocksamples,float speed_expansion)
{
 struct mpxplay_audio_decoder_info_s *adi = frp->infile_infos->audio_decoder_infos;
 long newpcmoutbufsize;

 funcbit_smp_int32_put(frp->pcmout_blocksize,(blocksamples*adi->outchannels));

 if(blocksamples < PCM_OUTSAMPLES)
  blocksamples = PCM_OUTSAMPLES;
 blocksamples += 1024; // tone control max (?) expansion (192000/200)
 blocksamples += 128;  // extra buffer overflow protection
 newpcmoutbufsize = (long)(speed_expansion*(float)(blocksamples*max(adi->outchannels,aui->chan_card))
                   *(float)(max(aui->freq_card,adi->freq))/(float)(adi->freq));
 newpcmoutbufsize *= sizeof(MPXPLAY_PCMOUT_FLOAT_T);
 if(frp->pcmout_bufsize < newpcmoutbufsize){
  mpxp_uint8_t *newbuf = (mpxp_uint8_t *)pds_malloc(newpcmoutbufsize);
  if(!newbuf)
   return 0;
  if(frp->pcmout_buffer){
   if(frp->pcmout_storedsamples)
    pds_memcpy(newbuf,frp->pcmout_buffer,(frp->pcmout_storedsamples*adi->bytespersample));
   pds_free(frp->pcmout_buffer);
  }
  funcbit_smp_pointer_put(frp->pcmout_buffer,newbuf);
  funcbit_smp_int32_put(frp->pcmout_bufsize,newpcmoutbufsize);
 }
 return 1;
}

static struct mpxp_aumixer_passinfo_s *aumixer_frpmpi_alloc(struct mpxpframe_s *frp)
{
 struct mpxp_aumixer_passinfo_s *mpi = frp->mpi;
 if(!mpi){
  mpi = (mpxp_aumixer_passinfo_s *)pds_calloc(1,sizeof(mpxp_aumixer_passinfo_s));
  if(!mpi)
   return NULL;
  funcbit_smp_pointer_put(frp->mpi,mpi);
  pds_threads_mutex_new(&mpi->mutex_aumixer);
 }
 return mpi;
}

//-------------------------------------------------------------------------
//configure mixer input to the new audio file (decoding)
static unsigned int MIXER_mixin_configure(struct mpxplay_audioout_info_s *aui, struct mpxpframe_s *frp)
{
 struct mpxplay_audio_decoder_info_s *adi = frp->infile_infos->audio_decoder_infos;
 struct mpxp_aumixer_passinfo_s *mpi = NULL;
 struct one_mixerfunc_info *infop_speed;
 unsigned int retcode = 0;
 float speed_expansion;

 if(adi->freq < PCM_MIN_FREQ)
  goto err_out_mixincfg;

 infop_speed = MIXER_getfunction("MIX_SPEED");
 speed_expansion = 0.0;
 if(infop_speed)
  speed_expansion = (float)infop_speed->var_center/(float)infop_speed->var_min; // 2 by default
 if(speed_expansion < 2.0)
  speed_expansion = 2.0;

 aumixer_pcmoutbuf_realloc(aui, frp,mpxplay_infile_get_samplenum_per_frame(adi->freq), speed_expansion);

 mpi = aumixer_frpmpi_alloc(frp);
 if(!mpi)
  goto err_out_mixincfg;

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 funcbit_smp_pointer_put(mpi->control_cb, mpxplay_aumixer_control_cb);
 funcbit_smp_pointer_put(mpi->ccb_data  , mpi);
 funcbit_smp_pointer_put(mpi->pcm_sample, (PCM_CV_TYPE_S *)frp->pcmout_buffer);
 funcbit_smp_int32_put(mpi->samplenum, 0);
 funcbit_smp_int32_put(mpi->bytespersample_mixer, sizeof(PCM_CV_TYPE_F));
 funcbit_smp_int32_put(mpi->freq_card, aui->freq_card);
 funcbit_smp_int32_put(mpi->chan_card, aui->chan_card);
 funcbit_smp_int32_put(mpi->bits_card, aui->bits_card);
 funcbit_smp_int32_put(mpi->freq_song, adi->freq);
 funcbit_smp_int32_put(mpi->chan_song, adi->outchannels);
 funcbit_smp_int32_put(mpi->bits_song, adi->bits);
 funcbit_smp_int32_put(mpi->pcmout_savedsamples , 0);
 funcbit_smp_pointer_put(mpi->aui, aui);
 funcbit_smp_pointer_put(mpi->mvp, aui->mvp);
 funcbit_smp_pointer_put(mpi->frp, frp);

 funcbit_smp_int32_put(frp->pcmdec_storedsamples,0); // FIXME: find better place (from mpxplay_decoders_clearbuf)
 funcbit_smp_int32_put(frp->pcmdec_leftsamples,0);
 funcbit_smp_int32_put(frp->pcmout_storedsamples,0);

 aumixer_reinit_independent_preprocesses(mpi);
 aumixer_checkallfunc_dependencies_internal_allflags(mpi);

 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);

 retcode = 1;

err_out_mixincfg:
 return retcode;
}

unsigned int MIXER_configure(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp)
{
 struct mpxp_aumixer_passinfo_s *mpi, *mpi1;
 struct mpxpframe_s *frp1;
 unsigned int retcode = 0;

 if(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT){
  frp->pcmout_blocksize = 1;
  return 1;
 }

 if(!MIXER_mixin_configure(aui, frp))
  goto err_out_mixcfg;

 if((frp->pcmout_bufsize/sizeof(MPXPLAY_PCMOUT_FLOAT_T))>(mmi->MIXER_int16pcm_bufsize/sizeof(PCM_CV_TYPE_S))){
  if(mmi->MIXER_int16pcm_buffer)
   pds_free(mmi->MIXER_int16pcm_buffer);
  funcbit_smp_int32_put(mmi->MIXER_int16pcm_bufsize,(frp->pcmout_bufsize/sizeof(MPXPLAY_PCMOUT_FLOAT_T)*sizeof(PCM_CV_TYPE_S)));
  funcbit_smp_pointer_put(mmi->MIXER_int16pcm_buffer,((PCM_CV_TYPE_S *)pds_malloc(mmi->MIXER_int16pcm_bufsize)));
  if(!mmi->MIXER_int16pcm_buffer){
   funcbit_smp_int32_put(mmi->MIXER_int16pcm_bufsize,0);
   goto err_out_mixcfg;
  }
#ifdef MPXPLAY_GUI_CONSOLE
  funcbit_smp_pointer_put(aui->mvp->vds->pcm_data,mmi->MIXER_int16pcm_buffer);
  funcbit_smp_int32_put(aui->mvp->vds->pcm_freq,aui->freq_card);
#endif
 }

 mpi = frp->mpi;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 funcbit_smp_int32_put(mpi->mixer_function_flags, 0);
 funcbit_smp_pointer_put(mpi->mmi, mmi);

 frp1 = frp->fro;
 mpi1 = frp1->mpi;
 if(!mpi1) // happens at -bn, -bs, non-crossfade skip
  goto skip_mpi1_copy;

 PDS_THREADS_MUTEX_LOCK(&mpi1->mutex_aumixer, -1);

 funcbit_smp_int32_put(mpi->mixer_function_flags, mpi1->mixer_function_flags);
 pds_smp_memcpy((char *)&mpi->mixerfuncs_private_datas[MIXER_BUILTIN_CFPREPROCESSNUM],
                (char *)&mpi1->mixerfuncs_private_datas[MIXER_BUILTIN_CFPREPROCESSNUM],
                ((MIXER_nb_mixerfuncs - MIXER_BUILTIN_CFPREPROCESSNUM) * sizeof(struct mpxp_aumixer_passinfo_s *)) );

 PDS_THREADS_MUTEX_UNLOCK(&mpi1->mutex_aumixer);

 aumixer_reinit_independent_preprocesses(mpi);

skip_mpi1_copy:

 aumixer_checkallfunc_dependencies_internal_allflags(mpi);

 retcode = 1;

 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);

 mpxplay_debugf(MPXPLAY_DEBUG_INIT, "MIXER_configure done");

err_out_mixcfg:
 return retcode;
}

// fill pcmout buffer (with pcmout_blocksize)
unsigned int AUMIXER_collect_pcmout_data(struct mpxpframe_s *frp,PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum)
{
 struct mpxplay_audio_decoder_info_s *adi = frp->infile_infos->audio_decoder_infos;
 struct mpxp_aumixer_passinfo_s *mpi = frp->mpi;
 unsigned long leftspace;

 if(adi->infobits&ADI_CNTRLBIT_BITSTREAMOUT){
  frp->pcmout_storedsamples = samplenum;
  return samplenum;
 }

#ifdef MPXPLAY_LINK_ORIGINAL_FFMPEG
 if(!mpi || (adi->freq != mpi->freq_song) || (adi->outchannels != mpi->chan_song)) // TODO: other checks?
  MIXER_mixin_configure(frp->mvp->aui, frp);
#endif

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 mpi->pcm_sample = (short *)frp->pcmout_buffer;
 if(!samplenum || !pcm_sample || !adi->bytespersample || ((frp->pcmout_storedsamples + mpi->pcmout_savedsamples)>=frp->pcmout_blocksize)) {
  samplenum = 0;
  goto err_out_col;
 }

 leftspace=frp->pcmout_blocksize - frp->pcmout_storedsamples - mpi->pcmout_savedsamples;
 if(samplenum > leftspace)
  samplenum = leftspace;
 samplenum -= samplenum % adi->outchannels;

 pds_memcpy(frp->pcmout_buffer + (mpi->pcmout_savedsamples*sizeof(PCM_CV_TYPE_F)) + (frp->pcmout_storedsamples*adi->bytespersample), pcm_sample, (samplenum*adi->bytespersample));

 frp->pcmout_storedsamples += samplenum;

err_out_col:
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
 return samplenum; // returns used_samples
}

// audio-data to mixer-data conversion
unsigned int MIXER_conversion(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp)
{
 struct mpxplay_audio_decoder_info_s *adi;
 struct mpxp_aumixer_passinfo_s *mpi;

 if(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT){
  aui->samplenum = frp->pcmout_storedsamples;
  frp->pcmout_storedsamples = 0;
  return aui->samplenum;
 }

 mpi = frp->mpi;

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 adi = frp->infile_infos->audio_decoder_infos;
 mpi->frp = frp; // !!! required

 mpi->samplenum = frp->pcmout_storedsamples;
 if(!mpi->samplenum)// && !(frp->filetype & HFT_FILE_EXT))
  goto err_out_mixconv;

 mpi->pcm_sample = (short *)(frp->pcmout_buffer + (mpi->pcmout_savedsamples * sizeof(PCM_CV_TYPE_F)));

 if(/*!(frp->filetype & HFT_FILE_EXT) &&*/(mpi->mixer_function_flags || ((adi->outchannels!=aui->chan_card) && (adi->outchannels>2) && adi->chanmatrix && (aui->chan_card<=2)))){
  funcbit_enable(mmi->mixer_infobits,(AUINFOS_MIXERINFOBIT_ACONV|AUINFOS_MIXERINFOBIT_FCONV));
  mpi->bytespersample_mixer=sizeof(PCM_CV_TYPE_F);
  if(adi->infobits&ADI_FLAG_FLOATOUT) // decoder makes a 32-bit float output (aac,ac3,dts,mp2,mp3,mpc,ogg,wav-float)
   cv_scale_float(mpi->pcm_sample,mpi->samplenum,adi->bits,MIXER_SCALE_BITS,0);
  else                           // pcm (8,16,24,32 bit) output (ape,cdw,flac,wav)
   cv_n_bits_to_float(mpi->pcm_sample,mpi->samplenum,adi->bytespersample,MIXER_SCALE_BITS);
 }else{
  funcbit_disable(mmi->mixer_infobits,(AUINFOS_MIXERINFOBIT_ACONV|AUINFOS_MIXERINFOBIT_FCONV));
  mpi->bytespersample_mixer=adi->bytespersample;
 }

 if((adi->infobits&ADI_FLAG_OWN_SPECTANAL) || (frp->infile_infos->audio_decoder_funcs && frp->infile_infos->audio_decoder_funcs->get_analiser_bands))
  funcbit_enable(mmi->mixer_infobits,AUINFOS_MIXERINFOBIT_DECODERANALISER);
 else
  funcbit_disable(mmi->mixer_infobits,AUINFOS_MIXERINFOBIT_DECODERANALISER);

 if(adi->chanmatrix && (aui->chan_card<=2))
  mpi->samplenum=cv_channels_downmix(mpi->pcm_sample,mpi->samplenum,adi->outchannels,adi->chanmatrix,aui->chan_card,aui->card_channelmap,mpi->bytespersample_mixer);
 else if((adi->outchannels!=aui->chan_card) || adi->chanmatrix){
  if(adi->outchannels==1)
   mpi->samplenum=cv_channels_1_to_n(mpi->pcm_sample,mpi->samplenum,aui->chan_card,mpi->bytespersample_mixer);
  else
   mpi->samplenum=cv_channels_remap(mpi->pcm_sample,mpi->samplenum,adi->outchannels,adi->chanmatrix,aui->chan_card,aui->card_channelmap,mpi->bytespersample_mixer);
 }

 if(mmi->mixer_infobits & (AUINFOS_MIXERINFOBIT_CROSSFADE | AUINFOS_MIXERINFOBIT_ACONV)){
  unsigned long i=0, funcflags=mpi->mixer_function_flags;
#ifdef MPXPLAY_USE_DEBUGF
  unsigned long old_samplenum=mpi->samplenum;
  unsigned int n=(frp-frp->mpi->mvp->fr_base);
  //if(n==1)
#endif
  do{
   if(funcflags&1){
    struct one_mixerfunc_info *infop=all_mixerfunc_info[i];
    if(infop){
     mpi->private_data=mpi->mixerfuncs_private_datas[i];
     infop->process_routine(mpi);
     if(!mpi->samplenum)// && !(frp->filetype & HFT_FILE_EXT))
      break;
    }
   }
   i++;
   funcflags>>=1;
  }while(funcflags && (i<MIXER_BUILTIN_CFPREPROCESSNUM));
#ifdef MPXPLAY_USE_DEBUGF
  if(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_CROSSFADE){
   //mpxplay_debugf(MPXPLAY_DEBUG_CF,"CV%d os:%4d ns:%4d sv:%d ps:%8.8X", n, old_samplenum,mpi->samplenum,mpi->pcmout_savedsamples,mpi->pcm_sample);
  }
 } else if(mpi->pcmout_savedsamples) {
  mpxplay_debugf(MPXPLAY_DEBUG_CF,"CVE ns:%4d sv:%d ps:%8.8X", (mpi->samplenum + mpi->pcmout_savedsamples), mpi->pcmout_savedsamples,mpi->pcm_sample);
#endif
 }

err_out_mixconv:
#ifdef MPXPLAY_USE_DEBUGF
 if(!(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_CROSSFADE) && mpi->pcmout_savedsamples) {
  mpxplay_debugf(MPXPLAY_DEBUG_CF,"CVX ss:%d ns:%4d sv:%d bs:%d", frp->pcmout_storedsamples, (mpi->samplenum + mpi->pcmout_savedsamples), mpi->pcmout_savedsamples, frp->pcmout_blocksize);
 }
#endif
 mpi->samplenum += mpi->pcmout_savedsamples;
 mpi->pcmout_savedsamples = 0;
 frp->pcmout_storedsamples = 0;
 mpi->pcm_sample = (short *)frp->pcmout_buffer;
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
 return mpi->samplenum;
}

// do mixer functions
void MIXER_main(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp)
{
 struct mpxplay_audio_decoder_info_s *adi;
 struct mpxp_aumixer_passinfo_s *mpi;
 unsigned int do_pcm16;

 if(aui->card_infobits&AUINFOS_CARDINFOBIT_BITSTREAMOUT){
  aui->pcm_sample = (PCM_CV_TYPE_S *)frp->pcmdec_buffer;
  return;
 }

 mpi=frp->mpi;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 if(crossfadepart != CROSS_FADE)
  funcbit_disable(mmi->mixer_infobits, AUINFOS_MIXERINFOBIT_CROSSFADE);

 mpi->pcm_sample = (PCM_CV_TYPE_S *)frp->pcmout_buffer;
 aui->pcm_sample = (PCM_CV_TYPE_S *)frp->pcmout_buffer;
 if(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_CROSSFADE) {
  struct mpxp_aumixer_passinfo_s *mpi1 = frp->fro->mpi;
  aui->samplenum = min(mpi->samplenum, mpi1->samplenum);
  mpi->pcmout_savedsamples = mpi->samplenum - aui->samplenum;
  mpi1->pcmout_savedsamples = mpi1->samplenum - aui->samplenum;
  mpxplay_debugf(MPXPLAY_DEBUG_CF,"MM  sn:%5d s1:%4d s2:%4d sv1:%d sv2:%4d", aui->samplenum, mpi->samplenum,mpi1->samplenum, mpi->pcmout_savedsamples, mpi1->pcmout_savedsamples);
  mpi->samplenum = mpi1->samplenum = aui->samplenum;
 } else {
#ifdef MPXPLAY_USE_DEBUGF
  if((mpi->samplenum != 1152) && (mpi->samplenum != 2304)) {
   //mpxplay_debugf(MPXPLAY_DEBUG_CF,"LAST s1:%4d sv1:%d ss:%d", mpi->samplenum, mpi->pcmout_savedsamples, frp->pcmout_storedsamples);
  }
#endif
  aui->samplenum = mpi->samplenum;
 }

#ifdef MPXPLAY_GUI_CONSOLE
  funcbit_smp_int32_put(aui->mvp->vds->pcm_samplenum, mpi->samplenum);
#endif

 if(!mpi->samplenum)
  goto err_out_mixmain;

 //mixer functions (volume,surround,speed,swpachan ...)
 if(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_ACONV){
  unsigned long i = MIXER_BUILTIN_CFPREPROCESSNUM;
  unsigned long funcflags = mpi->mixer_function_flags >> i;
  do{
   if(funcflags&1){
    struct one_mixerfunc_info *infop=all_mixerfunc_info[i];
    if(infop){
     mpi->private_data=mpi->mixerfuncs_private_datas[i];
     infop->process_routine(mpi);
     if(!mpi->samplenum){// && !(frp->filetype & HFT_FILE_EXT)){
      aui->samplenum=0;
      goto err_out_mixmain;
     }
    }
   }
   i++;
   funcflags>>=1;
  }while(funcflags);
 }

 if(((displaymode&DISP_ANALISER) && (displaymode&DISP_NOFULLEDIT) && !(mmi->mixer_infobits&AUINFOS_MIXERINFOBIT_DECODERANALISER))
#if defined(MPXPLAY_LINK_INFILE_FF_MPEG) && defined(MPXPLAY_GUI_QT)
  || (funcbit_test(frp->flags, MPXPLAY_MPXPFRAME_FLAG_SRVFFMV_CALLBACK) && mpxplay_config_video_audiovisualization_type)
#endif
 ){
  if(mpi->samplenum<2304)
   do_pcm16=2;
  else
   do_pcm16=1;
 }else
  if((displaymode&DISP_TIMEPOS) || SOUNDLIMITvol || aui->mvp->cfi->crossfadelimit)
   do_pcm16=1;
  else
   do_pcm16=0;

 if(do_pcm16){
  if((aui->card_wave_id==MPXPLAY_WAVEID_PCM_SLE) && (aui->bytespersample_card==2) && (do_pcm16<2)){
   mmi->pcm16=aui->pcm_sample;
   do_pcm16=0;
  }else{
   mmi->pcm16=mmi->MIXER_int16pcm_buffer;
   pds_memcpy((char *)mmi->pcm16,(char *)aui->pcm_sample,mpi->samplenum*mpi->bytespersample_mixer);
  }
 }

 adi=frp->infile_infos->audio_decoder_infos;
 if((mmi->mixer_infobits&AUINFOS_MIXERINFOBIT_ACONV) || (adi->infobits&ADI_FLAG_FLOATOUT)){
  unsigned int mixer_scalebits=(mmi->mixer_infobits&AUINFOS_MIXERINFOBIT_FCONV)? MIXER_SCALE_BITS:aui->bits_song;

  if(do_pcm16)
   cv_float_to_n_bits(mmi->pcm16,mpi->samplenum,mixer_scalebits,(16>>3),0);

  if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT)
   cv_scale_float(aui->pcm_sample,mpi->samplenum,mixer_scalebits,aui->bits_card,1);
  else
   cv_float_to_n_bits(aui->pcm_sample,mpi->samplenum,mixer_scalebits,aui->bytespersample_card,adi->infobits&ADI_FLAG_FPUROUND_CHOP);

 }else{
  if(do_pcm16)
   if(mpi->bytespersample_mixer!=2)
    cv_bits_n_to_m(mmi->pcm16,mpi->samplenum,mpi->bytespersample_mixer,2);

  if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT)
   cv_n_bits_to_float(aui->pcm_sample,mpi->samplenum,mpi->bytespersample_mixer,aui->bits_card);
  else
   cv_bits_n_to_m(aui->pcm_sample,mpi->samplenum,mpi->bytespersample_mixer,aui->bytespersample_card);
 }

#if defined(MPXPLAY_LINK_INFILE_FF_MPEG) && defined(MPXPLAY_GUI_QT)
 mpxplay_infile_callback_push_audio_packet((void *)frp, aui->freq_card, 16, aui->chan_card, mpi->samplenum, (unsigned char *)mmi->pcm16);
#endif

 if((displaymode&DISP_TIMEPOS) || SOUNDLIMITvol || aui->mvp->cfi->crossfadelimit)
  mixer_get_volumelevel(mmi->pcm16,mpi->samplenum,aui->chan_card);

#if defined(MPXPLAY_GUI_CONSOLE) && !defined(MPXPLAY_ARCH_X64)
 if((displaymode&DISP_ANALISER) && (displaymode&DISP_NOFULLEDIT) && !(mmi->mixer_infobits&AUINFOS_MIXERINFOBIT_DECODERANALISER))
  mixer_pcm_spectrum_analiser(mmi->pcm16,mpi->samplenum,aui->chan_card);
#endif

 aui->samplenum = mpi->samplenum;

 if(!funcbit_test(adi->infobits,ADI_CNTRLBIT_SILENTBLOCK))
  mixer_soundlimit_check(aui);

err_out_mixmain:
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

void MIXER_main_postprocess(struct mpxp_aumixer_main_info_s *mmi, struct mpxpframe_s *frp)
{
 struct mpxp_aumixer_passinfo_s *mpi = frp->mpi;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 if(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_CROSSFADE){
  struct mpxpframe_s *fro = frp->fro;
  struct mpxp_aumixer_passinfo_s *mpi1 = fro->mpi;
  if(mpi1->pcmout_savedsamples)
   pds_memcpy(fro->pcmout_buffer, fro->pcmout_buffer + (mpi1->samplenum * mpi1->bytespersample_mixer), (mpi1->pcmout_savedsamples * mpi1->bytespersample_mixer));
  mpi1->samplenum = 0;
 }
 if(mpi->pcmout_savedsamples) {
  pds_memcpy(frp->pcmout_buffer, frp->pcmout_buffer + (mpi->samplenum * mpi->bytespersample_mixer), (mpi->pcmout_savedsamples * mpi->bytespersample_mixer));
#ifdef MPXPLAY_USE_DEBUGF
  if(!(mmi->mixer_infobits & AUINFOS_MIXERINFOBIT_CROSSFADE)) {
   mpxplay_debugf(MPXPLAY_DEBUG_CF,"POST s1:%4d sv1:%d ss:%d", mpi->samplenum, mpi->pcmout_savedsamples, frp->pcmout_storedsamples);
  }
#endif
 }
 mpi->samplenum = 0;
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

//--------------------------------------------------------------------------
//search function-name in all_mixerfunc_info
static int mixer_common_search_functionname(char *func_name)
{
 unsigned int i;

 for(i=0;i<MIXER_nb_mixerfuncs;i++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[i];
  if(infop && infop->name){
   if(pds_stricmp(infop->name,func_name)==0)
    return i;
  }
 }
 return -1;
}

//set function's variable by a new value
static unsigned int mixer_common_setvar(struct mpxp_aumixer_passinfo_s *mpi,struct one_mixerfunc_info *infop,int functionnum,unsigned int setmode,int value)
{
 if(!infop)
  return 0;
 if(infop->infobits&MIXER_INFOBIT_BUSY)
  return 0;
 funcbit_smp_enable(infop->infobits,MIXER_INFOBIT_BUSY);

 if(mpi && infop->own_setvar_routine){

  mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
  infop->own_setvar_routine(mpi,setmode,value);

 }else if(infop->variablep){
  int currmixval=*(infop->variablep),newmixval=currmixval;

  if(infop->infobits&MIXER_INFOBIT_SWITCH){
   switch(setmode){
    case MIXER_SETMODE_RELATIVE:funcbit_inverse(newmixval,infop->var_max);break;
    case MIXER_SETMODE_ABSOLUTE:if(value>0)
                                 funcbit_enable(newmixval,infop->var_max);
                                else
                                 funcbit_disable(newmixval,infop->var_max);
                                break;
    case MIXER_SETMODE_RESET   :funcbit_disable(newmixval,infop->var_max);
                                newmixval|=infop->var_center;
                                break;
   }
  }else{
   switch(setmode){
    case MIXER_SETMODE_RELATIVE:newmixval=currmixval+(value*infop->var_step);
                                if((currmixval<infop->var_center && newmixval>infop->var_center) || (currmixval>infop->var_center && newmixval<infop->var_center))
                     newmixval=infop->var_center;
                break;
    case MIXER_SETMODE_ABSOLUTE:newmixval=value;break;
    case MIXER_SETMODE_RESET   :newmixval=infop->var_center;break;
   }
   if(newmixval<infop->var_min)
    newmixval=infop->var_min;
   else
    if(newmixval>infop->var_max)
     newmixval=infop->var_max;
  }

  funcbit_smp_int32_put(*(infop->variablep),newmixval);

 }else{
  funcbit_smp_disable(infop->infobits,MIXER_INFOBIT_BUSY);
  return 0;
 }

 funcbit_smp_disable(infop->infobits,MIXER_INFOBIT_BUSY);
 return 1;
}

//check function's variable and give back the (new) status of function (0/1)
static unsigned int mixer_common_checkvar(struct mpxp_aumixer_passinfo_s *mpi,struct one_mixerfunc_info *infop,int functionnum)
{
 unsigned int enable=0;

 if(!infop)
  return enable;

 if(infop->variablep){
  if(infop->infobits&MIXER_INFOBIT_SWITCH){
   if(*(infop->variablep))
    enable=1;
  }else{
   // ??? an extra range check
   if(*(infop->variablep)>infop->var_max)
    *(infop->variablep)=infop->var_max;
   else
    if(*(infop->variablep)<infop->var_min)
     *(infop->variablep)=infop->var_min;
   if(*(infop->variablep)!=infop->var_center)
    enable=1;
  }
 }

 if(mpi && infop->own_checkvar_routine){
  mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
  enable=infop->own_checkvar_routine(mpi);
 }

 return enable;
}

static void mixer_common_setflags(struct mpxp_aumixer_passinfo_s *mpi,struct one_mixerfunc_info *infop,int functionnum,unsigned int enable)
{
 unsigned int bit=1<<functionnum;

 if(!mpi || !infop)
  return;

 if(enable){
  funcbit_smp_enable(infop->infobits,MIXER_INFOBIT_ENABLED);
  if(infop->process_routine)
   funcbit_smp_enable(mpi->mixer_function_flags,bit);
 }else{
  funcbit_smp_disable(infop->infobits,MIXER_INFOBIT_ENABLED);
  funcbit_smp_disable(mpi->mixer_function_flags,bit);
 }
}

static unsigned int mixfunc_checkvar_setflags(struct mpxp_aumixer_passinfo_s *mpi,
  struct one_mixerfunc_info *infop,int functionnum)
{
 if(mpi && infop && (infop->infobits&MIXER_INFOBIT_RESETDONE) && !(infop->infobits&MIXER_INFOBIT_BUSY)){
  unsigned int enable,oldstatus;

  mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];

  funcbit_smp_enable(infop->infobits,MIXER_INFOBIT_BUSY);

  oldstatus=infop->infobits&MIXER_INFOBIT_ENABLED;
  enable=mixer_common_checkvar(mpi,infop,functionnum);
  mixer_common_setflags(mpi,infop,functionnum,enable);

  if(infop->function_init){
   if(!oldstatus && enable){
    mpxplay_debugf(MPXPLAY_DEBUG_CHKVAR, "mixfunc_checkvar_setflags START %d (oldstat:%d)", functionnum, oldstatus);
    if(!infop->function_init(mpi,MIXER_INITTYPE_START)){
     mixer_common_setflags(mpi,infop,functionnum,0);
     infop->function_init(mpi,MIXER_INITTYPE_STOP);
     mpxplay_debugf(MPXPLAY_DEBUG_CHKVAR, "mixfunc_checkvar_setflags STOP %d (oldstat:%d)", functionnum);
    }
   }else{
    if(oldstatus && !enable)
     infop->function_init(mpi,MIXER_INITTYPE_STOP);
   }
  }

  funcbit_smp_pointer_put(mpi->mixerfuncs_private_datas[functionnum],mpi->private_data);

  funcbit_smp_disable(infop->infobits,MIXER_INFOBIT_BUSY);
  return 1;
 }
 return 0;
}

//----------------------------------------------------------------------
// set (enable/disable) functions witch have external or parallel dependency
static void aumixer_checkallfunc_dependencies_internal(struct mpxp_aumixer_passinfo_s *mpi,int flag)
{
 unsigned int functionnum;
 for(functionnum=0;functionnum<MIXER_nb_mixerfuncs;functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop && (infop->infobits&flag))
   mixfunc_checkvar_setflags(mpi,infop,functionnum);
 }
}

static void aumixer_checkallfunc_dependencies_internal_allflags(struct mpxp_aumixer_passinfo_s *mpi)
{
 if(crossfadepart == CROSS_CLEAR){
  aumixer_checkallfunc_dependencies_internal(mpi,MIXER_INFOBIT_EXTERNAL_DEPENDENCY);
  aumixer_checkallfunc_dependencies_internal(mpi,MIXER_INFOBIT_PARALLEL_DEPENDENCY);
 }
}

//set one mixer function (modify,enable/disable)
static unsigned int aumixer_setfunction_internal(struct mpxp_aumixer_passinfo_s *mpi,char *func_name,unsigned int setmode,int value)
{
 struct one_mixerfunc_info *infop;
 int functionnum, enabled;

 functionnum=mixer_common_search_functionname(func_name);
 if(functionnum<0)
  return 0;

 infop=all_mixerfunc_info[functionnum];
 if(!mixer_common_setvar(mpi,infop,functionnum,setmode,value))
  return 0;

 enabled = mixfunc_checkvar_setflags(mpi,infop,functionnum);
 if(enabled)
  aumixer_checkallfunc_dependencies_internal(mpi,MIXER_INFOBIT_PARALLEL_DEPENDENCY);
 //mpxplay_debugf(MPXPLAY_DEBUG_INIT, "AMIXER SETFUNC frp:%8.8X ext:%8.8X", mpi->frp, (mpi->frp->filetype & HFT_FILE_EXT));
 return enabled;
}

//check one function and set its flag (enable/disable)
static unsigned int aumixer_checkfunc_setflags_internal(struct mpxp_aumixer_passinfo_s *mpi,char *func_name)
{
 int functionnum = mixer_common_search_functionname(func_name), enabled;
 if(functionnum < 0)
  return 0;
 enabled = mixfunc_checkvar_setflags(mpi,all_mixerfunc_info[functionnum], functionnum);
 if(enabled)
  aumixer_checkallfunc_dependencies_internal(mpi, MIXER_INFOBIT_PARALLEL_DEPENDENCY);
 return enabled;
}

// re-init main preprocess functions (speed, tone-bass)
static void aumixer_reinit_independent_preprocesses(struct mpxp_aumixer_passinfo_s *mpi)
{
 unsigned int i, funcbit = 1;
 for(i=0; (i<MIXER_BUILTIN_CFPREPROCESSNUM); i++,funcbit<<=1 ){
  struct one_mixerfunc_info *infop = all_mixerfunc_info[i];
  if(infop && infop->function_init){
   mpi->private_data = mpi->mixerfuncs_private_datas[i];
   infop->function_init(mpi,((mpi->private_data)? MIXER_INITTYPE_REINIT:MIXER_INITTYPE_INIT));
   funcbit_smp_pointer_put(mpi->mixerfuncs_private_datas[i],mpi->private_data);
   mixfunc_checkvar_setflags(mpi,infop,i);
   if(mpi->mixer_function_flags&funcbit)
    infop->function_init(mpi,MIXER_INITTYPE_RESET); // !!! we reset only the preprocess functions of the new file
  }
 }
}

//----------------------------------------------------------------------
//init all functions (if it has function_init)
#ifdef MPXPLAY_LINK_DLLLOAD
#include <mem.h>

static void MIXER_load_plugins(void)
{
 int functionnum;
 mpxplay_module_entry_s *dll_found=NULL;

 do{
  dll_found=newfunc_dllload_getmodule(MPXPLAY_DLLMODULETYPE_AUMIXER,0,NULL,dll_found); // get next
  //fprintf(stdout,"dll:%8.8X sv:%4.4X\n",dll_found,dll_found->module_structure_version);
  if(dll_found && (dll_found->module_structure_version==MPXPLAY_DLLMODULEVER_AUMIXER)){ // !!!
   struct one_mixerfunc_info *infop=(struct one_mixerfunc_info *)dll_found->module_callpoint;
   if(infop){
    functionnum=mixer_common_search_functionname(infop->name);
    if(functionnum>=0)                      // overwrite built-in dsp routine
     all_mixerfunc_info[functionnum]=infop;
    else{                                   // insert the new dsp routine
     if(MIXER_nb_mixerfuncs<MIXER_MAX_FUNCTIONS){
      memmove((void *)&all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT],(void *)&all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT+1],(MIXER_nb_mixerfuncs-MIXER_DLLFUNC_INSERTPOINT)*sizeof(struct one_mixerfunc_info *));
      all_mixerfunc_info[MIXER_DLLFUNC_INSERTPOINT]=infop;
      MIXER_nb_mixerfuncs++;
     }
    }
   }
  }
 }while(dll_found);
}
#endif

void MIXER_init(struct mainvars *mvp, struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp)
{
 struct mpxp_aumixer_main_info_s *mmi=&mvp->mmi;
 struct mpxp_aumixer_passinfo_s *mpi;

 mpi=aumixer_frpmpi_alloc(frp);
 if(!mpi)
  mpxplay_close_program(MPXERROR_XMS_MEM);

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 mpi->control_cb= mpxplay_aumixer_control_cb;
 mpi->ccb_data  = mpi;
 mpi->freq_card = mpi->freq_song = 44100;
 mpi->chan_card = mpi->chan_song = PCM_CHANNELS_CFG;
 mpi->bits_card = mpi->bits_song = 16;
 mpi->mmi       = mmi;
 mpi->mvp       = mvp;
 mpi->aui       = aui;
 mpi->frp       = frp;

 funcbit_enable(MIXER_controlbits,MIXER_CONTROLBIT_LIMITER);
#ifdef MPXPLAY_LINK_DLLLOAD
 MIXER_load_plugins();
#endif
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
 MIXER_resetallfunc(mpi);

 mpxplay_debugf(MPXPLAY_DEBUG_INIT, "MIXER INIT  frp:%8.8X mpi:%8.8X", (unsigned long)frp, (unsigned long)mpi);
}

void MIXER_config_set(struct mpxp_aumixer_main_info_s *mmi)
{
 if(MIXER_controlbits&MIXER_CONTROLBIT_SPEED1000)
  funcbit_enable(mmi->mixer_infobits,AUINFOS_MIXERCTRLBIT_SPEED1000);
}

//first init (malloc)
void MIXER_allfuncinit_init(struct mainvars *mvp,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp)
{
 struct mpxp_aumixer_main_info_s *mmi=&mvp->mmi;
 struct mpxp_aumixer_passinfo_s *mpi=frp->mpi;
 unsigned int functionnum;

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 mmi->MIXER_int16pcm_buffer=(PCM_CV_TYPE_S *)pds_malloc(2*PCM_BUFFER_SIZE);
 if(mmi->MIXER_int16pcm_buffer)
  mmi->MIXER_int16pcm_bufsize=PCM_BUFFER_SIZE;

 if(MIXER_controlbits&MIXER_CONTROLBIT_SOFTTONE)
  funcbit_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_HWTONE);

 for(functionnum=0;functionnum<MIXER_nb_mixerfuncs;functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop && infop->function_init){
   mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
   infop->function_init(mpi,MIXER_INITTYPE_INIT);
   mpi->mixerfuncs_private_datas[functionnum]=mpi->private_data;
  }
 }

 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

//func close (free)
void MIXER_allfuncinit_close(struct mainvars *mvp)
{
 struct mpxp_aumixer_main_info_s *mmi=&mvp->mmi;
 struct mpxp_aumixer_passinfo_s *mpi=mvp->fr_primary->mpi;
 unsigned int functionnum;

 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 for(functionnum=0;functionnum<MIXER_nb_mixerfuncs;functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop && infop->function_init){
   mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
   infop->function_init(mpi,MIXER_INITTYPE_CLOSE);
   mpi->mixerfuncs_private_datas[functionnum]=NULL;
  }
 }
 if(mmi->MIXER_int16pcm_buffer){
  pds_free(mmi->MIXER_int16pcm_buffer);
  mmi->MIXER_int16pcm_buffer=NULL;
  mmi->MIXER_int16pcm_bufsize=0;
 }
 pds_threads_mutex_del(&mpi->mutex_aumixer);
}

//----------------------------------------------------------------------
// the control API
void MIXER_setfunction(struct mpxp_aumixer_passinfo_s *mpi,char *func_name,unsigned int setmode,int value)
{
 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 aumixer_setfunction_internal(mpi,func_name,setmode,value);
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

// return mixerfunc structure (to read a value from it)
struct one_mixerfunc_info *MIXER_getfunction(char *func_name)
{
 struct one_mixerfunc_info *infop;
 int functionnum = mixer_common_search_functionname(func_name);
 infop = (functionnum >= 0)? all_mixerfunc_info[functionnum] : NULL;
 return infop;
}

int MIXER_getvalue(char *func_name)
{
 struct one_mixerfunc_info *infop=MIXER_getfunction(func_name);

 if(infop && infop->variablep)
  return (*infop->variablep);

 return 0;
}

// disabled(0)/enabled(1)
int MIXER_getstatus(char *func_name)
{
 struct one_mixerfunc_info *infop=MIXER_getfunction(func_name);

 if(infop)
  return (infop->infobits&MIXER_INFOBIT_ENABLED);

 return 0;
}

void MIXER_checkfunc_setflags(struct mpxp_aumixer_passinfo_s *mpi,char *func_name)
{
 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 aumixer_checkfunc_setflags_internal(mpi, func_name);
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

void MIXER_checkallfunc_setflags(struct mpxp_aumixer_passinfo_s *mpi)
{
 unsigned int functionnum;
 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 for(functionnum=0;functionnum<MIXER_nb_mixerfuncs;functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  mixfunc_checkvar_setflags(mpi,infop,functionnum);
 }
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

//set all mixer variables/functions to default (zero) value
void MIXER_resetallfunc(struct mpxp_aumixer_passinfo_s *mpi)
{
 unsigned int functionnum;
 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 for(functionnum=0;functionnum<MIXER_nb_mixerfuncs;functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop){
   mixer_common_setvar(mpi,infop,functionnum,MIXER_SETMODE_RESET,0);
   funcbit_smp_enable(infop->infobits,MIXER_INFOBIT_RESETDONE);
  }
 }
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

// re-init buffers (ie: at new file (external dep))
void MIXER_allfuncinit_reinit(struct mpxp_aumixer_passinfo_s *mpi)
{
 unsigned int functionnum;

 if(!mpi || !mpi->mvp || !mpi->mvp->fr_primary)
  return;

 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);
 mpi->frp = mpi->mvp->fr_primary; // !!!

 mpxplay_debugf(MPXPLAY_DEBUG_INIT, "AMIXER REINIT frp:%8.8X ext:%8.8X infobits:%8.8X", mpi->frp, (mpi->frp->filetype & HFT_FILE_EXT), all_mixerfunc_info[0]->infobits);

 for(functionnum=0; functionnum<MIXER_nb_mixerfuncs; functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop && infop->function_init && (infop->infobits&MIXER_INFOBIT_ENABLED)){
   mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
   infop->function_init(mpi,MIXER_INITTYPE_REINIT);
   mpi->mixerfuncs_private_datas[functionnum]=mpi->private_data;
  }
 }
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

//clear buffers (ie: at seeking)
void MIXER_allfuncinit_restart(struct mpxp_aumixer_passinfo_s *mpi)
{
 unsigned int functionnum;
 if(!mpi)
  return;
 PDS_THREADS_MUTEX_LOCK(&mpi->mutex_aumixer, -1);

 mpxplay_debugf(MPXPLAY_DEBUG_INIT, "AMIXER RESTART frp:%8.8X ext:%8.8X", mpi->frp, (mpi->frp->filetype & HFT_FILE_EXT));

 for(functionnum=0; functionnum<MIXER_nb_mixerfuncs; functionnum++){
  struct one_mixerfunc_info *infop=all_mixerfunc_info[functionnum];
  if(infop && infop->function_init && (infop->infobits&MIXER_INFOBIT_ENABLED)){
   mpi->private_data=mpi->mixerfuncs_private_datas[functionnum];
   infop->function_init(mpi,MIXER_INITTYPE_RESET);
   mpi->mixerfuncs_private_datas[functionnum]=mpi->private_data;
  }
 }
 PDS_THREADS_MUTEX_UNLOCK(&mpi->mutex_aumixer);
}

//----------------------------------------------------------------------
// control callback, called inside from mixer functions
mpxp_int32_t mpxplay_aumixer_control_cb(void *cb_data,mpxp_uint32_t funcnum,void *argp1,void *argp2)
{
 struct mpxp_aumixer_passinfo_s *mpi=cb_data;
 struct mpxplay_audioout_info_s *aui;
 struct mpxp_aumixer_main_info_s *mmi;
 struct mpxpframe_s *frp;

 if(!mpi || !argp1)
  return MPXPLAY_ERROR_CFGFUNC_ARGUMENTMISSING;

 switch(funcnum){
  case MPXPLAY_CFGFUNCNUM_AUMIXER_CHECKVAR:
   aumixer_checkfunc_setflags_internal(mpi,(char *)argp1);
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_GET:
   if(!argp2)
    return MPXPLAY_ERROR_CFGFUNC_ARGUMENTMISSING;
   *((mpxp_int32_t *)argp2)=MIXER_getvalue((char *)argp1);
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_REL:
  case MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_ABS:
   if(!argp2)
    return MPXPLAY_ERROR_CFGFUNC_ARGUMENTMISSING;
   aumixer_setfunction_internal(mpi, (char *)argp1, (funcnum&3), *((mpxp_int32_t *)argp2));
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_RES:
   aumixer_setfunction_internal(mpi, (char *)argp1, (funcnum&3), 0);
   goto err_out_ok;
 }

 mmi=mpi->mmi;
 if(!mmi)
  return MPXPLAY_ERROR_CFGFUNC_INVALIDDATA;

 switch(funcnum){
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_INFOBITS:
   *((mpxp_int32_t *)argp1)=mmi->mixer_infobits;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_BYTESPERSAMPLE:
   *((mpxp_int32_t *)argp1)=mpi->bytespersample_mixer;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_SET_INFOBITS:
   funcbit_smp_int32_put(mmi->mixer_infobits,*((mpxp_uint32_t *)argp1));
   goto err_out_ok;
 }

 aui=mpi->aui;
 if(!aui)
  return MPXPLAY_ERROR_CFGFUNC_INVALIDDATA;

 switch(funcnum){
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_FREQ:
   *((mpxp_int32_t *)argp1)=aui->freq_song;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_CHAN:
   *((mpxp_int32_t *)argp1)=aui->chan_song;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_BITS:
   *((mpxp_int32_t *)argp1)=aui->bits_song;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_INFOBITS:
   *((mpxp_int32_t *)argp1)=aui->card_infobits;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_CHANCFGNUM:
   *((mpxp_int32_t *)argp1)=PCM_CHANNELS_CFG;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BYTESPERSAMPLE:
   *((mpxp_int32_t *)argp1)=aui->bytespersample_card;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_FREQ:
   *((mpxp_int32_t *)argp1)=aui->freq_card;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_CHAN:
   *((mpxp_int32_t *)argp1)=aui->chan_card;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BITS:
   *((mpxp_int32_t *)argp1)=aui->bits_card;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BASS:
   *((mpxp_int32_t *)argp1)=aui->card_mixer_values[AU_MIXCHAN_BASS];
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_TREBLE:
   *((mpxp_int32_t *)argp1)=aui->card_mixer_values[AU_MIXCHAN_TREBLE];
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_BASS:
   funcbit_smp_int32_put(aui->card_mixer_values[AU_MIXCHAN_BASS],*((mpxp_uint32_t *)argp1));
   if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE)
    AU_setmixer_one(aui,AU_MIXCHAN_BASS,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_BASS]);
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_TREBLE:
   funcbit_smp_int32_put(aui->card_mixer_values[AU_MIXCHAN_TREBLE],*((mpxp_uint32_t *)argp1));
   if(aui->card_infobits&AUINFOS_CARDINFOBIT_HWTONE)
    AU_setmixer_one(aui,AU_MIXCHAN_TREBLE,MIXER_SETMODE_ABSOLUTE,aui->card_mixer_values[AU_MIXCHAN_TREBLE]);
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_INT08DECODERCYCLES:
   funcbit_smp_int32_put(aui->int08_decoder_cycles,*((mpxp_uint32_t *)argp1));
   goto err_out_ok;
 }

 frp=mpi->frp;
 if(!frp)
  return MPXPLAY_ERROR_CFGFUNC_INVALIDDATA;

 switch(funcnum){
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_PCMOUTBLOCKSIZE:
   *((mpxp_int32_t *)argp1)=frp->pcmout_blocksize;
   goto err_out_ok;
  case MPXPLAY_CFGFUNCNUM_AUMIXER_GET_PCMOUTBUFSIZE:
   *((mpxp_int32_t *)argp1)=frp->pcmout_bufsize;
   goto err_out_ok;
 }

 return MPXPLAY_ERROR_CFGFUNC_UNSUPPFUNC;

err_out_ok:
 return MPXPLAY_ERROR_OK;
}
