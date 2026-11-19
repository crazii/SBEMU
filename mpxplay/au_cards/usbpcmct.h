// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2026 Lukasz Bolesta, with the help of Claude (Anthropic).
 * MIT licensed so that DOS tools under other licences can share it.
 *
 * usbpcmct.h - control block of the SBEMU USB Audio driver (sc_usbpcm.c).
 *
 * The block lives in conventional memory (paragraph aligned) so that real-mode
 * tools (USBREC.COM and USBMIX.COM from the SBUSB project) can find it by its
 * signature and read/write it without DPMI. Shared by sc_usbpcm.c (DJGPP) and usbrec.c (gcc -m16).
 *
 * Streams: st[0] = playback (exactly what SBEMU sends to the card),
 *          st[1] = capture  (LINE IN of the card).
 * Each stream has its own ring in XMS (16-bit stereo 48 kHz little endian).
 * wr counts every byte ever written to the ring (mod 2^32); the newest byte is at
 * ring offset (wr-1) % size. The tool copies data with XMS function 0Bh: handle =
 * xms_handle, offset = st[i].phys - (base returned by XMS lock 0Ch).
 *
 * Fields marked [mixer] are read by the driver on every USB frame - a tool changes
 * them and the driver applies the change without any call.
 */
#ifndef USBPCMCT_H
#define USBPCMCT_H

#define USBPCM_CTL_SIG      "USBPCM01"
#define USBPCM_CTL_VERSION  1

/* flags (driver -> tool) */
#define UPF_REC_OUT   0x0001   /* playback stream is being recorded */
#define UPF_REC_IN    0x0002   /* capture stream is being recorded */
#define UPF_HAS_IN    0x0004   /* device has a 48 kHz stereo input and it is running */
#define UPF_STREAMING 0x0008   /* USB streaming active */
#define UPF_GATED     0x0010   /* playback silent > 1 s: rings not written (silence gate) */

/* ctl (tool -> driver) */
#define UPC_PAUSE     0x0001   /* stop writing to the rings (meters keep running) */
#define UPC_NOGATE    0x0002   /* record silence too (default: after 1 s of digital silence
                                  on the playback stream both rings stop, so the end of a game
                                  is not overwritten while the user types USBREC /W) */

typedef struct usbpcm_stream_s {
 unsigned long phys;      /* physical address of the ring (0 = not recorded) */
 unsigned long size;      /* ring size in bytes (multiple of 4) */
 unsigned long wr;        /* bytes written so far (mod 2^32) */
 unsigned long lost;      /* frames missing (late/failed packets, filled with silence) */
 unsigned short peak[2];  /* max |sample| L/R since the tool last cleared it */
 unsigned long frames;    /* USB frames (ms) seen */
} __attribute__((packed)) usbpcm_stream_t;

typedef struct usbpcm_ctl_s {
 char sig[8];             /* "USBPCM01" */
 unsigned short version;  /* USBPCM_CTL_VERSION */
 unsigned short size;     /* sizeof(usbpcm_ctl_t) */
 unsigned long self;      /* physical address of this block (sanity check) */
 unsigned short xms_handle;
 unsigned short flags;    /* UPF_* */
 unsigned long rate;      /* 48000 */
 usbpcm_stream_t st[2];
 unsigned long irqs, errors, late, resyncs;
 unsigned short ctl;      /* UPC_* */
 unsigned short in_maxpkt;/* largest IN packet seen (bytes) */
 unsigned short in_minpkt;/* smallest non-empty IN packet seen */
 unsigned short hcnum;    /* OHCI controller number */
 unsigned short vid, pid;
 /* [mixer] */
 unsigned short mon_gain;   /* LINE IN monitor (added to playback): 0 = off, 256 = 0 dB, max 512 */
 unsigned short out_gain;   /* reserved */
 unsigned short ring_ms;    /* playback ring length in ms (USBPCM=Ln) */
 unsigned short reserved[7];
} __attribute__((packed)) usbpcm_ctl_t;

#endif
