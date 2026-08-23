// PC Speaker (Internal Beeper) emulation for SBEMU
// Based on DOSBox pcspeaker.cpp
//
// SBEMU integration:
//   - Traps ports 0x42 (PIT Ch 2 Data), 0x43 (PIT Command), 0x61 (SysCtrl)
//   - Passes through writes to real hardware to maintain system stability.
//   - Emulates PIT Channel 2 counter and output state.
//   - Generates PC Speaker audio in VPCSPEAKER_GenSamples().

#include "platform.h"
#include "vpcspeaker.h"
#include <conio.h>
#include <string.h>
#include <pc.h>

#if SBEMU_PCSPEAKER

#define PIT_FREQ 1193182 // 1.193182 MHz
#define SPKR_VOLUME 4000 // Master volume for PC Speaker

// State definitions
enum SPKR_MODES {
    SPKR_OFF = 0,
    SPKR_ON = 1,
    SPKR_PIT_OFF = 2,
    SPKR_PIT_ON = 3
};

static struct {
    // Port 61h state
    uint8_t port61;
    
    // PIT state
    uint16_t pit_count;     // Current loaded divisor
    uint16_t pit_latch;     // Latched data
    uint8_t  pit_mode;      // PIT operating mode (0, 2, 3 etc)
    uint8_t  pit_bcd;
    uint8_t  pit_rl;        // Read/Load mode
    uint8_t  pit_state;     // State of read/write (0=LSB, 1=MSB)
    
    // Emulation state
    int      spkr_mode;     // enum SPKR_MODES
    int      pit_out;       // current logical output level (0 or 1)
    
    // Wave generation
    double   pit_index;     // fractional position in wave
    double   pit_max;       // total length of cycle
    double   pit_half;      // length of half cycle
    double   pit_new_max;
    double   pit_new_half;
    
    // Global state
    int      initialized;
    int      output_freq;
} spc;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Re-calculate the PIT wave parameters based on the new count and mode
static void vpcspeaker_update_pit(void)
{
    if(spc.pit_count == 0) return;
    
    double ticks = (double)spc.pit_count;
    
    switch(spc.pit_mode) {
        case 2: // Rate Generator (pulse)
            spc.pit_half = 1.0;
            spc.pit_max = ticks;
            break;
        case 3: // Square Wave
            spc.pit_new_max = ticks;
            spc.pit_new_half = ticks / 2.0;
            spc.pit_max = spc.pit_new_max;
            spc.pit_half = spc.pit_new_half;
            break;
        default: // Mode 0, 1, 4, 5 etc
            spc.pit_max = ticks;
            spc.pit_half = ticks;
            break;
    }
}

// Set Port 61h value
static void vpcspeaker_set_port61(uint8_t val)
{
    spc.port61 = val;
    int gate = (val & 0x01); // Timer 2 gate
    int data = (val & 0x02); // Speaker data
    
    if(gate && data) {
        spc.spkr_mode = SPKR_PIT_ON;
    } else if(!gate && data) {
        spc.spkr_mode = SPKR_ON;
    } else if(gate && !data) {
        spc.spkr_mode = SPKR_PIT_OFF;
    } else {
        spc.spkr_mode = SPKR_OFF;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void VPCSPEAKER_Init(int output_freq)
{
    memset(&spc, 0, sizeof(spc));
    spc.output_freq = output_freq;
    spc.pit_count = 0xFFFF; // Default divisor
    spc.pit_mode = 3;
    spc.pit_rl = 3; // LSB then MSB
    vpcspeaker_update_pit();
    
    // Read real port 61 to sync state
    vpcspeaker_set_port61(inportb(0x61));
    
    spc.initialized = 1;
}

void VPCSPEAKER_Shutdown(void)
{
    spc.initialized = 0;
}

int VPCSPEAKER_IsActive(void)
{
    return spc.initialized;
}

// ---------------------------------------------------------------------------
// IO Handler (matches SBEMU_IOTRAP_HANDLER signature)
// ---------------------------------------------------------------------------
uint32_t VPCSPEAKER_IOHandler(uint32_t port, uint32_t val, uint32_t out)
{
    if(!spc.initialized) return val;

    if(out) // Write
    {
        switch(port)
        {
            case 0x43: // PIT Mode/Command Register
            {
                outportb(0x43, val); // Passthrough to real hardware
                
                uint8_t channel = (val >> 6) & 3;
                if(channel == 2) { // We only care about Channel 2 (PC Speaker)
                    uint8_t rl = (val >> 4) & 3;
                    if(rl == 0) {
                        // Latch command
                        spc.pit_latch = spc.pit_count; // Simplified latching
                    } else {
                        spc.pit_rl = rl;
                        spc.pit_mode = (val >> 1) & 7;
                        if(spc.pit_mode > 5) spc.pit_mode -= 4; // Mode 6,7 map to 2,3
                        spc.pit_bcd = val & 1;
                        spc.pit_state = 0; // Next write is LSB
                    }
                }
                break;
            }
            case 0x42: // PIT Channel 2 Data
            {
                outportb(0x42, val); // Passthrough to real hardware
                
                if(spc.pit_rl == 1) { // LSB only
                    spc.pit_count = (spc.pit_count & 0xFF00) | (val & 0xFF);
                    vpcspeaker_update_pit();
                } else if(spc.pit_rl == 2) { // MSB only
                    spc.pit_count = (spc.pit_count & 0x00FF) | ((val & 0xFF) << 8);
                    vpcspeaker_update_pit();
                } else if(spc.pit_rl == 3) { // LSB then MSB
                    if(spc.pit_state == 0) {
                        spc.pit_count = (spc.pit_count & 0xFF00) | (val & 0xFF);
                        spc.pit_state = 1;
                    } else {
                        spc.pit_count = (spc.pit_count & 0x00FF) | ((val & 0xFF) << 8);
                        spc.pit_state = 0;
                        vpcspeaker_update_pit();
                        spc.pit_index = 0; // Reset phase on reload
                    }
                }
                break;
            }
            case 0x61: // System Control Port
            {
                outportb(0x61, val); // Passthrough to real hardware! Critical for DOS.
                vpcspeaker_set_port61(val & 0xFF);
                break;
            }
        }
    }
    else // Read
    {
        switch(port)
        {
            case 0x43:
                return inportb(0x43); // Passthrough
            case 0x42:
                // Return real PIT value to avoid breaking DOS timing routines
                return inportb(0x42);
            case 0x61:
            {
                // We must return the real port 61 (which contains timer tick bits etc)
                // but we also ensure our tracked bits 0 and 1 match just in case.
                uint8_t real61 = inportb(0x61);
                vpcspeaker_set_port61(real61);
                return real61;
            }
        }
    }
    return val;
}

// ---------------------------------------------------------------------------
// Audio generation — called every audio interrupt
// ---------------------------------------------------------------------------
void VPCSPEAKER_GenSamples(int16_t *pcm16, int samples, int freq, int domix)
{
    if(!spc.initialized) return;

    // How many PIT ticks happen per audio sample?
    double ticks_per_sample = (double)PIT_FREQ / freq;

    for(int i = 0; i < samples; i++)
    {
        int16_t sample_val = 0;
        
        switch(spc.spkr_mode)
        {
            case SPKR_OFF:
                sample_val = 0;
                break;
            case SPKR_ON: // Manual DAC (PWM)
                sample_val = SPKR_VOLUME;
                break;
            case SPKR_PIT_OFF:
                sample_val = 0;
                break;
            case SPKR_PIT_ON:
            {
                if(spc.pit_count < 2) {
                    sample_val = 0;
                    break;
                }
                
                // Advance PIT phase
                spc.pit_index += ticks_per_sample;
                while(spc.pit_index >= spc.pit_max) {
                    spc.pit_index -= spc.pit_max;
                }
                
                if(spc.pit_mode == 3) {
                    // Square wave
                    if(spc.pit_index < spc.pit_half) {
                        sample_val = SPKR_VOLUME;
                    } else {
                        sample_val = -SPKR_VOLUME;
                    }
                } else {
                    // Other modes (pulses) - approximated for audio
                    if(spc.pit_index < spc.pit_half) {
                        sample_val = -SPKR_VOLUME;
                    } else {
                        sample_val = SPKR_VOLUME;
                    }
                }
                break;
            }
        }

        if(domix) {
            // Saturating mix into both channels
            int32_t nl = (int32_t)pcm16[i*2]   + sample_val;
            int32_t nr = (int32_t)pcm16[i*2+1] + sample_val;
            pcm16[i*2]   = (nl >  32767) ?  32767 : (nl < -32768) ? -32768 : (int16_t)nl;
            pcm16[i*2+1] = (nr >  32767) ?  32767 : (nr < -32768) ? -32768 : (int16_t)nr;
        } else {
            pcm16[i*2]   = sample_val;
            pcm16[i*2+1] = sample_val;
        }
    }
}

#endif // SBEMU_PCSPEAKER
