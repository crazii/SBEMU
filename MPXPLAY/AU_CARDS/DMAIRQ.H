#ifndef dmairq_h
#define dmairq_h

#include "mpxplay.h"
#ifdef __DOS__
#include <conio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AUCARDS_DMABUFSIZE_NORMAL 32768
#define AUCARDS_DMABUFSIZE_MAX    131072
#define AUCARDS_DMABUFSIZE_BLOCK  4096   // default page (block) size

#define DMAMODE_AUTOINIT_OFF 0
#define DMAMODE_AUTOINIT_ON  0x10

typedef struct au_isadma_s{
 unsigned int dma_low;
 unsigned int dma_high;
 cardmem_t *card_dma_dosmem;
}au_isadma_s;

extern cardmem_t *MDma_alloc_cardmem(unsigned int buffsize);
extern void MDma_free_cardmem(cardmem_t *dm);
extern unsigned int MDma_get_max_pcmoutbufsize(struct mpxplay_audioout_info_s *aui,unsigned int max_bufsize,unsigned int pagesize,unsigned int samplesize,unsigned long freq_config);
extern unsigned int MDma_init_pcmoutbuf(struct mpxplay_audioout_info_s *aui,unsigned int maxbufsize,unsigned int pagesize,unsigned long freq_config);

extern void MDma_clearbuf(struct mpxplay_audioout_info_s *aui);
extern void MDma_writedata(struct mpxplay_audioout_info_s *aui,char *src,unsigned long left);
extern void MDma_interrupt_monitor(struct mpxplay_audioout_info_s *aui);

extern void MDma_ISA_FreeMem(struct mpxplay_audioout_info_s *aui);
extern void MDma_ISA_init(struct mpxplay_audioout_info_s *aui);
extern void MDma_ISA_Start(struct mpxplay_audioout_info_s *aui,unsigned int dmamode,unsigned long initbufpos,unsigned long initbufsize);
extern void MDma_ISA_Stop(struct mpxplay_audioout_info_s *aui);
extern long MDma_ISA_getbufpos(struct mpxplay_audioout_info_s *aui);
extern void MDma_ISA_Clear(struct mpxplay_audioout_info_s *aui);
extern unsigned int MDma_ISA_autodetect(struct mpxplay_audioout_info_s *aui);

extern unsigned int MIrq_Start(unsigned int irq_num,void (*irq_routine)(struct mpxplay_audioout_info_s *),unsigned long *card_infobits);
extern void MIrq_Stop(unsigned int irq_num,unsigned long *card_infobits);
extern unsigned int MIrq_autodetect(struct mpxplay_audioout_info_s *aui);

#ifdef __DOS__

#ifndef DJGPP

#define ENTER_CRITICAL IRQ_PUSH_OFF()
 void IRQ_PUSH_OFF (void);
#pragma aux IRQ_PUSH_OFF = \
        "pushfd",          \
        "cli"              \
        modify [esp];

#define LEAVE_CRITICAL IRQ_POP()
 void IRQ_POP (void);
#pragma aux IRQ_POP =   \
        "popfd" \
        "sti"   \
        modify [esp];

#else//DJGPP

#define ENTER_CRITICAL int cpu__fd = IRQ_PUSH_OFF()
static int /*__attribute__((naked))*/ IRQ_PUSH_OFF (void) { asm( "pushfl \n\t popl %eax \n\t cli"); }

#define LEAVE_CRITICAL IRQ_POP(cpu__fd)
static void /*__attribute__((naked))*/ IRQ_POP (int fd) {asm("pushl %0 \n\t popfl" ::"r"(fd)); }

#endif

#else

#define ENTER_CRITICAL {}
#define LEAVE_CRITICAL {}

#endif // __DOS__

#ifdef __cplusplus
}
#endif

#endif // dmairq_h
