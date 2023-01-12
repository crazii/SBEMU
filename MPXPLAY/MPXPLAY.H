//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2013 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: definitions

#ifndef mpxplay_h
#define mpxplay_h

#include <fcntl.h>

#ifndef MPXPLAY
#define MPXPLAY 1
#endif

#if defined(WIN32) || defined(__WINDOWS_386__) || defined(__NT__) || defined(_WIN32)
#define MPXPLAY_WIN32 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MPXPLAY_LINK_FULL     1
#ifdef __DOS__
 #define MPXPLAY_LINK_SWSCK32  1    // with SWSCK32 lib
 //#define MPXPLAY_LINK_WATTCP32 1    // with WATTCP32 lib
#endif

#if (defined(__DOS__) || (defined(MPXPLAY_WIN32) && !defined(MPXPLAY_GUI_QT))) && !defined(SBEMU)
#define MPXPLAY_GUI_CONSOLE 1
#endif

#if defined(MPXPLAY_WIN32) || defined(MPXPLAY_LINK_SWSCK32) || defined(MPXPLAY_LINK_WATTCP32)
 #define MPXPLAY_LINK_TCPIP 1
#endif

#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_CONSOLE)  // only win32 console has credentials (Win7 needs different solution)
#define MPXPLAY_LINK_CREDENTIALS 1
#endif

#if !defined(MPXPLAY_WIN32)
#undef MPXPLAY_LINK_INSTALLER
#endif

//#define USE_LSA_SCV 1            // show/use soundcard volume on desktop (instead of software volume)
#define MPXPLAY_INFILE_FILEHANDLERS_PLAY 2 // primary, secondary
#define MPXPLAY_INFILE_FILEHANDLERS_MAX  3 // primary, secondary, check

// out pcm defs
#define PCM_OUTSAMPLES    1152     // at 44100Hz
#define PCM_MIN_CHANNELS     1
#ifdef MPXPLAY_LINK_FULL
#define PCM_MAX_CHANNELS     8     // au_mixer output (au_card input) limit
#else
#define PCM_MAX_CHANNELS     2     // au_mixer output (au_card input) limit
#endif
#define PCM_CHANNELS_DEFAULT 2
#define PCM_CHANNELS_CFG ((aui->chan_card)? aui->chan_card:((aui->chan_set)? aui->chan_set:PCM_CHANNELS_DEFAULT))
#define PCM_MIN_BITS      1
#define PCM_MAX_BITS      32
#define PCM_MIN_FREQ      512
#define PCM_MAX_FREQ      192000   // program can play higher freq too
#define PCM_MAX_SAMPLES   (((PCM_OUTSAMPLES*PCM_MAX_FREQ)+22050)/44100*PCM_CHANNELS_CFG) // only the pcm buffer is limited (in one frame)
#define PCM_MAX_BYTES     (PCM_MAX_SAMPLES*(PCM_MAX_BITS/8))  // in one frame
#define PCM_BUFFER_SIZE   (2*PCM_MAX_BYTES) // *2 : speed control expansion

// outmodes
#define OUTMODE_TYPE_NONE            0  // ie:-is,-iw
#define OUTMODE_TYPE_TEST      (1 << 0) // testmode (-t) (null output without startup, to test the speed of decoding)
#define OUTMODE_TYPE_AUDIO     (1 << 1) // audio mode
#define OUTMODE_TYPE_FILE      (1 << 2) // write output into file (-o)
#define OUTMODE_TYPE_NULL      (1 << 3) // null output with startup
#define OUTMODE_TYPE_MASK      (OUTMODE_TYPE_TEST|OUTMODE_TYPE_AUDIO|OUTMODE_TYPE_FILE|OUTMODE_TYPE_NULL)
#define OUTMODE_CONTROL_FILE_BITSTREAMOUT (1 <<  4) // -obs
#define OUTMODE_CONTROL_FILE_FLOATOUT     (1 <<  5) // -obf
#define OUTMODE_CONTROL_FILE_TAGLFN       (1 <<  6) // -oni
#define OUTMODE_CONTROL_SNDCARD_DDMA      (1 <<  8) // -ddma
//#define OUTMODE_CONTROL_FILE_MASK (OUTMODE_CONTROL_FILE_BITSTREAMOUT|OUTMODE_CONTROL_FILE_FLOATOUT|OUTMODE_CONTROL_FILE_TAGLFN)

//adone values
#define ADONE_EOF     1   // end of file
#define ADONE_RESTART 2   // open new file
#define ADONE_REOPEN  3   // reopen the same file

//su_startuptype flags
#define MPXPLAY_STARTUP_FLAG_LOAD     1
#define MPXPLAY_STARTUP_FLAG_SAVE     2
#define MPXPLAY_STARTUP_FLAG_CMOS     4
#define MPXPLAY_STARTUP_FLAG_RESDIR   8 // restore directory and song-pos at non-playlist mode
#define MPXPLAY_STARTUP_FLAG_2SIDES  16 // restore both editorsides with all tabs
#define MPXPLAY_STARTUP_FLAG_NEWFILE 32 // save position at every new file (into mpxtabs.ini)
#define MPXPLAY_STARTUP_FLAGS_DEFAULT (MPXPLAY_STARTUP_FLAG_LOAD | MPXPLAY_STARTUP_FLAG_SAVE | MPXPLAY_STARTUP_FLAG_2SIDES) // for installer version only

//mpxplay_programcontrol
#define MPXPLAY_PROGRAMC_CONFIRMEXIT      (1 <<  0) // confirm exit (-xce)
#define MPXPLAY_PROGRAMC_SHUTDOWNATX      (1 <<  1) // ATX/APM shutdown at exit
#define MPXPLAY_PROGRAMC_AUTODETECTALWAYS (1 <<  3) // always autodetect unknown filetypes
#define MPXPLAY_PROGRAMC_SYSTEMAPPOPEN_UNKNOWN (1 <<  4) // open unknown files with default system application
#define MPXPLAY_PROGRAMC_SYSTEMAPPOPEN_ALLFILES (1 <<  5) // open all files with default system application
#define MPXPLAY_PROGRAMC_DISABLE_SMP      (1 << 6)  // disable multi threading
#define MPXPLAY_PROGRAMC_ENABLE_HYPERTH   (1 << 7)  // enable hyper threading

//mpxplay_inside_programcontrol
#define MPXPLAY_IPROGRAMC_SHUTDOWN_FORBIDD (1 << 30) // internal flag for shutdown override (forbidd)
#define MPXPLAY_IPROGRAMC_SHUTDOWN_FORCE   (1 << 31) // internal flag for shutdown override (force)

//playcontrol flags (status and control)
#define PLAYC_PAUSEALL         1 // -ppa (pause before each song)
#define PLAYC_HIGHSCAN         2 // -phs
#define PLAYC_PAUSENEXT        4 // -ppn (pause at first/next song)
#define PLAYC_HSSTARTPOSHALF   8 // -phsh
//#define PLAYC_AUTOGFX          8 // switch to video mode if file has video stream
#define PLAYC_CONTINUOUS_SEEK 16 //
#define PLAYC_STARTNEXT       32 // start playing at next song
#define PLAYC_RUNNING         64 // playing is running
#define PLAYC_BEGINOFSONG    128 // begin of song (while no sound)
#define PLAYC_ABORTNEXT      256 // skip to next song is aborted
#define PLAYC_EXITENDLIST    512 // exit at end of list
#define PLAYC_NOAUTOPRGSKIP  1024 // don't skip (DVB/TS) program automatically (if the current/selected program lost)
#define PLAYC_FIRSTPLAYFLAG (1<<28) // first start of playing (to enter fullscreen at program start)
#define PLAYC_STARTFLAG     (1<<29) // playing has just started or it was temporary stopped (EXT files / QT video)
#define PLAYC_ENTERFLAG     (1<<30) // new file has started with ENTER key
#define PLAYC_ENTERNEWFILE  (1<<31) // ENTER key open a new file
#ifdef MPXPLAY_GUI_CONSOLE
#define PLAYC_LOADMASK       (PLAYC_HSSTARTPOSHALF)
#define PLAYC_SAVEMASK       (PLAYC_PAUSEALL|PLAYC_PAUSENEXT)
#else
#define PLAYC_SAVEMASK       (PLAYC_PAUSEALL|PLAYC_PAUSENEXT|PLAYC_HSSTARTPOSHALF|PLAYC_NOAUTOPRGSKIP)
#endif

//hi-lite scan defaults
#define PLAYC_HS_STARTSONG  1
#define PLAYC_HS_STARTTIME  "1:00"
#define PLAYC_HS_TIMECOUNT  "0:10"

//playreplay values
#define REPLAY_SONG   1
#define REPLAY_LIST   2

//crossfadepart values
#define CROSS_CLEAR   0
#define CROSS_OUT     1   // fade-out part running
#define CROSS_LOAD    5   // new file loading
#define CROSS_FADE    3   // crossfade
#define CROSS_IN      2   // fade-in part running

//crossfadetype values
#define CFT_FADEOUT   1   // fade out enabled
#define CFT_FADEIN    2   // fade out disabled

//intsoundconfig and intsoundcontrol function bits
#define INTSOUND_NONE      0  // no interrupt functions
#define INTSOUND_DECODER   1  // interrupt decoder
#define INTSOUND_TSR       2  // tsr mode
#define INTSOUND_FUNCTIONS (INTSOUND_DECODER|INTSOUND_TSR)
#define INTSOUND_NOINTDEC  4  // disable interrupt decoder (intsoundconfig)
#define INTSOUND_NOINT08   8  // disable int08 (intsoundconfig)
#define INTSOUND_DOSSHELL  16 // intsoundconfig:-xs intsoundcontrol:ctrl-d
#ifdef SBEMU
#define INTSOUND_NOBUSYWAIT 32
#endif
#define INTSOUND_INT08RUN  1024 // int08 process is running (has not finished)

//timer settings
#define MPXPLAY_TIMER_INT      0x08
#define INT08_DIVISOR_DEFAULT  65536
#define INT08_CYCLES_DEFAULT   (1000.0/55.0)  // 18.181818
#ifdef __DOS__
 #define INT08_DIVISOR_NEW     10375  // = 18.181818*65536 / (3 * 44100/1152)
 //#define INT08_DIVISOR_NEW      1194  // for 1ms (1/1000 sec) refresh (not recommended)
#else
 #define INT08_DIVISOR_NEW      8400  // for 7ms refresh (~130fps) (9600-> 8ms doesn't work at me)
 //#define INT08_DIVISOR_NEW      20750  // 60fps (for default Win program refresh)
#endif
#define INT08_CYCLES_NEW  (INT08_CYCLES_DEFAULT*INT08_DIVISOR_DEFAULT/INT08_DIVISOR_NEW) // 114.8 or 130

#define REFRESH_DELAY_JOYMOUSE (INT08_CYCLES_NEW/36) // 38 char/s

#ifdef MPXPLAY_WIN32
 #define MPXPLAY_INTSOUNDDECODER_DISALLOW \
  newfunc_newhandler08_waitfor_threadend();\
  funcbit_smp_int32_put(intsoundcntrl_save,intsoundcontrol);\
  funcbit_smp_disable(intsoundcontrol,INTSOUND_DECODER);
 #define MPXPLAY_INTSOUNDDECODER_ALLOW \
  if(funcbit_smp_test(intsoundconfig,INTSOUND_DECODER)) \
   funcbit_smp_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);
#else
 #define MPXPLAY_INTSOUNDDECODER_DISALLOW intsoundcntrl_save=intsoundcontrol;funcbit_disable(intsoundcontrol,INTSOUND_DECODER);
 #define MPXPLAY_INTSOUNDDECODER_ALLOW    if(intsoundconfig&INTSOUND_DECODER) funcbit_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);
#endif

// Mpxplay exit error codes
#define MPXERROR_OK                0
#define MPXERROR_SNDCARD           8
#define MPXERROR_XMS_MEM           9
#define MPXERROR_CONVENTIONAL_MEM 10
#define MPXERROR_NOFILE           11
#define MPXERROR_CANTWRITEFILE    12
#define MPXERROR_DIV0             13
#define MPXERROR_EXCEPTION        14
#define MPXERROR_UNDEFINED        15 // sometimes not error, just don't save playlists and mpxplay.ini

// Mpxplay error range begins
#define MPXPLAY_ERROR_RANGE_INFILE          0
#define MPXPLAY_ERROR_RANGE_MPXINBUF     -100
#define MPXPLAY_ERROR_RANGE_FILEHAND     -200
#define MPXPLAY_ERROR_RANGE_CFGFUNC      -300
#define MPXPLAY_ERROR_RANGE_DISKDRIV     -400
#define MPXPLAY_ERROR_RANGE_GENERAL      -800

#define MPXPLAY_ERROR_OK                   0
#define MPXPLAY_ERROR_MPXINBUF_OK          0 // no error
#define MPXPLAY_ERROR_MPXINBUF_READ_EOF -100 // can't read, because EOF
#define MPXPLAY_ERROR_MPXINBUF_READ_BUF -101 // can't read, because no data left in buffer
#define MPXPLAY_ERROR_MPXINBUF_READ_LOW -102 // can't read from file-device (but not eof yet)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_EOF -110 // invalid newfilepos (out of filesize)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_BUF -111 // newfilepos is out of buffer
#define MPXPLAY_ERROR_MPXINBUF_SEEK_LOW -112 // can't seek file-device
#define MPXPLAY_ERROR_MPXINBUF_ARGUMENT -120 // argument error

#define MPXPLAY_ERROR_CFGFUNC_INVALIDDATA     -300
#define MPXPLAY_ERROR_CFGFUNC_UNSUPPFUNC      -301
#define MPXPLAY_ERROR_CFGFUNC_ARGUMENTMISSING -302

#define MPXPLAY_ERROR_ARGS              -800

//dosshell types
#define DSH_NO        0
#define DSH_CHILD     1
#define DSH_NEWSHELL  2
#define DSH_START     3

//mpxplay_config_videoplayer_control
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DECODEVIDEO      (1 << 0)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_USEHWDECODING    (1 << 1)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DISABLESDHWDEC   (1 << 2)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DISABLESKIPFRAME (1 << 3)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_TEXBUFFORSWDECODED  (1 << 4) // pre-processed texture buffer for sw decoded video frames (sw decoder to d3d11 rendering)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_STREAMLIST_SHORT_LANGNAMES  (1 << 8) // use short language codes in video context menu ([lng] instead of Language)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_STREAMS_FORCED_DISABLE      (1 << 9) // disable auto use of forced embedded streams
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_SUBTITLES_EMBEDDED_DISABLE  (1 << 10) // disable auto use of embedded subtitles
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_USEAV1HWDECODING (1 << 20)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DEFAULT  (MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DECODEVIDEO | \
                                                    MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_USEHWDECODING | \
                                                    MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DISABLESDHWDEC)
#define MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_REOPEN_REQUIRED (MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DECODEVIDEO | \
                                                           MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_USEHWDECODING | \
                                                           MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_USEAV1HWDECODING | \
                                                           MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_DISABLESDHWDEC | \
                                                           MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_STREAMS_FORCED_DISABLE | \
                                                           MPXPLAY_CONFIG_VIDEOPLAYERCONTROL_SUBTITLES_EMBEDDED_DISABLE)  // TODO: move these to in_ffmpgdec_infile_callback

//other
#define IRQ_STACK_SIZE 16384   // size of irq (errorhand and soundcard) stacks

//wave (codec) IDs at input/output
#define MPXPLAY_WAVEID_UNKNOWN   0x0000
#define MPXPLAY_WAVEID_PCM_SLE   0x0001 // signed little endian
#define MPXPLAY_WAVEID_PCM_FLOAT 0x0003 // 32/64-bit float le
#define MPXPLAY_WAVEID_AC3       0x2000
#define MPXPLAY_WAVEID_DTS       0x2001
#define MPXPLAY_WAVEID_MP2       0x0050
#define MPXPLAY_WAVEID_MP3       0x0055
#define MPXPLAY_WAVEID_WMAV1     0x0160 // 7.0
#define MPXPLAY_WAVEID_WMAV2     0x0161 // 8.0
#define MPXPLAY_WAVEID_LATMAAC   0x1602 // AAC with LATM header
#define MPXPLAY_WAVEID_AAC       0x706D
#define MPXPLAY_WAVEID_FLAC      0xF1AC
#define MPXPLAY_WAVEID_VORBIS    (('V' << 8) + 'o') // ??? from FFmpeg
// non-standard (internal) wave-ids
#define MPXPLAY_WAVEID_PCM_ULE   0x00017000 // unsigned little endian pcm
#define MPXPLAY_WAVEID_PCM_SBE   0x00017001 // signed big endian pcm
#define MPXPLAY_WAVEID_PCM_UBE   0x00017002 // unsigned big endian pcm
#define MPXPLAY_WAVEID_PCM_DVD   0x00017005 // big endian interleaved 16 + 4/8 bits
#define MPXPLAY_WAVEID_PCM_F32BE 0x00017010 // 32-bit float big endian
#define MPXPLAY_WAVEID_PCM_F64LE 0x00017015 // 64-bit float little endian
#define MPXPLAY_WAVEID_PCM_F64BE 0x00017016 // 64-bit float big endian
#define MPXPLAY_WAVEID_SPEEX     0x00018002
#define MPXPLAY_WAVEID_OPUS      0x00018003
#define MPXPLAY_WAVEID_ALAC      0x00018005
#define MPXPLAY_WAVEID_UNSUPPORTED 0x0001FFFF // !!! higher wave-ids blocked (fails) in decoder.c (modify it if it's required)

enum mpxplay_video_player_types {
    MPXPLAY_VIDEOPLAYERTYPE_NONE = 0,
    MPXPLAY_VIDEOPLAYERTYPE_FFMPEG = 1,
    MPXPLAY_VIDEOPLAYERTYPE_EXTERNAL = 5,
    MPXPLAY_VIDEOPLAYERTYPE_QTMEDIA = 5,
    MPXPLAY_VIDEOPLAYERTYPE_MAX
};

//------------------------------------------------------------------------
//main structure definitions

typedef struct crossfade_info{
 unsigned int usecrossfade;
 unsigned int crossfadepart;
 unsigned int crossfade_out_len;
 unsigned int crossfade_in_len;
 unsigned int crossfadepoint;
 unsigned int crossfadetype;
 unsigned int crossfadebegin;
 unsigned int crossfadeend;
 unsigned int crossfadelimit;
 unsigned int crosscounter;      // counter of crosswait
 unsigned int crosswait;         // don't start crossfade (ie: at rewind)
}crossfade_info;

#include "in_file.h"
#include "au_cards\au_cards.h"
#ifdef MPXPLAY_GUI_CONSOLE
#include "display\visualpi.h"
#include "videoout\videoout.h"
#endif

#define MPXPLAY_MPXPFRAME_FLAG_OWN_SRVFFMV_CALLBACK  (1 << 0)  // parser has own dispqt_worker callback function (in_ffmpeg.c)
#define MPXPLAY_MPXPFRAME_FLAG_SRVFFMV_CALLBACK      (1 << 1)  // else we use a general callback (in_file.c)
#define MPXPLAY_MPXPFRAME_FLAG_FILEFORMAT_CONTAINER  (1 << 4)

struct mpxpframe_s {
 long frameNum;
 long allframes;
 long timesec;
 unsigned long framecounter;
 long index_start; // =0         if not indexed
 long index_end;   // =allframes if not indexed
 long index_len;   // =allframes if not indexed (avoids inline calculation)

 unsigned int filetype;     // HFT_XXX

 //file/source infos
 mpxp_filesize_t filesize;
 mpxp_filesize_t filepos;
 struct mpxplay_diskdrive_data_s *mdds;
 struct mpxplay_filehand_low_func_s *filehand_funcs;
 void *filehand_datas; // private datas of filehand_funcs
 mpxp_uint64_t readwait_endtime;

 //file-buffer variables
 unsigned int buffertype;
 char *prebufferbegin;
 long prebufferblocksize;
 long prebuffersize;   // frp->prebufferblocksize * prebufferblocks (4 or 32)
 long prebuffergetp;
 long prebufferputp;
 long prebufferbytes_rewind;
 long prebufferbytes_forward;
 mpxp_filesize_t prebuffer_seek_retry;
 struct mpxplay_filehand_buffered_func_s *filebuf_funcs; // static!

 //decoder infos
 struct mpxplay_infile_info_s *infile_infos; // !!! static! don't clear!
 struct mpxplay_infile_func_s *infile_funcs;

 //pcm (decoder,mixer,card)
 mpxp_uint8_t *pcmdec_buffer;
 unsigned int pcmdec_bufsize;
 unsigned int pcmdec_storedsamples;
 unsigned int pcmdec_leftsamples;
 mpxp_uint8_t *pcmout_buffer;
 unsigned int pcmout_bufsize;        // in bytes
 unsigned int pcmout_blocksize;      // samplenum (with ch)
 unsigned int pcmout_storedsamples;  // samplenum (with ch)

 struct mainvars *mvp;
 struct playlist_side_info *psi;
 struct playlist_entry_info *pei;
 struct mpxp_aumixer_passinfo_s *mpi;
 void *mutexhnd_frame;
 void *mutexhnd_fillbuffer;
 int  last_errorcode;
 // for internal use only (DLL API break)
 unsigned int flags;      // MPXPLAY_MPXPFRAME_FLAG_
#if defined(MPXPLAY_LINK_INFILE_FF_MPEG) && defined(MPXPLAY_GUI_QT)
 mpxplay_packetqueue_t audio_packet_queue; // for audio visualization
 mpxp_int64_t avsync_clocktime;
 mpxp_int64_t avsync_timestamp;
#endif
 struct mpxpframe_s *fro; // crosslink (fr[0]->fr[1],fr[1]->fr[0])
};

#include "playlist\playlist.h"
#include "au_mixer\au_mixer.h"

struct mainvars{
 struct mpxpframe_s *fr_base;    // pointed to fr[0]
 struct mpxpframe_s *fr_primary; // currently playing file infos (secondary is frp->fro) (flip-flop between fr[0]/fr[1])
 struct mpxpframe_s *fr_check;   // used for pre-analyze a file
 struct mpxplay_audioout_info_s *aui;
#ifdef MPXPLAY_GUI_CONSOLE
 struct mpxplay_videoout_info_s *voi;
#endif

 struct playlist_side_info *psi0;    // side 0 info
 struct playlist_side_info *psie;    // editorside info
 struct playlist_side_info *psip;    // playside info
 struct playlist_side_info *psil;    // playlist side info

 struct playlist_entry_info *pei0;    // 0. side 0. entry (base entry of static playlist and pointers of current song)
 struct playlist_entry_info *aktfilenum;// currsong entry
 struct playlist_entry_info *newfilenum;// nextsong entry

 struct playlist_entry_info *newsong; // skip to newsong absolute

 long seek_absolute;                 // ie: with mouse on songposline
 long seek_relative;                 // rewind/forward
 unsigned int seek_frames;           // rewind/forward framenum
 int  step;                          // step (skip) -1/+1 song relative
 int  direction;                     // direction of step (skip)

 int idone;                          // end of decoding
 unsigned int fdone;                 // end of file
 unsigned int adone;                 // end of file + end of decoding
 unsigned int sndempty;              // no sound (above soundlimit)
 unsigned int partselect;            // open newfile or decoding
 unsigned int foundfile;             // found 1 playable audio file (at least) in the playlist
 struct crossfade_info *cfi;
#ifdef MPXPLAY_GUI_CONSOLE
 display_visual_data_s *vds;
#endif

 unsigned int editorside_selected;  // 0: left 1:right
 unsigned int editorside_all_tabs;
 unsigned int editorsides_num_tabs[PLAYLIST_MAX_SIDES];
 unsigned int editorsides_selected_tab[PLAYLIST_MAX_SIDES];

 struct mpxp_aumixer_main_info_s mmi;
};

//----------------------------------------------------------------------
#define MPXPLAY_IS_FILE_OPEN(frp) ((frp)->filetype & HFT_FILE_ALL)

#define MPXPLAY_STOPANDCLEAR_FLAG_STOPMEDIA          (1 << 0) // stop audio/video playing (else just pause)
#define MPXPLAY_STOPANDCLEAR_FLAG_ENDWAIT            (1 << 1) // wait for end of (empty) soundcard buffer (don't stop soundcard before)
#define MPXPLAY_STOPANDCLEAR_FLAG_DELAYEDFILECLOSE   (1 << 2) // delayed file closing (use mpxplay_timer_addfunc to close file)

//mpxplay.c
extern long mpxplay_calculate_timesec_to_framenum(struct mpxpframe_s *frp,char *pst);
extern long mpxplay_calculate_index_start_end(struct mpxpframe_s *frp,struct mainvars *mvp,struct playlist_entry_info *pei); // return index_end
extern void mpxplay_stop_and_clear(struct mainvars *,unsigned int stopclearflags);
extern void mpxplay_playcontrol_pause(struct mainvars *mvp, unsigned int stopclearflags);
extern void mpxplay_playcontrol_start(struct mainvars *mvp);
extern void mpxplay_play_start_or_pause(struct mainvars *mvp);
extern int  mpxplay_newsong_aktfilenum_set(struct mainvars *mvp, struct playlist_entry_info *pei);
extern unsigned int mpxplay_check_buffers_full(struct mainvars *mvp);
extern void crossfade_part_step(struct mainvars *);
extern void crossfade_reset(struct mainvars *);
extern void mpxplay_starttimes_clear(void);
extern void mpxplay_close_program(unsigned int errorcode);

//in_file.c
extern int mpxplay_infile_decode(struct mpxplay_audioout_info_s *aui);
extern int mpxplay_infile_get_infilefuncs_by_ext(struct mpxpframe_s *frp,struct mpxplay_diskdrive_data_s *mdds,char *filename,mpxp_uint32_t openmode);
extern unsigned int mpxplay_infile_check_extension(char *filename,struct mpxplay_diskdrive_data_s *mdds);
extern int  mpxplay_infile_get_header_by_ext(struct mpxpframe_s *frp,struct mpxplay_diskdrive_data_s *mdds,char *filename,mpxp_uint32_t openmode);
extern int  mpxplay_infile_get_id3tag(struct mpxpframe_s *frp);
extern int  mpxplay_infile_write_id3tag(struct mpxpframe_s *frp,char *filename,char **id3ip,unsigned long control);
extern long mpxplay_infile_fseek(struct mpxpframe_s *frp,long newframenum);
extern mpxp_int32_t mpxplay_infile_call_control_func(struct mpxpframe_s *frp, mpxp_uint32_t funcnum, mpxp_ptrsize_t arg1, mpxp_ptrsize_t arg2);
extern int  mpxplay_infile_controlcb_modify_pei_by_filename(unsigned int cb_cmd, struct playlist_side_info *psi, struct playlist_entry_info *pei, char *filename, char *str_data, int str_len);

unsigned int mpxplay_infile_get_samplenum_per_frame(unsigned int freq);
void miis_to_frp(struct mpxplay_infile_info_s *miis,struct mpxpframe_s *frp);
#ifdef MPXPLAY_LINK_VIDEO
void mpxplay_infile_video_config_open(struct mpxplay_videoout_info_s *voi,struct mpxplay_video_decoder_info_s *vdi);
#endif
#ifdef MPXPLAY_GUI_QT
extern void mpxplay_infile_duration_set(struct mpxpframe_s *frp, mpxp_int64_t duration_ms);
extern void mpxplay_infile_framenum_refresh(struct mpxpframe_s *frp);
#endif
int  mpxplay_infile_open(struct mpxpframe_s *frp,char *filename,mpxp_uint32_t openmode);
int  mpxplay_infile_avfile_open(struct mpxpframe_s *frp,char *filename,mpxp_uint32_t openmode,unsigned long framenum);
void mpxplay_infile_suspend_close(struct mpxpframe_s *frp);
void mpxplay_infile_nodrv_close(struct mpxpframe_s *frp);
void mpxplay_infile_close(struct mpxpframe_s *frp);
void mpxplay_infile_playing_close(void);
void mpxplay_infile_reset(struct mpxpframe_s *frp);
unsigned int mpxplay_infile_realloc_pcmoutdata(struct mpxplay_audio_decoder_info_s *adi,struct mpxpframe_s *frp);

void mpxplay_infile_init(struct mainvars *mvp);
void mpxplay_infile_deinit(struct mainvars *mvp);

struct mpxpframe_s *mpxplay_infile_frame_alloc(struct mpxpframe_s *frp);
void mpxplay_infile_frame_free(struct mpxpframe_s *frp);
void *mpxplay_infile_filehandler_alloc(struct mpxplay_filehand_buffered_func_s **fbfs);
void  mpxplay_infile_filehandler_free(void *frp);

//newfunc
extern int  mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,unsigned int needbytes); // fill with a file-read
extern unsigned int newfunc_newhandler08_maincycles_init(struct mainvars *mvp,void *cycleinit,void *cycle1,void *cycle2);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_h
