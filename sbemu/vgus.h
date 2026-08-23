#ifndef _VGUS_H_
#define _VGUS_H_
//Gravis UltraSound (GUS) emulation
//Based on UltraSound SDK / reverse-engineering docs and DOSBox-X gus.cpp as reference.
//
//GUS I/O map (base = 2X0, where X nibble is configurable; default 0x240):
//  2X0  Mix Control (W)
//  2X6  IRQ Status (R)
//  2X8  Timer Control (R/W)
//  2X9  Timer 1 Count (W)
//  2XA  Timer 2 Count (W)
//  2XB  IRQ/DMA Control (W)
//  2XF  Register Controls (Rev 3.4+)
//  3X0  MIDI Control (W) / MIDI Status (R)
//  3X1  MIDI Tx (W) / MIDI Rx (R)
//  3X2  GF1 Page Register (R/W)
//  3X3  GF1 Register Select (R/W)
//  3X4  GF1 Data Low (R/W)
//  3X5  GF1 Data High (R/W)
//  3X7  DRAM access (R/W)

#include <stdint.h>
#include <sbemucfg.h>

#if SBEMU_GUS

// Port offsets from base (2X0)
#define VGUS_PORT_MIXCTRL       0x000   // Mix Control (W)
#define VGUS_PORT_IRQ_STATUS    0x006   // IRQ Status (R)
#define VGUS_PORT_TIMER_CTRL    0x008   // Timer Control
#define VGUS_PORT_TIMER1_CNT    0x009   // Timer 1 count
#define VGUS_PORT_TIMER2_CNT    0x00A   // Timer 2 count
#define VGUS_PORT_IRQ_DMA_CTRL  0x00B   // IRQ/DMA Control (W)
#define VGUS_PORT_REG_CTRL      0x00F   // Register Controls Rev 3.4+

// GF1 offsets from base+0x100 (3X0)
#define VGUS_PORT_MIDI_CTRL     0x100   // MIDI Control (W) / Status (R)
#define VGUS_PORT_MIDI_DATA     0x101   // MIDI Tx/Rx
#define VGUS_PORT_GF1_PAGE      0x102   // GF1 Page / Voice select
#define VGUS_PORT_GF1_REGSEL    0x103   // GF1 Register Select
#define VGUS_PORT_GF1_DATALO    0x104   // GF1 Data Low byte
#define VGUS_PORT_GF1_DATAHI    0x105   // GF1 Data High byte
#define VGUS_PORT_GF1_DRAM      0x107   // DRAM access

// GF1 voice registers (selected via page + regsel, low 7 bits)
#define VGUS_REG_VOICE_CTRL     0x00    // Voice Control
#define VGUS_REG_FREQ_CTRL      0x01    // Frequency Control
#define VGUS_REG_ADDR_HI        0x02    // Loop Start Address high
#define VGUS_REG_ADDR_LO        0x03    // Loop Start Address low
#define VGUS_REG_END_HI         0x04    // Loop End Address high
#define VGUS_REG_END_LO         0x05    // Loop End Address low
#define VGUS_REG_VOL_RATE       0x06    // Volume ramp rate
#define VGUS_REG_VOL_START      0x07    // Volume ramp start
#define VGUS_REG_VOL_END        0x08    // Volume ramp end
#define VGUS_REG_CUR_VOL        0x09    // Current volume
#define VGUS_REG_CUR_ADDR_HI    0x0A    // Current address high
#define VGUS_REG_CUR_ADDR_LO    0x0B    // Current address low
#define VGUS_REG_PAN            0x0C    // Panning (0=left, 8=center, 15=right)
#define VGUS_REG_VOL_CTRL       0x0D    // Volume ramp control

// GF1 global registers (page = 0x40+)
#define VGUS_GREG_DMA_CTRL      0x41    // DMA control
#define VGUS_GREG_DMA_START     0x42    // DMA start address in GUS DRAM (>>4)
#define VGUS_GREG_DRAM_ADDR_LO  0x43    // CPU DRAM access address pointer LSB
#define VGUS_GREG_DRAM_ADDR_HI  0x44    // CPU DRAM access address pointer MSB
#define VGUS_GREG_TIMER_CTRL    0x45    // Timer control
#define VGUS_GREG_TIMER1_CNT    0x46    // Timer 1 count
#define VGUS_GREG_TIMER2_CNT    0x47    // Timer 2 count
#define VGUS_GREG_SAMPLE_FREQ   0x48    // Sample frequency (for ADC)
#define VGUS_GREG_SAMPLE_CTRL   0x49    // Sampling control
#define VGUS_GREG_RESET         0x4C    // Master reset + IRQ/DAC enable

// Voice Control bits
#define VGUS_VC_STOPPED         0x01    // Voice stopped (read-only status)
#define VGUS_VC_STOP            0x02    // Stop voice
#define VGUS_VC_16BIT           0x04    // 16-bit samples
#define VGUS_VC_LOOP            0x08    // Loop enable
#define VGUS_VC_BIDIR           0x10    // Bidirectional loop
#define VGUS_VC_IRQ             0x20    // Fire IRQ on loop end / stop
#define VGUS_VC_BACKWARDS       0x40    // Reverse playback direction

// DMA control bits (0x41)
#define VGUS_DMA_ENABLE         0x01    // Enable DMA transfer
#define VGUS_DMA_TO_HOST        0x02    // Direction: 0=PC->GUS, 1=GUS->PC
#define VGUS_DMA_WIDTH16        0x04    // 16-bit wide DMA channel
#define VGUS_DMA_IRQ            0x20    // Fire IRQ on DMA complete
#define VGUS_DMA_DONE           0x40    // Transfer complete status (R)
#define VGUS_DMA_FLIP_MSB       0x80    // Flip MSB of transferred bytes

// IRQ status bits (2X6)
#define VGUS_IRQ_WAVETABLE      0x01    // Voice wavetable IRQ pending
#define VGUS_IRQ_VOLRAMP        0x02    // Volume ramp IRQ pending
#define VGUS_IRQ_TIMER1         0x04    // Timer 1 expired
#define VGUS_IRQ_TIMER2         0x08    // Timer 2 expired
#define VGUS_IRQ_DMA_TC         0x80    // DMA terminal count

#define VGUS_MAX_VOICES         32
#define VGUS_DRAM_SIZE          (1024*1024)  // 1MB GUS DRAM simulation

// Per-voice playback state
typedef struct
{
    uint8_t  ctrl;          // Voice control register
    uint8_t  vol_ctrl;      // Volume ramp control
    uint32_t freq;          // Frequency control word (drives increment per sample)
    uint32_t start;         // Loop start address (<<9 for sub-sample precision)
    uint32_t end;           // Loop end address   (<<9)
    uint32_t current;       // Current playback position (<<9)
    uint16_t volume;        // Current volume (0-4095)
    uint16_t vol_start;     // Volume ramp start value
    uint16_t vol_end;       // Volume ramp end value
    uint8_t  vol_rate;      // Volume ramp rate/step
    uint8_t  panning;       // 0=full left .. 15=full right
    int      wave_irq;      // Pending wave IRQ flag
    int      vol_irq;       // Pending volume IRQ flag
} VGUS_Voice;

// GUS emulator global state
typedef struct
{
    int       initialized;       // Initialized flag
    int       base_addr;         // I/O base (e.g. 0x240)
    int       irq;               // Configured IRQ
    int       dma;               // Configured DMA channel

    // GF1 register interface
    uint8_t   gf1_page;          // Selected voice (0-31) or global (0x40+)
    uint8_t   gf1_reg;           // Selected register within page
    uint16_t  gf1_data_latch;    // Low byte latch for 16-bit reads

    // Voices
    uint8_t   active_voices;     // Number of active voices (14-32)
    VGUS_Voice voice[VGUS_MAX_VOICES];

    // DRAM
    uint8_t  *dram;              // Simulated 1MB GUS RAM
    uint32_t  dram_ptr;          // CPU DRAM read/write pointer (byte address)

    // DMA state
    uint8_t   dma_ctrl;          // DMA control register (0x41)
    uint32_t  dma_gus_addr;      // Target DRAM address for DMA
    uint32_t  dma_count;         // Bytes to transfer
    int       dma_irq_pending;   // DMA IRQ pending flag

    // Mixer/control
    uint8_t   mix_ctrl;          // Mix control register (2X0)
    uint8_t   irq_dma_ctrl;      // IRQ/DMA control register (2XB) written value
    uint8_t   reg_ctrl;          // Register controls (2XF)
    uint8_t   reset_reg;         // Reset / IRQ-enable register (0x4C)

    // Timer state
    uint8_t   timer_ctrl;        // Timer control (from 2X8 / GREG 0x45)
    uint8_t   timer1_count;      // Timer 1 reload count
    uint8_t   timer2_count;      // Timer 2 reload count
    uint8_t   timer1_mask;       // Timer 1 masked from IRQ status
    uint8_t   timer2_mask;       // Timer 2 masked from IRQ status
    uint32_t  timer1_ticks;      // Countdown ticks (in output samples)
    uint32_t  timer2_ticks;      // Countdown ticks

    // IRQ status register
    uint8_t   irq_status;        // IRQ status (2X6)
    uint8_t   last_irq_status;   // previous status for edge-triggering

    // Output
    int       output_freq;       // PCM output sample rate

} VGUS_State;

#ifdef __cplusplus
extern "C"
{
#endif

// Initialize GUS emulation.
// base: I/O base address (e.g. 0x240), irq, dma: hardware config, output_freq: PCM rate.
// Returns 1 on success, 0 on failure.
int  VGUS_Init(int base, int irq, int dma, int output_freq);

// Free resources allocated by VGUS_Init.
void VGUS_Shutdown(void);

// Reset GUS to power-on state (keeps base/irq/dma config).
void VGUS_Reset(void);

// Unified I/O handler for all GUS ports.  Matches SBEMU_IOTRAP_HANDLER signature:
//   port = absolute I/O port, val = value for write, out = 1 write / 0 read.
// Returns read value (for reads), or val unchanged (for writes).
uint32_t VGUS_IOHandler(uint32_t port, uint32_t val, uint32_t out);

// Generate stereo 16-bit PCM audio into pcm16[0..samples*2-1].
// domix: 1 = add to existing buffer contents; 0 = overwrite.
void VGUS_GenSamples(int16_t *pcm16, int samples, int freq, int domix);

// Advance timers by delta samples at the given frequency.
// Calls raiseIRQ(irq_num) when a timer fires.
void VGUS_TickTimers(uint32_t delta_samples, int freq, void (*raiseIRQ)(uint8_t));

// Returns 1 if GUS is ready (initialized, not in reset).
int  VGUS_IsActive(void);

// Simulate a DMA upload to GUS DRAM.
// data: source bytes, count: number of bytes.
// Call this after detecting a GUS DMA transfer in the VDMA handler.
void VGUS_DMATransfer(uint8_t *data, uint32_t count);

// DMA-ready callback: registered by main.c so that DMA can be serviced
// immediately when GUS register 0x41 is written with ENABLE set, rather than
// waiting for the next audio interrupt (~46 ms later at 22 KHz).
// This eliminates the ULTRINIT "No card found" polling timeout.
typedef void (*VGUS_DMAReady_Callback)(void);
void VGUS_SetDMACallback(VGUS_DMAReady_Callback cb);

// Get internal GUS state (for debugging)
VGUS_State* VGUS_GetState(void);

#ifdef __cplusplus
}
#endif

#else // !SBEMU_GUS

#define VGUS_IsActive() 0

#endif // SBEMU_GUS

#endif//_VGUS_H_