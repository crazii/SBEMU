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
//Basic constants & functions
//This file is not an original Mpxplay file, but a collection of constants & functions used by AU_CARDS

#ifndef au_base_h
#define au_base_h

#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//basic types
typedef unsigned int   mpxp_bool_t;
typedef long long      mpxp_int64_t;
typedef unsigned long long mpxp_uint64_t;
typedef long           mpxp_int32_t;
typedef unsigned long  mpxp_uint32_t;
typedef short          mpxp_int16_t;
typedef unsigned short mpxp_uint16_t;
typedef signed char    mpxp_int8_t;
typedef unsigned char  mpxp_uint8_t;
typedef float          mpxp_float_t;
typedef double         mpxp_double_t;
#ifdef MPXPLAY_UTF8
typedef mpxp_uint8_t   mpxp_char_t;
typedef mpxp_uint16_t  mpxp_wchar_t;
#else
typedef char           mpxp_char_t;
typedef char           mpxp_wchar_t;
#endif
#ifdef MPXPLAY_ARCH_X64
typedef mpxp_int64_t  mpxp_ptrsize_t;
#else
typedef int           mpxp_ptrsize_t;
#endif
#ifdef MPXPLAY_FSIZE64
 typedef mpxp_int64_t  mpxp_filesize_t;
 #define __WATCOM_INT64__ 1
#else
 typedef long          mpxp_filesize_t;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef offsetof
#define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))? (a):(b))
#endif

#if defined(SBEMU) && defined(DJGPP)
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

//au cards usage
#define MPXPLAY_LINK_FULL     1

//uselfn
#define USELFN_ENABLED         1 // lfn is enabled
#define USELFN_AUTO_SFN_TO_LFN 2 // auto sfn to lfn conversion at playlists

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

#define MPXPLAY_INTSOUNDDECODER_DISALLOW intsoundcntrl_save=intsoundcontrol;funcbit_disable(intsoundcontrol,INTSOUND_DECODER);
#define MPXPLAY_INTSOUNDDECODER_ALLOW    if(intsoundconfig&INTSOUND_DECODER) funcbit_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);

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

#define MPXPLAY_TIMERTYPE_WAKEUP        0  // wake up once (delete after running)
#define MPXPLAY_TIMERTYPE_REPEAT   (1<< 0) // repeat forever (don't delete after running)
#define MPXPLAY_TIMERTYPE_INT08    (1<< 2) // run from int08 (else run from main_cycle)
#define MPXPLAY_TIMERTYPE_SIGNAL   (1<< 3) // signalling (call func at mpxplay_signal_events) (cannot use it with INT08)
#if defined(__DOS__)
#define MPXPLAY_TIMERTYPE_THREAD   MPXPLAY_TIMERTYPE_WAKEUP // under dos there's no threads
#else
#define MPXPLAY_TIMERTYPE_THREAD   (1<< 4) // execute in a separate thread
#endif

#define MPXPLAY_TIMERFLAG_MULTIPLY (1<< 6) // multiply instance (eg: same function with timer and signal, or repeated functions) (note: same functions with different datas are automatically MULTIPLY) (same functions with same datas and flags shall not have MULTIPLY flags, eg: close functions)
#define MPXPLAY_TIMERFLAG_LOWPRIOR (1<< 7) // runs in lower priority (check buffers fullness)
#define MPXPLAY_TIMERFLAG_BUSY     (1<< 8) // busy flag (no re-entrance)
#define MPXPLAY_TIMERFLAG_OWNSTACK (1<< 9) // use a separated small (32k) stack for the function (for int08 functions)
#define MPXPLAY_TIMERFLAG_STI      (1<<10) // disable maskable interrupts while function running (for int08 functions)
#define MPXPLAY_TIMERFLAG_INDOS    (1<<11) // function calls DOS routines (check indos flag before)
#define MPXPLAY_TIMERFLAG_OWNSTCK2 (1<<12) // use a separated large (256k) stack for the function (for int08 functions)
#define MPXPLAY_TIMERFLAG_MVPDATA  (1<<13) // passdata=mvp
#define MPXPLAY_TIMERFLAG_MULTCORE (1<<14) // use multicore at thread (don't set affinity to core 0)
//internal flags
#ifndef __DOS__
#define MPXPLAY_TIMERFLAG_THEXITEVENT  (1<<28) // exit from thread init
#define MPXPLAY_TIMERFLAG_THEXITDONE   (1<<29) // exit from thread done
#endif

//adi->infobits
//info
#define ADI_FLAG_FLOATOUT        (1 << 0) // float audio decoder output (else integer)
#define ADI_FLAG_FPUROUND_CHOP   (1 << 1) // roundtype at float output (else round to nearest)
#define ADI_FLAG_OWN_SPECTANAL   (1 << 2) // own spectrum analiser
#define ADI_FLAG_BITSTREAMOUT    (1 << 3) // bitstream out is enabled/supported (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_BITSTREAMNOFRH  (1 << 4) // bitstream out "no-frame-headers" is supported
#define ADI_FLAG_BITSTREAMHEAD   (1 << 5) // write a header at the begin of file (ie: AAC,OGG) (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_FLOAT64OUT      (1 << 6) // 64-bit float output
#define ADI_FLAG_FLOAT80OUT      (ADI_FLAG_FLOATOUT | ADI_FLAG_FLOAT64OUT) // 80-bit float output ???
#define ADI_FLAG_FLOATMASK       (ADI_FLAG_FLOATOUT | ADI_FLAG_FLOAT64OUT)
//control
#define ADI_CNTRLBIT_DECODERSPECINF  (1 <<  7) // bitstream (init) contains a decoder specific info
#define ADI_CNTRLBIT_BITSTREAMOUT    (1 <<  8) // bitstream out (don't decode) (write into file or soundcard decoding) (except: APE,CDW,MPC,WAV)
#define ADI_CNTRLBIT_BITSTREAMNOFRH  (1 <<  9) // cut frame headers (usually 4 bytes) with the frame (so don't cut it) (usually: file out with header, soundcard out without header)
#define ADI_CNTRLBIT_SILENTBLOCK     (1 << 10) // frame contains no sound (to disable soundlimitvol)

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1
#define MIXER_SETMODE_RESET    2

#define MIXER_SCALE_BITS    16 // used bits in integer based (lq) functions and default scale bits in hq functions
#define MIXER_SCALE_VAL  65536
#define MIXER_SCALE_MIN -32768
#define MIXER_SCALE_MAX  32767

#define REFRESH_DELAY_JOYMOUSE (INT08_CYCLES_NEW/36) // 38 char/s

//newfunc
//other events (not used at TIMERTYPE_SIGNAL)
#define MPXPLAY_SIGNALTYPE_REALTIME      (1 <<12) // realtime (delay=0) function is in the timer (don't use cpu-halt)
#define MPXPLAY_SIGNALTYPE_DISKACCESS    (1 <<13) // file read/write
#define MPXPLAY_SIGNALTYPE_DISKDRIVRUN   (1 <<14) // diskdrive (open) is running
#define MPXPLAY_SIGNALTYPE_DISKDRIVTERM  (1 <<15) // terminate diskdrive (open)
#define MPXPLAY_SIGNALMASK_OTHER     (MPXPLAY_SIGNALTYPE_REALTIME|MPXPLAY_SIGNALTYPE_DISKACCESS|MPXPLAY_SIGNALTYPE_DISKDRIVTERM)

#define funcbit_test(var,bit)       ((var)&(bit))
#define funcbit_enable(var,bit)     ((var)|=(bit))
#define funcbit_disable(var,bit)    ((var)&=~(bit))
#define funcbit_inverse(var,bit)    ((var)^=(bit))
//#define funcbit_copy(var1,var2,bit) ((var1)|=(var2)&(bit))
#define funcbit_copy(var1,var2,bit) ((var1)=((var1)&(~(bit)))|((var2)&(bit)))

// note LE: lowest byte first, highest byte last
#define PDS_GETB_8S(p)   *((volatile mpxp_int8_t *)(p))               // signed 8 bit (1 byte)
#define PDS_GETB_8U(p)   *((volatile mpxp_uint8_t *)(p))              // unsigned 8 bit (1 byte)
#define PDS_GETB_LE16(p) *((volatile mpxp_int16_t *)(p))              // 2bytes LE to short
#define PDS_GETB_LEU16(p)*((volatile mpxp_uint16_t *)(p))             // 2bytes LE to unsigned short
#define PDS_GETB_BE16(p) pds_bswap16(*((volatile mpxp_uint16_t *)(p)))// 2bytes BE to unsigned short
#define PDS_GETB_LE32(p) *((volatile mpxp_int32_t *)(p))              // 4bytes LE to long
#define PDS_GETB_LEU32(p) *((volatile mpxp_uint32_t *)(p))            // 4bytes LE to unsigned long
#define PDS_GETB_BE32(p) pds_bswap32(*((volatile mpxp_uint32_t *)(p)))// 4bytes BE to unsigned long
#define PDS_GETB_LE24(p) ((PDS_GETB_LEU32(p))&0x00ffffff)
#define PDS_GETB_BE24(p) ((PDS_GETB_BE32(p))>>8)
#define PDS_GETB_LE64(p) *((volatile mpxp_int64_t *)(p))              // 8bytes LE to int64
#define PDS_GETB_LEU64(p) *((volatile mpxp_uint64_t *)(p))            // 8bytes LE to uint64
#define PDS_GETB_BEU64(p) ((((volatile mpxp_uint64_t)PDS_GETB_BE32(p))<<32)|((mpxp_uint64_t)PDS_GETB_BE32(((volatile mpxp_uint8_t *)(p)+4))))
#define PDS_GETBD_BEU64(d,p) *(((volatile mpxp_uint32_t *)(d))+1)=PDS_GETB_BE32(p); *((volatile mpxp_uint32_t *)(d))=PDS_GETB_BE32(((volatile mpxp_uint32_t *)(p))+1)
#define PDS_GET4C_LE32(a,b,c,d) ((mpxp_uint32_t)(a) | ((mpxp_uint32_t)(b) << 8) | ((mpxp_uint32_t)(c) << 16) | ((mpxp_uint32_t)(d) << 24))
#define PDS_GETS_LE32(p) ((char *)&(p))                    // unsigned long to 4 bytes string

#define PDS_PUTB_8S(p,v)   *((volatile mpxp_int8_t *)(p))=(v)               //
#define PDS_PUTB_8U(p,v)   *((volatile mpxp_uint8_t *)(p))=(v)              //
#define PDS_PUTB_LE16(p,v) *((volatile mpxp_int16_t *)(p))=(v)              //
#define PDS_PUTB_LEU16(p,v) *((volatile mpxp_uint16_t *)(p))=(v)            //
#define PDS_PUTB_BEU16(p,v) *((volatile mpxp_uint16_t *)(p))=pds_bswap16((v))//
#define PDS_PUTB_LE24(p,v) *((volatile mpxp_uint8_t *)(p))=((v)&0xff); PDS_PUTB_LE16(((mpxp_uint8_t*)p+1),((v)>>8))
#define PDS_PUTB_LE32(p,v) *((volatile mpxp_int32_t *)(p))=(v)              // long to 4bytes LE
#define PDS_PUTB_BEU32(p,v) *((volatile mpxp_uint32_t *)(p))=pds_bswap32((v)) // long to 4bytes BE
#define PDS_PUTB_LE64(p,v) *((volatile mpxp_int64_t *)(p))=(v)              // int64 to 8bytes LE
#define PDS_PUTB_BEU64(p,v) *((volatile mpxp_uint32_t *)(p)+1)=pds_bswap32((v)&0xffffffff); *((mpxp_uint32_t *)(p))=pds_bswap32((mpxp_uint64_t)(v)>>32)

#define PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN '\\'
#define PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP '/'
#define PDS_DIRECTORY_SEPARATOR_STR_DOSWIN  "\\"
#define PDS_DIRECTORY_SEPARATOR_STR_UNXFTP  "/"

#if defined(__DOS__) || defined(WIN32) || defined(_WIN32) || defined(MPXPLAY_WIN32)
 #define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
 #define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
 #define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_DOSWIN
 #define PDS_DIRECTORY_ROOTDIR_STR "c:\\"
#else // Linux
 #define PDS_DIRECTORY_SEPARATOR_CHAR PDS_DIRECTORY_SEPARATOR_CHAR_UNXFTP
 #define PDS_DIRECTORY_SEPARATOR_CHRO PDS_DIRECTORY_SEPARATOR_CHAR_DOSWIN
 #define PDS_DIRECTORY_SEPARATOR_STR  PDS_DIRECTORY_SEPARATOR_STR_UNXFTP
 #define PDS_DIRECTORY_ROOTDIR_STR "c:/"
#endif

#define funcbit_smp_test(var,bit)       funcbit_test(var,bit)
#define funcbit_smp_enable(var,bit)     funcbit_enable(var,bit)
#define funcbit_smp_disable(var,bit)    funcbit_disable(var,bit)
#define funcbit_smp_inverse(var,bit)    funcbit_inverse(var,bit)
#define funcbit_smp_copy(var1,var2,bit) funcbit_copy(var1,var2,bit)
#define funcbit_smp_int32_get(var)       (var)
#define funcbit_smp_int64_get(var)       (var)
#define funcbit_smp_pointer_get(var)     (var)
#define funcbit_smp_int32_put(var,val)   (var)=(val)
#define funcbit_smp_int64_put(var,val)   (var)=(val)
#ifdef MPXPLAY_FSIZE64
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int64_put(var,val)
#else
#define funcbit_smp_filesize_put(var,val) funcbit_smp_int32_put(var,val)
#endif
#define funcbit_smp_pointer_put(var,val) (var)=(val)
#define funcbit_smp_int32_increment(var) var++
#define funcbit_smp_int32_decrement(var) var--

typedef struct dosmem_t{
 unsigned short selector;
 unsigned short segment;
 char *linearptr;
}dosmem_t;

//for DOS interrupt callings
typedef struct rminfo{
 long EDI;
 long ESI;
 long EBP;
 long reserved_by_system;
 long EBX;
 long EDX;
 long ECX;
 long EAX;
 short flags;
 short ES,DS,FS,GS,IP,CS,SP,SS;
}rminfo;

#if defined(DJGPP)
#include <sys/segments.h>
#include <sys/movedata.h>

typedef struct xmsmem_t{
 unsigned short remap;
 unsigned short xms;
 unsigned long handle;
 char *physicalptr;
 char *linearptr;
}xmsmem_t;
typedef xmsmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem)->physicalptr + ((char*)(ptr) - (cardmem)->linearptr))

#define far
#define __far
#define __interrupt
#define __loadds
typedef struct _farptr {unsigned long off;unsigned short sel;} farptr;//DJGPP has no 16:32 flat model far ptr.
typedef farptr int_handler_t;
static farptr pds_fardata(void* d) { farptr ptr = {(long)d, _my_ds()}; return ptr;}
static void pds_call_int_handler(int_handler_t h) { asm("pushf\n\t lcall *%0" ::"m"(h));} //simulate a iret frame for handler
static int_handler_t pds_int_handler(void (*f)(void)) { int_handler_t h = {(long)f, _my_cs()}; return h;}
static int pds_valid_int_handler(int_handler_t h) {return h.off && h.sel;}

extern farptr pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, farptr vect);
extern farptr pds_dos_getvect(unsigned int intno);
extern void pds_dos_setvect(unsigned int intno, farptr vect);
extern int  pds_dpmi_xms_allocmem(xmsmem_t *,unsigned int size);
extern void pds_dpmi_xms_freemem(xmsmem_t *);
#define dosput(d, s, l) dosmemput(s, l, (uintptr_t)(d))
#define dosget(d, s, l) dosmemget((uintptr_t)(s), l, d)

#else //__WATCOMC__

#define far
#define __far
#define __interrupt
#define __loadds //TODO
typedef dosmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) (ptr)
typedef void* far farptr;
typedef void (__far __interrupt *int_handler_t)();
#define pds_fardata(d) (d)
static void pds_call_int_handler(int_handler_t h) {h();}
#define pds_int_handler(f) (f)
#define pds_valid_int_handler(h) (h)
extern void far *pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, void far *vect);
extern void far *pds_dos_getvect(unsigned int intno);
extern void pds_dos_setvect(unsigned int intno, void far *vect);
#define dosput(d, s, l) memcpy(d, s, l)
#define dosget(d, s, l) memcpy(d, s, l)

#endif//DJGPP

extern int  pds_dpmi_dos_allocmem(dosmem_t *,unsigned int size);
extern void pds_dpmi_dos_freemem(dosmem_t *);
extern void pds_dpmi_realmodeint_call(unsigned int intnum,struct rminfo *rmi);
extern unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize);
extern void pds_dpmi_unmap_physycal_memory(unsigned long linear_address);
#define pds_dpmi_rmi_clear(rmi) pds_memset(rmi,0,sizeof(struct rminfo))

extern unsigned int pds_strcpy(char *,char *); // returns the length of string!
extern unsigned int pds_strmove(char *dest,char *src); // returns the length of string
extern unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen); // returns the length of string!
extern unsigned int pds_strcat(char *,char *); // returns the lenght of string!
extern int  pds_strcmp(char *strp1,char *strp2);
extern int  pds_stricmp(char *,char *);
extern unsigned int pds_stri_compare(char *strp1,char *strp2); // returns 1 if equal, else 0 (no pointer check!)
extern int  pds_strricmp(char *,char *);
extern int  pds_strlicmp(char *,char *);
extern int  pds_strncmp(char *,char *,unsigned int);
extern int  pds_strnicmp(char *strp1,char *strp2,unsigned int counter);
extern unsigned int pds_strlen(char *strp);
extern unsigned int pds_strlenc(char *strp,char seek);
extern char *pds_strchr(char *,char);
extern char *pds_strrchr(char *,char);
extern char *pds_strnchr(char *strp,char seek,unsigned int len);
extern char *pds_strstr(char *s1,char *s2);
extern char *pds_strstri(char *s1,char *s2);
extern unsigned int pds_strcutspc(char *src); // returns (new)len
extern char *pds_getfilename_from_fullname(char *fullname);

#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_qmemreset(t,l)  memset(t,0,(l)*4))  // TODO: replace pds_qmem functions with normal mem based one everywhere
#define pds_qmemcpy(t,s,l)  memcpy(t,s,(l)*4)
#define pds_qmemcpyr(t,s,l) memmove(t,s,(l)*4)
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
extern void pds_memxch(char *,char *,unsigned int);
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_zalloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void *pds_realloc(void *bufptr,unsigned int bufsize);
extern void pds_free(void *bufptr);

extern unsigned long pds_gettimeh(void); // clock time in hsec
extern mpxp_int64_t pds_gettimem(void);  // clock time in msec
extern mpxp_int64_t pds_gettimeu(void);  // clock time in usec
extern void pds_delay_10us(unsigned int ticks);
extern void pds_delay_1695ns (unsigned int ticks); //each tick is approximately 1695ns
extern void pds_mdelay(unsigned long msec);
#define pds_textdisplay_printf(text) { fprintf(stdout, "%s", (text)); fprintf(stdout, "\n"); fflush(stdout); }
//-newfunc

//in_file
typedef struct mpxplay_audio_decoder_info_s{
 void *private_data;         // decoder
 //info
 unsigned int infobits;      // flags
 unsigned int wave_id;       // audio type (wav:0x0001,mp3:0x0055,ac3:0x2000)
 unsigned int freq;          // frequency (44100,48000,...)
 unsigned int filechannels;  // number of channels in file
 unsigned int outchannels;   // decoded (used) channels, comes out from the decoder
 mpxp_uint8_t *chanmatrix;   // output channel matrix (ie: 5.1)
 unsigned int bits;          // 8,16 ... (scalebits at float,filebits at integer output)
 unsigned int bytespersample;// used in au_mixer
 unsigned int bitrate;       // in kbit/s (lossy formats)(ie: mp3,ogg,ac3,dts)
 unsigned int pcm_framelen;  // comes out from the decoder (samplenum without channels!)(=bytes/bytespersample)
 mpxp_float_t replaygain;    // not used yet
 char *shortname;            // set by decoder (3 chars), can static ("MP3","OGG","AC3","WMA")
 char *bitratetext;          // set by decoder (8 chars), else displays bitrate (if exists) or bits
 char *freqtext;             // set by decoder (7 chars), else displays freq
 char *channeltext;          // set by decoder (8 chars), can static ("msStereo","i-Stereo","DualChan","c-Stereo","5.1 chan") else displays 1ch->"mono",2ch->"stereo",Nch->"n-chan")
 //control
 unsigned int  channelcfg;    // configure output channels (eq to channelmode)
 //pcm out
 mpxp_uint8_t *pcm_bufptr;    // head ptr in pcm_buffer
 unsigned int  pcm_samplenum; // got back from decoder (with ch)
 unsigned char reserved[5*4];
}mpxplay_audio_decoder_info_s;// 24*4=96 bytes
//-in_file

typedef mpxp_int32_t  PCM_CV_TYPE_I;  // max. 32 bit input/output pcm format (integer)
typedef mpxp_uint32_t PCM_CV_TYPE_UI; //
typedef mpxp_float_t  PCM_CV_TYPE_F;  // 32-bit float part of mixer
typedef mpxp_int16_t  PCM_CV_TYPE_S;  // 16-bit integer part of mixer
typedef mpxp_int8_t   PCM_CV_TYPE_C;  // 8-bit signed
typedef mpxp_uint8_t  PCM_CV_TYPE_UC; // 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;// largest datatype (currently)

//bits
extern void cv_bits_n_to_m(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_bytespersample);
//channels
extern unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int newchannels,unsigned int bytespersample);
//sample rates
unsigned int mixer_speed_lq(PCM_CV_TYPE_S* dest, unsigned int destsample, const PCM_CV_TYPE_S* source, unsigned int sourcesample, unsigned int channels, unsigned int samplerate, unsigned int newrate);

#ifdef MPXPLAY_USE_DEBUGF

#ifndef MPXPLAY_DEBUG_OUTPUT
#define MPXPLAY_DEBUG_OUTPUT stdout
#endif

#include <stdarg.h>
#include <time.h>
#include "serial.h"

#if !defined(MPXPLAY_USE_DEBUGMSG) && !defined(MPXPLAY_USE_WARNINGMSG) && defined(__GNUC__) && ((__STDC_VERSION__ >= 199901L) || defined(__cplusplus))
#define MPXPLAY_EXTENDED_DEBUG
#endif

#ifdef MPXPLAY_USE_DEBUGMSG
extern void display_static_message(unsigned int linepos,unsigned int blink,char *msg);
#endif
#ifdef MPXPLAY_USE_WARNINGMSG
extern void display_warning_message(char *message);
#endif

#ifdef MPXPLAY_EXTENDED_DEBUG
static inline void mpxplay_debug_create_timestamp(char *outbuf, unsigned int bufsize)
{
 mpxp_int64_t timemsec;
 time_t timesec;
 struct tm *t;

 timemsec = pds_gettimem();
 timesec = timemsec / 1000;
 t = localtime(&timesec);

 if(t)
 {
  snprintf(outbuf, bufsize, "%2.2d:%2.2d:%2.2d.%3.3d", t->tm_hour, t->tm_min, t->tm_sec, (int)(timemsec % 1000));
 }
 else
 {
  outbuf[0] = 0;
 }
}
#endif

#ifdef MPXPLAY_EXTENDED_DEBUG
static void mpxplay_debug_f(FILE *fp, const char *srcfile, int linenum, const char *format, ...)
#else
static void mpxplay_debugf(FILE *fp, const char *format, ...)
#endif
{
 va_list ap;
 char sout[4096];
#ifdef MPXPLAY_EXTENDED_DEBUG
 char timestamp[64];
#endif

 va_start(ap,format);
 vsnprintf(sout, 4096, format, ap);
 va_end(ap);

 if (ser_puts(sout)) {
     ser_puts("\n");
     return;
 }

 if(fp){
#ifdef MPXPLAY_EXTENDED_DEBUG
  mpxplay_debug_create_timestamp(timestamp, sizeof(timestamp));
  fprintf(fp,"%s %-12s %4d %s\n",timestamp, pds_getfilename_from_fullname((char *)srcfile), linenum, sout);
#else
  fprintf(fp,"%s\n",sout);
#endif
  fflush(fp);
 }else{
#ifdef MPXPLAY_USE_DEBUGMSG
  display_static_message(1,0,sout);
#elif defined(MPXPLAY_USE_WARNINGMSG)
  display_warning_message(sout);
#elif MPXPLAY_LINKMODE_DLL
  fprintf(stdout, sout);
  fprintf(stdout, "\n");
  fflush(stdout);
#elif MPXPLAY_GUI_CONSOLE
  pds_textdisplay_printf(sout);
#endif
 }
}

#ifdef MPXPLAY_EXTENDED_DEBUG
#define mpxplay_debugf(fp, format, ...) mpxplay_debug_f(fp, __FILE__, __LINE__, format, ## __VA_ARGS__)
#endif

#else
 #define mpxplay_debugf(...)
#endif// MPXPLAY_USE_DEBUGF

#ifdef __cplusplus
}//extern "C"
#endif

#endif//au_base_h