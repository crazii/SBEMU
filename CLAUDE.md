# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Fork of [SBEMU](https://github.com/crazii/SBEMU) — a DOS Sound Blaster emulator that intercepts SB16/FM/MPU-401 I/O and routes it to a real PCI audio card running under DPMI. This fork adds support for the **AMD CS5535/CS5536 audio controller** (PCI ID 0x1022:0x2093 / 0x100B:0x002E), found in Wyse SX0 thin clients and other Geode-based systems.

## Build

Requires the DJGPP cross-compiler (`i586-pc-msdosdjgpp-gcc`). Output is `output/sbemu.exe`.

```bash
make              # release build
make DEBUG=1      # debug build (no strip, DEBUG=1 defined)
make clean        # remove object files
make distclean    # remove object files + exe
```

There is no test suite. Functional testing requires booting DOS on real or emulated hardware. The `output/` directory is where build artefacts land.

## Architecture

### Two-layer structure

```
sbemu/          SoundBlaster emulation core
                Traps DOS I/O ports (SB16 0x220, FM 0x388, MPU-401 0x330) via DPMI
                and forwards decoded PCM/events to the audio layer below.

mpxplay/        Audio output layer (ported from Mpxplay player)
  au_cards/     One driver per PCI audio chip.
                au_cards.c  — driver registry (all_sndcard_info[]), AU_init() auto-detect loop
                au_cards.h  — one_sndcard_info vtable + mpxplay_audioout_info_s context struct
                dmairq.h/c  — shared DMA helpers (MDma_alloc_cardmem, MDma_writedata, etc.)
                pcibios.h/c — DOS PCI BIOS enumeration
                ac97_def.h  — AC97 register constants
                sc_*.c      — individual soundcard drivers

drivers/        Lightly-modified Linux ALSA driver source trees for complex chips.
                Simpler chips (ICH, CS5535, CMI, etc.) are self-contained in sc_*.c.
```

### Driver vtable (`one_sndcard_info`, defined in `au_cards.h`)

Every driver must fill this struct and export it with the name `FOO_sndcard_info`:

| Slot | Role |
|---|---|
| `card_detect` | PCI auto-detect; returns 1 if card found, 0 otherwise |
| `card_info` | Print detected card name/port/IRQ |
| `card_start/stop` | Enable/disable DMA playback |
| `card_close` | Free all resources |
| `card_setrate` | Configure freq/chan/bits, set up DMA buffer + PRD table |
| `cardbuf_writedata` | Write PCM into the ring buffer (usually `&MDma_writedata`) |
| `cardbuf_pos` | Return current DMA play position in bytes |
| `cardbuf_clear` | Silence the buffer (usually `&MDma_clearbuf`) |
| `cardbuf_int_monitor` | Periodic DMA underrun check (usually `&MDma_interrupt_monitor`) |
| `irq_routine` | PCI IRQ handler; return non-zero if interrupt was consumed |
| `card_writemixer/readmixer` | AC97/hardware mixer register access |
| `card_mixerchans` | Array of mixer channel descriptors (volume/mute mappings) |
| `card_fm_write/read` | OPL3 FM I/O (use `&ioport_fm_write/read` if port-based) |
| `card_mpu401_write/read` | MPU-401 MIDI I/O |

Mandatory `infobits` for PCI drivers: `SNDCARD_LOWLEVELHAND | SNDCARD_INT08_ALLOWED`.

### Runtime context (`mpxplay_audioout_info_s`, the `aui` pointer)

Key fields a driver reads/writes during `card_setrate`:

- `aui->freq_card`, `chan_card`, `bits_card` — set to the negotiated format (16-bit stereo, freq clamped to card capability)
- `aui->card_DMABUFF` — set to the virtual address of the PCM ring buffer
- `aui->card_dmasize` — usable ring size in bytes (set by `MDma_init_pcmoutbuf`)
- `aui->card_samples_per_int` — samples per DMA interrupt period (set by driver)
- `aui->card_irq`, `aui->card_pci_dev` — set during detect

### Adding a new PCI driver

1. Create `mpxplay/au_cards/sc_foo.c` (self-contained, see `sc_cs5535.c` or `sc_ich.c` as templates)
2. Add `#define AU_CARDS_LINK_FOO 1` in both `#ifdef AU_CARDS_LINK_PCI` blocks in `au_cards.h`
3. Add `extern one_sndcard_info FOO_sndcard_info;` and `&FOO_sndcard_info` entry to `au_cards.c`
4. Add `mpxplay/au_cards/sc_foo.c` to `CARDS_SRC` in `makefile`

For complex chips with an existing Linux ALSA driver, use the `drivers/<name>/` + `au_linux.h` bridge pattern (see `sc_als4000.c` as a clean example).

## CS5535/CS5536 driver (`sc_cs5535.c`)

The new driver follows the `sc_ich.c` direct-hardware pattern. Key hardware facts:

- **Single BAR0** (I/O port space, 16-byte aligned) for all ACC registers
- **AC97 codec** access via `ACC_CODEC_CNTL` / `ACC_CODEC_STATUS` — no separate mixer I/O range
- **DMA**: PRD table (8 period entries + 1 JMP entry for circular operation). The address written to `ACC_BM0_PRD` is the physical address of the JMP descriptor, not the first entry.
- **Position**: read `ACC_BM0_PNTR` for the physical DMA byte address; subtract the buffer's physical base to get the ring offset.
- **IRQ**: read `ACC_IRQ_STATUS`; ACK codec/wakeup bits by reading `ACC_GPIO_STATUS`, ACK BM0 by reading `ACC_BM0_STATUS`.
- **VRA** (Variable Rate Audio): enable via AC97 `AC97_EXTENDED_STATUS` bit 0; if the codec supports it, arbitrary sample rates (8–48 kHz) work. Otherwise the driver locks to 48 kHz.
