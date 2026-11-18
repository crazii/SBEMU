// SPDX-License-Identifier: GPL-2.0-only
//**************************************************************************
//  USB Audio Class 1.0 output (e.g. TI PCM2902, C-Media CM108) through an
//  OHCI USB controller for SBEMU.  Written for the eBOX-2300SX (DM&P
//  Vortex86SX, no FPU), which has no sound chip.
//
//  - finds an OHCI controller that is NOT used by the BIOS (USB keyboard!),
//  - takes it over, enumerates the device on its root ports,
//  - selects a 16-bit stereo 48 kHz isochronous OUT alternate setting,
//  - plays a ring buffer (card_DMABUFF) with one isochronous TD per USB frame,
//  - uses the OHCI WritebackDoneHead interrupt as the "sound card IRQ",
//  - optionally streams the card's input (LINE IN / microphone) for level
//    meters, monitoring and recording (control block in usbpcmct.h).
//
//  Integer arithmetic only (the target CPU has no FPU).
//
//  The driver is used ONLY when the environment variable USBPCM exists, e.g.
//  SET USBPCM=L32 (it takes over a USB controller, so it is opt-in).
//  Options in USBPCM:
//    Cn  use OHCI controller number n (order of PCI scan, 0 = first OHCI)
//    Ln  ring length (latency) in milliseconds (16..128, default 64)
//    V   verbose messages on screen
//    W   swap left/right (SBEMU_SWAP_STEREO is not applied to this card)
//    Rn  record the last n seconds (1..60) of playback and input into XMS
//        rings (control block "USBPCM01" in low memory, see usbpcmct.h)
//    I / O  with R: record only the input (I) or only the playback stream (O)
//    X   do not start the input stream (no input meters / monitor / recording)
//    N, E, Sn  diagnostics: no OHCI interrupts / leave EHCI alone / stop
//        detection at stage n
//  A log is written to USBPCM.LOG in the current directory.
//
//  Copyright (C) 2026 Lukasz Bolesta, with the help of Claude (Anthropic).
//  This program comes with ABSOLUTELY NO WARRANTY.
//**************************************************************************

#include "au_cards.h"

#ifdef AU_CARDS_LINK_USBPCM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pc.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/farptr.h>
#include <go32.h>
#include "dmairq.h"
#include "pcibios.h"
#include "usbpcmct.h"

#define USBPCM_RATE        48000
#define USBPCM_FRAMEBYTES  (48*2*2)   /* 48 stereo 16-bit samples per 1 ms frame */
#define USBPCM_IRQ_EVERY   4          /* WDH interrupt every 4 frames */
#define USBPCM_MAXFRAMES   128

/* OHCI registers */
#define HcRevision         0x00
#define HcControl          0x04
#define HcCommandStatus    0x08
#define HcInterruptStatus  0x0C
#define HcInterruptEnable  0x10
#define HcInterruptDisable 0x14
#define HcHCCA             0x18
#define HcControlHeadED    0x20
#define HcControlCurrentED 0x24
#define HcBulkHeadED       0x28
#define HcFmInterval       0x34
#define HcFmNumber         0x3C
#define HcPeriodicStart    0x40
#define HcLSThreshold      0x44
#define HcRhDescriptorA    0x48
#define HcRhDescriptorB    0x4C
#define HcRhStatus         0x50
#define HcRhPortStatus     0x54
#define HceControl         0x100

#define CTL_CBSR  3
#define CTL_PLE   (1<<2)
#define CTL_IE    (1<<3)
#define CTL_CLE   (1<<4)
#define CTL_HCFS  (3<<6)
#define CTL_OPER  (2<<6)
#define CTL_IR    (1<<8)
#define CTL_RWC   (1<<9)

#define PS_CCS  (1<<0)
#define PS_PES  (1<<1)
#define PS_PRS  (1<<4)
#define PS_PPS  (1<<8)
#define PS_LSDA (1<<9)
#define PS_CSC  (1<<16)
#define PS_PESC (1<<17)
#define PS_PRSC (1<<20)

#define INT_WDH (1<<1)
#define INT_MIE (1u<<31)
/* Vortex86SX: the MIE bit appears to be shared by both OHCI controllers - clearing it
   on the sound card's controller also silenced the controller with the USB keyboard
   (verified on an eBOX-2300SX). Never clear MIE; only clear interrupt sources. */
#define INT_SOURCES 0x4000007Fu

/* Memory layout.
   LOW block (conventional memory, < 1 MB): everything the host controller registers
   point to (HCCA, EDs, TDs). The AMI BIOS USB code keeps looking at the structures
   of its controllers from V86 mode and can only reach memory below 1 MB - if the
   HCCA is in XMS it faults and the USB keyboard dies.
   HIGH block (XMS): control-transfer data and the audio ring buffer.            */
#define HI       0x10000
#define O_HCCA   0x0000
#define O_CED    0x0100
#define O_IED    0x0110
#define O_IED2   0x0120   /* capture (LINE IN) endpoint */
#define O_CTD    0x0200   /* 4 x 32 */
#define O_SETUP  0x0280
#define MON_FIFO 1024     /* LINE IN monitor FIFO, stereo samples (21 ms) */
#define MON_AHEAD 10      /* monitor is added this many frames ahead of the play position */
#define NITD_IN  33       /* capture TD queue: 32 frames + dummy */
#define O_ITD2   0x0300                               /* NITD_IN x 32 = 1056 bytes */
#define O_CTL    (O_ITD2 + NITD_IN*32)                /* 0x0720, control block (usbpcmct.h) */
#define O_ITD    (O_CTL + 256)                        /* 0x0820, (ring frames + 1) x 32 */
#define LOW_TOTAL (O_ITD + (usbpcm_lat + 1) * 32)     /* 4.1 KB at the default 64 ms */
#define O_CBUF   (HI + 0x0000)  /* 2048 bytes control data */
#define O_RING   (HI + 0x1000)  /* ring buffer, up to 128*192 = 24576 bytes */
#define O_IBUF2  (HI + 0x7000)  /* capture packets, NITD_IN x 256 bytes */
#define HIGH_TOTAL (0x7000 + NITD_IN*256)

typedef struct usbpcm_alt_s {
 uint8_t ifn, alt, ep, nch, sub, nfreq, sync, fctl;
 uint16_t mps;
 uint32_t fmin, fmax, freq[8];
} usbpcm_alt_s;

typedef struct usbpcm_card_s {
 struct pci_config_s *pci_dev;
 int hcnum, port, ls;
 unsigned long mmio_phys;
 volatile char *mmio;
 cardmem_t *dm;
 char *base;               /* XMS block, 4 KB aligned */
 uint32_t base_phys;
 char *lo;                 /* conventional memory block, 256 B aligned */
 uint32_t lo_phys;
 int lo_sel;
 int owned;                /* controller taken over by us */
 uint8_t addr, mps0;
 usbpcm_alt_s alt;
 usbpcm_alt_s ain;         /* capture alternate setting (valid if has_in) */
 int has_in;
 /* capture queue */
 int hi, ti;
 uint16_t next_frame_in;
 /* recording */
 xmsmem_t rec;             /* both rings, one XMS block */
 int rec_ok;
 char *ring[2];            /* linear pointers of the rings */
 unsigned int silent;      /* consecutive digitally silent playback frames */
 /* LINE IN monitor (ctl->mon_gain): capture samples -> FIFO -> added to playback slots */
 int16_t mon_fifo[MON_FIFO * 2];
 unsigned int mon_rd, mon_wr; /* in stereo samples, mod MON_FIFO */
 uint32_t mon_next;        /* next playback frame to receive monitor samples */
 volatile usbpcm_ctl_t *ctl;
 /* streaming */
 int nframes;              /* ring length in frames */
 int ntd;                  /* nframes + 1 (dummy) */
 int h, t;                 /* TD queue head (oldest) and tail (dummy) */
 uint32_t queued_slot;     /* next ring slot to queue */
 uint32_t played;          /* retired frames counter */
 uint16_t next_frame;
 int streaming;
 uint32_t irqs, errors, late, resyncs;
 uint16_t vid, pid;
 char product[32];
} usbpcm_card_s;

#define MAXSIB 6
static struct { int ehci; volatile char *mmio; uint32_t op; } usbpcm_sib[MAXSIB];
static int usbpcm_nsib;
static uint32_t usbpcm_chained;

static int usbpcm_verbose;
static int usbpcm_noirq;      /* diagnostics: do not enable OHCI interrupts */
static int usbpcm_noehci;     /* diagnostics: do not touch EHCI */
static int usbpcm_stage;      /* diagnostics: stop detection at stage n */
static int usbpcm_force_hc = -1;
static int usbpcm_lat = 64;
static int usbpcm_rec_sec;     /* R: seconds to record, 0 = off */
static int usbpcm_rec_what = 3;/* bit0 playback, bit1 LINE IN */
static int usbpcm_noin;        /* X: do not start the LINE IN stream at all */
static FILE *usbpcm_logf;

static int usbpcm_log_started;
static void ulog(const char *fmt, ...)
{
 va_list ap;
 /* open/append/close every time: the TSR never closes files and DOS only
    updates the file size on close */
 usbpcm_logf = fopen("USBPCM.LOG", usbpcm_log_started ? "a" : "w");
 usbpcm_log_started = 1;
 if(usbpcm_logf){
  va_start(ap, fmt); vfprintf(usbpcm_logf, fmt, ap); va_end(ap);
  fclose(usbpcm_logf);
  usbpcm_logf = NULL;
 }
 if(usbpcm_verbose){
  va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
 }
}
static void ushow(const char *fmt, ...)   /* screen + log */
{
 va_list ap;
 char b[600];
 va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
 if(!usbpcm_verbose) printf("%s", b);
 ulog("%s", b);
}

/* ------------------------------------------------------------------ */
/* raw PCI config (we need class-code search)                          */
/* ------------------------------------------------------------------ */
static uint32_t pci_rd(int b, int d, int f, int r)
{ outportl(0xCF8, 0x80000000u | (b<<16) | (d<<11) | (f<<8) | (r & 0xFC)); return inportl(0xCFC); }

/* ------------------------------------------------------------------ */
/* register / memory access                                            */
/* ------------------------------------------------------------------ */
#define ORD(c,r)    (*(volatile uint32_t *)((c)->mmio + (r)))
#define OWR(c,r,v)  (*(volatile uint32_t *)((c)->mmio + (r)) = (uint32_t)(v))
#define P32(c,o)    ((volatile uint32_t *)(((o) & HI) ? (c)->base + ((o) & 0xFFFF) : (c)->lo + (o)))
#define P8(c,o)     ((volatile uint8_t *)(((o) & HI) ? (c)->base + ((o) & 0xFFFF) : (c)->lo + (o)))
#define PH(c,o)     (((o) & HI) ? (c)->base_phys + ((o) & 0xFFFF) : (c)->lo_phys + (o))

static uint16_t frame(usbpcm_card_s *c) { return (uint16_t)ORD(c, HcFmNumber); }

static void wait_ms_raw(unsigned int ms) { while(ms--) pds_delay_10us(100); }

static void wait_frames(usbpcm_card_s *c, unsigned int n)
{
 uint16_t s = frame(c);
 unsigned int guard = 0;
 while((uint16_t)(frame(c) - s) < n){
  pds_delay_10us(10);
  if(++guard > n * 20 + 100) break;           /* frame counter stuck */
 }
}

static const char *cc_name(unsigned int cc)
{
 static const char *n[17] = { "OK","CRC","BitStuffing","DataToggle","Stall","NotResponding","PIDCheck",
  "UnexpectedPID","DataOverrun","DataUnderrun","res10","res11","BufferOverrun","BufferUnderrun",
  "NotAccessed","NotAccessed","Timeout" };
 return n[cc > 16 ? 16 : cc];
}

/* ------------------------------------------------------------------ */
/* Physical memory below 1 MB (BIOS data at E000h, USB registers at E0000h-E3FFFh on
   the eBOX) must NOT be mapped and released through DPMI: releasing such a mapping
   can remove the page from the shared low-memory page table, which kills the BIOS
   USB stack (USB keyboard). Use the DOS selector there instead.                   */
#define LOWMEM(a) ((uint32_t)(a) < 0x10FFF0u)
static uint32_t prd(uint32_t a)
{
 uint32_t v;
 unsigned long m;
 if(LOWMEM(a)) return _farpeekl(_dos_ds, a);
 m = pds_dpmi_map_physical_memory(a & ~0xFFFu, 4096);
 if(!m) return 0xFFFFFFFFu;
 v = *(volatile uint32_t *)(m + (a & 0xFFF));
 pds_dpmi_unmap_physycal_memory(m);
 return v;
}
static void pwr(uint32_t a, uint32_t v)
{
 unsigned long m;
 if(LOWMEM(a)) { _farpokel(_dos_ds, a, v); return; }
 m = pds_dpmi_map_physical_memory(a & ~0xFFFu, 4096);
 if(!m) return;
 *(volatile uint32_t *)(m + (a & 0xFFF)) = v;
 pds_dpmi_unmap_physycal_memory(m);
}
static void unmap_if_high(unsigned long lin, uint32_t phys)
{
 if(lin && !LOWMEM(phys)) pds_dpmi_unmap_physycal_memory(lin);
 /* low memory mappings are kept (never released) */
}

/* ------------------------------------------------------------------ */
/* is the controller used by the BIOS? (active interrupt ED = keyboard) */
/* ------------------------------------------------------------------ */
static int any_port_enabled(volatile char *mmio)
{
 int p, n = *(volatile uint32_t *)(mmio + HcRhDescriptorA) & 0xFF;
 for(p = 0; p < n && p < 15; p++) if(*(volatile uint32_t *)(mmio + HcRhPortStatus + 4*p) & PS_PES) return 1;
 return 0;
}

static int bios_uses_hc(volatile char *mmio)
{
 uint32_t ctl = *(volatile uint32_t *)(mmio + HcControl);
 uint32_t hcca_phys, i, nonzero = 0;
 if(ctl & CTL_IR){ ulog("  owned by BIOS SMM (IR=1)\n"); return 1; }
 if((ctl & CTL_HCFS) != CTL_OPER || !(ctl & CTL_PLE))
  return 0;
 hcca_phys = *(volatile uint32_t *)(mmio + HcHCCA) & ~0xFFu;
 if(!hcca_phys) return any_port_enabled(mmio);
 for(i = 0; i < 32; i++){
  uint32_t ed = prd(hcca_phys + i*4) & ~0xFu;
  int hops = 0;
  if(ed && ed != 0xFFFFFFF0u) nonzero++;
  while(ed && ed != 0xFFFFFFF0u && hops++ < 8){
   uint32_t w0 = prd(ed), next = prd(ed + 12) & ~0xFu;
   if(!(w0 & (1<<14)) && (w0 & 0x7F)){
    ulog("  BIOS polls device %d EP %d (interrupt ED %08X)\n", w0 & 0x7F, (w0 >> 7) & 15, ed);
    return 1;
   }
   ed = next;
  }
 }
 if(!nonzero && any_port_enabled(mmio)){ ulog("  periodic table empty but a port is enabled - assuming BIOS use\n"); return 1; }
 return 0;
}

/* EHCI: hand full/low-speed devices that EHCI did not enable over to the OHCI companions */
static void ehci_release_all(void)
{
 int b, d, f;
 for(b = 0; b < 2; b++) for(d = 0; d < 32; d++) for(f = 0; f < 8; f++){
  uint32_t id = pci_rd(b, d, f, 0), cls, bar, op, n, p;
  if((id & 0xFFFF) == 0xFFFF || !id) { if(!f) break; continue; }
  cls = pci_rd(b, d, f, 8) >> 8;
  if(cls != 0x0C0320) continue;
  bar = pci_rd(b, d, f, 0x10) & ~0xFu;
  op = prd(bar & ~3u) >> ((bar & 3) * 8) & 0xFF;     /* CAPLENGTH */
  n = prd(bar + 4) & 15;
  ulog("EHCI %d:%02X.%d CONFIGFLAG=%X\n", b, d, f, prd(bar + op + 0x40));
  for(p = 0; p < n; p++){
   uint32_t a = bar + op + 0x44 + 4*p, st = prd(a);
   ulog("  port %d: %08X\n", p + 1, st);
   if((st & 1) && !(st & 4) && !(st & (1<<13))){
    pwr(a, (st & ~0x2Au) | (1u<<13));
    ulog("  -> handed to companion OHCI\n");
    wait_ms_raw(100);
   }
  }
 }
}


/* BIOS USB data (AMI, from the BIOS dump): busy flag at E623:0032h */
#define BIOS_USB_BUSY 0xE6232u
static int bios_busy(void) { return _farpeekb(_dos_ds, BIOS_USB_BUSY); }

static int usbpcm_vec_done;
static void neutralize_bios_irq(int card_irq)
{
 /* The BIOS drove this controller before us and may still have its own USB
    interrupt handler on our IRQ. If it ran on our interrupts it would process our
    transfer descriptors as its own and break the BIOS USB stack (USB keyboard on
    the other controller stops working). Replace the real-mode vector with a
    neutral EOI stub BEFORE SBEMU installs its handlers (SBEMU chains to it). */
  if(card_irq > 0 && card_irq < 16 && card_irq != 2 && !getenv("USBPCM_KEEPVEC")){
  int vec = card_irq < 8 ? 8 + card_irq : 0x70 + card_irq - 8;
  __dpmi_raddr old, nr;
  int sel, seg;
  static const uint8_t stub_hi[] = { 0x50, 0xB0, 0x20, 0xE6, 0xA0, 0xE6, 0x20, 0x58, 0xCF }; /* push ax; mov al,20h; out A0h,al; out 20h,al; pop ax; iret */
  static const uint8_t stub_lo[] = { 0x50, 0xB0, 0x20, 0xE6, 0x20, 0x58, 0xCF, 0x90, 0x90 };
  __dpmi_get_real_mode_interrupt_vector(vec, &old);
  seg = __dpmi_allocate_dos_memory(1, &sel);
  if(seg > 0){
   dosmemput(card_irq >= 8 ? stub_hi : stub_lo, 9, seg * 16);
   nr.segment = seg; nr.offset16 = 0;
   __dpmi_set_real_mode_interrupt_vector(vec, &nr);
   ushow("USBPCM: IRQ %d vector %02Xh was %04X:%04X - replaced by EOI stub at %04X:0000\n",
         card_irq, vec, old.segment, old.offset16, seg);
  }
 }
 usbpcm_vec_done = 1;
}

/* ------------------------------------------------------------------ */
/* controller takeover                                                 */
/* ------------------------------------------------------------------ */
static int ohci_takeover(usbpcm_card_s *c)
{
 uint32_t ctl = ORD(c, HcControl), rev = ORD(c, HcRevision), fmi, i;
 ulog("HcRevision=%08X HcControl=%08X FmInterval=%08X RhDescA=%08X\n", rev, ctl, ORD(c,HcFmInterval), ORD(c,HcRhDescriptorA));
 if(ctl & CTL_IR){
  OWR(c, HcInterruptEnable, 1u<<30);
  OWR(c, HcCommandStatus, 1u<<3);
  for(i = 0; i < 200 && (ORD(c, HcControl) & CTL_IR); i++) wait_ms_raw(10);
  if(ORD(c, HcControl) & CTL_IR){ ushow("USBPCM: BIOS (SMM) did not release the USB controller\n"); return 0; }
 }
 OWR(c, HcInterruptDisable, INT_SOURCES);
 /* HceControl (legacy keyboard emulation) is deliberately left untouched:
    the BIOS may need it for the USB keyboard on another controller */
 OWR(c, HcControl, ctl & CTL_RWC);
 (void)ORD(c, HcControl);
 wait_ms_raw(60);
 fmi = ORD(c, HcFmInterval) & 0x3FFF;
 if(fmi < 0x2E00 || fmi > 0x2FC0) fmi = 0x2EDF;
 OWR(c, HcCommandStatus, 1);
 for(i = 0; i < 1000 && (ORD(c, HcCommandStatus) & 1); i++) pds_delay_10us(1);
 if(ORD(c, HcCommandStatus) & 1){ ushow("USBPCM: controller reset failed\n"); return 0; }
 OWR(c, HcInterruptEnable, INT_MIE);   /* HCR cleared MIE - restore it (shared, see INT_MIE) */
 OWR(c, HcControlHeadED, 0);
 OWR(c, HcBulkHeadED, 0);
 OWR(c, HcHCCA, PH(c, O_HCCA));
 {
  uint32_t fsmps = (6 * (fmi - 210)) / 7;
  uint32_t fit = ORD(c, HcFmInterval) & 0x80000000u;
  OWR(c, HcFmInterval, (fit ^ 0x80000000u) | (fsmps << 16) | fmi);
  OWR(c, HcPeriodicStart, (fmi * 9) / 10);
 }
 OWR(c, HcControl, (ctl & CTL_RWC) | CTL_CBSR | CTL_OPER);
 OWR(c, HcLSThreshold, 0x628);
 OWR(c, HcInterruptStatus, 0x7F);
 {
  uint32_t a = ORD(c, HcRhDescriptorA), potpgt = (a >> 24) * 2;
  a &= ~((1u<<8) | (1u<<11));
  OWR(c, HcRhDescriptorA, a);
  OWR(c, HcRhStatus, 1u<<16);
  OWR(c, HcRhDescriptorB, (a & (1u<<9)) ? 0 : 0xFFFF0000u);
  if(potpgt < 100) potpgt = 100;
  wait_ms_raw(potpgt);
 }
 {
  uint16_t f1 = frame(c); wait_ms_raw(20);
  ulog("frame counter %u -> %u, HcControl=%08X HCCA=%08X\n", f1, frame(c), ORD(c,HcControl), ORD(c,HcHCCA));
  if(f1 == frame(c)){ ushow("USBPCM: USB frame counter not running\n"); return 0; }
 }
 return 1;
}

static int port_reset(usbpcm_card_s *c, int p)
{
 uint32_t r = HcRhPortStatus + 4*p, st;
 int i, k;
 if(!(ORD(c, r) & PS_CCS)) return 0;
 wait_frames(c, 100);
 for(k = 0; k < 5; k++){
  OWR(c, r, PS_PRS);
  for(i = 0; i < 50 && !(ORD(c, r) & PS_PRSC); i++) wait_frames(c, 1);
  OWR(c, r, PS_PRSC);
 }
 OWR(c, r, PS_CSC | PS_PESC);
 wait_frames(c, 20);
 st = ORD(c, r);
 ulog("port %d after reset: %08X\n", p + 1, st);
 if(!(st & PS_PES)) return 0;
 c->ls = (st & PS_LSDA) ? 1 : 0;
 return 1;
}

/* ------------------------------------------------------------------ */
/* control transfers                                                   */
/* ------------------------------------------------------------------ */
static int control(usbpcm_card_s *c, uint8_t addr, uint16_t mps, uint8_t rt, uint8_t req, uint16_t val,
                   uint16_t idx, uint16_t len, void *data, int *actual)
{
 volatile uint32_t *ed = P32(c, O_CED);
 volatile uint32_t *td0 = P32(c, O_CTD), *td1 = P32(c, O_CTD+32), *td2 = P32(c, O_CTD+64);
 volatile uint8_t *su = P8(c, O_SETUP);
 int in = (rt & 0x80) != 0, i;
 uint32_t cc, c0, c1, c2;
 uint16_t t0;
 if(len > 2048) len = 2048;
 OWR(c, HcControl, ORD(c, HcControl) & ~CTL_CLE);
 wait_frames(c, 2);
 su[0]=rt; su[1]=req; su[2]=(uint8_t)val; su[3]=(uint8_t)(val>>8);
 su[4]=(uint8_t)idx; su[5]=(uint8_t)(idx>>8); su[6]=(uint8_t)len; su[7]=(uint8_t)(len>>8);
 if(!in && len) memcpy((void *)P8(c, O_CBUF), data, len);
 for(i = 0; i < 32; i++) td0[i] = 0;
 td0[0] = (0xFu<<28) | (2u<<24) | (7u<<21);
 td0[1] = PH(c, O_SETUP); td0[3] = PH(c, O_SETUP) + 7;
 if(len){
  td0[2] = PH(c, O_CTD+32);
  td1[0] = (0xFu<<28) | (3u<<24) | (7u<<21) | ((in ? 2u : 1u) << 19) | (in ? (1u<<18) : 0);
  td1[1] = PH(c, O_CBUF); td1[3] = PH(c, O_CBUF) + len - 1; td1[2] = PH(c, O_CTD+64);
 } else td0[2] = PH(c, O_CTD+64);
 td2[0] = (0xFu<<28) | (3u<<24) | (7u<<21) | ((in && len ? 1u : 2u) << 19);
 td2[1] = 0; td2[3] = 0; td2[2] = PH(c, O_CTD+96);
 ed[0] = addr | (c->ls ? (1u<<13) : 0) | ((uint32_t)mps << 16);
 ed[1] = PH(c, O_CTD+96);
 ed[2] = PH(c, O_CTD);
 ed[3] = 0;
 OWR(c, HcControlHeadED, PH(c, O_CED));
 OWR(c, HcControlCurrentED, 0);
 OWR(c, HcControl, ORD(c, HcControl) | CTL_CLE);
 OWR(c, HcCommandStatus, 1u<<1);
 t0 = frame(c);
 while(((ed[2] & ~0xFu) != PH(c, O_CTD+96)) && !(ed[2] & 1)){
  if((uint16_t)(frame(c) - t0) > 1000) break;
 }
 if(actual) *actual = 0;
 c0 = td0[0] >> 28; c1 = len ? (td1[0] >> 28) : 0; c2 = td2[0] >> 28;
 cc = c0 ? c0 : (c1 ? c1 : c2);
 if((ed[2] & ~0xFu) != PH(c, O_CTD+96) && !(ed[2] & 1) && cc == 0xF) cc = 16;
 if(cc){
  ulog("  CTRL %02X %02X %04X %04X %u: %s (%X %X %X)\n", rt, req, val, idx, len, cc_name(cc), c0, c1, c2);
  ed[2] = PH(c, O_CTD+96);
  return 0;
 }
 if(len && in){
  uint32_t n = td1[1] ? (td1[1] - PH(c, O_CBUF)) : len;
  if(n > len) n = len;
  memcpy(data, (void *)P8(c, O_CBUF), n);
  if(actual) *actual = n;
 } else if(actual) *actual = len;
 return 1;
}

/* ------------------------------------------------------------------ */
/* descriptors                                                         */
/* ------------------------------------------------------------------ */
static int alt_has_rate(usbpcm_alt_s *a, uint32_t r)
{
 int k;
 if(a->nfreq == 0) return r >= a->fmin && r <= a->fmax;
 for(k = 0; k < a->nfreq && k < 8; k++) if(a->freq[k] == r) return 1;
 return 0;
}

static int usbpcm_acif, usbpcm_nfu;
static uint8_t usbpcm_fu[8];   /* Feature Unit IDs (volume/mute) */
/* Audio Control topology: terminals and units with their sources, used to tell a
   playback/record Feature Unit from a "sidetone" one (microphone mixed into the
   speaker inside the device - e.g. GeneralPlus 1B3F:2008), which must stay muted. */
#define MAXUNIT 16
static struct { uint8_t id, kind, nsrc, src[4]; uint16_t ttype; } usbpcm_unit[MAXUNIT];
static int usbpcm_nunit;
enum { U_IT = 2, U_OT = 3, U_MU = 4, U_SU = 5, U_FU = 6 };
static int unit_idx(int id){ int i; for(i = 0; i < usbpcm_nunit; i++) if(usbpcm_unit[i].id == id) return i; return -1; }
/* bit0: fed by a USB streaming input terminal, bit1: fed by a physical input (mic/line) */
static int unit_inputs(int id, int depth)
{
 int i = unit_idx(id), k, r = 0;
 if(i < 0 || depth > 8) return 0;
 if(usbpcm_unit[i].kind == U_IT) return usbpcm_unit[i].ttype == 0x0101 ? 1 : 2;
 for(k = 0; k < usbpcm_unit[i].nsrc; k++) r |= unit_inputs(usbpcm_unit[i].src[k], depth + 1);
 return r;
}
/* bit0: reaches a USB streaming output terminal (recording), bit1: reaches a speaker/line out */
static int unit_outputs(int id, int depth)
{
 int i, k, r = 0;
 if(depth > 8) return 0;
 for(i = 0; i < usbpcm_nunit; i++)
  for(k = 0; k < usbpcm_unit[i].nsrc; k++)
   if(usbpcm_unit[i].src[k] == id){
    if(usbpcm_unit[i].kind == U_OT) r |= usbpcm_unit[i].ttype == 0x0101 ? 1 : 2;
    else r |= unit_outputs(usbpcm_unit[i].id, depth + 1);
   }
 return r;
}

static int find_alt(const uint8_t *d, int len, usbpcm_alt_s *best, usbpcm_alt_s *bestin, int *foundin)
{
 int i = 0, cls = 0, sub = 0, ifn = 0, altn = 0, have = 0, found = 0;
 *foundin = 0;
 usbpcm_alt_s cur;
 memset(&cur, 0, sizeof cur);
 usbpcm_nfu = 0; usbpcm_acif = 0; usbpcm_nunit = 0;
 while(i + 2 <= len){
  int bl = d[i], bt = d[i+1];
  const uint8_t *p = d + i;
  if(bl < 2 || i + bl > len) break;
  if(bt == 4 && bl >= 9){ ifn = p[2]; altn = p[3]; cls = p[5]; sub = p[6]; have = 0; if(cls == 1 && sub == 1) usbpcm_acif = ifn; }
  else if(bt == 0x24 && cls == 1 && sub == 1 && bl >= 4 && p[2] >= U_IT && p[2] <= U_FU && usbpcm_nunit < MAXUNIT){
   int n = usbpcm_nunit++, k;
   memset(&usbpcm_unit[n], 0, sizeof usbpcm_unit[n]);
   usbpcm_unit[n].id = p[3]; usbpcm_unit[n].kind = p[2];
   switch(p[2]){
    case U_IT: if(bl >= 6) usbpcm_unit[n].ttype = p[4] | (p[5] << 8); break;
    case U_OT: if(bl >= 8){ usbpcm_unit[n].ttype = p[4] | (p[5] << 8); usbpcm_unit[n].nsrc = 1; usbpcm_unit[n].src[0] = p[7]; } break;
    case U_FU: if(bl >= 5){ usbpcm_unit[n].nsrc = 1; usbpcm_unit[n].src[0] = p[4]; } if(usbpcm_nfu < 8) usbpcm_fu[usbpcm_nfu++] = p[3]; break;
    case U_MU: case U_SU:
     if(bl >= 5) for(k = 0; k < p[4] && k < 4 && 5 + k < bl; k++) usbpcm_unit[n].src[usbpcm_unit[n].nsrc++] = p[5 + k];
     break;
   }
  }
  else if(bt == 0x24 && cls == 1 && sub == 2 && p[2] == 2 && bl >= 8 && p[3] == 1){
   int k;
   memset(&cur, 0, sizeof cur);
   cur.ifn = ifn; cur.alt = altn; cur.nch = p[4]; cur.sub = p[5]; cur.nfreq = p[7];
   if(cur.nfreq == 0 && bl >= 14){
    cur.fmin = p[8] | (p[9]<<8) | ((uint32_t)p[10]<<16);
    cur.fmax = p[11] | (p[12]<<8) | ((uint32_t)p[13]<<16);
   } else for(k = 0; k < cur.nfreq && k < 8 && 10 + 3*k < bl; k++)
    cur.freq[k] = p[8+3*k] | (p[9+3*k]<<8) | ((uint32_t)p[10+3*k]<<16);
   have = 1;
  } else if(bt == 5 && bl >= 7 && cls == 1 && sub == 2 && have && (p[3] & 3) == 1){
   int isin = (p[2] & 0x80) != 0;
   cur.ep = p[2] & 15; cur.mps = (p[4] | (p[5]<<8)) & 0x7FF; cur.sync = (p[3] >> 2) & 3;
   ulog("  alt: if %d alt %d ep %d %s ch %d bytes %d mps %d %s48k\n", cur.ifn, cur.alt, cur.ep, isin ? "IN" : "OUT",
        cur.nch, cur.sub, cur.mps, alt_has_rate(&cur, USBPCM_RATE) ? "" : "NO ");
   if(!isin && cur.nch == 2 && cur.sub == 2 && cur.mps >= USBPCM_FRAMEBYTES && cur.mps <= 256 && alt_has_rate(&cur, USBPCM_RATE) && !found){
    *best = cur; found = 1;
   }
   /* capture: 16-bit stereo (PCM2902) or 16-bit mono (C-Media CM108 microphone); stereo preferred */
   if(isin && cur.sub == 2 && (cur.nch == 2 || cur.nch == 1) && cur.mps >= 48 * 2 * cur.nch && cur.mps <= 256 &&
      alt_has_rate(&cur, USBPCM_RATE) && (!*foundin || (bestin->nch == 1 && cur.nch == 2))){
    *bestin = cur; *foundin = 1;
   }
   have = 0;
  }
  i += bl;
 }
 return found;
}

static int enumerate(usbpcm_card_s *c, int p)
{
 uint8_t dd[18], c9[9];
 static uint8_t cfg[2048];
 int fin;
 int act;
 uint16_t total;
 if(!port_reset(c, p)) return 0;
 if(!control(c, 0, 8, 0x80, 6, 0x0100, 0, 8, dd, &act)) return 0;
 c->mps0 = dd[7];
 if(c->mps0 != 8 && c->mps0 != 16 && c->mps0 != 32 && c->mps0 != 64) c->mps0 = 8;
 if(!port_reset(c, p)) return 0;
 if(!control(c, 0, c->mps0, 0x00, 5, 1, 0, 0, NULL, NULL)) return 0;
 wait_frames(c, 10);
 c->addr = 1;
 if(!control(c, 1, c->mps0, 0x80, 6, 0x0100, 0, 18, dd, &act) || act < 18) return 0;
 c->vid = dd[8] | (dd[9]<<8); c->pid = dd[10] | (dd[11]<<8);
 ulog("device VID %04X PID %04X class %02X\n", c->vid, c->pid, dd[4]);
 c->product[0] = 0;
 if(dd[15]){
  uint8_t sd[64]; int k, m = 0;
  if(control(c, 1, c->mps0, 0x80, 6, 0x0300 | dd[15], 0x0409, 64, sd, &act) && act > 2){
   for(k = 2; k + 1 < act && m < 31; k += 2) c->product[m++] = (sd[k] >= 32 && sd[k] < 127) ? sd[k] : '?';
   c->product[m] = 0;
   while(m > 0 && c->product[m-1] == ' ') c->product[--m] = 0;
  }
 }
 if(!control(c, 1, c->mps0, 0x80, 6, 0x0200, 0, 9, c9, &act) || act < 9) return 0;
 total = c9[2] | (c9[3]<<8); if(total > sizeof cfg) total = sizeof cfg;
 if(!control(c, 1, c->mps0, 0x80, 6, 0x0200, 0, total, cfg, &act)) return 0;
 if(!find_alt(cfg, act, &c->alt, &c->ain, &fin)){ ulog("no 16-bit stereo 48 kHz output on this device\n"); return 0; }
 if(!control(c, 1, c->mps0, 0x00, 9, c9[5], 0, 0, NULL, NULL)) return 0;
 if(!control(c, 1, c->mps0, 0x01, 11, c->alt.alt, c->alt.ifn, 0, NULL, NULL)) return 0;
 {
  uint8_t f[3] = { USBPCM_RATE & 0xFF, (USBPCM_RATE >> 8) & 0xFF, USBPCM_RATE >> 16 };
  control(c, 1, c->mps0, 0x22, 1, 0x0100, c->alt.ep, 3, f, NULL);   /* may stall (PCM2902) */
  {   /* Feature Units: mute off, volume 0 dB on master and channels 1-2 (some devices,
         e.g. C-Media CM108, may start muted or attenuated). Stalls = control not present. */
   int u, ch;
   for(u = 0; u < usbpcm_nfu; u++){
    uint16_t idx = (uint16_t)((usbpcm_fu[u] << 8) | usbpcm_acif);
    int okm = 0, okv = 0, in = unit_inputs(usbpcm_fu[u], 0), out = unit_outputs(usbpcm_fu[u], 0);
    if(!(in & 1) && (in & 2) && (out & 2) && !(out & 1)){
     /* microphone/line straight to the speaker inside the device: keep it muted */
     uint8_t one = 1;
     for(ch = 0; ch <= 2; ch++) if(control(c, 1, c->mps0, 0x21, 1, (uint16_t)(0x0100 | ch), idx, 1, &one, NULL)) okm |= 1 << ch;
     ulog("feature unit %d: input monitor (sidetone) inside the device - muted on channels %x\n", usbpcm_fu[u], okm);
     continue;
    }
    for(ch = 0; ch <= 2; ch++){
     uint8_t z1 = 0, z2[2] = { 0, 0 };
     if(control(c, 1, c->mps0, 0x21, 1, (uint16_t)(0x0100 | ch), idx, 1, &z1, NULL)) okm |= 1 << ch;
     if(control(c, 1, c->mps0, 0x21, 1, (uint16_t)(0x0200 | ch), idx, 2, z2, NULL)) okv |= 1 << ch;
    }
    ulog("feature unit %d (%s): mute off on channels %x, 0 dB on channels %x\n", usbpcm_fu[u],
         (in & 1) ? "playback" : "recording", okm, okv);
   }
  }
  c->has_in = 0;
  if(fin && !usbpcm_noin){   /* LINE IN: meters, monitor (USBMIX), recording (R) */
   if(control(c, 1, c->mps0, 0x01, 11, c->ain.alt, c->ain.ifn, 0, NULL, NULL)){
    control(c, 1, c->mps0, 0x22, 1, 0x0100, 0x80 | c->ain.ep, 3, f, NULL);  /* capture rate (PCM2902 has several) */
    c->has_in = 1;
    ulog("capture: if %d alt %d ep %d IN mps %d, %s\n", c->ain.ifn, c->ain.alt, c->ain.ep, c->ain.mps,
         c->ain.nch == 1 ? "mono (copied to both channels)" : "stereo");
   } else ulog("capture: SET_INTERFACE failed - LINE IN not recorded\n");
  } else if(!usbpcm_noin)
   ulog("capture: no 16-bit 48 kHz input (stereo or mono) on this device\n");
 }
 return 1;
}

/* ------------------------------------------------------------------ */
/* isochronous streaming                                               */
/* ------------------------------------------------------------------ */
#define TDOFF(i) (O_ITD + 32*(i))

static void queue_one(usbpcm_card_s *c)
{
 volatile uint32_t *td = P32(c, TDOFF(c->t));
 int nt = (c->t + 1) % c->ntd;
 volatile uint32_t *nd = P32(c, TDOFF(nt));
 uint32_t slot = c->queued_slot % c->nframes;
 uint32_t bp = PH(c, O_RING) + slot * USBPCM_FRAMEBYTES;
 int di = ((slot % USBPCM_IRQ_EVERY) == USBPCM_IRQ_EVERY - 1) ? 0 : 7;
 nd[0] = nd[1] = nd[2] = nd[3] = nd[4] = nd[5] = nd[6] = nd[7] = 0;
 td[1] = bp & ~0xFFFu;
 td[2] = PH(c, TDOFF(nt));
 td[3] = bp + USBPCM_FRAMEBYTES - 1;
 td[4] = 0xE000u | (bp & 0xFFFu);
 td[5] = td[6] = td[7] = 0;
 td[0] = (0xFu<<28) | ((uint32_t)di << 21) | c->next_frame;
 c->next_frame++;
 c->queued_slot++;
 c->t = nt;
 P32(c, O_IED)[1] = PH(c, TDOFF(nt));
}

#define TDOFF2(i) (O_ITD2 + 32*(i))

static void queue_in(usbpcm_card_s *c)
{
 volatile uint32_t *td = P32(c, TDOFF2(c->ti));
 int nt = (c->ti + 1) % NITD_IN;
 volatile uint32_t *nd = P32(c, TDOFF2(nt));
 uint32_t bp = PH(c, O_IBUF2 + 256 * c->ti);
 nd[0] = nd[1] = nd[2] = nd[3] = nd[4] = nd[5] = nd[6] = nd[7] = 0;
 td[1] = bp & ~0xFFFu;
 td[2] = PH(c, TDOFF2(nt));
 td[3] = bp + c->ain.mps - 1;
 td[4] = 0xE000u | (bp & 0xFFFu);
 td[5] = td[6] = td[7] = 0;
 td[0] = (0xFu<<28) | (7u << 21) | c->next_frame_in;    /* no interrupt: serviced with the OUT stream */
 c->next_frame_in++;
 c->ti = nt;
 P32(c, O_IED2)[1] = PH(c, TDOFF2(nt));
}

/* level meters: max |sample| per channel (16-bit stereo) */
static void peaks(volatile usbpcm_ctl_t *k, int st, const volatile int16_t *s, unsigned int nsamp)
{
 unsigned int i, pl = k->st[st].peak[0], pr = k->st[st].peak[1];
 for(i = 0; i < nsamp; i++){
  int l = s[2*i], r = s[2*i+1];
  if(l < 0) l = -l;
  if(r < 0) r = -r;
  if((unsigned)l > pl) pl = l;
  if((unsigned)r > pr) pr = r;
 }
 k->st[st].peak[0] = pl > 32767 ? 32767 : pl;
 k->st[st].peak[1] = pr > 32767 ? 32767 : pr;
}

/* append n bytes (NULL = silence) to recording ring st */
static void rec_put(usbpcm_card_s *c, int st, const volatile void *src, unsigned int n)
{
 volatile usbpcm_ctl_t *k = c->ctl;
 unsigned long size, pos, first;
 char *r = c->ring[st];
 if(!r || (k->ctl & UPC_PAUSE)) return;
 if((k->flags & UPF_GATED) && !(k->ctl & UPC_NOGATE)) return;
 size = k->st[st].size;
 pos = k->st[st].wr % size;
 first = size - pos;
 if(first > n) first = n;
 if(src){
  memcpy(r + pos, (const void *)src, first);
  if(n > first) memcpy(r, (const char *)src + first, n - first);
 } else {
  memset(r + pos, 0, first);
  if(n > first) memset(r, 0, n - first);
 }
 k->st[st].wr += n;
}

static void service_in(usbpcm_card_s *c)
{
 volatile uint32_t *ed = P32(c, O_IED2);
 volatile usbpcm_ctl_t *k = c->ctl;
 uint32_t hp = ed[2];
 if(hp & 1){ ed[2] = hp & ~1u; c->errors++; }
 hp &= ~0xFu;
 while(c->hi != c->ti && PH(c, TDOFF2(c->hi)) != hp){
  volatile uint32_t *td = P32(c, TDOFF2(c->hi));
  uint32_t cc = td[0] >> 28, psw = td[4] & 0xFFFF, pcc = psw >> 12, sz = psw & 0x7FF;
  if(cc != 8 && (pcc == 0 || pcc == 9)){
   if(sz){
    const volatile int16_t *d = (const volatile int16_t *)P8(c, O_IBUF2 + 256 * c->hi);
    if(sz > c->ain.mps) sz = c->ain.mps;
    if(c->ain.nch == 1){            /* mono capture: duplicate to L and R */
     static int16_t st[128 * 2];
     unsigned int n = sz / 2, j;
     if(n > 128) n = 128;
     for(j = 0; j < n; j++) st[2*j] = st[2*j+1] = d[j];
     d = st; sz = n * 4;
    }
    sz &= ~3u;
    peaks(k, 1, d, sz / 4);
    rec_put(c, 1, d, sz);
    if(k->mon_gain){
     unsigned int n = sz / 4, j;
     for(j = 0; j < n; j++){
      unsigned int nw = (c->mon_wr + 1) % MON_FIFO;
      if(nw == c->mon_rd) break;                  /* full: drop */
      c->mon_fifo[2*c->mon_wr] = d[2*j]; c->mon_fifo[2*c->mon_wr+1] = d[2*j+1];
      c->mon_wr = nw;
     }
    }
    if(sz > k->in_maxpkt) k->in_maxpkt = sz;
    if(!k->in_minpkt || sz < k->in_minpkt) k->in_minpkt = sz;
   }
  } else {
   if(cc == 8) c->late++; else c->errors++;
   k->st[1].lost++;
   rec_put(c, 1, NULL, USBPCM_FRAMEBYTES);
  }
  k->st[1].frames++;
  c->hi = (c->hi + 1) % NITD_IN;
 }
 if((int16_t)(c->next_frame_in - frame(c)) < 2){
  uint16_t nf = frame(c) + 3;
  unsigned int gap = (uint16_t)(nf - c->next_frame_in);
  if(gap > 1000) gap = 1000;
  k->st[1].lost += gap;
  while(gap--) rec_put(c, 1, NULL, USBPCM_FRAMEBYTES);   /* keep the time line */
  c->next_frame_in = nf;
  c->resyncs++;
 }
 while(((c->ti + 1) % NITD_IN) != c->hi)
  queue_in(c);
}

/* LINE IN monitor: add capture samples (gain ctl->mon_gain/256) to playback slots that
   SBEMU has already filled (the ring is kept full ahead of the play position) but the
   host controller has not sent yet (>= 3 frames ahead). */
static void monitor(usbpcm_card_s *c)
{
 volatile usbpcm_ctl_t *k = c->ctl;
 int32_t g = k->mon_gain;
 uint32_t target = c->played + MON_AHEAD;
 unsigned int fill;
 if(!g){ c->mon_rd = c->mon_wr; c->mon_next = target; return; }
 if(g > 512) g = 512;
 if((int32_t)(c->mon_next - (c->played + 3)) < 0) c->mon_next = c->played + 3;
 fill = (c->mon_wr + MON_FIFO - c->mon_rd) % MON_FIFO;
 if(fill > 48 * 8) c->mon_rd = (c->mon_wr + MON_FIFO - 48 * 4) % MON_FIFO;   /* keep latency low */
 while((int32_t)(c->mon_next - target) < 0){
  volatile int16_t *d = (volatile int16_t *)P8(c, O_RING + (c->mon_next % c->nframes) * USBPCM_FRAMEBYTES);
  int j;
  if((c->mon_wr + MON_FIFO - c->mon_rd) % MON_FIFO < 48) break;   /* wait for capture data */
  for(j = 0; j < 96; j++){
   int32_t v = d[j] + ((int32_t)c->mon_fifo[2*c->mon_rd + (j & 1)] * g >> 8);
   if(v > 32767) v = 32767; else if(v < -32768) v = -32768;
   d[j] = (int16_t)v;
   if(j & 1) c->mon_rd = (c->mon_rd + 1) % MON_FIFO;
  }
  c->mon_next++;
 }
}

/* retire finished TDs and keep the queue full; called from IRQ and getbufpos */
static void service(usbpcm_card_s *c)
{
 volatile uint32_t *ed = P32(c, O_IED);
 uint32_t hp;
 if(!c->streaming) return;
 hp = ed[2];
 if(hp & 1){ ed[2] = hp & ~1u; c->errors++; }
 hp &= ~0xFu;
 while(c->h != c->t && PH(c, TDOFF(c->h)) != hp){
  volatile uint32_t *td = P32(c, TDOFF(c->h));
  uint32_t cc = td[0] >> 28;
  if(cc == 8) c->late++; else if(cc) c->errors++;
  if(c->ctl){
   const volatile int16_t *d = (const volatile int16_t *)P8(c, O_RING + (c->played % c->nframes) * USBPCM_FRAMEBYTES);
   const volatile uint32_t *w = (const volatile uint32_t *)d;
   int z;
   for(z = 0; z < 48 && !w[z]; z++) ;
   if(z < 48){ c->silent = 0; c->ctl->flags &= ~UPF_GATED; }
   else if(++c->silent >= 1000) c->ctl->flags |= UPF_GATED;
   peaks(c->ctl, 0, d, 48);
   rec_put(c, 0, d, USBPCM_FRAMEBYTES);
   c->ctl->st[0].frames++;
  }
  c->played++;
  c->h = (c->h + 1) % c->ntd;
 }
 if(c->h == c->t || (int16_t)(c->next_frame - frame(c)) < 2){
  /* queue ran dry (interrupts were blocked too long) - restart a few frames ahead */
  if((int16_t)(c->next_frame - frame(c)) < 2){
   c->next_frame = frame(c) + 3;
   c->resyncs++;
  }
 }
 while(((c->t + 1) % c->ntd) != c->h)
  queue_one(c);
 if(c->has_in) service_in(c);
 if(c->has_in && c->ctl) monitor(c);
 if(c->ctl){
  c->ctl->irqs = c->irqs; c->ctl->errors = c->errors; c->ctl->late = c->late; c->ctl->resyncs = c->resyncs;
 }
}

/* ------------------------------------------------------------------ */
/* SBEMU card interface                                                */
/* ------------------------------------------------------------------ */
static void USBPCM_close(struct mpxplay_audioout_info_s *aui);

static void parse_env(void)
{
 const char *e = getenv("USBPCM");
 if(!e) return;
 while(*e){
  char ch = *e++;
  if(ch == 'C' || ch == 'c') usbpcm_force_hc = atoi(e);
  else if(ch == 'L' || ch == 'l') usbpcm_lat = atoi(e);
  else if(ch == 'V' || ch == 'v') usbpcm_verbose = 1;
  else if(ch == 'N' || ch == 'n') usbpcm_noirq = 1;
  else if(ch == 'E' || ch == 'e') usbpcm_noehci = 1;
  else if(ch == 'S' || ch == 's') usbpcm_stage = atoi(e);
  else if(ch == 'R' || ch == 'r'){ usbpcm_rec_sec = atoi(e); if(usbpcm_rec_sec <= 0) usbpcm_rec_sec = 20; }
  else if(ch == 'I' || ch == 'i') usbpcm_rec_what = 2;
  else if(ch == 'O' || ch == 'o') usbpcm_rec_what = 1;
  else if(ch == 'X' || ch == 'x') usbpcm_noin = 1;
 }
 if(usbpcm_lat < 16) usbpcm_lat = 16;
 if(usbpcm_lat > USBPCM_MAXFRAMES) usbpcm_lat = USBPCM_MAXFRAMES;
 usbpcm_lat -= usbpcm_lat % USBPCM_IRQ_EVERY;
 if(usbpcm_rec_sec > 60) usbpcm_rec_sec = 60;
}

/* control block in low memory + optional XMS recording rings */
static void setup_ctl(usbpcm_card_s *c)
{
 volatile usbpcm_ctl_t *k = (volatile usbpcm_ctl_t *)P8(c, O_CTL);
 int what = usbpcm_rec_what, sec = usbpcm_rec_sec;
 memset((void *)k, 0, sizeof(*k));
 k->version = USBPCM_CTL_VERSION;
 k->size = sizeof(usbpcm_ctl_t);
 k->self = c->lo_phys + O_CTL;
 k->rate = USBPCM_RATE;
 k->hcnum = c->hcnum; k->vid = c->vid; k->pid = c->pid;
 k->ring_ms = usbpcm_lat;
 if(!c->has_in) what &= ~2;
 while(sec && what){
  unsigned long one = (unsigned long)sec * USBPCM_RATE * 4, total = one * ((what & 1) + ((what >> 1) & 1));
  memset(&c->rec, 0, sizeof c->rec);
  if(pds_dpmi_xms_allocmem(&c->rec, total)){
   unsigned long ph = (unsigned long)c->rec.physicalptr;
   int st, n = 0;
   c->rec_ok = 1;
   k->xms_handle = c->rec.xms;
   for(st = 0; st < 2; st++){
    if(!(what & (1 << st))) continue;
    c->ring[st] = c->rec.linearptr + n * one;
    k->st[st].phys = ph + n * one;
    k->st[st].size = one;
    memset(c->ring[st], 0, one);
    n++;
   }
   ushow("USBPCM: recording last %d s of%s%s (USBREC.COM), XMS %lu KB\n", sec,
         (what & 1) ? " playback" : "", (what & 2) ? " LINE-IN" : "", total / 1024);
   break;
  }
  sec /= 2;
 }
 if(usbpcm_rec_sec && !c->rec_ok) ushow("USBPCM: no XMS for recording - recording disabled\n");
 if((usbpcm_rec_what & 2) && usbpcm_rec_sec && !c->has_in) ushow("USBPCM: device has no 48 kHz stereo input - LINE IN not recorded\n");
 k->flags = (c->ring[0] ? UPF_REC_OUT : 0) | (c->ring[1] ? UPF_REC_IN : 0) | (c->has_in ? UPF_HAS_IN : 0);
 c->ctl = k;
 memcpy((void *)k->sig, USBPCM_CTL_SIG, 8);    /* signature last: block is complete */
}

static int USBPCM_adetect(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c;
 int b, d, f, hcn = 0, p, ok = 0, skipped = 0;
 if(!getenv("USBPCM")) return 0;   /* opt-in: the driver takes over a USB controller */
 parse_env();
 ulog("USBPCM detect (SBEMU USB Audio output, OHCI)\n");
 c = (usbpcm_card_s *)calloc(1, sizeof(usbpcm_card_s));
 if(!c) return 0;
 aui->card_private_data = c;
 c->pci_dev = (struct pci_config_s *)calloc(1, sizeof(struct pci_config_s));
 c->dm = MDma_alloc_cardmem(HIGH_TOTAL + 4096);
 if(!c->pci_dev || !c->dm) goto err;
 c->base = (char *)(((unsigned long)c->dm->linearptr + 4095) & ~4095ul);
 c->base_phys = (uint32_t)pds_cardmem_physicalptr(c->dm, c->base);
 if(c->base_phys & 0xFFF){ ushow("USBPCM: DMA memory not page aligned\n"); goto err; }
 memset(c->base, 0, HIGH_TOTAL);
 {
  int seg;
  {  /* allocate at the TOP of conventional memory (DOS "last fit", no UMB): the block
        stays resident with the TSR and would otherwise split the free memory below it.
        UMBs are not usable - under JEMM their linear and physical addresses differ. */
   __dpmi_regs r;
   int strat, link;
   memset(&r, 0, sizeof r); r.x.ax = 0x5800; __dpmi_int(0x21, &r); strat = r.x.ax;
   memset(&r, 0, sizeof r); r.x.ax = 0x5802; __dpmi_int(0x21, &r); link = r.h.al;
   memset(&r, 0, sizeof r); r.x.ax = 0x5803; r.x.bx = 0; __dpmi_int(0x21, &r);
   memset(&r, 0, sizeof r); r.x.ax = 0x5801; r.x.bx = 0x0002; __dpmi_int(0x21, &r);
   seg = __dpmi_allocate_dos_memory((LOW_TOTAL + 256 + 15) / 16, &c->lo_sel);
   memset(&r, 0, sizeof r); r.x.ax = 0x5801; r.x.bx = strat; __dpmi_int(0x21, &r);
   memset(&r, 0, sizeof r); r.x.ax = 0x5803; r.x.bx = link; __dpmi_int(0x21, &r);
   if(seg > 0 && seg + (LOW_TOTAL + 256 + 15) / 16 > 0xA000){
    ulog("low block at %04X is above 640 KB - not usable for USB DMA\n", seg);
    __dpmi_free_dos_memory(c->lo_sel); seg = 0; c->lo_sel = 0;
   }
  }
  if(seg <= 0){ ushow("USBPCM: not enough conventional memory (%d bytes)\n", LOW_TOTAL + 256); c->lo_sel = 0; goto err; }
  c->lo_phys = ((uint32_t)seg * 16 + 255) & ~255u;
  {   /* low-memory mapping (kept for the lifetime of the TSR, never released) */
   unsigned long m = pds_dpmi_map_physical_memory(c->lo_phys & ~0xFFFu, (c->lo_phys & 0xFFF) + LOW_TOTAL);
   if(!m){ ushow("USBPCM: cannot address conventional memory\n"); goto err; }
   c->lo = (char *)(m + (c->lo_phys & 0xFFF));
  }
  memset(c->lo, 0, LOW_TOTAL);
 }
 ulog("memory: low %08X (%d B), high %08X (%d B)\n", c->lo_phys, LOW_TOTAL, c->base_phys, HIGH_TOTAL);
 if(usbpcm_stage == 1){ ushow("USBPCM: diagnostic stop at stage 1 (memory only)\n"); goto err; }

 if(usbpcm_stage == 2){ ushow("USBPCM: diagnostic stop at stage 2 (before EHCI)\n"); goto err; }
 if(!usbpcm_noehci) ehci_release_all(); else ulog("EHCI left untouched (USBPCM=E)\n");
 if(usbpcm_stage == 3){ ushow("USBPCM: diagnostic stop at stage 3 (after EHCI)\n"); goto err; }

 for(b = 0; b < 2 && !ok; b++) for(d = 0; d < 32 && !ok; d++) for(f = 0; f < 8 && !ok; f++){
  uint32_t id = pci_rd(b, d, f, 0), cls, bar, nports;
  if((id & 0xFFFF) == 0xFFFF || !id) { if(!f) break; continue; }
  cls = pci_rd(b, d, f, 8) >> 8;
  if(cls != 0x0C0310) { if(!f && !(pci_rd(b, d, f, 0x0C) & 0x00800000)) break; continue; }
  bar = pci_rd(b, d, f, 0x10);
  ulog("OHCI #%d at PCI %d:%02X.%d ID %04X:%04X BAR %08X IRQ %d\n", hcn, b, d, f, id & 0xFFFF, id >> 16, bar,
       pci_rd(b, d, f, 0x3C) & 0xFF);
  if((bar & 1) || (usbpcm_force_hc >= 0 && usbpcm_force_hc != hcn)) { hcn++; continue; }
  c->pci_dev->bBus = b; c->pci_dev->bDev = d; c->pci_dev->bFunc = f;
  c->pci_dev->vendor_id = id & 0xFFFF; c->pci_dev->device_id = id >> 16;
  c->mmio_phys = bar & ~0xFu;
  c->mmio = (volatile char *)pds_dpmi_map_physical_memory(c->mmio_phys, 4096);
  if(!c->mmio){ ulog("  cannot map MMIO\n"); hcn++; continue; }
  ulog("  MMIO %08lX mapped at %08lX\n", c->mmio_phys, (unsigned long)c->mmio);
  nports = ORD(c, HcRhDescriptorA) & 0xFF; if(nports > 15) nports = 15;
  {
   int any = 0;
   for(p = 0; p < (int)nports; p++){ uint32_t st = ORD(c, HcRhPortStatus + 4*p); ulog("  port %d: %08X\n", p+1, st); if(st & PS_CCS) any = 1; }
   if(!any){ ulog("  no device connected\n"); goto next; }
  }
  ulog("  HcControl=%08X HCCA=%08X\n", ORD(c, HcControl), ORD(c, HcHCCA));
  if(usbpcm_stage == 4){ ushow("USBPCM: diagnostic stop at stage 4 (registers read)\n"); goto err; }
  if(usbpcm_force_hc < 0 && bios_uses_hc(c->mmio)){ ushow("USBPCM: OHCI #%d is used by the BIOS (USB keyboard?) - skipped\n", hcn); skipped++; goto next; }
  pcibios_enable_memmap_set_master(c->pci_dev);
  if(usbpcm_stage == 5){ ushow("USBPCM: diagnostic stop at stage 5 (BIOS schedule checked)\n"); goto err; }
  {
   int irq = pci_rd(b, d, f, 0x3C) & 0xFF;
   ulog("  BIOS USB busy flag before takeover: %02X\n", bios_busy());
   if(!usbpcm_vec_done) neutralize_bios_irq(irq);
   OWR(c, HcInterruptDisable, INT_SOURCES);      /* no more interrupts towards the BIOS (MIE stays) */
   OWR(c, HcInterruptStatus, 0x7F);
  }
  ushow("USBPCM: taking over OHCI #%d - other USB devices on this controller stop working\n", hcn);
  c->owned = 1;
  if(!ohci_takeover(c)) goto next;
  ulog("  BIOS USB busy flag after takeover: %02X\n", bios_busy());
  if(usbpcm_stage == 6){ ushow("USBPCM: diagnostic stop at stage 6 (after takeover)\n"); goto err; }
  c->hcnum = hcn;
  for(p = 0; p < (int)nports && !ok; p++){
   if(!(ORD(c, HcRhPortStatus + 4*p) & PS_CCS)) continue;
   c->port = p;
   if(enumerate(c, p)) ok = 1;
   else ulog("  port %d: not a usable USB audio device\n", p + 1);
  }
  if(ok) break;
  ushow("USBPCM: OHCI #%d was reset but has no usable audio device\n", hcn);
next:
  unmap_if_high((unsigned long)c->mmio, c->mmio_phys);
  c->mmio = NULL;
  hcn++;
 }
 if(!ok){
  if(skipped)
   ushow("USBPCM: no USB audio device on a free OHCI controller. If the card is connected,\n"
         "        it probably shares a controller with a USB keyboard used by the BIOS -\n"
         "        move the card or the keyboard to another USB port.\n");
  goto err;
 }

 aui->card_irq = pcibios_ReadConfig_Byte(c->pci_dev, PCIR_INTR_LN);
 aui->card_pci_dev = c->pci_dev;
 /* find other USB controllers using the same IRQ line */
 usbpcm_nsib = 0;
 for(b = 0; b < 2; b++) for(d = 0; d < 32; d++) for(f = 0; f < 8; f++){
  uint32_t id = pci_rd(b, d, f, 0), cls, irq, bar;
  if((id & 0xFFFF) == 0xFFFF || !id) { if(!f) break; continue; }
  cls = pci_rd(b, d, f, 8) >> 8;
  if(cls != 0x0C0310 && cls != 0x0C0320) continue;
  if(b == c->pci_dev->bBus && d == c->pci_dev->bDev && f == c->pci_dev->bFunc) continue;
  irq = pci_rd(b, d, f, 0x3C) & 0xFF;
  bar = pci_rd(b, d, f, 0x10) & ~0xFu;
  ushow("USBPCM: %s %d:%02X.%d IRQ %d%s\n", cls == 0x0C0310 ? "OHCI" : "EHCI", b, d, f, irq,
        irq == aui->card_irq ? "  <- shared with the sound card" : "");
  if(irq == aui->card_irq && usbpcm_nsib < MAXSIB){
   unsigned long m = pds_dpmi_map_physical_memory(bar, 4096);
   if(m){
    usbpcm_sib[usbpcm_nsib].ehci = (cls == 0x0C0320);
    usbpcm_sib[usbpcm_nsib].mmio = (volatile char *)m;
    usbpcm_sib[usbpcm_nsib].op = (cls == 0x0C0320) ? *(volatile uint8_t *)m : 0;
    usbpcm_nsib++;
   }
  }
 }
 {
  uint8_t elcr = inportb(aui->card_irq < 8 ? 0x4D0 : 0x4D1);
  ushow("USBPCM: sound card IRQ %d, %s-triggered, %d other USB controller(s) share it\n", aui->card_irq,
        ((elcr >> (aui->card_irq & 7)) & 1) ? "level" : "EDGE", usbpcm_nsib);
 }
 ulog("selected OHCI #%d port %d, IRQ %d, %s (%04X:%04X), alt %d ep %d mps %d\n", c->hcnum, c->port + 1,
      aui->card_irq, c->product, c->vid, c->pid, c->alt.alt, c->alt.ep, c->alt.mps);
 setup_ctl(c);
 ulog("control block at %08X (low memory), flags %04X\n", c->lo_phys + O_CTL, c->ctl->flags);
 return 1;
err:
 USBPCM_close(aui);
 return 0;
}

static void USBPCM_card_info(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 char sout[100];
 snprintf(sout, sizeof sout, "USB Audio: %s (%04X:%04X) on OHCI #%d port %d, IRQ %d, %d ms buffer",
          c->product[0] ? c->product : "device", c->vid, c->pid, c->hcnum, c->port + 1, aui->card_irq, usbpcm_lat);
 pds_textdisplay_printf(sout);
}

static void USBPCM_setrate(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE;
 aui->chan_card = 2;
 aui->bits_card = 16;
 aui->freq_card = USBPCM_RATE;
 c->nframes = usbpcm_lat;
 c->ntd = c->nframes + 1;
 /* integer replacement of MDma_init_pcmoutbuf() */
 aui->card_DMABUFF = (char *)P8(c, O_RING);
 aui->card_dmasize = c->nframes * USBPCM_FRAMEBYTES;
 aui->card_bytespersign = 4;
 if(!aui->card_outbytes) aui->card_outbytes = PCM_OUTSAMPLES * 4;
 aui->card_dma_lastgoodpos = 0;
 aui->card_dmalastput = aui->card_dmasize / 2;
 aui->card_dmafilled = aui->card_dmalastput;
 aui->card_dmaspace = aui->card_dmasize - aui->card_dmafilled;
 aui->card_samples_per_int = 48 * USBPCM_IRQ_EVERY;
 memset(aui->card_DMABUFF, 0, aui->card_dmasize);
 ulog("setrate: 48000 Hz 16-bit stereo, ring %d bytes\n", aui->card_dmasize);
}

static void USBPCM_start(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 volatile uint32_t *ed = P32(c, O_IED), *hcca = P32(c, O_HCCA);
 int i;
 if(c->streaming) return;
 for(i = 0; i < (c->ntd) * 8; i++) P32(c, O_ITD)[i] = 0;
 c->h = c->t = 0;
 c->queued_slot = 0;
 c->played = 0;
 ed[0] = c->addr | ((uint32_t)c->alt.ep << 7) | (1u<<11) | (c->ls ? (1u<<13) : 0) | (1u<<15) | ((uint32_t)c->alt.mps << 16);
 ed[1] = PH(c, TDOFF(0));
 ed[2] = PH(c, TDOFF(0));
 ed[3] = 0;
 if(c->has_in){
  volatile uint32_t *edi = P32(c, O_IED2);
  for(i = 0; i < NITD_IN * 8; i++) P32(c, O_ITD2)[i] = 0;
  c->hi = c->ti = 0;
  edi[0] = c->addr | ((uint32_t)c->ain.ep << 7) | (2u<<11) | (c->ls ? (1u<<13) : 0) | (1u<<15) | ((uint32_t)c->ain.mps << 16);
  edi[1] = edi[2] = PH(c, TDOFF2(0));
  edi[3] = 0;
  ed[3] = PH(c, O_IED2);
 }
 for(i = 0; i < 32; i++) hcca[i] = PH(c, O_IED);
 c->next_frame = frame(c) + 4;
 c->next_frame_in = c->next_frame;
 c->streaming = 1;
 while(((c->t + 1) % c->ntd) != c->h) queue_one(c);
 if(c->has_in) while(((c->ti + 1) % NITD_IN) != c->hi) queue_in(c);
 if(c->ctl) c->ctl->flags |= UPF_STREAMING;
 OWR(c, HcInterruptStatus, 0x7F);
 if(!usbpcm_noirq) OWR(c, HcInterruptEnable, INT_MIE | INT_WDH);
 else ushow("USBPCM: diagnostic mode N - no USB interrupts (no sound expected)\n");
 OWR(c, HcControl, ORD(c, HcControl) | CTL_PLE | CTL_IE);
 ulog("start: frame %u, %d TDs queued, BIOS USB busy flag %02X\n", frame(c), c->nframes, bios_busy());
}

static void USBPCM_stop(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 if(!c || !c->mmio || !c->streaming) return;
 OWR(c, HcInterruptDisable, INT_WDH);
 OWR(c, HcControl, ORD(c, HcControl) & ~(CTL_PLE | CTL_IE));
 c->streaming = 0;
 if(c->ctl) c->ctl->flags &= ~UPF_STREAMING;
 ulog("stop: played %u frames, irqs %u, errors %u, late %u, resyncs %u\n", c->played, c->irqs, c->errors, c->late, c->resyncs);
}

static void USBPCM_close(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 if(!c) return;
 USBPCM_stop(aui);
 if(c->mmio){
  if(c->owned){ OWR(c, HcInterruptDisable, INT_SOURCES); OWR(c, HcControl, ORD(c, HcControl) & CTL_RWC); OWR(c, HcInterruptEnable, INT_MIE); }
  unmap_if_high((unsigned long)c->mmio, c->mmio_phys);
 }
 if(c->ctl){ c->ctl->sig[0] = 0; c->ctl = NULL; }
 if(c->rec_ok){ pds_dpmi_xms_freemem(&c->rec); c->rec_ok = 0; }
 if(c->dm) MDma_free_cardmem(c->dm);
 if(c->lo_sel) __dpmi_free_dos_memory(c->lo_sel);
 if(c->pci_dev) free(c->pci_dev);
 free(c);
 aui->card_private_data = NULL;
}

static long USBPCM_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 service(c);
 aui->card_dma_lastgoodpos = (c->played % c->nframes) * USBPCM_FRAMEBYTES;
 return aui->card_dma_lastgoodpos;
}

static void USBPCM_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 if(aui->card_DMABUFF && aui->card_dmasize) memset(aui->card_DMABUFF, 0, aui->card_dmasize);
}

/* another USB controller on the same IRQ line (e.g. the one with the USB keyboard,
   driven by the BIOS) has a pending interrupt? */
static int sibling_pending(void)
{
 int i;
 for(i = 0; i < usbpcm_nsib; i++){
  volatile char *m = usbpcm_sib[i].mmio;
  if(usbpcm_sib[i].ehci){
   uint32_t sts = *(volatile uint32_t *)(m + usbpcm_sib[i].op + 4), en = *(volatile uint32_t *)(m + usbpcm_sib[i].op + 8);
   if(sts & en & 0x3F) return 1;
  } else {
   uint32_t en = *(volatile uint32_t *)(m + HcInterruptEnable), sts = *(volatile uint32_t *)(m + HcInterruptStatus);
   if((en & INT_MIE) && (sts & en & 0x7F)) return 1;
  }
 }
 return 0;
}

static int USBPCM_IRQRoutine(struct mpxplay_audioout_info_s *aui)
{
 usbpcm_card_s *c = aui->card_private_data;
 uint32_t st;
 int mine = 0;
 if(!c || !c->mmio) return 0;
 st = ORD(c, HcInterruptStatus) & ORD(c, HcInterruptEnable);
 if(st & INT_WDH){
  P32(c, O_HCCA)[0x84/4] = 0;            /* HccaDoneHead consumed */
  OWR(c, HcInterruptStatus, INT_WDH);
  c->irqs++;
  service(c);
  mine = 1;
 }
 /* Shared IRQ: if the BIOS-driven controller also wants attention, report "not ours"
    so SBEMU chains to the BIOS handler. Our ring is already serviced; mixing simply
    happens on the next interrupt (the ring holds far more than 4 ms). */
 if(usbpcm_nsib && sibling_pending()){ usbpcm_chained++; return 0; }
 return mine;
}

one_sndcard_info USBPCM_sndcard_info = {
 "USB Audio (OHCI)",
 SNDCARD_LOWLEVELHAND,
 NULL,
 NULL,
 &USBPCM_adetect,
 &USBPCM_card_info,
 &USBPCM_start,
 &USBPCM_stop,
 &USBPCM_close,
 &USBPCM_setrate,
 &MDma_writedata,
 &USBPCM_getbufpos,
 &USBPCM_clearbuf,
 NULL,
 &USBPCM_IRQRoutine,
 NULL,
 NULL,
 NULL
};

#endif /* AU_CARDS_LINK_USBPCM */
