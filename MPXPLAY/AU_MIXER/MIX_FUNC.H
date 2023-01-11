#ifndef mix_func_h
#define mix_func_h

//#include "mpxplay.h"
#include "au_mixer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void mixer_crossfade_configure(struct mpxplay_audioout_info_s *,struct mpxpframe_s *,struct mpxpframe_s *);
extern void calculate_crossvolumes(struct mpxplay_audioout_info_s *,long *,long *);

//cv_bits.c
extern void cv_bits_n_to_m(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_bytespersample);
extern void cv_n_bits_to_float(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_scalebits);
extern void cv_float_to_n_bits(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_scalebits,unsigned int out_bytespersample,unsigned int fpuround_chop);
extern void cv_float16_to_int16(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int fpuround_chop);
extern void cv_scale_float(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_scalebits,unsigned int out_scalebits,unsigned int rangecheck);
extern void aumixer_cvbits_float64le_to_float32le(PCM_CV_TYPE_S *pcm,unsigned int samplenum);

//cv_freq.c
//extern unsigned int mixer_speed_hq(PCM_CV_TYPE_F *pcm,unsigned int samplenum_in);
//extern unsigned int mixer_speed_lq(PCM_CV_TYPE_S *pcm,unsigned int samplenum_in);
#ifdef SBEMU
extern unsigned int mixer_speed_lq(PCM_CV_TYPE_S *pcm16, unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate);
#endif

//cv_chan.c
extern unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int newchannels,unsigned int bytespersample);
extern unsigned int cv_channels_n_to_1(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int oldchannels,unsigned int bytespersample);
extern unsigned int cv_channels_n_to_2(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int oldchannels,unsigned int bytespersample);
extern unsigned int cv_channels_remap(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int channelnum_in,mpxp_uint8_t *chanmatrix_in,unsigned int channelnum_out,mpxp_uint8_t *chanmatrix_out,unsigned int bytespersample);
extern unsigned int cv_channels_downmix(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int channelnum_in,mpxp_uint8_t *chanmatrix_in,unsigned int channelnum_out,mpxp_uint8_t *chanmatrix_out,unsigned int bytespersample);

//analiser.c
extern void mixer_get_volumelevel(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int channelnum);
#if defined(MPXPLAY_GUI_CONSOLE) && !defined(MPXPLAY_ARCH_X64)
extern void mixer_pcm_spectrum_analiser(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int channelnum);
#endif

//cutsilen.c
extern void mixer_soundlimit_check(struct mpxplay_audioout_info_s *);

//mx_volum.c
extern unsigned int mpxplay_aumixer_mxvolum_getrealvolume_fast(void);
extern unsigned int mpxplay_aumixer_mxvolum_getrealvolume_slow(void);

#ifdef __cplusplus
}
#endif

#endif
