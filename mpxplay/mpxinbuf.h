#ifndef mpxplay_mpxinbuf_h
#define mpxplay_mpxinbuf_h

#include "mpxplay.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MPXPLAY_WIN32
 #define MPXINBUF_USE_MUTEX 1
 #ifdef MPXPLAY_LINK_INFILE_FF_MPEG
  //#define MPXINBUF_USE_FILLBUF_MUTEX 1
  #define MPXINBUF_FILLBUF_MUTEX_TIMEOUT 10
 #endif
#endif

//prebuffertype and frp->buffertype controlbits
#define PREBUFTYPE_NONE             0  // -bn
#define PREBUFTYPE_SHORTRING    (1<<0) // -bs
#define PREBUFTYPE_LONGRING     (1<<1) // -bp
#define PREBUFTYPE_FULL         (1<<2) // -bl
#define PREBUFTYPE_RING         (1<<3)
#define PREBUFTYPE_MASK      (PREBUFTYPE_SHORTRING|PREBUFTYPE_LONGRING|PREBUFTYPE_RING|PREBUFTYPE_FULL)
#define PREBUFTYPE_PRELOADNEXT  (1<<4) // -bpn
#define PREBUFTYPE_BACK         (1<<5) // -bb
#define PREBUFTYPE_INT          (1<<6) // use intsound (-bp,-bl)
#define PREBUFTYPE_PUT_BACKBUF_PERCENT(t,p) t|=((p&0x7f)<<24)
#define PREBUFTYPE_GET_BACKBUF_PERCENT(t)   (((t)>>24)&0x7f)
#define PREBUFTYPE_BACKBUF_PERCENT_MAX     98
//frp->buffertype flags
#define PREBUFTYPE_FILLED           (1<< 8) // buffer is full
#define PREBUFTYPE_WRITEPROTECT     (1<< 9) // don't write into buffer (only read)
#define PREBUFTYPE_LOADNEXT_OK      (1<<10) // next file is opened
#define PREBUFTYPE_LOADNEXT_FAILED  (1<<11) // next file open failed
#define PREBUFTYPE_LOADNEXT_MASK    (PREBUFTYPE_LOADNEXT_OK|PREBUFTYPE_LOADNEXT_FAILED)
#define PREBUFTYPE_NON_SEEKABLE     (1<<12) // non-seekable media (live-stream)
#define PREBUFTYPE_IND_LOWFILE_OPEN (1<<13) // independent low-level file open/close (assign an existent low-level file to frp) (for stream copy pre-analyze)
#define PREBUFTYPE_BUFFFILL_PAUSE   (1<<14) // audio has paused due to few buffer data
#define PREBUFTYPE_BUFFFILL_NOCHK   (1<<20) // do not check buffer fullness (do not fill buffer in main thread) (for ffmpeg)
#define PREBUFTYPE_BUFFFILL_NOSKHLP (1<<21) // do not use seek helper (not needed) (for ffmpeg)
#define PREBUFTYPE_DONTCLEAR        (PREBUFTYPE_WRITEPROTECT|PREBUFTYPE_IND_LOWFILE_OPEN|PREBUFTYPE_BUFFFILL_NOCHK|PREBUFTYPE_BUFFFILL_NOSKHLP) // WRITEPROTECT (and all other bits) is cleared if no IND_LOWFILE (in mpxplay_mpxinbuf_fopen -> mpxplay_mpxinbuf_buffer_reset)

#define PREBUFFERBLOCKSIZE_CHECK   2048 // read block size
#define PREBUFFERBLOCKSIZE_DECODE 32768

#define PREBUFFERBLOCKS_SHORTRING  4 // default number of blocks in normal (non -bp) mode
#define PREBUFFERBLOCKS_LONGRING  32 // default number of blocks in -bp mode

#define PREBUFFERSIZE_CHECK (PREBUFFERBLOCKS_SHORTRING*PREBUFFERBLOCKSIZE_DECODE)

#define PREBUFFER_SEEKRETRY_INVALID  ((mpxp_filesize_t) -1)

extern void         mpxplay_mpxinbuf_init(struct mainvars *mvp);
extern void         mpxplay_mpxinbuf_assign_funcs(struct mpxpframe_s *frp);
extern void         mpxplay_mpxinbuf_prealloc(struct mainvars *mvp);
extern void         mpxplay_mpxinbuf_close(struct mainvars *mvp);
extern unsigned int mpxplay_mpxinbuf_alloc(struct mainvars *,struct mpxpframe_s *);
extern unsigned int mpxplay_mpxinbuf_alloc_ringbuffer(struct mainvars *mvp,struct mpxpframe_s *frp,unsigned long blocks);
extern unsigned int mpxplay_mpxinbuf_buffer_check(struct mpxpframe_s *frp, mpxp_bool_t wait_for_data);
extern unsigned int mpxplay_mpxinbuf_buffer_protected_check(struct mpxpframe_s *frp);
extern unsigned int mpxplay_mpxinbuf_buffer_fillness_check(struct mainvars *mvp,struct mpxpframe_s *frp);
extern int          mpxplay_mpxinbuf_buffer_fill(struct mpxpframe_s *frp, mpxp_uint8_t *dataptr, unsigned int datalen);
extern void         mpxplay_mpxinbuf_set_intsound(struct mpxpframe_s *frp,unsigned int intcfg);
extern void         mpxplay_mpxinbuf_buffer_reset(struct mpxpframe_s *frp);
extern unsigned int mpxplay_mpxinbuf_fopen(void *fbds,char *filename,unsigned long openmode,unsigned long file_blocksize);

extern struct mpxpframe_s *mpxplay_mpxinbuf_seekhelper_init(struct mpxpframe_s *frp);
extern void         mpxplay_mpxinbuf_seekhelper_close(struct mpxpframe_s *frp);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_mpxinbuf_h
