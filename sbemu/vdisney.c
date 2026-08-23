// Disney Sound Source / Covox Speech Thing emulation for SBEMU
//
// How it works:
//   - Parallel port DAC. Programs write 8-bit PCM samples to LPT1 data port (0x378).
//   - The hardware has a 16-byte FIFO. Status port bit 6 = "FIFO not full" (1 = not full).
//   - Stereo: control port bit1 falling edge = right latch, bit0 falling edge = left latch.
//   - Detection: games read 0x379, bit7 mirrors bit7 of data byte (inverted on real HW).
//
// SBEMU integration:
//   - Trap ports 0x378 (data), 0x379 (status), 0x37A (control).
//   - Use a CIRCULAR ring buffer per channel. IO writes push to the ring.
//   - GenSamples drains the ring at a *fixed* rate with linear interpolation.
//   - Fixed rate = 7000 Hz (Disney FIFO clock). Games that use Covox directly
//     write faster; we handle that via the adaptive sample-rate estimator below.

#include "platform.h"
#include "vdisney.h"
#include <string.h>
#include <stdlib.h>

#if SBEMU_DISNEY

// ---------------------------------------------------------------------------
// Ring buffer
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t  buf[VDISNEY_FIFO];
    int      head;       // next read position
    int      tail;       // next write position
    int      count;      // bytes currently in buffer
    uint32_t total_written;
} VDISNEY_Ring;

static inline void ring_push(VDISNEY_Ring *r, uint8_t v)
{
    // Overwrite oldest if full (prevents blocking TSR)
    if(r->count == VDISNEY_FIFO) {
        r->head = (r->head + 1) & (VDISNEY_FIFO - 1);
        r->count--;
    }
    r->buf[r->tail] = v;
    r->tail = (r->tail + 1) & (VDISNEY_FIFO - 1);
    r->count++;
    r->total_written++;
}

// Peek at offset i from head (no bounds check - caller must verify)
static inline uint8_t ring_peek(VDISNEY_Ring *r, int i)
{
    return r->buf[(r->head + i) & (VDISNEY_FIFO - 1)];
}

static inline void ring_consume(VDISNEY_Ring *r, int n)
{
    if(n > r->count) n = r->count;
    r->head  = (r->head + n) & (VDISNEY_FIFO - 1);
    r->count -= n;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static struct {
    uint8_t       data;        // last byte written to 0x378
    uint8_t       control;     // last byte written to 0x37A
    int           interface_ext; // FIFO-mode detected (bit3 strobe counter)

    VDISNEY_Ring  ch[2];       // ch[0]=left/mono, ch[1]=right

    int           stereo;
    int           leader;      // which channel drives timing (0 or 1)

    // Adaptive sample rate
    uint32_t      last_total;  // total_written snapshot at last GenSamples
    int           rate_est;    // current estimated sample rate (Hz)

    // Fixed-point resampling accumulator
    uint32_t      phase_fp;    // fractional position (16.16)

    int           initialized;
    int           running;

    int           output_freq;
} vds;

// ---------------------------------------------------------------------------
// Conversion: unsigned 8-bit PCM -> signed 16-bit
// ---------------------------------------------------------------------------
static inline int16_t u8_to_s16(uint8_t v)
{
    return (int16_t)(((int)v - 128) * 256);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void VDISNEY_Init(int output_freq)
{
    memset(&vds, 0, sizeof(vds));
    vds.output_freq = output_freq;
    vds.rate_est    = 7000;   // Default Disney FIFO clock
    vds.initialized = 1;
}

void VDISNEY_Shutdown(void)
{
    vds.initialized = 0;
}

int VDISNEY_IsActive(void)
{
    return vds.initialized;
}

// ---------------------------------------------------------------------------
// IO Handler
// ---------------------------------------------------------------------------
uint32_t VDISNEY_IOHandler(uint32_t port, uint32_t val, uint32_t out)
{
    if(!vds.initialized) return val;

    int offset = (int)(port - VDISNEY_BASE);

    if(out)
    {
        switch(offset)
        {
            case 0: // 0x378 — DAC sample byte (mono / leader channel)
                vds.data = (uint8_t)(val & 0xFF);
                ring_push(&vds.ch[0], vds.data);
                vds.running = 1;
                break;

            case 1: // 0x379 — read-only status, ignore writes
                break;

            case 2: // 0x37A — control port
            {
                uint8_t prev = vds.control;
                uint8_t cur  = (uint8_t)(val & 0xFF);

                // Bit1 falling edge -> right channel
                if((prev & 0x02) && !(cur & 0x02)) {
                    ring_push(&vds.ch[1], vds.data);
                    vds.running = 1;
                }
                // Bit0 falling edge -> left channel
                if((prev & 0x01) && !(cur & 0x01)) {
                    ring_push(&vds.ch[0], vds.data);
                    vds.running = 1;
                }
                // Bit3 falling edge -> FIFO-mode device
                if((prev & 0x08) && !(cur & 0x08)) {
                    vds.interface_ext++;
                }

                vds.control = cur;
                break;
            }
        }
    }
    else
    {
        switch(offset)
        {
            case 0: return vds.data;

            case 1: // 0x379 Status
            {
                uint8_t ret = 0x07;
                // In FIFO mode: assert busy (clear bit 6) when >= 16 bytes queued
                if(vds.interface_ext > 5) {
                    if(vds.ch[vds.leader].count >= 16)
                        ret |= 0x40; // FIFO full
                    else
                        ret &= ~0x04;
                }
                // Bit7: mirrors ~bit7 of data
                if(!(vds.data & 0x80)) ret |= 0x80;
                return ret;
            }

            case 2: return vds.control;
        }
    }
    return val;
}

// ---------------------------------------------------------------------------
// Audio generation
// ---------------------------------------------------------------------------
void VDISNEY_GenSamples(int16_t *pcm16, int samples, int freq, int domix)
{
    if(!vds.initialized || !vds.running) return;

    // --- 1. Detect stereo ---
    vds.stereo = (vds.ch[1].total_written > 32) ? 1 : 0;
    vds.leader = 0;

    // --- 2. Silence / timeout check ---
    uint32_t new_writes = vds.ch[vds.leader].total_written - vds.last_total;
    vds.last_total = vds.ch[vds.leader].total_written;

    if(vds.ch[vds.leader].count == 0 && new_writes == 0) {
        vds.running = 0;
        vds.rate_est = 7000; // reset rate for next session
        if(!domix) memset(pcm16, 0, samples * 2 * sizeof(int16_t));
        return;
    }

    // --- 3. Stable rate via buffer fill level feedback ---
    //
    // We target a steady-state buffer fill of TARGET_FILL samples.
    // The "natural" consume rate per output block is:
    //   consume = rate_est * samples / freq
    //
    // If fill > target: we consumed too slowly last time → increase rate_est.
    // If fill < target: we consumed too fast → decrease rate_est.
    //
    // This is a simple proportional controller. It is inherently stable because:
    //   - rate increases drain faster → fill drops → rate stops rising
    //   - rate decreases drain slower → fill rises → rate stops falling
    //
    // No EMA over new_writes needed. No trembling.

    // Target: ~80ms of audio at our estimated rate.
    // Initial boot: if rate_est=7000 and freq=44100, target ≈ 7000*0.08 = 560 samples
    int fill = vds.ch[vds.leader].count;

    // Compute "what rate would exactly consume fill samples in one output block"
    // fill = rate * samples / freq  →  rate = fill * freq / samples
    if(fill > 0) {
        int fill_rate = (int)((double)fill * freq / samples);
        // Clamp to reasonable audio range
        if(fill_rate < 4000)  fill_rate = 4000;
        if(fill_rate > 50000) fill_rate = 50000;

        // Smooth toward fill_rate with heavy inertia to avoid trembling:
        // 7/8 old + 1/8 new  (converges in ~8 blocks ≈ ~90ms at 44100/512)
        vds.rate_est = (vds.rate_est * 7 + fill_rate) / 8;
    }

    // Final clamp
    if(vds.rate_est < 4000)  vds.rate_est = 4000;
    if(vds.rate_est > 50000) vds.rate_est = 50000;

    // --- 4. Resample with linear interpolation ---
    uint32_t step_fp = (uint32_t)((double)vds.rate_est / freq * 65536.0 + 0.5);

    for(int i = 0; i < samples; i++)
    {
        int s0   = (int)(vds.phase_fp >> 16);
        int frac = (int)(vds.phase_fp & 0xFFFF);

        uint8_t a0_l, a1_l, a0_r, a1_r;

        if(vds.stereo) {
            a0_l = (vds.ch[0].count > s0)     ? ring_peek(&vds.ch[0], s0)     :
                   (vds.ch[0].count > 0)        ? ring_peek(&vds.ch[0], vds.ch[0].count-1) : 128;
            a1_l = (vds.ch[0].count > s0 + 1)  ? ring_peek(&vds.ch[0], s0 + 1) : a0_l;
            a0_r = (vds.ch[1].count > s0)     ? ring_peek(&vds.ch[1], s0)     :
                   (vds.ch[1].count > 0)        ? ring_peek(&vds.ch[1], vds.ch[1].count-1) : 128;
            a1_r = (vds.ch[1].count > s0 + 1)  ? ring_peek(&vds.ch[1], s0 + 1) : a0_r;
        } else {
            a0_l = a0_r = (vds.ch[0].count > s0)     ? ring_peek(&vds.ch[0], s0)     :
                           (vds.ch[0].count > 0)        ? ring_peek(&vds.ch[0], vds.ch[0].count-1) : 128;
            a1_l = a1_r = (vds.ch[0].count > s0 + 1)  ? ring_peek(&vds.ch[0], s0 + 1) : a0_l;
        }

        int32_t sl_raw = ((int32_t)a0_l * (65536 - frac) + (int32_t)a1_l * frac) >> 16;
        int32_t sr_raw = ((int32_t)a0_r * (65536 - frac) + (int32_t)a1_r * frac) >> 16;

        int16_t sl = u8_to_s16((uint8_t)sl_raw);
        int16_t sr = u8_to_s16((uint8_t)sr_raw);

        if(domix) {
            int32_t nl = (int32_t)pcm16[i*2]   + sl;
            int32_t nr = (int32_t)pcm16[i*2+1] + sr;
            pcm16[i*2]   = nl >  32767 ?  32767 : nl < -32768 ? -32768 : (int16_t)nl;
            pcm16[i*2+1] = nr >  32767 ?  32767 : nr < -32768 ? -32768 : (int16_t)nr;
        } else {
            pcm16[i*2]   = sl;
            pcm16[i*2+1] = sr;
        }

        vds.phase_fp += step_fp;
    }

    // --- 5. Consume processed source samples ---
    int consumed = (int)(vds.phase_fp >> 16);
    vds.phase_fp &= 0xFFFF; // keep fractional part for continuity

    ring_consume(&vds.ch[0], consumed);
    ring_consume(&vds.ch[1], consumed);
}

#endif // SBEMU_DISNEY
