
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <dos.h>
#include <string.h>
#include <stdlib.h>

#include <dpmi/dbgutil.h>
#include "vmpu.h"

#if SBEMU_VMPU

#define TSF_IMPLEMENTATION
#include "tsf.h"

static unsigned char fpu_buffer[512] __attribute__((aligned(16)));

/* 0x330: data port
 * 0x331: read: status port
 *       write: command port
 * status port:
 * bit 6: 0=ready to write cmd or MIDI data; 1=interface busy
 * bit 7: 0=data ready to read; 1=no data at data port
 * command port:
 *  0xff: reset - triggers ACK (FE) to be read from data port
 *  0x3f: set to UART mode - triggers ACK (FE) to be read from data port
 */

static bool bReset = false;
static uint8_t bUART = 0;

static const int midi_lengths[8] = {3, 3, 3, 3, 2, 2, 3, 1};
static unsigned char midi_buffer[4096];
static unsigned char midi_temp_buffer[4096];
static unsigned int midi_ptr = 0;
static unsigned int midi_available_ptr = 0;
static unsigned int midi_message_cntr = 0;
static bool midi_check_status_byte = 0;
static bool midi_in_sysex = false;
static unsigned char midi_status_byte = 0x80;
static unsigned char midi_mpu_status = 0x80;
static const unsigned char gm_reset[6] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
static const unsigned char gs_reset[11] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};

static char loaded_sf2[512];
static tsf *tsfrenderer;
static int VMPU_base;

static void VMPU_Process_Messages(void)
{
    unsigned char *temp_buffer = midi_buffer;
    unsigned int index = 0;
    while (index < midi_available_ptr)
    {
        switch (*temp_buffer & 0xF0)
        {
        case 0xD0:
            tsf_channel_set_pressure(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1] / 127.f);
        case 0xA0:
        {
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        }
        case 0x80:
            tsf_channel_note_off(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1]);
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        case 0x90:
            tsf_channel_note_on(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], temp_buffer[2] / 127.0f);
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        case 0xE0:
            tsf_channel_set_pitchwheel(tsfrenderer, temp_buffer[0] & 0xf, (temp_buffer[1] & 0x7f) | ((temp_buffer[2] & 0x7f) << 7));
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        case 0xC0:
            tsf_channel_set_presetnumber(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], (temp_buffer[0] & 0xf) == 0x9);
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        case 0xB0:
            tsf_channel_midi_control(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], temp_buffer[2]);
            index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            break;
        case 0xF0:
        {
            if (*temp_buffer == 0xFF)
            {
                {
                    tsf_reset(tsfrenderer);
                    tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                    tsf_set_volume(tsfrenderer, 1.0f);
                }
            }
            if (*temp_buffer == 0xF0)
            {
                unsigned char *sysexbuf = temp_buffer;
                unsigned int sysexlen = 0, i = 0;
                while (*temp_buffer != 0xF7)
                {
                    index++;
                    temp_buffer += 1;
                    sysexlen++;
                }

                index++;
                temp_buffer += 1;
                sysexlen++;

                //						_dprintf("SYSEX MESSAGE: ");
                //						for (i = 0; i < sysexlen; i++) {
                //							_dprintf("0x%02X ", sysexbuf[i]);
                //						}
                //						_dprintf("\n");

                if (sysexbuf[1] == 0x41 && sysexbuf[3] == 0x42 && sysexbuf[4] == 0x12 && sysexlen >= 9)
                {
                    uint32_t addr = ((uint32_t)sysexbuf[5] << 16) + ((uint32_t)sysexbuf[6] << 8) + (uint32_t)sysexbuf[7];
                    if (addr == 0x400004 && tsfrenderer)
                    {
                        tsf_set_volume(tsfrenderer, ((sysexbuf[8] > 127) ? 127 : sysexbuf[8]) / 127.f);
                    }
                }
                if (tsfrenderer && sysexbuf[1] == 0x7f && sysexbuf[2] == 0x7f && sysexbuf[3] == 0x04 && sysexbuf[4] == 0x01)
                {
                    //                                                        _dprintf("GM Master Vol 0x%02X\n", sysexbuf[6]);
                    tsf_set_volume(tsfrenderer, sysexbuf[6] / 127.f);
                }
                // TODO: Differentiate between GS and GM Resets.
                if (!memcmp(sysexbuf, gs_reset, sizeof(gs_reset)) || !memcmp(sysexbuf, gm_reset, sizeof(gm_reset)))
                {
                    tsf_reset(tsfrenderer);
                    tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                    tsf_set_volume(tsfrenderer, 1.0f);
                }
            }
            else
            {
                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
            }
            break;
        }
        }
    }

    memcpy(midi_temp_buffer, midi_buffer + midi_available_ptr, midi_ptr - midi_available_ptr);
    memcpy(midi_buffer, midi_temp_buffer, midi_ptr - midi_available_ptr);
    midi_ptr -= midi_available_ptr;
    midi_available_ptr = 0;
}

static void VMPU_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
    _LOG(("VMPU_Write(%X)=%X\n", port, value));
    if (port == VMPU_base+1)
    {
        if (value == 0x3f)
        {
            midi_mpu_status &= ~0x80;
        }
        if (value == 0xff)
        {
            fpu_save(fpu_buffer);
            VMPU_Process_Messages();
            fpu_restore(fpu_buffer);
            bReset = true;
            midi_ptr = 0;
            midi_available_ptr = 0;
            midi_message_cntr = 0;
            midi_check_status_byte = false;
            midi_mpu_status &= ~0x80;
        }
    }
    else
    {
        if (!bReset)
        {
            {
                if (midi_ptr >= 4092)
                    return;

                if (midi_in_sysex)
                {
                    midi_buffer[midi_ptr++] = value;
                    if (value == 0xF7)
                    {
                        midi_available_ptr = midi_ptr;
                        midi_message_cntr = 0;
                        midi_check_status_byte = false;
                        midi_in_sysex = false;
                    }
                    return;
                }

                if ((value & 0xF0) < 0x80 && midi_check_status_byte)
                {
                    midi_buffer[midi_ptr++] = midi_status_byte;
                    midi_check_status_byte = false;
                }

                if ((value & 0xF0) >= 0x80)
                {
                    midi_status_byte = value;
                    midi_check_status_byte = false;
                }

                midi_buffer[midi_ptr++] = value;
                midi_message_cntr++;

                if (midi_message_cntr >= midi_lengths[(midi_buffer[midi_available_ptr] >> 4) - 0x8])
                {
                    if (value == 0xF0)
                    {
                        midi_message_cntr = 0;
                        midi_in_sysex = true;
                        return;
                    }
                    midi_available_ptr = midi_ptr;
                    midi_message_cntr = 0;
                    midi_check_status_byte = true;
                }
            }
        }
    }
    return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
    _LOG(("VMPU_Read(%X)\n", port));
    if (port == VMPU_base)
    {
        midi_mpu_status |= 0x80;
        if (bReset)
        {
            bReset = false;
            return 0xfe;
        }
        return 0xfe; // Always return Active Sensing.
    }
    else
    {
        return midi_mpu_status | (midi_ptr >= 4092 ? 0x40 : 0);
    }
}


static int tsf_stream_linear_memory_read(struct tsf_stream_memory* m, void* ptr, unsigned int size) { if (size > m->total - m->pos) size = m->total - m->pos; DPMI_LMemcpy(DPMI_PTR2L(ptr), (uintptr_t)m->buffer+m->pos, size); m->pos += size; return size; }
static int tsf_stream_linear_memory_skip(struct tsf_stream_memory* m, unsigned int count) { if (m->pos + count > m->total) return 0; m->pos += count; return 1; }
static tsf* tsf_load_linear_memory(uint32_t linear_mem, int size)
{
	struct tsf_stream stream = { TSF_NULL, (int(*)(void*,void*,unsigned int))&tsf_stream_linear_memory_read, (int(*)(void*,unsigned int))&tsf_stream_linear_memory_skip };
	struct tsf_stream_memory f = { 0, 0, 0 };
	f.buffer = (const char*)linear_mem;
	f.total = size;
	stream.data = &f;
	return tsf_load(&stream);
}

BOOL VMPU_Init(int baseaddr, int* voices, int freq, const char* sf2)
{
    VMPU_base = 0x330;
    if (*voices < 32)
        *voices = 32;
    if (*voices > 256)
        *voices = 256;
    
    tsfrenderer = tsf_load_filename(sf2);

    if (tsfrenderer)
    {
        int channel = 0;
        tsf_set_max_voices(tsfrenderer, *voices);
        tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, freq, 0);
        for (channel = 15; channel >= 0; --channel)
            tsf_channel_midi_control(tsfrenderer, channel, 121, 0);
        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);

        VMPU_base = baseaddr;

        memcpy(loaded_sf2, sf2, min(strlen(sf2)+1, sizeof(loaded_sf2)));
        loaded_sf2[sizeof(loaded_sf2)-1] = '\0';
        return TRUE;
    }
    else
    {
        memset(loaded_sf2, 0, sizeof(loaded_sf2));
        return FALSE;
    }
}

BOOL VMPU_Reset(int baseaddr, int* voices, int freq, const char* sf2, uint32_t sf2_linear_mem, int bytes)
{
    VMPU_base = 0x330;
    if (*voices < 32)
        *voices = 32;
    if (*voices > 256)
        *voices = 256;

    if(tsfrenderer && stricmp(loaded_sf2, sf2) != 0)
    {
        tsf_close(tsfrenderer);
        tsfrenderer = NULL;
    }

    if(!tsfrenderer)
        tsfrenderer = tsf_load_linear_memory(sf2_linear_mem, bytes);

    if (tsfrenderer)
    {
        int channel = 0;
        tsf_set_max_voices(tsfrenderer, *voices);
        tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, freq, 0);
        for (channel = 15; channel >= 0; --channel)
            tsf_channel_midi_control(tsfrenderer, channel, 121, 0);
        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);

        VMPU_base = baseaddr;

        memcpy(loaded_sf2, sf2, min(strlen(sf2), sizeof(loaded_sf2)-1));
        loaded_sf2[sizeof(loaded_sf2)-1] = '\0';
        return TRUE;
    }
    else
    {
        memset(loaded_sf2, 0, sizeof(loaded_sf2));
        return FALSE;
    }
}

BOOL VMPU_IsActive()
{
    if(!tsfrenderer) return FALSE;
    fpu_save(fpu_buffer);
    VMPU_Process_Messages();
    fpu_restore(fpu_buffer);
    return !!tsf_active_voice_count(tsfrenderer);
}

void VMPU_GenSamples(int16_t* pcm16, int samples, int freq, BOOL domix)
{
    if (tsfrenderer)
    {
        fpu_save(fpu_buffer);
        VMPU_Process_Messages();
        tsf_set_samplerate_output(tsfrenderer, freq);
        tsf_render_short(tsfrenderer, pcm16, samples, domix);
        fpu_restore(fpu_buffer);
    }
}

/* SB-MIDI data written with DSP cmd 0x38 */

void VMPU_SBMidi_RawWrite(uint8_t value)
//////////////////////////////////////////
{
}

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VMPU_Write(port, val), val) : (val &= ~0xff, val |= VMPU_Read(port));
}
#endif
