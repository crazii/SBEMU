//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
// in-file and decoder definitions

#ifndef mpxplay_in_file_h
#define mpxplay_in_file_h

#include "newfunc\newfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MPXPLAY_LINK_ORIGINAL_FFMPEG
 #define MPXPLAY_LINK_INFILE_FF_MPEG 1 // ffmpeg library parser
 #define MPXPLAY_LINK_INFILE_OGG  1
#elif defined(MPXPLAY_LINK_FULL)
 #define MPXPLAY_LINK_INFILE_AAC  1
 #define MPXPLAY_LINK_INFILE_AC3  1
 #define MPXPLAY_LINK_INFILE_FLAC 1
 #define MPXPLAY_LINK_INFILE_WAVPACK 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_INFILE_APE  1
 #define MPXPLAY_LINK_INFILE_MPX  1
 #define MPXPLAY_LINK_INFILE_MPC  1
 #endif

 #define MPXPLAY_LINK_INFILE_ASF  1
 #define MPXPLAY_LINK_INFILE_AVI  1
 #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_OGG  1
 #define MPXPLAY_LINK_INFILE_WAV  1
 #define MPXPLAY_LINK_INFILE_FFMPG 1 // MKV,MPG,TS

 #define MPXPLAY_LINK_DECODER_AAC  1
 #define MPXPLAY_LINK_DECODER_AC3  1
 #define MPXPLAY_LINK_DECODER_ALAC 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_DECODER_MPX  1
 #endif
 #define MPXPLAY_LINK_DECODER_VORBIS 1

 #define MPXPLAY_LINK_DECODER_FFMPG 1 // FLAC,WMA
 #define MPXPLAY_LINK_DECODER_FLAC 1  //
 #define MPXPLAY_LINK_DECODER_WMA  1  //

 //#define MPXPLAY_LINK_VIDEO        1
#else
 #define MPXPLAY_LINK_INFILE_AAC  1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_INFILE_MPX  1
 #endif
 #define MPXPLAY_LINK_INFILE_MPC  1

 #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_OGG  1
 #define MPXPLAY_LINK_INFILE_WAV  1

 #define MPXPLAY_LINK_DECODER_AAC 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_DECODER_MPX 1
 #endif
 #define MPXPLAY_LINK_DECODER_VORBIS 1
#endif

typedef float MPXPLAY_PCMOUT_FLOAT_T;
typedef short MPXPLAY_PCMOUT_INT16_T;

#define MPXPLAY_TIME_BASE 1000 // ms timebase/resolution

// header filetypes in pei->entrytype and frp->filetype
#define HFT_NULL          0
#define HFT_DFT       (1<<0)  // directory & playlist
#define HFT_FILE_INT  (1<<1)  // file handling (internal by Mpxplay)
#define HFT_FILE_EXT  (1<<2)  // non-built-in file handling (QT video)
#define HFT_STREAM    (1<<3)  // live stream (non-seekable, endless)
#define HFT_FILE_DLL  (1<<4)  // file handling (via DLL)  // FIXME: out of pei->entrytype (not problem yet)
#define HFT_FILE_BUILTIN  (HFT_FILE_INT | HFT_FILE_DLL)  // built-in filetype
#define HFT_FILE_ALL      (HFT_FILE_BUILTIN | HFT_FILE_EXT)

#define MPXPLAY_HFT_PUT(mph) ((unsigned long)(mph)<<28) // put HFT number to pei->entrytype (!!! max (1<<3) )
#define GET_HFT(mph) ((mph)>>28) // get HFT number from pei->entrytype

//adi->channelcfg (mp3,ogg)
#define CHM_LEFT    0  // -cl
#define CHM_STEREO  1  // normal (default)
#define CHM_DOWNMIX 2  // -cm (stereo to mono)
#define CHM_USERCNTRL_MASK (CHM_LEFT|CHM_STEREO|CHM_DOWNMIX)

//clearbuff seektypes
#define MPX_SEEKTYPE_NORM   1 // seek in file
#define MPX_SEEKTYPE_BOF    2 // seek to begin of file (reset buffers)
#define MPX_SEEKTYPE_PAUSE  4 // seek in pause mode (can be used with BOF and NORM type)
#define MPX_SEEKTYPES_CLEARBUF (MPX_SEEKTYPE_NORM|MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)
//fseek seektypes
//#define MPX_SEEKTYPE_TIMEMPOS 8  // seek to timepos (in millisecs)
#define MPX_SEEKTYPE_VIDEO    16 // seek to video (key)frame (else seek to audio frame)
#define MPX_SEEKTYPE_FRAME    32 // seek to (video) frame (usually with relative seek)
#define MPX_SEEKTYPE_RELATIVE 64 // -/+ value (else absolute value)
#define MPX_SEEKTYPE_BACKWARD 128 // else forward
#define MPX_SEEKTYPE_PRECISE  256 // not implemented yet
#define MPX_SEEKTYPES_FSEEK   (MPX_SEEKTYPE_VIDEO|MPX_SEEKTYPE_FRAME|MPX_SEEKTYPE_RELATIVE|MPX_SEEKTYPE_BACKWARD|MPX_SEEKTYPE_PRECISE)

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

// for displaying
#define MPXPLAY_ADITEXTSIZE_LONGNAME  8
#define MPXPLAY_ADITEXTSIZE_SHORTNAME 3
#define MPXPLAY_ADITEXTSIZE_BITRATE   8
#define MPXPLAY_ADITEXTSIZE_FREQ      7
#define MPXPLAY_ADITEXTSIZE_CHANNEL   8

// adi->chanmatrix, aucards_card_channelmap
#define MPXPLAY_PCMOUTCHAN_FRONT_LEFT    0
#define MPXPLAY_PCMOUTCHAN_FRONT_RIGHT   1
#define MPXPLAY_PCMOUTCHAN_FRONT_CENTER  2
#define MPXPLAY_PCMOUTCHAN_REAR_LEFT     3
#define MPXPLAY_PCMOUTCHAN_REAR_RIGHT    4
#define MPXPLAY_PCMOUTCHAN_LFE           5
#define MPXPLAY_PCMOUTCHAN_REAR_CENTER   6
#define MPXPLAY_PCMOUTCHAN_SIDE_LEFT     7
#define MPXPLAY_PCMOUTCHAN_SIDE_RIGHT    8
#define MPXPLAY_PCMOUTCHAN_FCENTER_LEFT  9
#define MPXPLAY_PCMOUTCHAN_FCENTER_RIGHT 10
#define MPXPLAY_PCMOUTCHAN_DISABLED      126
#define MPXPLAY_PCMOUTCHAN_MAX           11

// infile/decoder error codes
#define MPXPLAY_ERROR_INFILE_OK           0
#define MPXPLAY_ERROR_INFILE_NODATA      -1 // no input data
#define MPXPLAY_ERROR_INFILE_EOF         -2 // end of file (finish playing)
#define MPXPLAY_ERROR_INFILE_SYNCLOST    -3 // sync lost
#define MPXPLAY_ERROR_INFILE_SYNC_IN     -4 // decoder is in sync state (after seeking, or locking frp mutex)
#define MPXPLAY_ERROR_INFILE_RESYNC      -5 // decoder needs resync (seek to next frame)(at wrong bitstream)
#define MPXPLAY_ERROR_INFILE_CANTOPEN   -10 // can't open file as media
#define MPXPLAY_ERROR_INFILE_MEMORY     -11 // memory exhausted
#define MPXPLAY_ERROR_INFILE_FILEOPEN   -12 // can't open low level file
#define MPXPLAY_ERROR_INFILE_FILETYPE   -13 // file has invalid type (no extension)
#define MPXPLAY_ERROR_INFILE_SPIBUFSIZE -14 // mpxplay_streampacket_info bufsize is small
#define MPXPLAY_ERROR_INFILE_READONLYFS -20 // read only filesystem / filetype
#define MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE   -21 // not supported filetype (dts,wav)
#define MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE    -22 // not supported tagtype (APEv2)
#define MPXPLAY_ERROR_INFILE_WRITETAG_NOSPACE    -23 // not enough space to store id3 infos
#define MPXPLAY_ERROR_INFILE_WRITETAG_NOSPMMRQ   -24 // no space, move-meta request
#define MPXPLAY_ERROR_INFILE_WRITETAG_UNKNOWN    -29 // unknown error

#define MPXPLAY_WRITETAG_CNTRL_TRIMTAGS (1<<0) // trim/cut tags if there's not enough space to store them
#define MPXPLAY_WRITETAG_CNTRL_DUPFILE  (1<<1) // duplicate file and write all tags
#define MPXPLAY_WRITETAG_CNTRL_MOVEMETA (1<<2) // move metadata to the end of file (MP4 only)
#define MPXPLAY_WRITETAG_CNTRL_NOPADD   (1<<9) // don't add padding (vorbiscomment)

// for text (char) conversion
#ifndef MPXPLAY_UTF8
#define ID3TEXTCONV_CODEPAGE   1 // convert codepage to another (winchars/doschars)
#define ID3TEXTCONV_UTF16      2 // decode UTF-16 (unicode) strings
#define ID3TEXTCONV_UTF8       4 // decode UTF-8 strings (in every tags/filetypes)
#define ID3TEXTCONV_UTF_ALL    (ID3TEXTCONV_UTF16|ID3TEXTCONV_UTF8)
#define ID3TEXTCONV_CVTYPE_ALL (ID3TEXTCONV_CODEPAGE|ID3TEXTCONV_UTF_ALL)
#define ID3TEXTCONV_UTF_AUTO   8 // auto utf16 and utf8 decoding (ID3v2,Ogg,APEv2)
#define ID3TEXTCONV_VALIDATE  16 // validate (use only) Winchars at UTF-8 decoding (if unicode>=128)
#define ID3TEXTCONV_FILENAME  32 // convert chars in filenames too (in playlists)
#define ID3TEXTCONV_GET_WINCP 64 // get source and target codepages from windows (ansi and oem)
#define ID3TEXTCONV_CP_BACK  128 // tag-list has written in DOS (-8b)
#endif

#define MPXPLAY_TEXTCONV_TYPES_PUT(srctype,desttype) (((desttype)<<4)|(srctype))
#define MPXPLAY_TEXTCONV_TYPES_GET_SRC(type)  ((type)&0x0f)
#define MPXPLAY_TEXTCONV_TYPES_GET_DEST(type) (((type)>>4)&0x0f)
#define MPXPLAY_TEXTCONV_TYPE_CHAR       0  // FIXME: should start with 1 (at next API break)
#define MPXPLAY_TEXTCONV_TYPE_UTF8       1
#define MPXPLAY_TEXTCONV_TYPE_UTF16LE    2
#define MPXPLAY_TEXTCONV_TYPE_UTF16BE    3
#define MPXPLAY_TEXTCONV_TYPE_MPXPLAY   15
#define MPXPLAY_TEXTCONV_TYPEFLAG_HTML  (1 << 8)
#ifdef MPXPLAY_UTF8
 #define MPXPLAY_TEXTCONV_TYPE_MAX        3
 #define MPXPLAY_TEXTCONV_TYPE_MPXNATIVE MPXPLAY_TEXTCONV_TYPE_UTF8 // should use internally only
#else
 #define MPXPLAY_TEXTCONV_TYPE_CP_IN      4 // should use internally only
 #define MPXPLAY_TEXTCONV_TYPE_CP_OUT     5 // should use internally only
 #define MPXPLAY_TEXTCONV_TYPE_MAX        5
 #define MPXPLAY_TEXTCONV_TYPE_MPXNATIVE MPXPLAY_TEXTCONV_TYPE_CHAR
#endif
#define MPXPLAY_TEXTCONV_MAX_ID3LEN  MAX_ID3LEN

// id3info index values
#define I3I_ARTIST     1
#define I3I_TITLE      0
#define I3I_ALBUM      2
#define I3I_YEAR       3
#define I3I_COMMENT    4
#define I3I_GENRE      5
#define I3I_TRACKNUM   6
#define I3I_MAX        6
#define I3I_DFT_STORE  I3I_TITLE  // store directory names (<DRIVE>,[sublist]) in I3I_TITLE
#define I3I_INVALID    15

//low level file handling -> buffered file handling -> demux (infile) -> decoding

typedef struct mpxplay_filehand_low_data_s{
 int  filept;
 mpxp_filesize_t filesize;
 mpxp_filesize_t filepos;
 unsigned char reserved[4];
}mpxplay_filehand_low_data_s; // 4*4=16 bytes

//low level file handling (normal-file, cdw, ftp-file-source)
typedef struct mpxplay_filehand_low_func_s{
 int   (*check) (char *filename);     // true (1) or false (0) (check filename (ie:filename begins with "http:\\"), use these functions if 'true')
 void *(*open_read) (char *filename); // returns filehand_data
 void *(*open_write)(char *filename); // returns filehand_data
 void *(*open_create)(char *filename);// returns filehand_data
 void  (*close) (void *filehand_data);
 long  (*read)  (void *filehand_data,char *buf,unsigned int len);
 long  (*write) (void *filehand_data,char *buf,unsigned int len);
 mpxp_filesize_t (*seek)(void *filehand_data,mpxp_filesize_t offset,int fromwhere);
 mpxp_filesize_t (*tell)(void *filehand_data);
 mpxp_filesize_t (*filelength)(void *filehand_data);
 int   (*eof)   (void *filehand_data);
 int   (*chsize)(void *filehand_data,mpxp_filesize_t offset);
}mpxplay_filehand_low_func_s;

// buffered file handling
typedef struct mpxplay_filehand_buffered_func_s{
 unsigned int (*fopen)(void *fbds,char *filename,unsigned long openmode,unsigned long pb_blocksize);
 void  (*fclose) (void *fbds);
 unsigned long (*fread)(void *fbds,void *buf,unsigned long len);
 long  (*fwrite) (void *fbds,void *buf,unsigned long len);
 mpxp_filesize_t (*fseek)(void *fbds,mpxp_filesize_t offset,int whence);
 mpxp_filesize_t (*ftell)(void *fbds);
 mpxp_filesize_t (*filelength)(void *fbds);
 int (*eof)(void *fbds); // don't modify it to feof
 int (*chsize) (void *fbds,mpxp_filesize_t offset);

 mpxp_int32_t  (*get_bufbytes)(void *fbds,char *destbuf,mpxp_int32_t bytes);
 mpxp_uint32_t (*get_byte)(void *fbds);
 mpxp_uint32_t (*get_le16)(void *fbds);
 mpxp_uint32_t (*get_le24)(void *fbds);
 mpxp_uint32_t (*get_le32)(void *fbds);
 mpxp_uint64_t (*get_le64)(void *fbds);
 mpxp_uint32_t (*get_be16)(void *fbds);
 mpxp_uint32_t (*get_be24)(void *fbds);
 mpxp_uint32_t (*get_be32)(void *fbds);
 mpxp_uint64_t (*get_be64)(void *fbds);

 long (*put_byte)(void *fbds,unsigned int data);
 long (*put_le16)(void *fbds,unsigned int data);
 long (*put_le24)(void *fbds,mpxp_uint32_t data);
 long (*put_le32)(void *fbds,mpxp_uint32_t data);
 long (*put_le64)(void *fbds,mpxp_uint64_t data);
 long (*put_be16)(void *fbds,unsigned int data);
 long (*put_be24)(void *fbds,mpxp_uint32_t data);
 long (*put_be32)(void *fbds,mpxp_uint32_t data);
 long (*put_be64)(void *fbds,mpxp_uint64_t data);
}mpxplay_filehand_buffered_func_s;

// *************************************************************************
// audio decoder

#define MPXPLAY_SPI_STREAMTYPE_UNKNOWN  0
#define MPXPLAY_SPI_STREAMTYPE_AUDIO    1
#define MPXPLAY_SPI_STREAMTYPE_VIDEO    2
#define MPXPLAY_SPI_STREAMTYPE_SUBTITLE 3
#define MPXPLAY_SPI_STREAMTYPE_DATA     4

#define MPXPLAY_SPI_FLAG_NEED_DECODER   (1 <<  0) // search for a decoder by wave_id or fourcc
#define MPXPLAY_SPI_FLAG_NEED_PARSING   (1 <<  1) // parse audio infos by adf->parse_frame (get informations from the stream, passed in spi->bitstreambuf)
#define MPXPLAY_SPI_FLAG_CONTAINER      (1 <<  2) // stream data is in container (run a longer parsing)
#define MPXPLAY_SPI_FLAG_BS_ERROR       (1 <<  4) // bitstream error detected in demuxer
#define MPXPLAY_SPI_FLAG_SILENTBLOCK    (1 <<  5) // pcm frame contains no sound (to disable soundlimitvol)

#define MPXPLAY_SPI_MINBSREADSIZE          32 // ???
#define MPXPLAY_SPI_MAXBSREADSIZE_AUDIO 32768 // pre-init sizes
#define MPXPLAY_SPI_MAXBSREADSIZE_VIDEO 65536

#define MPXPLAY_SPI_EXTRADATA_PADDING 8

typedef struct mpxplay_streampacket_info_s{
 unsigned int  flags;
 //config
 unsigned int  stream_select;  // for this streamtype
 //info
 unsigned int  streamtype;     // audio/video/other
 unsigned int  wave_id;        // or fourcc in 4C_LE32
 unsigned int  nb_streams;     // number of streams for this streamtype
 unsigned int  block_align;    // WAV,WMA

 mpxp_uint8_t  *extradata;     // decoder extra data (use as a pointer, don't alloc directly to it)
 unsigned long  extradata_size;

 //decoding
 mpxp_uint8_t  *bitstreambuf;  // from demuxer/parser to decoder
 unsigned long  bs_bufsize;    //
 unsigned long  bs_framesize;  // adf->maxbsframesize or set by parser or set by Mpxplay (MAXBSREADSIZE)
 unsigned long  bs_readsize;   // used by (raw) parsers (mp3,ac3,dts) (usually =bs_framesize)
 unsigned long  bs_leftbytes;  //
 unsigned long  bs_usedbytes;  //

 //struct mpxplay_streampacket_info_s *next_demuxerdata;
 //void                               *next_demuxerfunc;
 unsigned char reserved[6*4];
}mpxplay_streampacket_info_s; // 20*4=80 bytes

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

typedef struct mpxplay_audio_decoder_type_s{
 unsigned int wave_id;  // 0x0055,????,0x2000,0x0160
 char *wave_name;       // "MP3","OGG","AC3","WMA"
}mpxplay_audio_decoder_type_s;

typedef struct mpxplay_audio_decoder_func_s{
 unsigned long flags;

 void (*preinit)(void); // initialize static data fields (mp3)
 void (*deinit) (void); // close audio decoder (cdw)
 void (*config_decoder)(void *notused); // not used yet

 int  (*open)  (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*close) (struct mpxplay_audio_decoder_info_s *adi);
 int  (*parse_extra) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*parse_frame) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*decode)(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*clearbuf)(struct mpxplay_audio_decoder_info_s *adi,unsigned int mpx_seektype);

 void (*get_analiser_bands)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *banddataptr);
 void (*set_eq)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *band_freqs,float *band_powers);

 unsigned long maxbsframesize;  // max. size of input (bitstream) frames
 unsigned long maxpcmblocksamplenum; // max. pcm block (frame) size (!!! have to multiply with channelnum and bytes_per_sample)
 mpxplay_audio_decoder_type_s decodertypes[];
}mpxplay_audio_decoder_func_s;

// ***********************************************************************
// video decoder

//control
#define VDI_CNTRLBIT_DECODEVIDEO 1   // user configured
#define VDI_CNTRLBIT_SHOWVIDEO   32  // depending on the display type
//#define VDI_CNTRLBIT_DIRECTWRITE 64  // no postprocess, data can be written directly to screen
//info
#define VDI_FLAG_NEWPIC      256 // there's a new picture in the buffer
//#define VDI_FLAG_DIRECTWRITE 512 // data has written directly to screen

typedef struct mpxplay_video_decoder_info_s{
 void *private_data;
 unsigned long flags;
 unsigned char *picture;
 unsigned long picture_size;      // in bytes
 unsigned long video_frames;      // number of video frames
 unsigned long video_framepos;    // current frame position
 unsigned int  video_fps;         // frames/sec
 unsigned long video_pixelformat;
 unsigned long video_res_x;
 unsigned long video_res_y;
 unsigned long video_bitrate;     // just info
 unsigned long video_bpp;         //
 //unsigned char *screen_ptr;
 unsigned long screen_res_x;
 unsigned long screen_res_y;
 unsigned long output_res_x;
 unsigned long output_res_y;
}mpxplay_video_decoder_info_s;

typedef struct mpxplay_video_decoder_func_s{
 char *fourcc;
 unsigned long flags;

 int  (*open)  (struct mpxplay_video_decoder_info_s *vdi,struct mpxplay_streampacket_info_s *spi);
 void (*close) (struct mpxplay_video_decoder_info_s *vdi);
 int  (*decode)(struct mpxplay_video_decoder_info_s *vdi,struct mpxplay_streampacket_info_s *spi);
 void (*flush) (struct mpxplay_video_decoder_info_s *vdi,unsigned int mpx_seektype);

 unsigned int maxbsframesize;// max. size of input (bitstream) frames
}mpxplay_video_decoder_func_s;

// ***********************************************************************

// control_cb function numbers
#define MPXPLAY_CFGFUNCNUM_MASK_MAJOR 0xff000000

#define MPXPLAY_CFGFUNCNUM_INFILE_MAJOR 0x02000000

// ---------------- belongs to control_cb ----------------------
#if defined(__WATCOMC__) && !defined(STACK_CALLING)
#define MPXPLAY_CFGFUNCNUM_INFILE_FILEOPEN_FUNCMASK 0xfffff000
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_OPEN         0x02001000 // with O_ flags, argp1:void **fbds argp2:char *filename
#else
#define MPXPLAY_CFGFUNCNUM_INFILE_FILEOPEN_FUNCMASK 0xfff00000
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_OPEN         0x02100000 // with O_ flags, argp1:void **fbds argp2:char *filename
#endif
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_CLOSE        0x02002001 // argp1:void **fbds
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_SEEK         0x02002002 // ccb_data:void*fbds argp1:mpxp_int64_t *offset argp2:int *fromwhere
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_TELL         0x02002003 // ccb_data:void*fbds argp1:mpxp_int64_t *filepos
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_LENGTH       0x02002004 // ccb_data:void*fbds argp1:mpxp_int64_t *filelen
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_EOF          0x02002005 // ccb_data:void*fbds return eof 0/1
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_CHSIZE       0x02002006 // ccb_data:void*fbds argp1:mpxp_int64_t *offset
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_RENAME       0x02002010 // argp1:char *oldfilename argp2:char *newfilename
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_UNLINK       0x02002011 // argp1:char *filename
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_COPY         0x0200201D // ccb_data:void*fbds_src argp1:void*fbds_dest argp2:mpxp_int64_t *copylen
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_TMPOPEN      0x0200201E // ccb_data:void*fbds_src argp1:void **fbds_tmp
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_TMPXCHCLOSE  0x0200201F // ccb_data:void*fbds_src argp1:void **fbds_tmp
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_READ_BYTES   0x02002020 // ccb_data:void*fbds argp1:char*buf argp2:unsigned int *bytes
#define MPXPLAY_CFGFUNCNUM_INFILE_FILE_WRITE_BYTES  0x02002030 // ccb_data:void*fbds argp1:char*buf argp2:unsigned int *bytes

#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_FINAME_GET  0x02003000 // argp1:char *filenamedestbuf argp2:unsigned int *fnbufsize
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_FISIZE_GET  0x02003001 // argp1:mpxp_int64_t **filesize
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_RBLOCKSIZE  0x02003005 // returns read-block size of file/device
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_ISSEEKABLE  0x02003008 // returns 1 if yes
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_ISSTREAM    0x02003009 // returns 1 if yes
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_ISASYNCREAD 0x0200300A // returns 1 if yes
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_PROGID_GET  0x02003010 // argp1:char *filename - get program_id from filename (DVB)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_PROGID_SET  0x02003011 // argp1:mpxp_int32_t prog_id - program is changed by demuxer, search/set new aktfilenum by program_id (DVB)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_DURATION_SET   0x02003020 // argp1:mpxp_int32_t *duration_msecs
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_POSITION_SET   0x02003022 // argp1:mpxp_int32_t *realpos_msecs
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_PLAYSPEED_SET  0x02003080 // argp1:mpxp_int32_t *speed (0 - 1000 - 10000)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_PLAYVOLUME_SET 0x02003084 // argp1:mpxp_int32_t *volume (0 - 700)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_FUNCMASK 0xffffff00
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_GET     0x02003100 // argp1:char **destbuf argp2:unsigned long *bufsize (if bufsize==0 then destbuf is allocated)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_PUT     0x02003200 // argp1:char *srcbuf argp2:long *datalen (if datalen==-1 then srcbuf is null terminated)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_UPDATE  0x02003300 // same like PUT, but PUT doesn't overwrite existent id3info, but UPDATE does
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAGS_CLEAR  0x02003500 // clear all tags
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAGS_RELOAD 0x02003600 // reload tags from file
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_PACKFUNCTTI3I(func,texttype,i3i) (func|(texttype<<4)|i3i)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_UNPACKFUNC_TEXTTYPE(cmd) (((cmd)>>4)&0x0f)
#define MPXPLAY_CFGFUNCNUM_INFILE_ENTRY_TAG_UNPACKFUNC_I3I(cmd) ((cmd)&0xf)

// ---------------- belongs to control_func ----------------------
#define MPXPLAY_CFGFUNCNUM_INFILE_CF_NEWFILENAME_CHK   0x02006000 // arg1: char *filename - check a new entry by demuxer (eg. DVB program), without calling infile_open, returns MPXPLAY_ERROR_OK at success
#define MPXPLAY_CFGFUNCNUM_INFILE_CF_NEWFILENAME_SET   0x02006001 // arg1: char *filename - open a new entry by demuxer (eg. DVB program), without calling infile_open, returns MPXPLAY_ERROR_OK at success

//infile openmode/loadinfo types
#define MPXPLAY_INFILE_OPENMODE_MASK_SYS      0xfffff  // original O_ flags
#define MPXPLAY_INFILE_OPENMODE_AUTODETECT    (1<<20)
#define MPXPLAY_INFILE_OPENMODE_INFO_HEAD     (1<<21)
#define MPXPLAY_INFILE_OPENMODE_INFO_LENGTH   (1<<22)
#define MPXPLAY_INFILE_OPENMODE_INFO_DECODER  (1<<23)
#define MPXPLAY_INFILE_OPENMODE_INFO_ID3      (1<<24)
#define MPXPLAY_INFILE_OPENMODE_INFO_CHAPTERS (1<<25)
#define MPXPLAY_INFILE_OPENMODE_LOAD_SEEKTAB  (1<<26)
#define MPXPLAY_INFILE_OPENMODE_READ_CONTENT  (1<<28) // read stream(s) from the file
#define MPXPLAY_INFILE_OPENMODE_LOAD_PLAY     (1<<29) // open for play
#define MPXPLAY_INFILE_OPENMODE_LOAD_FULLHEAD (1<<30) // used internally for tag editing
#define MPXPLAY_INFILE_OPENMODE_CHECK (MPXPLAY_INFILE_OPENMODE_INFO_HEAD|MPXPLAY_INFILE_OPENMODE_INFO_LENGTH)
#define MPXPLAY_INFILE_OPENMODE_PLAY (MPXPLAY_INFILE_OPENMODE_INFO_HEAD|MPXPLAY_INFILE_OPENMODE_INFO_DECODER|MPXPLAY_INFILE_OPENMODE_LOAD_SEEKTAB|MPXPLAY_INFILE_OPENMODE_READ_CONTENT|MPXPLAY_INFILE_OPENMODE_LOAD_PLAY)
#define MPXPLAY_INFILE_OPENMODE_MASK_NONSTREAM (MPXPLAY_INFILE_OPENMODE_INFO_LENGTH|MPXPLAY_INFILE_OPENMODE_INFO_CHAPTERS|MPXPLAY_INFILE_OPENMODE_LOAD_SEEKTAB) // cannot use these at live-streams

typedef struct mpxplay_infile_info_s{
 long (*control_cb)(void *ccb_data,unsigned long funcnum,void *argp1,void *argp2); // deparser/demuxer calls back application to get/set data
 void *ccb_data;
 int  (*textconv_func)(mpxp_uint32_t convtype,char *src_string,int src_len,char *dest_string,unsigned int dest_buflen);
 void *private_data;// of infile_func (demuxer)
 mpxp_filesize_t filesize;
 long timemsec;     // length in msecs
 long allframes;    // in mpxplay frames (1 sec = ca. 38 frames)
 char *longname;    // set by file-parser (8 chars), can static ("LayerIII","OgVorbis","DolbyAC3")
 unsigned int seektype; // MPX_SEEKTYPE_xxx (decoder/demuxer clearbuf,infile_fseek)

 struct mpxplay_streampacket_info_s  *audio_stream;        // static!
 struct mpxplay_audio_decoder_info_s *audio_decoder_infos; // static! don't clear!
 struct mpxplay_audio_decoder_func_s *audio_decoder_funcs;

 struct mpxplay_streampacket_info_s  *video_stream;        // static!
 struct mpxplay_video_decoder_info_s *video_decoder_infos; // static! don't clear!
 struct mpxplay_video_decoder_func_s *video_decoder_funcs;

 unsigned long standard_id3tag_support;

 mpxp_int32_t (*control_func)(void *private_data, mpxp_uint32_t funcnum, mpxp_ptrsize_t arg1, mpxp_ptrsize_t arg2); // application calls deparser/demuxer to get/set data (if demuxer sets a function to this pointer)

 mpxp_uint32_t reserved[3];
}mpxplay_infile_info_s;

// mpxplay_infile_func_s->flags (max 16 bits!)
#define MPXPLAY_INFILEFUNC_FLAG_NOADETECT  (1<< 0) // don't open for autodetection
#define MPXPLAY_INFILEFUNC_FLAG_NOCHECK    (1<< 1) // don't open for checking
#define MPXPLAY_INFILEFUNC_FLAG_VIDEO      (1<< 8) // assumed video file (by extension)
#define MPXPLAY_INFILEFUNC_FLAG_ONLYVIDEO  (1<< 9) // open in video player mode only

struct mpxplay_infile_func_s{
 unsigned long flags;

 void (*preinit)(void); // initialize static data fields (mp3)
 void (*deinit) (void); // close audio decoder (cdw)

 int  (*open)  (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,char *filename,struct mpxplay_infile_info_s *,mpxp_uint32_t openmode); //
 void (*close) (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *);
 int  (*decode)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *);// return MPXPLAY_ERROR_INFILE_xxx
 long (*fseek) (struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *,long newmpxframenum); // return MPXPLAY_ERROR_INFILE_xxx or newmpxframenum
 void (*seek_postprocess)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,unsigned int mpx_seektype);

 int  (*get_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis); // return MPXPLAY_ERROR_
 int  (*write_id3tag)(struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,struct mpxplay_infile_info_s *miis,char *filename,unsigned long writetag_control);  // return error code

 struct mpxplay_filehand_low_func_s *own_filehand_funcs; // to set an own low level file handling (CDW)

 char *file_extensions[]; // {"MP2","MP3",NULL} {"OGG",NULL} (the first extension have to be a 3-char file description too (ie: MP4, ASF))
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// mpxplay_config_dvbepg_control_flags
#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_SORT_BY_PROGID     (0)            // sort channels by program id after scanning
#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_SORT_BY_FREQ       (1)            // sort channels by frequency after scanning
//#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_SORT_BY_CHANID   (2)            // sort channels by logical channel id after scanning // TODO: current freq/prog structure doesn't allow this
#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_SORTTYPE_MASK      ((1 << 2) - 1) //
#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_FILTER_ENCRYPTED   (1 << 4)       // filter/ignore encrypted channels (generally, not only at scan)
#define MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_FILTER_NONMEDIA    (1 << 5)       // filter/ignore non-media/invalid channels (which has no logical channel id or prog-name) (generally, not only at scan)
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOSURFACE_SHOW_EPGDIALOG (1 << 12)      // show epg dialog window (list) over left side of video frame in fullscreen (single video & videwall too)
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_HIDEMAINWIN       (1 << 16)      // hide main window over fullscreen videowall (don't wake up by move or click event, only by wheel event)
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_KEYFRAMES_ONLY    (1 << 17)      // display only keyframes of non-primary videowalls
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_FILTER_WINDOW     (1 << 18)      // enable base filtering (deinterlace) for all windows on videowall
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_RESTORE           (1 << 19)      // restore videowall if it was enabled
#define MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_WAS_ENABLED       (1 << 20)      // videowall was enabled (restore it)
#define MPXPLAY_CONFIG_DVBEPGCTRL_DEFAULTS (MPXPLAY_CONFIG_DVBEPGCTRL_CHANNELS_SORT_BY_PROGID | MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOSURFACE_SHOW_EPGDIALOG \
                                            | MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_HIDEMAINWIN | MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_KEYFRAMES_ONLY \
                                            | MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_FILTER_WINDOW | MPXPLAY_CONFIG_DVBEPGCTRL_VIDEOWALL_RESTORE)

#define MPXPLAY_STREAMTYPE_VIDEOWALL_NUM  49  // max number of sub-windows on videowall (eg. 7 x 7) (probably there are no more programs in a multiplex at DVB-T2 neither)

enum mpxplay_streamindex_types{
    MPXPLAY_STREAMTYPEINDEX_AUDIO = 0,     // 0. audio
    MPXPLAY_STREAMTYPEINDEX_VIDEO,         // 1. video
    MPXPLAY_STREAMTYPEINDEX_SUBTITLE,      // 2. subtitle
    MPXPLAY_STREAMTYPEINDEX_EXTSTREAMNUM,  // external stream types, currently it's 3, truly it's 2 only (audio, subtitle)
    MPXPLAY_STREAMTYPEINDEX_PLAYNUM = MPXPLAY_STREAMTYPEINDEX_EXTSTREAMNUM, // 3 stream types to play (audio, video, subtitle)
    MPXPLAY_STREAMTYPEINDEX_SEEKPREVIEW = MPXPLAY_STREAMTYPEINDEX_PLAYNUM,  // 3. preview window at seeking
    MPXPLAY_STREAMTYPEINDEX_NUM,           // 4  // TODO: it's not required to use _NUM everywhere, _PLAYNUM is enough
    MPXPLAY_STREAMTYPEINDEX_VIDEOWALL = MPXPLAY_STREAMTYPEINDEX_NUM,
    MPXPLAY_STREAMTYPEINDEX_MAX = MPXPLAY_STREAMTYPEINDEX_VIDEOWALL + MPXPLAY_STREAMTYPE_VIDEOWALL_NUM,
    MPXPLAY_STREAMTYPEINDEX_TEMP           // used to store temporary video frames in a queue within a thread (no mutex is initialized)
};

enum dispqt_video_externalstreamload_types {
    MPXPLAY_EXTERNALSTREAM_LOADTYPE_NONE = 0, // do nothing with external streams (no auto search, no auto load)
    MPXPLAY_EXTERNALSTREAM_LOADTYPE_SEARCH,   // automatically search for additional external streams, adding them to the selectable stream list (video contextmenu)
    MPXPLAY_EXTERNALSTREAM_LOADTYPE_USE,      // automatically search for additional external streams, use if no internal stream found
    MPXPLAY_EXTERNALSTREAM_LOADTYPE_PREFER,   // automatically search for additional external streams, prefer external stream over internal one
    MPXPLAY_EXTERNALSTREAM_LOADTYPE_MAX
};

//callback config (from infile to service) and control (from service to infile)
#define MPXPLAY_INFILE_CBKCFGCTRL_MAJOR                   0x10000000 // callback config and control major function ID
#define MPXPLAY_INFILE_CBKCFG_MAJOR_SERVICE_FFMPEGVIDEO   0x10001000 // ffmpeg video decoder service
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_CLEAR_CALLBACKS     0x10001000 // *arg1: NULL, *arg2:NULL  (clear all callbacks)
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_PAUSE               0x10001002 // *arg1: NULL, *arg2:NULL  (pause playing)
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_UNPAUSE             0x10001003 // *arg1: NULL, *arg2:NULL  (unpause playing)
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_OPEN_CALLBACK       0x10001004 // *arg1:callbackfunc *arg2:callbackdata
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_CLOSE_CALLBACK      0x10001005 // *arg1:callbackfunc *arg2:callbackdata
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_REFRESH_CALLBACK    0x10001006 // *arg1:callbackfunc *arg2:callbackdata
#define MPXPLAY_INFILE_CBKCFG_SRVFFMV_RECHECK_SURFACE     0x10001007 // *arg1: NULL, *arg2:NULL  (check video surface usage)

typedef int (mpxplay_infile_callback_t)(mpxp_ptrsize_t passdata, unsigned int control, mpxp_ptrsize_t arg1, mpxp_ptrsize_t arg2, int timeout_ms);

#define MPXPLAY_INFILE_CBKCTRL_MAJOR  0x10008000
#define MPXPLAY_INFILE_CBKCTRL_GET_PROGRAMLIST            0x10008001  // **arg1_out: mpxplay_programinfo_t
#define MPXPLAY_INFILE_CBKCTRL_GET_STREAMLIST             0x10008003  // **arg1_out: mpxplay_streaminfo_t
#define MPXPLAY_INFILE_CBKCTRL_GET_CODECCTXINIT_FFMPEG    0x10008005  // **arg1_out: mpxplay_packetlist_t *pktlist_elem  arg2_in: mpxp_ptrsize_t streamtypeindex
#define MPXPLAY_INFILE_CBKCTRL_GET_CODECCTX_FFMPEG        0x10008006  // **arg1_out: mpxplay_packetlist_t *pktlist_elem  arg2_in: mpxp_ptrsize_t streamtypeindex
#define MPXPLAY_INFILE_CBKCTRL_GET_TIMESYNC               0x10008010  // *arg1_inout: mpxp_int64_t *timesync_us          arg2_in: mpxp_ptrsize_t streamtypeindex
#define MPXPLAY_INFILE_CBKCTRL_GET_CORRECTEDVTIMESTAMP    0x10008012  // *arg1_in: AVFrame *avframe                      *arg2_out: mpxp_int64_t *timesync_us
#define MPXPLAY_INFILE_CBKCTRL_SET_VIDEOWALLMODE          0x10008030  //  arg1_in: mpxp_ptrsize_t 0/1 (disable/enable)
#define MPXPLAY_INFILE_CBKCTRL_GET_VIDEOWALLISAVAILABLE   0x10008031  // *arg1_out: mpxp_int32_t *isAvailable
#define MPXPLAY_INFILE_CBKCTRL_GET_VIDEOWALLISENABLED     0x10008032  // *arg1_out: mpxp_int32_t *isEnabled
#define MPXPLAY_INFILE_CBKCTRL_GET_VIDEOWALLCURRSTI       0x10008033  // *arg1_out: mpxp_int32_t *videowall_primary_streamindex
#define MPXPLAY_INFILE_CBKCTRL_SET_VIDEOWALLCURRSTI       0x10008034  //  arg1_in: mpxp_ptrsize_t videowall_primary_streamindex
#define MPXPLAY_INFILE_CBKCTRL_SET_VIDEOWALLCURRSTI_EW    0x10008035  //  arg1_in: mpxp_ptrsize_t videowall_primary_streamindex
#define MPXPLAY_INFILE_CBKCTRL_GET_VIDEOWALLPRGEPGLIST    0x10008036  //  arg1_in: mpxp_ptrsize_t videowall_selected_streamindex  **arg2_out: mpxplay_dvb_epgeventlist_t
#define MPXPLAY_INFILE_CBKCTRL_SET_SELECT_PROGRAM         0x10008040  //  arg1_in: mpxp_ptrsize_t prognum
#define MPXPLAY_INFILE_CBKCTRL_SET_SELECT_STREAM          0x10008045  //  arg1_in: mpxp_ptrsize_t streamtypeindex         arg2_in: mpxp_ptrsize_t streamnum
#define MPXPLAY_INFILE_CBKCTRL_SET_ADDSTREAMFILESTOVIDEO  0x10008050  // *arg1_in: mpxplay_loadfile_callbackdata_s *mlcs
#define MPXPLAY_INFILE_CBKCTRL_SET_RECORDSTREAMSTOFILE    0x10008055  // *arg1_in: mpxplay_loadfile_callbackdata_s *mlcs
#define MPXPLAY_INFILE_CBKCTRL_SET_RECORDSTREAMSSTOP      0x10008051  //
#define MPXPLAY_INFILE_CBKCTRL_GET_PACKETLIST_ELEM        0x10008080  // **arg1_out: mpxplay_packetlist_t *pktlist_elem   arg2_in: mpxp_ptrsize_t streamtypeindex
#define MPXPLAY_INFILE_CBKCTRL_SET_PACKETQUEUE_CLEAR      0x10008100  //  arg1_in: mpxp_ptrsize_t streamtypeindex         arg2_in: mpxp_ptrsize_t flags
#define MPXPLAY_INFILE_CBKCTRL_SET_CODECCTX_CLEAR         0x10008102  //  arg1_in: mpxp_ptrsize_t streamtypeindex         arg2_in: mpxp_ptrsize_t flags
#define MPXPLAY_INFILE_CBKCTRL_SET_CODECCTX_CONSOLIDATE   0x10008110  // *arg1_in: *codec_ctx                             arg2_in: mpxp_ptrsize_t streamtypeindex
#define MPXPLAY_INFILE_CBKCTRL_SET_VIDFRAME_METADATA      0x10008120  // *arg1_in: AVMetadata *metadata

// for video contextmenu
typedef struct mpxplay_streaminfo_t{
    unsigned int nb_entries[MPXPLAY_STREAMTYPEINDEX_NUM];
    int selected_stream_index[MPXPLAY_STREAMTYPEINDEX_NUM];
    char **stream_names[MPXPLAY_STREAMTYPEINDEX_NUM];
    int *stream_idx[MPXPLAY_STREAMTYPEINDEX_NUM];
    mpxp_bool_t is_record_running;      // by in_ffmpfile_file_output_running
    mpxp_bool_t is_videowall_enabled;
    char primary_media_filename[MAX_PATHNAMELEN];
}mpxplay_streaminfo_t;

// for video contextmenu (should be replaced with common structures bellow)
typedef struct mpxplay_programinfo_t{
    unsigned int nb_programs;           //
    int selected_prognum;               // -1 if no program is selected
    int *program_ids;                   // can be NULL (then no programs)
    char **program_names;               // can be NULL (then no programs)
}mpxplay_programinfo_t;

// for videowall (should be replaced with common structures bellow)
typedef struct mpxplay_dvb_epgeventlist_t{
    char *program_name;                 // name of the DVB program channel (dynamically allocated string)
    unsigned int nb_program_events;     // number of EPG list entries
    char **program_event_list;          // dynamically allocated (simple) string list
    int videowall_window_pos_x, videowall_window_pos_y;  // video window x,y position on the videowall (given by video_worker for epg tooltip displaying, not by ffmpepg)
    int videowall_window_width;         // video window width
    int videowall_window_height;        // video window height (to limit epg list displaying)
}mpxplay_dvb_epgeventlist_t;

typedef struct mpxplay_loadfile_callbackdata_s{
    int load_type;                      // Mainwindow_SelectLoadType (used in maindwindow.cpp only)
    int sidenum;                        // editor side select
    int tabnum;                         // editor tab select
    unsigned int nb_filenames;          //
    char **filenames;                   //
}mpxplay_loadfile_callbackdata_t;

#define MPXPLAY_VIDEO_RENDERER_FRAME_POOL_SIZE_MIN  12
#define MPXPLAY_VIDEO_RENDERER_FRAME_POOL_SIZE_MAX  16
#define MPXPLAY_VIDEO_RENDERER_FRAME_POOL_ELEMFLAG_WRITEONLY (1 << 29)

#ifndef GUID_DEFINED
#define GUID_DEFINED

typedef struct _GUID {
  unsigned long Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
} GUID;
#endif /* GUID_DEFINED */

typedef struct mpxplay_video_renderer_info_s{
    void *d3d_device_mutex;
    void *d3d_close_mutex;
    mpxp_ptrsize_t d3d_dll_hnd;      // dll handler for video renderer DLL (D3D11.DLL or D3D9.DLL)
    void *d3d_device_ptr;            // ID3D11Device
    void *d3d_device_ctx;            // ID3D11DeviceContext
    void *d3d_video_device;          // ID3D11VideoDevice / IDirect3DDeviceManager9
    void *d3d_video_ctx;             // ID3D11VideoContext
    void *d3d_render_handler;        // opaque evr_data
    void *renderer_private_data;     // opaque evr_data or vulkan_data
    int  hwdevice_avpixfmt;          // FFmpeg hw decoder output format of this renderer (eg. AV_PIX_FMT_D3D11)
    mpxp_bool_t   (*render_function_check_videodecoder_inputformat)(const GUID format_guid, int av_pixfmt);
    mpxp_uint32_t (*render_function_get_frame_delay)(void *render_private_data); // get the rendering delay in frames (at deinterlacing)
    mpxp_bool_t   (*render_function_poolbufelem_validity_check)(void *av_frame); // check that the av_frame is pool buffer elem
    mpxp_bool_t   (*render_function_is_framebuffer_support)(void);               // is hw texture pool buffer is available?
    int           (*render_function_get_frame_buffer)(void *avctx, void *avframe, int flags); // prepare a pool buffer elem
    void          (*render_function_frame_buffer_consolidate)(void);             // clear pool buffer
    mpxp_bool_t   (*render_function_copy_to_pool_buffer)(void *dest_avframe, void *src_avframe); // copy simple avframe into a pool buffer elem
    void          (*render_function_poolbufelem_unmap)(void *av_frame);          // close memory mapping of pool buffer elem
    void *        (*render_function_poolbuf_bufptr_get)(void);
    void *        (*render_function_poolbuf_pixform_get)(void *format_datas, int pix_format);
    mpxp_bool_t   (*render_function_poolbuf_texture_alloc)(void *format_datas, int tex_width, int tex_height, void **poolbufelem_texture_2d, void **poolbufelem_shader_resource_views, void *mapres);
    void          (*render_function_poolbuf_texture_free)(void **poolbufelem_texture_2d, void **poolbufelem_shader_resource_views);
    mpxp_bool_t   (*render_function_poolbuf_texture_map)(void *poolbufelem_texture_2d, void *mapped_resource);
    void          (*render_function_poolbuf_texture_unmap)(void *poolbufelem_texture_2d, void *mapres);
    mpxp_uint32_t renderer_capabilities;
}mpxplay_video_renderer_info_s;

//---------------------------------------------------------------------------------------------------------
// DVB/EPG common structures (dtvdbase.c, ffmpepg.c)

#define MPXPLAY_DVBEPG_EVENT_DURATION_MIN        1 // to show program as a valid entry (to start playing anyway) if there's no stored event for the program
#define MPXPLAY_DVBEPG_EVENTS_DEFAULT_PAST_HOURS 2 // default value to show past program events till this time limit (-2h)
#define MPXPLAY_DVBEPG_EVENTS_LIMIT_MAX_HOURS    (30 * 24) // show future program events till this time limit (30 days) (drop events after this)

#define MPXPLAY_DVBEPG_EVENTFLAGS_SAVEMASK      ((1 << 8) - 1) // some flags are not saved to xml

// datas belonging to a dvb program event
typedef struct mpxplay_dvb_epg_event_data_s{
    char *event_shortdesc_name;                                  // UTF-8 short description (title) of program event
    char *event_shortdesc_details;                               // UTF-8 long description (details) of program event
    char *event_extended_desc;                                   // UTF-8 extended description (details) of program event
    mpxp_uint64_t event_begin_date_time;                         // start of the event in 0xYYYYMMDD00HHMMSS format (not BCD, just byte aligned numbers)
    mpxp_uint32_t event_duration_time;                           // duration time of the event (byte aligned 0x00HHMMSS)
    mpxp_uint32_t event_id;                                      // event id from EIT
    mpxp_uint8_t extdesc_counter;                                // expected descriptor_number of extended event descriptor
    mpxp_uint8_t parental_rating;                                // recommended (limit) age for this program (4 - 18)
    mpxp_uint8_t event_flags;                                    // additional flags for this program event
    struct mpxplay_dvb_epg_event_data_s *next_event;             // next element of the program events chain
}mpxplay_dvb_epg_program_event_s;

// datas belonging to a dvb program
typedef struct mpxplay_dvb_epg_program_data_s{
    char *program_name;                                          // program name (service_name)
    char *current_event_name;                                    // UTF-8 short description (title) of program current event
    mpxp_uint32_t current_event_duration;                        // duration time of the current event (byte aligned 0x00HHMMSS)
    mpxp_int32_t program_id;                                     // program id (service_id / event_id)
    mpxp_uint32_t channel_id;                                    // logical channel id
    mpxp_uint32_t ctrl_flags;                                    // control flags for this program
    mpxp_bool_t current_event_changed;                           // changed event flag
    unsigned int nb_program_events;                              // number of collected program events in the chain
    struct mpxplay_dvb_epg_event_data_s *epg_program_event_chain;// first element of the program events chain
    struct mpxplay_dvb_epg_program_data_s *next_program;         // next element of program datas chain
}mpxplay_dvb_epg_program_data_s;

#define MPXPLAY_DVBEPG_FREQ_FLAG_LOADED_SDT  (1 << 0) // SDT section has been loaded (programs list)
#define MPXPLAY_DVBEPG_FREQ_FLAG_LOADED_NIT  (1 << 1) // NIT section has been loaded (logical channel IDs)

// datas belonging to a mux/frequency/channel
typedef struct mpxplay_dvb_epg_frequency_data_s
{
    mpxp_uint32_t frequency;                                     // frequency in Hz
    mpxp_uint32_t freq_flags;                                    // MPXPLAY_DVBEPG_FREQ_FLAG_
    mpxp_uint32_t freq_signal_quality;                           // detected tuner signal quality (0 - 100 %)
    unsigned int nb_programs;
    struct mpxplay_dvb_epg_program_data_s *program_data_chain;
    struct mpxplay_dvb_epg_frequency_data_s *next_frequency;
}mpxplay_dvb_epg_frequency_data_s;

#define MPXPLAY_DVBEPG_PROTOCOL_FLAG_FULLDATA    (1 << 0) // sending all data of a protocol
#define MPXPLAY_DVBEPG_PROTOCOL_FLAG_SCANFINISH  (1 << 1) // scan has been finished
#define MPXPLAY_DVBEPG_PROTOCOL_FLAG_LOCALFILE   (1 << 2) // EPG info came from a local file (not from DVB device)
#define MPXPLAY_DVBEPG_PROTOCOL_FLAG_NEWFILE     (1 << 3) // EPG info is created at new file signal

// datas belonging to a device/protocol
typedef struct mpxplay_dvb_epg_protocol_data_s
{
    mpxp_uint32_t protocol_id;                                   // BDADEVTYPE_
    mpxp_uint32_t prot_flags;                                    // MPXPLAY_DVBEPG_PROTOCOL_FLAG_
    mpxp_uint64_t stream_datetime_val;                           // value of TDT in YYYYMMDD00HHMMSS format (packed byte aligned, not BCD)
    mpxp_uint32_t freq_bandwidth;                                // bandwidth in Hz
    mpxp_uint32_t scan_freq_begin;                               // scan start frequency in Hz
    mpxp_uint32_t scan_freq_end;                                 // scan end frequency in Hz
    mpxp_uint32_t curr_program_id;                               // currently played program
    unsigned int nb_frequencies;
    struct mpxplay_dvb_epg_frequency_data_s *frequency_data_chain;
    struct mpxplay_dvb_epg_protocol_data_s *next_protocol;
}mpxplay_dvb_epg_protocol_data_s;

//---------------------------------------------------------------------------------------------------------
// general / common ffmpeg packet handling (ffmpegdec.c, video_worker.cpp)

enum {MPXPLAY_PACKETTYPE_AVCODECCTX = 0, MPXPLAY_PACKETTYPE_AVPACKET, MPXPLAY_PACKETTYPE_AVFRAME_AUDIO, MPXPLAY_PACKETTYPE_AVFRAME_VIDEO, MPXPLAY_PACKETTYPE_MAX};

#define MPXPLAY_PACKETLISTFLAG_INIT        (1 << 0) // initialize packet before use (avcodec)
#define MPXPLAY_PACKETLISTFLAG_RESET       (1 << 1) // pop/clear all packets (avcodec)
#define MPXPLAY_PACKETLISTFLAG_CLEARBUF    (1 << 2) // pop packets till this one, and clear decoder / codec buffer(s)
#define MPXPLAY_PACKETLISTFLAG_KEYFRAME    (1 << 3) // pop packets till this one, and clear decoder / codec buffer(s)
#define MPXPLAY_PACKETLISTFLAG_STILLPICT   (1 << 4) // still picture (aspect ratio and multiply picture frame workaround)
//#define MPXPLAY_PACKETLISTFLAG_IGNORE_TS   (1 << 5) // ignore timestamp (audio visualization workaround)
#define MPXPLAY_PACKETLISTFLAG_TEXTTYPE_BITPOS  (24)   // text (de)coding type for subtitles
#define MPXPLAY_PACKETLISTFLAG_TEXTTYPE_MASK    (0x03) // MPXPLAY_TEXTCONV_TYPE_CHAR (0),  MPXPLAY_TEXTCONV_TYPE_UTF8 (1), MPXPLAY_TEXTCONV_TYPE_UTF16LE (2), MPXPLAY_TEXTCONV_TYPE_UTF16BE (3)

typedef struct mpxplay_packetlist_t{
    unsigned int flags;                 // control flags
    unsigned int packet_type;           // MPXPLAY_PACKETTYPE_
    void *frame_pkt;                    // AVPacket
    void *codec_ctx;                    // AVCodecContext belongs to frame_pkt
    struct mpxplay_packetlist_t *next;  // next elem in the list
    mpxp_int64_t timestamp_us;          // timestamp in usec
}mpxplay_packetlist_t;

typedef struct mpxplay_packetqueue_t{
    mpxplay_packetlist_t *first_pkt, *last_pkt;
    int nb_packets;
    void *mutexhnd_queue;
//    void *condhnd_queue;
    int streamtype_index;
}mpxplay_packetqueue_t;

extern mpxp_int64_t mpxplay_infile_callback_check_audio_timesync(void *fr_data, mpxp_int64_t avsync_clocktime, mpxp_int64_t *avsync_timestamp, mpxp_int64_t curr_pts, int bs_leftbytes, int sample_rate);
extern void mpxplay_infile_callback_push_audio_packet(void *fr_data, int sample_rate, int nb_bits, int nb_channels, int nb_samples_all_channels, unsigned char *data);
extern int  mpxplay_ffmpgdec_packetqueue_get(mpxp_ptrsize_t demuxer_data, mpxplay_packetqueue_t *packet_queue, mpxplay_packetlist_t **pktlist_elem_dest, int wait_ms);
extern int  mpxplay_ffmpgdec_packetqueue_put(mpxplay_packetqueue_t *packet_queue, unsigned int packet_type, unsigned int packet_flags, void *codec_ctx, void *pkt_new);
extern void mpxplay_ffmpgdec_queuelistelem_clear(mpxplay_packetlist_t **list_elem_p);
extern unsigned int mpxplay_ffmpgdec_packetqueue_clear(mpxplay_packetqueue_t *packet_queue, int flag_test); // returns 1 of found flag
extern void mpxplay_ffmpgdec_packetqueue_init(mpxplay_packetqueue_t *packet_queue, unsigned int streamtype_index);
extern mpxp_bool_t mpxplay_ffmpgdec_packetqueue_check(mpxplay_packetqueue_t *packet_queue);
extern void mpxplay_ffmpgdec_packetqueue_close(mpxplay_packetqueue_t *packet_queue);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_in_file_h
