#ifndef au_mixer_h
#define au_mixer_h

#include "mpxplay.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef mpxp_int32_t  PCM_CV_TYPE_I;  // max. 32 bit input/output pcm format (integer)
typedef mpxp_uint32_t PCM_CV_TYPE_UI; //
typedef mpxp_float_t  PCM_CV_TYPE_F;  // 32-bit float part of mixer
typedef mpxp_int16_t  PCM_CV_TYPE_S;  // 16-bit integer part of mixer
typedef mpxp_int8_t   PCM_CV_TYPE_C;  // 8-bit signed
typedef mpxp_uint8_t  PCM_CV_TYPE_UC; // 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;// largest datatype (currently)

#define MIXER_SCALE_BITS    16 // used bits in integer based (lq) functions and default scale bits in hq functions
#define MIXER_SCALE_VAL  65536
#define MIXER_SCALE_MIN -32768
#define MIXER_SCALE_MAX  32767

#define MIXER_MAX_FUNCTIONS 31 // built-in + dll
#define MIXER_BUILTIN_FUNCTIONS  13 // !!!
#define MIXER_BUILTIN_CFPREPROCESSNUM 2 // last is speed/freq control
#define MIXER_DLLFUNC_INSERTPOINT   3 // after crossfader
/*#define MIXER_BUILTIN_FUNCTIONS  4    // !!!
#define MIXER_BUILTIN_CFPREPROCESSNUM 2 // last is speed/freq control
#define MIXER_DLLFUNC_INSERTPOINT   3   // after crossfader*/

#define AUINFOS_MIXERINFOBIT_ACONV      (1<<0)
#define AUINFOS_MIXERINFOBIT_FCONV      (1<<1)
#define AUINFOS_MIXERINFOBIT_DECODERANALISER (1<<5) // else enable pcm spectrum analiser
#define AUINFOS_MIXERCTRLBIT_SPEED1000  (1<<6) // (try to) enable 0.1% speed control precision (set in mixer-init)
#define AUINFOS_MIXERINFOBIT_SPEED1000  (1<<7) // 0.1% speed control precision is enabled (set in speed control init)
#define AUINFOS_MIXERINFOBIT_CROSSFADE  (1<<9) // crossfade flag for mixer

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1
#define MIXER_SETMODE_RESET    2

// one_mixerfunc_info->function_init(inittype) values
#define MIXER_INITTYPE_INIT    0 // first initialization (alloc buffers,calculate constant tables)
#define MIXER_INITTYPE_REINIT  1 // re-init buffers (ie: at external dep (newfile))
#define MIXER_INITTYPE_START   2 // start flag
#define MIXER_INITTYPE_RESET   4 // reset flag (ie:at seeking)
#define MIXER_INITTYPE_STOP    8 // stop flag
#define MIXER_INITTYPE_CLOSE  16 // close/dealloc

// one_mixerfunc_info->infobits
#define MIXER_INFOBIT_RESETDONE            1 // mixer value is set to default (reset done)
#define MIXER_INFOBIT_ENABLED              2 // mixer function is enabled/used
#define MIXER_INFOBIT_BUSY                 4 // mixer function is under control/modification (to avoid short circuit (endless cycle) in the mixer)
#define MIXER_INFOBIT_SWITCH               8 // function variable a switch (flip-flop)(its value can be 0 or 1 only)
#define MIXER_INFOBIT_EXTERNAL_DEPENDENCY 16 // function has an external dependency (controlled by another/external variable too)(ie:freq conversion,swapchan,surround)
#define MIXER_INFOBIT_PARALLEL_DEPENDENCY 32 // dependant/enabled by another mixer function too
//#define MIXER_INFOBIT_USE_LIMITER       64 // mixer function modifies amp -> enable limiter
//#define MIXER_INFOBIT_RDT_OPTIONS      128 // value modification has desktop effect (RDT_OPTIONS)
//#define MIXER_INFOBIT_ANALISER16       256 //

// MIXER_controlbits
#define MIXER_CONTROLBIT_LIMITER   1  // enable limiter
#define MIXER_CONTROLBIT_SOFTTONE  2  // allways use software tone
#define MIXER_CONTROLBIT_SPEED1000 4  // (try to) enable 0.1% speed control
//#define MIXER_CONTROLBIT_NOSURMIX  8  // disable surround downmix (at 5.1->stereo)
#define MIXER_CONTROLBIT_DISABLE_DECODERTONE 16
#define MIXER_CONTROLBIT_DISABLE_MIXERTONE   32

#define MPXPLAY_CFGFUNCNUM_AUMIXER_MAJOR 0x08000000

#define MPXPLAY_CFGFUNCNUM_AUMIXER_CHECKVAR     0x08000000 // ccb_data:void*mpi argp1:char*mixname
#define MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_GET 0x08000001 // ccb_data:void*mpi argp1:char*mixname argp2:mpxp_int32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_REL 0x08000010 // relative set, ccb_data:void*mpi argp1:char*mixname argp2:mpxp_int32_t*set_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_ABS 0x08000011 // absolute set
#define MPXPLAY_CFGFUNCNUM_AUMIXER_MIXVALUE_SET_RES 0x08000012 // reset value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_INFOBITS 0x08000030     // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_BYTESPERSAMPLE 0x08000032 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_SET_INFOBITS  0x08000040 // ccb_data:void*mpi argp1:mpxp_uint32_t*set_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_PCMOUTBLOCKSIZE 0x08000050 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_PCMOUTBUFSIZE 0x08000051 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_FREQ 0x08000100 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_CHAN 0x08000101 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_SONG_BITS 0x08000102 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_INFOBITS 0x08000120 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_CHANCFGNUM 0x08000121 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BYTESPERSAMPLE 0x08000122 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_FREQ 0x08000125 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_CHAN 0x08000126 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BITS 0x08000127 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_BASS 0x0800012A // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_TREBLE 0x0800012B // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_BASS 0x0800013A // ccb_data:void*mpi argp1:mpxp_uint32_t*set_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_TREBLE 0x0800013B // ccb_data:void*mpi argp1:mpxp_uint32_t*set_value
#define MPXPLAY_CFGFUNCNUM_AUMIXER_SET_CARD_INT08DECODERCYCLES 0x08000140 // ccb_data:void*mpi argp1:mpxp_uint32_t*set_value

#define MPXPLAY_AUMIXER_MUTE_SWITCH_SIGN 65535  // on/off switch instead of 'push' (ctrl-'M')

struct mpxp_aumixer_main_info_s;

typedef struct mpxp_aumixer_passinfo_s{
 // fixed structure -----------
 mpxp_int32_t (*control_cb)(void *ccb_data,mpxp_uint32_t cfgnum,void *argp1,void *argp2);
 void *ccb_data;
 void *private_data;
 short *pcm_sample;
 unsigned int samplenum;
 // ---------------------------
 // only built-in functions can use these variables (external/dll must use mpi->control_cb to get informations)
 unsigned int mixer_function_flags; // enabled functions
 int  bytespersample_mixer; // in
 int  freq_song;
 int  freq_card;
 int  chan_song;
 int  chan_card;
 int  bits_song;
 int  bits_card;
 int  pcmout_savedsamples;
 struct mpxp_aumixer_main_info_s *mmi;
 struct mainvars *mvp;
 struct mpxplay_audioout_info_s *aui;
 struct mpxpframe_s *frp;
 void *mutex_aumixer;
 void *mixerfuncs_private_datas[MIXER_MAX_FUNCTIONS];
}mpxp_aumixer_passinfo_s;

typedef struct mpxp_aumixer_main_info_s{
 short *pcm16;                  // at mixer end
 short *pcm_sample;             // at mixer end
 unsigned long samplenum;       // at mixer end
 PCM_CV_TYPE_S *MIXER_int16pcm_buffer;
 unsigned long MIXER_int16pcm_bufsize;
 unsigned long mixer_infobits;  // MIXERINFOBIT_
}mpxp_aumixer_main_info_s;

typedef struct one_mixerfunc_info{
 char *name;
 char *cmdlineopt;
 int  *variablep;
 int  infobits;
 int  var_min;     // minimum value (must be set if variable is not a SWITCH)
 int  var_max;     // maximum value (must be set if variable is not a SWITCH)
 int  var_center;  // center (zero) value (must be set, if center value is not 0)
 int  var_step;    // step
 int  (*function_init)(struct mpxp_aumixer_passinfo_s *,int inittype); // see inittype flags
 long (*function_config)(struct mpxp_aumixer_passinfo_s *,unsigned long cfgnum,void *argp1,void *argp2); // pre-config without argument
 void (*process_routine)(struct mpxp_aumixer_passinfo_s *); // float (32 bit) based audio modification/transformation
 int  (*own_checkvar_routine)(struct mpxp_aumixer_passinfo_s *);  // if(*variablep!=var_center && check_var())
 void (*own_setvar_routine)(struct mpxp_aumixer_passinfo_s *,unsigned int setmode,int value);
}one_mixerfunc_info;

extern unsigned long MIXER_controlbits;
extern int MIXER_var_volume, MIXER_var_balance, MIXER_var_mute_voldiv, MIXER_var_swapchan, MIXER_var_autovolume, MIXER_var_limiter_overflow;
extern unsigned long MIXER_loudness_val_volume, MIXER_loudness_val_surround, MIXER_loudness_val_bass, MIXER_loudness_val_treble;

extern unsigned int MIXER_configure(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp);
extern unsigned int AUMIXER_collect_pcmout_data(struct mpxpframe_s *frp,PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum);
extern unsigned int MIXER_conversion(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp);
extern void MIXER_main(struct mpxp_aumixer_main_info_s *mmi,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp);
extern void MIXER_main_postprocess(struct mpxp_aumixer_main_info_s *mmi, struct mpxpframe_s *frp);

extern void MIXER_setfunction(struct mpxp_aumixer_passinfo_s *mpi,char *func_name,unsigned int setmode,int value);
extern one_mixerfunc_info *MIXER_getfunction(char *func_name);
extern int  MIXER_getvalue(char *func_name);
extern int  MIXER_getstatus(char *func_name);
extern void MIXER_checkfunc_setflags(struct mpxp_aumixer_passinfo_s *mpi,char *func_name);
extern void MIXER_checkallfunc_setflags(struct mpxp_aumixer_passinfo_s *mpi);
//extern void MIXER_checkallfunc_dependencies(struct mpxp_aumixer_passinfo_s *mpi,int flag);
extern void MIXER_resetallfunc(struct mpxp_aumixer_passinfo_s *mpi);

extern void MIXER_init(struct mainvars *mvp,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp);
extern void MIXER_config_set(struct mpxp_aumixer_main_info_s *mmi);
extern void MIXER_allfuncinit_init(struct mainvars *mvp,struct mpxplay_audioout_info_s *aui,struct mpxpframe_s *frp);
extern void MIXER_allfuncinit_close(struct mainvars *mvp);
extern void MIXER_allfuncinit_reinit(struct mpxp_aumixer_passinfo_s *mpi);
extern void MIXER_allfuncinit_restart(struct mpxp_aumixer_passinfo_s *mpi);

extern mpxp_int32_t mpxplay_aumixer_control_cb(void *cb_data,mpxp_uint32_t funcnum,void *argp1,void *argp2);

#ifdef __cplusplus
}
#endif

#endif
