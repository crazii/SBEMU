#ifndef au_cards_h
#define au_cards_h

#ifdef __cplusplus
extern "C" {
#endif

//au_infos->card_controlbits
#define AUINFOS_CARDCNTRLBIT_TESTCARD         1
#define AUINFOS_CARDCNTRLBIT_DOUBLEDMA        2
#define AUINFOS_CARDCNTRLBIT_MIDASMANUALCFG   4
#define AUINFOS_CARDCNTRLBIT_DMACLEAR         8 // run AU_clearbuffs
#define AUINFOS_CARDCNTRLBIT_DMADONTWAIT     16 // don't wait for card-buffer space (at seeking)
#define AUINFOS_CARDCNTRLBIT_BITSTREAMOUT    32 // enable bitstream out
#define AUINFOS_CARDCNTRLBIT_BITSTREAMNOFRH  64 // no-frame-header output is supported by decoder
#define AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD  128 // write main header (at wav out only)
#define AUINFOS_CARDCNTRLBIT_AUTOTAGGING    256 // copy tags (id3infos) from infile to outfile (if possible)
#define AUINFOS_CARDCNTRLBIT_AUTOTAGLFN     512 // create filename from id3infos (usually "NN. Artist - Title.ext"
#define AUINFOS_CARDCNTRLBIT_UPDATEFREQ    1024 // change/update soundcard freq

//au_infos->card_infobits
#define AUINFOS_CARDINFOBIT_PLAYING          1
#define AUINFOS_CARDINFOBIT_IRQRUN           2
#define AUINFOS_CARDINFOBIT_IRQSTACKBUSY     4
#define AUINFOS_CARDINFOBIT_DMAUNDERRUN      8 // dma buffer is empty (set by dma-monitor)
#define AUINFOS_CARDINFOBIT_DMAFULL         16 // dma buffer is full (set by AU_writedata)
#define AUINFOS_CARDINFOBIT_HWTONE          32
#define AUINFOS_CARDINFOBIT_BITSTREAMOUT    64 // bitstream out enabled/supported
#define AUINFOS_CARDINFOBIT_BITSTREAMNOFRH 128 // no frame headers (cut)

//one_sndcard_info->infobits
#define SNDCARD_SELECT_ONLY     1 // program doesn't try to use automatically (ie: wav output)
#define SNDCARD_INT08_ALLOWED   2 // use of INT08 (and interrupt decoder) is allowed
#define SNDCARD_CARDBUF_SPACE   4 // routine gives back the cardbuf space, not the bufpos
#define SNDCARD_SPECIAL_IRQ     8 // card has a special (long) irq routine (requires stack & irq protection)
#define SNDCARD_SETRATE        16 // always call setrate before each song (special wav-out and test-out flag!)
#define SNDCARD_LOWLEVELHAND   32 // native soundcard handling (PCI)
#define SNDCARD_IGNORE_STARTUP 64 // ignore startup (do not restore songpos) (ie: wav out)
#define SNDCARD_FLAGS_DISKWRITER (SNDCARD_SELECT_ONLY|SNDCARD_SETRATE|SNDCARD_IGNORE_STARTUP)

//au_cards mixer channels
#define AU_AUTO_UNMUTE 1 // automatically unmute (switch off mute flags) at -scv

#define AU_MIXCHAN_MASTER       0  // master out
#define AU_MIXCHAN_PCM          1  // pcm out
#define AU_MIXCHAN_HEADPHONE    2  // headphone out
#define AU_MIXCHAN_SPDIFOUT     3  // digital out
#define AU_MIXCHAN_SYNTH        4  // midi/synth out
#define AU_MIXCHAN_MICIN        5  // MIC input
#define AU_MIXCHAN_LINEIN       6  // LINE in
#define AU_MIXCHAN_CDIN         7  // CD in
#define AU_MIXCHAN_AUXIN        8  // AUX in
#define AU_MIXCHAN_BASS         9  // !!! default: -1 in au_cards, +50 in au_mixer
#define AU_MIXCHAN_TREBLE      10  // -"-
#define AU_MIXCHANS_NUM        11

// aucards_mixchandata_s->channeltype
#define AU_MIXCHANFUNC_VOLUME 0  // volume control (of master,pcm,etc.)
#define AU_MIXCHANFUNC_MUTE   1  // mute switch (of master,pcm,etc.)
#define AU_MIXCHANFUNCS_NUM   2  // number of mixchanfuncs
#define AU_MIXCHANFUNCS_FUNCSHIFT 8
#define AU_MIXCHANFUNCS_FUNCMASK  0xff
#define AU_MIXCHANFUNCS_CHANMASK  ((1<<AU_MIXCHANFUNCS_FUNCSHIFT)-1)
#define AU_MIXCHANFUNCS_PACK(chan,func) (((func)<<AU_MIXCHANFUNCS_FUNCSHIFT)|(chan))
#define AU_MIXCHANFUNCS_GETCHAN(c) ((c) & AU_MIXCHANFUNCS_CHANMASK)
#define AU_MIXCHANFUNCS_GETFUNC(c) (((c) >> AU_MIXCHANFUNCS_FUNCSHIFT)&AU_MIXCHANFUNCS_FUNCMASK)

#define AU_MIXCHAN_MAX_VALUE_VOLUME    100  // mater, pcm, etc
#define AU_MIXCHAN_MAX_VALUE_TONE      200  // bass, treble

//for verifying
#define AU_MIXERCHAN_MAX_SUBCHANNELS    8 // this is enough for a 7.1 setting too :)
#define AU_MIXERCHAN_MAX_REGISTER   65535 // check this again at future cards (2^bits)
#define AU_MIXERCHAN_MAX_BITS          32 //
#define AU_MIXERCHAN_MAX_VALUE 0xffffffff // 2^32-1

//aucards_submixerchan_s->infobits
#define SUBMIXCH_INFOBIT_REVERSEDVALUE  1 // reversed value
#define SUBMIXCH_INFOBIT_SUBCH_SWITCH   2 // set register if value!=submixch_max

//soundcard mixer structures
typedef struct aucards_submixerchan_s{
 unsigned long submixch_register; // register-address of channel
 unsigned long submixch_max;      // max value (and mask) (must be 2^n-1 (ie:1,3,7,15,31,63,127,255))
 unsigned long submixch_shift;    // bit-shift from 0. bit
 unsigned long submixch_infobits; //
}aucards_submixerchan_s;

typedef struct aucards_onemixerchan_s{
 unsigned long mixchan;       // master,pcm,etc. & volume,mute-sw
 unsigned long subchannelnum; // sub-channels (mono (1) or left&right (2))
 aucards_submixerchan_s submixerchans[]; // infos of 1 or 2 subchannels (reg,max,shift,flag)
}aucards_onemixerchan_s;

typedef struct aucards_onemixerchan_s* aucards_allmixerchan_s;

/*typedef struct au_cardbuf_v0154_s{
 char          *card_DMABUFF;        // pointer to dma buffer
 unsigned long  card_dma_buffer_size;// allocated size
 unsigned long  card_dmasize;        // used size
 unsigned long  card_dmalastput;     // last put position
 unsigned long  card_dma_lastgoodpos;// last good bufferpos
 unsigned long  card_dmafilled;      // databytes in buffer
 unsigned long  card_dmaspace;       // dmasize - dmafilled (empty bytes)
 unsigned int   card_dmaout_under_int08;
}au_cardbuf_v0150_s;

typedef struct au_cardconfig_v0154_s{
 unsigned int   card_wave_id;        // 0x0001,0x0003,0x0055,0x2000,etc.
 char          *card_wave_name;      // pcm-xx,pcm-fp,"MP3","AC3" (WAV-out/disk-writer(s) needs it)
 unsigned long  card_controlbits;    // card control flags
 unsigned long  card_infobits;       // card info flags
 unsigned long  card_freq;
 unsigned int   card_chans;
 unsigned int   card_bits;           // pcm or scale bits
 unsigned long  set_freq;
 unsigned int   set_chans;
 unsigned int   set_bits;
}au_cardconfig_v0150_s;*/

/*typedef struct au_cardinfo_s{
 one_sndcard_info *card_handler;    // function structure of the card
 void         *card_private_data;   // extra private datas can be pointed here (with malloc)
 char         *card_selectname;     // select card by name
 struct au_cardconfig_v0154_s cardconfig;
 struct au_cardbuf_v0154_s    cardbuf;
 unsigned int  card_bytespersample; // float-pcm:4, int-pcm:(bits+7)/8
 unsigned int  card_bytespersign;   // pcm: card_bytespersample*card_chans, bitstream:1 (for masking)
 unsigned long card_outbytes;       // samplenum*bytespersample_card
 unsigned int  int08_decoder_cycles;
 struct playlist_entry_info *pei;   // used by disk writers (filename,id3-info)
 unsigned int  card_mixer_values[AU_MIXCHANS_NUM];
}au_cardinfo_s;*/

#ifdef __DOS__
#ifdef SBEMU
#define AU_CARDS_LINK_ISA 0
#else
#define AU_CARDS_LINK_ISA 1
#endif
#define AU_CARDS_LINK_PCI 1
#endif

#ifdef MPXPLAY_WIN32
#define AU_CARDS_LINK_WIN 1
#endif

//link low level soundcard routines
#ifdef MPXPLAY_LINK_FULL
 #ifdef AU_CARDS_LINK_ISA
  #define AU_CARDS_LINK_SB16    1
  #define AU_CARDS_LINK_ESS     1
  #define AU_CARDS_LINK_WSS     1
  #define AU_CARDS_LINK_GUS     1
  #define AU_CARDS_LINK_SB      1
  //#define AU_CARDS_LINK_MIDAS  1
 #endif
 #ifdef AU_CARDS_LINK_PCI
  #define AU_CARDS_LINK_CMI8X38 1
  //#define AU_CARDS_LINK_EMU20KX 1
  #define AU_CARDS_LINK_ES1371  1
  #define AU_CARDS_LINK_ICH     1
  #define AU_CARDS_LINK_IHD     1
  #define AU_CARDS_LINK_SBLIVE  1
  #define AU_CARDS_LINK_VIA82XX 1
 #endif
 #ifdef AU_CARDS_LINK_WIN
  #define AU_CARDS_LINK_WINDSOUND 1
  #define AU_CARDS_LINK_WINWAVOUT 1
 #endif
#else
 #ifdef AU_CARDS_LINK_ISA
  #define AU_CARDS_LINK_SB16    1
  #define AU_CARDS_LINK_ESS     1
  #define AU_CARDS_LINK_WSS     1
  #define AU_CARDS_LINK_GUS     1
  #define AU_CARDS_LINK_SB      1
 #endif
 #ifdef AU_CARDS_LINK_PCI
  #define AU_CARDS_LINK_CMI8X38 1
  //#define AU_CARDS_LINK_EMU20KX 1
  #define AU_CARDS_LINK_ES1371  1
  #define AU_CARDS_LINK_ICH     1
  #define AU_CARDS_LINK_IHD     1
  #define AU_CARDS_LINK_SBLIVE  1
  #define AU_CARDS_LINK_VIA82XX 1
 #endif
#endif

#include <newfunc\newfunc.h>

typedef struct mpxplay_audioout_info_s{
 short *pcm_sample;
 unsigned int  samplenum;
 unsigned char bytespersample_card;
 unsigned int  freq_set;
 unsigned int  freq_song;
 unsigned int  freq_card;
 unsigned int  chan_set;
 unsigned char chan_song;
 unsigned char chan_card;
 unsigned int  bits_set;
 unsigned char bits_song;
 unsigned char bits_card;

 unsigned int   card_wave_id;    // 0x0001,0x0003,0x0055,0x2000,etc.
 char          *card_wave_name;  // currently file extension for -obs ("MP3","AC3")
 unsigned char *card_channelmap; // multichannel output mapping (WDS,WAV)
 unsigned long  card_select_devicenum;
 long           card_select_config;
 unsigned long  card_controlbits;  // card control flags
 unsigned long  card_infobits;     // card info flags
 unsigned long  card_outbytes;     // samplenum*bytespersample_card
 unsigned long  card_dma_buffer_size;
 unsigned long  card_dmasize;
 unsigned long  card_dmalastput;
 unsigned long  card_dmaspace;
 unsigned long  card_dmafilled;
 unsigned long  card_dma_lastgoodpos;
 unsigned int   card_bytespersign; // bytespersample_card*chan_card
 unsigned int   card_dmaout_under_int08;
 unsigned short int08_decoder_cycles;
 cardmem_t *card_dma_dosmem;
 char *card_DMABUFF;

 char *card_selectname;          // select card by name
 struct one_sndcard_info *card_handler; // function structure of the card
 void *card_private_data;        // extra private datas can be pointed here (with malloc)
 unsigned short card_type;
 unsigned short card_port;
 unsigned char  card_irq;
 unsigned char  card_isa_dma;
 unsigned char  card_isa_hidma;

 struct mainvars *mvp;
 struct playlist_entry_info *pei; // for encoders
 int card_master_volume;
 int card_mixer_values[AU_MIXCHANS_NUM]; // -1, 0-100
 //int card_mixer_values[AU_MIXCHANS_NUM][AU_MIXCHANFUNCS_NUM]; // -1, 0-100
}mpxplay_audioout_info_s;

typedef struct one_sndcard_info{
 char *shortname;
 unsigned long infobits;

 int  (*card_config)(struct mpxplay_audioout_info_s *); // not used yet
 int  (*card_init)(struct mpxplay_audioout_info_s *);   // read the environment variable and try to init the card
 int  (*card_detect)(struct mpxplay_audioout_info_s *); // try to autodetect the card
 void (*card_info)(struct mpxplay_audioout_info_s *);   // show card infos
 void (*card_start)(struct mpxplay_audioout_info_s *);  // start playing
 void (*card_stop)(struct mpxplay_audioout_info_s *);   // stop playing (immediately)
 void (*card_close)(struct mpxplay_audioout_info_s *);  // close soundcard
 void (*card_setrate)(struct mpxplay_audioout_info_s *);// set freqency,channels,bits

 void (*cardbuf_writedata)(struct mpxplay_audioout_info_s *,char *buffer,unsigned long bytes); // write output data into the card's buffer
 long (*cardbuf_pos)(struct mpxplay_audioout_info_s *);  // get the buffer (playing) position (usually the DMA buffer get-position)(returns negative number on error)
 void (*cardbuf_clear)(struct mpxplay_audioout_info_s *);// clear the soundcard buffer (usually the DMA buffer)
 void (*cardbuf_int_monitor)(struct mpxplay_audioout_info_s *); // interrupt (DMA) monitor function
 int (*irq_routine)(struct mpxplay_audioout_info_s *);  // as is

 void (*card_writemixer)(struct mpxplay_audioout_info_s *,unsigned long mixreg,unsigned long value);
 unsigned long (*card_readmixer)(struct mpxplay_audioout_info_s *,unsigned long mixreg);
 aucards_allmixerchan_s *card_mixerchans;
}one_sndcard_info;

//main soundcard routines
extern void AU_init(struct mpxplay_audioout_info_s *aui);
extern void AU_ini_interrupts(struct mpxplay_audioout_info_s *);
extern void AU_del_interrupts(struct mpxplay_audioout_info_s *aui);
extern void AU_prestart(struct mpxplay_audioout_info_s *);
extern void AU_start(struct mpxplay_audioout_info_s *);
extern void AU_wait_and_stop(struct mpxplay_audioout_info_s *);
extern void AU_stop(struct mpxplay_audioout_info_s *);
extern void AU_suspend_decoding(struct mpxplay_audioout_info_s *aui);
extern void AU_resume_decoding(struct mpxplay_audioout_info_s *aui);
extern void AU_close(struct mpxplay_audioout_info_s *);
extern void AU_setrate(struct mpxplay_audioout_info_s *aui,struct mpxplay_audio_decoder_info_s *adi);
extern void AU_setmixer_init(struct mpxplay_audioout_info_s *aui);
extern void AU_setmixer_one(struct mpxplay_audioout_info_s *,unsigned int mixch,unsigned int setmode,int value);
extern void AU_setmixer_outs(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int newvalue);
extern void AU_setmixer_all(struct mpxplay_audioout_info_s *);
extern void AU_clearbuffs(struct mpxplay_audioout_info_s *);
extern void AU_pause_process(struct mpxplay_audioout_info_s *);
#ifdef SBEMU
extern unsigned int AU_cardbuf_space(struct mpxplay_audioout_info_s *aui);
#endif
extern int  AU_writedata(struct mpxplay_audioout_info_s *);

#ifdef __cplusplus
}
#endif

#endif // au_cards_h
