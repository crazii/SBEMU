#include "platform.h"
#include "vgus.h"
#include "dpmi/dbgutil.h"
#include <string.h>
#include <stdlib.h>

#if SBEMU_GUS

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static VGUS_State vgus;
static VGUS_DMAReady_Callback vgus_dma_cb = NULL;

void VGUS_SetDMACallback(VGUS_DMAReady_Callback cb) { vgus_dma_cb = cb; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Clamp int to int16 range
static inline int16_t clamp16(int v)
{
    if(v >  32767) return  32767;
    if(v < -32768) return -32768;
    return (int16_t)v;
}

// Read an 8-bit sample from GUS DRAM at byte address (with 8/16-bit awareness)
static inline int16_t vgus_read_sample8(uint32_t byte_addr)
{
    if(!vgus.dram || byte_addr >= VGUS_DRAM_SIZE) return 0;
    // Convert unsigned 8-bit -> signed 16-bit
    return (int16_t)((int)(vgus.dram[byte_addr]) - 128) << 8;
}

static inline int16_t vgus_read_sample16(uint32_t byte_addr)
{
    if(!vgus.dram || (byte_addr + 1) >= VGUS_DRAM_SIZE) return 0;
    int16_t s;
    s = (int16_t)(((uint16_t)vgus.dram[byte_addr]) |
                  ((uint16_t)vgus.dram[byte_addr+1] << 8));
    return s;
}

// ---------------------------------------------------------------------------
// GF1 register read/write helpers
// ---------------------------------------------------------------------------

// reg: low 7 bits select register, bit7 selects read/write bank (handled by caller)
static void vgus_gf1_write(uint8_t page, uint8_t reg, uint16_t data)
{
    // Global registers: page >= 0x40
    if(page >= 0x40)
    {
        switch(reg & 0x7F)
        {
            case VGUS_GREG_DMA_CTRL:    // 0x41
                vgus.dma_ctrl = (uint8_t)data;
                // A write here clears DMA-done status and starts transfer
                if(vgus.dma_ctrl & VGUS_DMA_ENABLE)
                {
                    vgus.dma_ctrl &= ~VGUS_DMA_DONE; // clear done flag
                    // Invoke the main.c callback immediately so that ULTRINIT-style
                    // tight-loop polling of the DONE bit does not time out before
                    // the next audio interrupt (~46 ms).
                    if(vgus_dma_cb) vgus_dma_cb();
                }
                break;
            case VGUS_GREG_DMA_START:   // 0x42 — DMA destination (>>4 of DRAM byte addr)
                vgus.dma_gus_addr = ((uint32_t)data & 0x1FFF) << 4;
                break;
            case VGUS_GREG_DRAM_ADDR_LO: // 0x43
                vgus.dram_ptr = (vgus.dram_ptr & 0xFF0000) | data;
                break;
            case VGUS_GREG_DRAM_ADDR_HI: // 0x44 (8-bit)
                // GUS Classic only supports up to 1MB (20-bit address). Mask to 4 bits.
                vgus.dram_ptr = (vgus.dram_ptr & 0x00FFFF) | (((uint32_t)data & 0x0F) << 16);
                break;
            case VGUS_GREG_TIMER_CTRL:  // 0x45
            {
                uint8_t old_tc = vgus.timer_ctrl;
                vgus.timer_ctrl = (uint8_t)data;
                // Start Timer 1 if bit 0 transitioned 0->1 (or remains 1 but was reset? Let's just reset if 1)
                if(data & 0x01) {
                    if(!(old_tc & 0x01) || vgus.timer1_ticks == 0) {
                        vgus.timer1_ticks = (vgus.output_freq * (uint32_t)(256 - vgus.timer1_count) * 80) / 1000000;
                        if(vgus.timer1_ticks < 1) vgus.timer1_ticks = 1;
                    }
                } else {
                    vgus.timer1_ticks = 0;
                }
                // Start Timer 2 if bit 1 transitioned 0->1
                if(data & 0x02) {
                    if(!(old_tc & 0x02) || vgus.timer2_ticks == 0) {
                        vgus.timer2_ticks = (vgus.output_freq * (uint32_t)(256 - vgus.timer2_count) * 320) / 1000000;
                        if(vgus.timer2_ticks < 1) vgus.timer2_ticks = 1;
                    }
                } else {
                    vgus.timer2_ticks = 0;
                }
                break;
            }
            case VGUS_GREG_TIMER1_CNT:  // 0x46
                vgus.timer1_count = (uint8_t)data;
                break;
            case VGUS_GREG_TIMER2_CNT:  // 0x47
                vgus.timer2_count = (uint8_t)data;
                break;
            case VGUS_GREG_RESET:       // 0x4C
                vgus.reset_reg = (uint8_t)data;
                if(!(vgus.reset_reg & 0x01))
                {
                    // Master reset asserted (bit0=0): clear all voices
                    memset(vgus.voice, 0, sizeof(vgus.voice));
                    for(int i = 0; i < VGUS_MAX_VOICES; i++)
                        vgus.voice[i].ctrl = VGUS_VC_STOPPED|VGUS_VC_STOP;
                    vgus.irq_status = 0;
                    vgus.last_irq_status = 0;
                    vgus.dma_ctrl   = 0;
                    vgus.timer_ctrl = 0;
                    vgus.timer1_ticks = 0;
                    vgus.timer2_ticks = 0;
                    _LOG("GUS: master reset\n");
                }
                break;
            case 0x0E:                  // 0x0E — Active voices register (global)
            case 0x4E:
                // bits 5..0: number of active voices - 1 (14..31 mapped to 14..32)
                vgus.active_voices = (uint8_t)((data & 0x3F) + 1);
                if(vgus.active_voices < 14) vgus.active_voices = 14;
                if(vgus.active_voices > 32) vgus.active_voices = 32;
                break;
            default:
                break;
        }
        return;
    }

    // Voice registers: page < 32
    uint8_t v = page & 0x1F;
    VGUS_Voice *vv = &vgus.voice[v];

    switch(reg & 0x7F)
    {
        case VGUS_REG_VOICE_CTRL:   // 0x00
            vv->ctrl = (uint8_t)data;
            if(vv->ctrl & VGUS_VC_STOP)
                vv->ctrl |= VGUS_VC_STOPPED;
            break;
        case VGUS_REG_FREQ_CTRL:    // 0x01 — frequency control word
            vv->freq = data & 0xFFFF;
            break;
        case VGUS_REG_ADDR_HI:      // 0x02 — start address high (bits 22..7)
            vv->start = (vv->start & 0x0000007FUL) | (((uint32_t)data & 0x1FFF) << 7);
            break;
        case VGUS_REG_ADDR_LO:      // 0x03 — start address low (bits 6..0 as fixed point)
            vv->start = (vv->start & ~0x7FUL) | ((uint32_t)data & 0x7F);
            break;
        case VGUS_REG_END_HI:       // 0x04 — end address high
            vv->end = (vv->end & 0x0000007FUL) | (((uint32_t)data & 0x1FFF) << 7);
            break;
        case VGUS_REG_END_LO:       // 0x05 — end address low
            vv->end = (vv->end & ~0x7FUL) | ((uint32_t)data & 0x7F);
            break;
        case VGUS_REG_VOL_RATE:     // 0x06
            vv->vol_rate = (uint8_t)data;
            break;
        case VGUS_REG_VOL_START:    // 0x07 — 12-bit index in bits [15:4] of 16-bit register
            // GUS SDK: register value = volume_index << 4.  Extract: vol = data >> 4.
            vv->vol_start = ((uint16_t)data) << 4;
            break;
        case VGUS_REG_VOL_END:      // 0x08
            vv->vol_end   = ((uint16_t)data) << 4;
            break;
        case VGUS_REG_CUR_VOL:      // 0x09 — set current volume directly
            vv->volume    = (uint16_t)((data >> 4) & 0x0FFF);
            break;
        case VGUS_REG_CUR_ADDR_HI:  // 0x0A
            vv->current = (vv->current & 0x7FUL) | (((uint32_t)data & 0x1FFF) << 7);
            break;
        case VGUS_REG_CUR_ADDR_LO:  // 0x0B
            vv->current = (vv->current & ~0x7FUL) | ((uint32_t)data & 0x7F);
            break;
        case VGUS_REG_PAN:          // 0x0C
            vv->panning = (uint8_t)(data & 0x0F);
            break;
        case VGUS_REG_VOL_CTRL:     // 0x0D
            vv->vol_ctrl = (uint8_t)data;
            break;
        case 0x0E:  // Active voices — accessible with page < 0x40 too
            vgus.active_voices = (uint8_t)((data & 0x3F) + 1);
            if(vgus.active_voices < 14) vgus.active_voices = 14;
            if(vgus.active_voices > 32) vgus.active_voices = 32;
            break;
        default:
            break;
    }
}

static uint16_t vgus_gf1_read(uint8_t page, uint8_t reg)
{
    if(page >= 0x40)
    {
        switch(reg & 0x7F)
        {
            case VGUS_GREG_DRAM_ADDR_LO: return (uint16_t)(vgus.dram_ptr & 0xFFFF);
            case VGUS_GREG_DRAM_ADDR_HI: return (uint16_t)((vgus.dram_ptr >> 16) & 0xFF);
            case VGUS_GREG_TIMER_CTRL:return vgus.timer_ctrl;
            case VGUS_GREG_TIMER1_CNT:return vgus.timer1_count;
            case VGUS_GREG_TIMER2_CNT:return vgus.timer2_count;
            // Reset register: bit0=master enable, bit1=DAC enable, bit2=IRQ enable
            // After writing 0x01 (out of reset), read should return 0x01 immediately
            case VGUS_GREG_RESET:     return vgus.reset_reg;
            case 0x0E:
            case 0x4E: return (uint16_t)((vgus.active_voices - 1) & 0x3F);
            // IRQ source for DMA: returns DMA_DONE in bit6
            case VGUS_GREG_DMA_CTRL:
            {
                uint16_t r = vgus.dma_ctrl;
                vgus.irq_status &= ~VGUS_IRQ_DMA_TC; // clear on read
                return r;
            }
            default: return 0;
        }
    }

    uint8_t v = page & 0x1F;
    VGUS_Voice *vv = &vgus.voice[v];

    switch(reg & 0x7F)
    {
        case VGUS_REG_VOICE_CTRL:   return vv->ctrl;
        case VGUS_REG_FREQ_CTRL:    return (uint16_t)vv->freq;
        case VGUS_REG_ADDR_HI:      return (uint16_t)((vv->start >> 7) & 0x1FFF);
        case VGUS_REG_ADDR_LO:      return (uint16_t)(vv->start & 0x7F);
        case VGUS_REG_END_HI:       return (uint16_t)((vv->end >> 7) & 0x1FFF);
        case VGUS_REG_END_LO:       return (uint16_t)(vv->end & 0x7F);
        case VGUS_REG_VOL_RATE:     return vv->vol_rate;
        // Return volume with the 12-bit index back in bits [15:4] as hardware does.
        case VGUS_REG_VOL_START:    return (uint16_t)(vv->vol_start >> 4);
        case VGUS_REG_VOL_END:      return (uint16_t)(vv->vol_end   >> 4);
        case VGUS_REG_CUR_VOL:      return (uint16_t)(vv->volume    << 4);
        case VGUS_REG_CUR_ADDR_HI:  return (uint16_t)((vv->current >> 7) & 0x1FFF);
        case VGUS_REG_CUR_ADDR_LO:  return (uint16_t)(vv->current & 0x7F);
        case VGUS_REG_PAN:          return vv->panning;
        case VGUS_REG_VOL_CTRL:     return vv->vol_ctrl;
        default:                    return 0;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int VGUS_Init(int base, int irq, int dma, int output_freq)
{
    memset(&vgus, 0, sizeof(vgus));

    vgus.dram = (uint8_t*)malloc(VGUS_DRAM_SIZE);
    if(!vgus.dram)
    {
        _LOG("GUS: DRAM malloc failed\n");
        return 0;
    }
    memset(vgus.dram, 0, VGUS_DRAM_SIZE);

    vgus.base_addr    = base;
    vgus.irq          = irq;
    vgus.dma          = dma;
    vgus.output_freq  = output_freq;
    vgus.active_voices = 14; // GUS minimum active voices
    vgus.initialized  = 1;

    VGUS_Reset();

    _LOG("GUS: init base=%x irq=%d dma=%d freq=%d\n", base, irq, dma, output_freq);
    return 1;
}

void VGUS_Shutdown(void)
{
    if(vgus.dram)
    {
        free(vgus.dram);
        vgus.dram = NULL;
    }
    vgus.initialized = 0;
}

void VGUS_Reset(void)
{
    // Reset all voices to stopped state
    for(int i = 0; i < VGUS_MAX_VOICES; i++)
    {
        vgus.voice[i].ctrl     = VGUS_VC_STOPPED | VGUS_VC_STOP;
        vgus.voice[i].vol_ctrl = VGUS_VC_STOPPED | VGUS_VC_STOP;
        vgus.voice[i].volume   = 0;
    }
    vgus.reset_reg     = 0x00; // In reset
    vgus.irq_status    = 0;
    vgus.last_irq_status = 0;
    vgus.dma_ctrl      = 0;
    vgus.mix_ctrl      = 0;
    vgus.timer_ctrl    = 0;
    vgus.timer1_ticks  = 0;
    vgus.timer2_ticks  = 0;
    vgus.gf1_page      = 0;
    vgus.gf1_reg       = 0;
    vgus.dram_ptr      = 0;
    vgus.dma_irq_pending = 0;
}

int VGUS_IsActive(void)
{
    // Active = initialized + reset bit 0 set (out of reset)
    return vgus.initialized && (vgus.reset_reg & 0x01);
}

VGUS_State* VGUS_GetState(void)
{
    return &vgus;
}

// ---------------------------------------------------------------------------
// Unified I/O handler (called for all trapped GUS ports)
// ---------------------------------------------------------------------------
uint32_t VGUS_IOHandler(uint32_t port, uint32_t val, uint32_t out)
{
    if(!vgus.initialized) return val;

    int offset = (int)port - vgus.base_addr;

    // --- 2X0 group (base + 0..F) ---
    if(offset >= 0 && offset <= 0x0F)
    {
        if(out)
        {
            switch(offset)
            {
                case VGUS_PORT_MIXCTRL: // 2X0
                    // bit6=1: 2XB writes IRQ control; bit6=0: DMA control
                    // bit0=0: line in enabled; bit1=0: line out enabled; bit2=0: mic in
                    vgus.mix_ctrl = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_TIMER_CTRL: // 2X8 — GF1 timer control
                {
                    uint8_t tc = (uint8_t)(val & 0xFF);
                    // Per real GUS 2X8 layout:
                    // Bit 7: set=clear timer IRQ status; clear=load new control
                    // Bit 6: mask Timer 1 (1=masked)
                    // Bit 5: mask Timer 2 (1=masked)
                    // Bit 1: start Timer 2
                    // Bit 0: start Timer 1
                    if(tc & 0x80)
                    {
                        // Clear timer IRQ status bits only
                        vgus.irq_status &= ~(VGUS_IRQ_TIMER1 | VGUS_IRQ_TIMER2);
                        vgus.last_irq_status &= ~(VGUS_IRQ_TIMER1 | VGUS_IRQ_TIMER2);
                    }
                    else
                    {
                        vgus.timer1_mask = (tc & 0x40) ? 1 : 0;
                        vgus.timer2_mask = (tc & 0x20) ? 1 : 0;
                        if(tc & 0x01) // start timer 1
                        {
                            vgus.timer1_ticks = (vgus.output_freq * (uint32_t)(256 - vgus.timer1_count) * 80) / 1000000;
                            if(vgus.timer1_ticks < 1) vgus.timer1_ticks = 1;
                        } else {
                            vgus.timer1_ticks = 0;
                        }
                        if(tc & 0x02) // start timer 2
                        {
                            vgus.timer2_ticks = (vgus.output_freq * (uint32_t)(256 - vgus.timer2_count) * 320) / 1000000;
                            if(vgus.timer2_ticks < 1) vgus.timer2_ticks = 1;
                        } else {
                            vgus.timer2_ticks = 0;
                        }
                        vgus.timer_ctrl = (vgus.timer_ctrl & ~0x03) | (tc & 0x03);
                    }
                    break;
                }
                case VGUS_PORT_TIMER1_CNT: // 2X9
                    vgus.timer1_count = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_TIMER2_CNT: // 2XA
                    vgus.timer2_count = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_IRQ_DMA_CTRL: // 2XB
                    vgus.irq_dma_ctrl = (uint8_t)(val & 0xFF);
                    // This register controls which IRQ/DMA lines are active.
                    // We store it for ULTRASND detection; actual routing is fixed.
                    break;
                case VGUS_PORT_REG_CTRL: // 2XF
                    vgus.reg_ctrl = (uint8_t)(val & 0xFF);
                    break;
                default: break;
            }
        }
        else // read
        {
            switch(offset)
            {
                case VGUS_PORT_MIXCTRL:     return vgus.mix_ctrl;
                case VGUS_PORT_IRQ_STATUS:  // 2X6
                {
                    uint8_t s = vgus.irq_status;
                    // Reading 2X6 only clears Timer IRQs. DMA/Voice IRQs are cleared via 0x41/0x8F.
                    vgus.irq_status &= ~(VGUS_IRQ_TIMER1 | VGUS_IRQ_TIMER2);
                    vgus.last_irq_status &= ~(VGUS_IRQ_TIMER1 | VGUS_IRQ_TIMER2);
                    return s;
                }
                case VGUS_PORT_TIMER_CTRL:  return vgus.timer_ctrl;
                case VGUS_PORT_TIMER1_CNT:  return vgus.timer1_count;
                case VGUS_PORT_TIMER2_CNT:  return vgus.timer2_count;
                case VGUS_PORT_IRQ_DMA_CTRL: return vgus.irq_dma_ctrl;
                case VGUS_PORT_REG_CTRL:
                    // Bits [7:4] = board revision 4 (GUS Classic production).
                    // Bits [3:0] = 0xF (all capability flags asserted).
                    // ULTRINIT checks this port to confirm GUS presence.
                    return 0x4F;
                default:                    return 0xFF;
            }
        }
        return val;
    }

    // --- 3X0 group (base + 0x100..0x10F) ---
    if(offset >= 0x100 && offset <= 0x10F)
    {
        int off2 = offset - 0x100;
        
        // Helper to determine if current GF1 register is 8-bit
        int is8bit = 0;
        uint8_t r = vgus.gf1_reg & 0x7F;
        if(vgus.gf1_page >= 0x40) {
            // 8-bit global regs: DMA ctrl, timer ctrl/counts, sample ctrl, IRQ latch, reset, active voices, DRAM addr high
            if(r == 0x41 || r == 0x44 || r == 0x45 || r == 0x46 || r == 0x47 || r == 0x49 || r == 0x4B || r == 0x4C
               || r == 0x0E || r == 0x4E) is8bit = 1;
        } else {
            // 8-bit voice regs: voice ctrl, vol rate, panning, vol ctrl, vol start, vol end, active voices
            if(r == 0x00 || r == 0x06 || r == 0x07 || r == 0x08 || r == 0x0C || r == 0x0D || r == 0x0E || r == 0x4E) is8bit = 1;
        }

        if(out)
        {
            switch(off2)
            {
                case VGUS_PORT_MIDI_CTRL - 0x100: // 3X0
                    break;
                case VGUS_PORT_MIDI_DATA - 0x100: // 3X1
                    break;
                case VGUS_PORT_GF1_PAGE - 0x100:  // 3X2
                    vgus.gf1_page = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_GF1_REGSEL - 0x100: // 3X3
                    vgus.gf1_reg = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_GF1_DATALO - 0x100: // 3X4
                    vgus.gf1_data_latch = (uint8_t)(val & 0xFF);
                    break;
                case VGUS_PORT_GF1_DATAHI - 0x100: // 3X5
                {
                    if(is8bit) {
                        _LOG("GUS W8: page=%02x reg=%02x val=%02x rst=%02x\n",
                             vgus.gf1_page, vgus.gf1_reg & 0x7F, (unsigned)(val & 0xFF), vgus.reset_reg);
                        vgus_gf1_write(vgus.gf1_page, vgus.gf1_reg, (uint16_t)(val & 0xFF));
                    } else {
                        uint16_t data16 = vgus.gf1_data_latch | ((uint16_t)(val & 0xFF) << 8);
                        _LOG("GUS W16: page=%02x reg=%02x val=%04x\n",
                             vgus.gf1_page, vgus.gf1_reg & 0x7F, (unsigned)data16);
                        vgus_gf1_write(vgus.gf1_page, vgus.gf1_reg, data16);
                    }
                    break;
                }
                case VGUS_PORT_GF1_DRAM - 0x100:  // 3X7 write
                    _LOG("GUS DRAM W: ptr=%06x val=%02x\n", vgus.dram_ptr, (unsigned)(val & 0xFF));
                    if(vgus.dram)
                    {
                        uint32_t addr = vgus.dram_ptr & (VGUS_DRAM_SIZE - 1);
                        vgus.dram[addr] = (uint8_t)(val & 0xFF);
                        vgus.dram_ptr = (vgus.dram_ptr + 1) & 0xFFFFFF;
                    }
                    break;
                default: break;
            }
        }
        else // read
        {
            switch(off2)
            {
                case VGUS_PORT_MIDI_CTRL - 0x100:  return 0x03; // TX ready, RX empty
                case VGUS_PORT_MIDI_DATA - 0x100:  return 0xFF;
                case VGUS_PORT_GF1_PAGE - 0x100:   return vgus.gf1_page;
                case VGUS_PORT_GF1_REGSEL - 0x100: return vgus.gf1_reg;
                case VGUS_PORT_GF1_DATALO - 0x100: // 3X4
                {
                    if(is8bit) {
                        // 8-bit GF1 registers follow DOSBox convention: the real value
                        // is placed in the HIGH byte, so 3X4 (low byte) always reads 0.
                        // Programs that want the value must read from 3X5 instead.
                        return 0;
                    } else {
                        // 16-bit register: return low byte and latch high byte for 3X5.
                        uint16_t d = vgus_gf1_read(vgus.gf1_page, vgus.gf1_reg);
                        vgus.gf1_data_latch = (uint8_t)(d >> 8);
                        return (d & 0xFF);
                    }
                }
                case VGUS_PORT_GF1_DATAHI - 0x100: // 3X5
                {
                    uint32_t rd;
                    if(is8bit) {
                        // 8-bit register: read the actual value directly (like DOSBox >> 8 on val<<8)
                        rd = vgus_gf1_read(vgus.gf1_page, vgus.gf1_reg) & 0xFF;
                    } else {
                        // 16-bit register: return the high byte latched during 3X4 read
                        rd = vgus.gf1_data_latch;
                    }
                    _LOG("GUS R5: page=%02x reg=%02x ret=%02x is8=%d\n",
                         vgus.gf1_page, vgus.gf1_reg & 0x7F, rd, is8bit);
                    return rd;
                }

                case VGUS_PORT_GF1_DRAM - 0x100:  // 3X7 read
                    if(vgus.dram)
                    {
                        uint32_t addr = vgus.dram_ptr & (VGUS_DRAM_SIZE - 1);
                        uint8_t b = vgus.dram[addr];
                        // _LOG("GUS DRAM R: ptr=%06x val=%02x\n", vgus.dram_ptr, b);
                        vgus.dram_ptr = (vgus.dram_ptr + 1) & 0xFFFFFF;
                        return b;
                    }
                    return 0xFF;
                default: return 0xFF;
            }
        }
        return val;
    }

    return val;
}

// ---------------------------------------------------------------------------
// DMA simulation: called when the ISA DMA transfer completes from main.c
// bytes: pointer to data, count: number of bytes transferred
// ---------------------------------------------------------------------------
void VGUS_DMATransfer(uint8_t *data, uint32_t count)
{
    if(!vgus.initialized || !vgus.dram) return;
    if(!(vgus.dma_ctrl & VGUS_DMA_ENABLE)) return;

    uint32_t addr = vgus.dma_gus_addr & (VGUS_DRAM_SIZE - 1);
    int direction = vgus.dma_ctrl & 0x02; // 0x02 = Read from GUS to PC RAM
    
    for(uint32_t i = 0; i < count; i++)
    {
        if(direction) {
            uint8_t b = vgus.dram[addr];
            if(vgus.dma_ctrl & VGUS_DMA_FLIP_MSB)
                b ^= 0x80;
            data[i] = b;
        } else {
            uint8_t b = data[i];
            if(vgus.dma_ctrl & VGUS_DMA_FLIP_MSB)
                b ^= 0x80; // convert unsigned to signed (common for GUS samples)
            vgus.dram[addr] = b;
        }
        addr = (addr + 1) & (VGUS_DRAM_SIZE - 1);
    }
    vgus.dma_gus_addr = addr;

    // Mark transfer complete
    vgus.dma_ctrl |= VGUS_DMA_DONE;
    vgus.dma_ctrl &= ~VGUS_DMA_ENABLE;

    if(vgus.dma_ctrl & VGUS_DMA_IRQ)
    {
        vgus.irq_status |= VGUS_IRQ_DMA_TC;
        vgus.dma_irq_pending = 1;
    }
}

// ---------------------------------------------------------------------------
// Audio synthesis: mix up to active_voices voices into stereo pcm16
// ---------------------------------------------------------------------------
void VGUS_GenSamples(int16_t *pcm16, int samples, int freq, int domix)
{
    if(!VGUS_IsActive()) return;

    // Frequency step per sample for a given voice:
    //   GF1 freq register = (playback_hz * 512) / (output_hz * voices_factor)
    //   increment = freq_reg * output_freq_scale
    //   The GF1 uses a 19-bit integer + 9-bit fractional address format.
    //   freq_reg encodes: (desired_hz / (output_hz)) * 512 in 16-bit form.
    //   So per output sample, position advances by: freq_reg * (gus_osc_rate / output_freq)
    //   The GUS oscillator base is 9.878 MHz / (active_voices * 2) — simplified here.

    int voices = vgus.active_voices;
    if(voices < 14) voices = 14;
    if(voices > 32) voices = 32;

    for(int s = 0; s < samples; s++)
    {
        int32_t out_l = 0, out_r = 0;

        for(int v = 0; v < voices; v++)
        {
            VGUS_Voice *vv = &vgus.voice[v];

            // Skip stopped or muted voices
            if((vv->ctrl & VGUS_VC_STOPPED) || (vv->ctrl & VGUS_VC_STOP))
                continue;
            if(vv->volume == 0)
                continue;

            // Get sample from DRAM
            // Position is in 23.9 fixed-point (bits 31..9 = byte addr, bits 8..0 = fraction)
            uint32_t byte_addr = vv->current >> 9;
            int16_t sample;
            if(vv->ctrl & VGUS_VC_16BIT)
            {
                // 16-bit samples: byte address must be doubled and aligned
                byte_addr = (byte_addr & 0x0C0000) | ((byte_addr & ~0x0C0000) << 1);
                sample = vgus_read_sample16(byte_addr);
            }
            else
            {
                sample = vgus_read_sample8(byte_addr);
            }

            // Apply volume (0-4095 → 0-1.0 scale, using shift)
            int32_t sv = ((int32_t)sample * vv->volume) >> 12;

            // Panning: 0=left, 8=center, 15=right
            int pan = vv->panning; // 0..15
            int32_t lvol = (15 - pan); // 15..0
            int32_t rvol = pan;        // 0..15
            out_l += (sv * lvol) >> 4;
            out_r += (sv * rvol) >> 4;

            // Advance position.
            // GF1 freq register: bit 0 is always 0 on hardware; the real step is
            // (freq_reg >> 1) in 1/512 sub-sample units — matching DOSBox WAVE_FRACT=9.
            // Using the raw register value (without >>1) doubles pitch/speed.
            uint32_t advance = vv->freq >> 1;
            int backwards = (vv->ctrl & VGUS_VC_BACKWARDS) ? 1 : 0;

            if(backwards)
            {
                if(vv->current > advance)
                    vv->current -= advance;
                else
                    vv->current = vv->end;
            }
            else
            {
                vv->current += advance;
            }

            // Loop / boundary handling
            if(!backwards && vv->current >= vv->end)
            {
                if(vv->ctrl & VGUS_VC_LOOP)
                {
                    if(vv->ctrl & VGUS_VC_BIDIR)
                    {
                        vv->current = vv->end;
                        vv->ctrl |= VGUS_VC_BACKWARDS; // reverse
                    }
                    else
                    {
                        vv->current = vv->start + (vv->current - vv->end);
                    }
                }
                else
                {
                    vv->ctrl |= VGUS_VC_STOPPED | VGUS_VC_STOP;
                    if(vv->ctrl & VGUS_VC_IRQ)
                        vv->wave_irq = 1;
                }
                if(vv->ctrl & VGUS_VC_IRQ)
                    vgus.irq_status |= VGUS_IRQ_WAVETABLE;
            }
            else if(backwards && vv->current <= vv->start)
            {
                if(vv->ctrl & VGUS_VC_LOOP)
                {
                    if(vv->ctrl & VGUS_VC_BIDIR)
                    {
                        vv->current = vv->start;
                        vv->ctrl &= ~VGUS_VC_BACKWARDS; // forward again
                    }
                    else
                    {
                        vv->current = vv->end;
                    }
                }
                else
                {
                    vv->ctrl |= VGUS_VC_STOPPED | VGUS_VC_STOP;
                    if(vv->ctrl & VGUS_VC_IRQ)
                        vv->wave_irq = 1;
                }
                if(vv->ctrl & VGUS_VC_IRQ)
                    vgus.irq_status |= VGUS_IRQ_WAVETABLE;
            }

            // Volume ramp
            if(!(vv->vol_ctrl & (VGUS_VC_STOPPED | VGUS_VC_STOP)))
            {
                int going_up = !(vv->vol_ctrl & VGUS_VC_BACKWARDS);
                // GUS SDK: vol_rate bits [7:6] = scale (0=×1, 1=×8, 2=×64, 3=×512),
                // bits [5:0] = speed index. Scale is encoded as 3 extra shift bits.
                int scale_shift = ((vv->vol_rate >> 6) & 0x3) * 3; // 0, 3, 6, or 9
                int speed       = vv->vol_rate & 0x3F;
                int step        = speed ? (1 << scale_shift) : 0;
                if(step < 1 && speed) step = 1;
                if(step > 4095) step = 4095;
                if(going_up)
                {
                    if((int)vv->volume + step <= (int)vv->vol_end)
                        vv->volume += step;
                    else
                    {
                        vv->volume = vv->vol_end;
                        vv->vol_ctrl |= VGUS_VC_STOPPED;
                        if(vv->vol_ctrl & VGUS_VC_IRQ)
                            vgus.irq_status |= VGUS_IRQ_VOLRAMP;
                    }
                }
                else
                {
                    if((int)vv->volume - step >= (int)vv->vol_start)
                        vv->volume -= step;
                    else
                    {
                        vv->volume = vv->vol_start;
                        vv->vol_ctrl |= VGUS_VC_STOPPED;
                        if(vv->vol_ctrl & VGUS_VC_IRQ)
                            vgus.irq_status |= VGUS_IRQ_VOLRAMP;
                    }
                }
            }
        }

        // GUS hardware sums all voices and clips — no per-voice normalization.
        // Dividing by active_voices is wrong: it mutes single voices when 32 are
        // configured but only 1 or 2 are actually playing.
        // clamp16() below handles saturation naturally.

        if(domix)
        {
            pcm16[s*2]   = clamp16((int)pcm16[s*2]   + out_l);
            pcm16[s*2+1] = clamp16((int)pcm16[s*2+1] + out_r);
        }
        else
        {
            pcm16[s*2]   = clamp16(out_l);
            pcm16[s*2+1] = clamp16(out_r);
        }
    }
}

// ---------------------------------------------------------------------------
// Timer tick — called from audio interrupt in main.c
// ---------------------------------------------------------------------------
void VGUS_TickTimers(uint32_t delta_samples, int freq, void (*raiseIRQ)(uint8_t))
{
    if(!VGUS_IsActive()) return;
    if(!(vgus.reset_reg & 0x04)) return; // IRQ not enabled in reset register

    // Timer 1 (80µs resolution) — bit0 of timer_ctrl = started
    if(vgus.timer_ctrl & 0x01) // started
    {
        if(vgus.timer1_ticks <= delta_samples)
        {
            vgus.timer1_ticks = 0;
            if(!vgus.timer1_mask)
            {
                vgus.irq_status |= VGUS_IRQ_TIMER1;
            }
            // Restart timer
            vgus.timer1_ticks = (freq * (uint32_t)(256 - vgus.timer1_count) * 80) / 1000000;
            if(vgus.timer1_ticks < 1) vgus.timer1_ticks = 1;
        }
        else
        {
            vgus.timer1_ticks -= delta_samples;
        }
    }

    // Timer 2 (320µs resolution) — bit1 of timer_ctrl = started
    if(vgus.timer_ctrl & 0x02) // started
    {
        if(vgus.timer2_ticks <= delta_samples)
        {
            vgus.timer2_ticks = 0;
            if(!vgus.timer2_mask)
            {
                vgus.irq_status |= VGUS_IRQ_TIMER2;
            }
            vgus.timer2_ticks = (freq * (uint32_t)(256 - vgus.timer2_count) * 320) / 1000000;
            if(vgus.timer2_ticks < 1) vgus.timer2_ticks = 1;
        }
        else
        {
            vgus.timer2_ticks -= delta_samples;
        }
    }

    // DMA-done IRQ: fire the GUS IRQ line so the game's ISR is notified
    // that the patch upload has completed.  Previously this flag was cleared
    // without ever calling raiseIRQ, so ULTRINIT waited forever.
    if(vgus.dma_irq_pending)
    {
        vgus.dma_irq_pending = 0;
        vgus.irq_status |= VGUS_IRQ_DMA_TC;
        if(raiseIRQ) raiseIRQ((uint8_t)vgus.irq);
    }

    // Edge-trigger ALL IRQs — only fire when a new bit becomes set
    // AND only when IRQ is enabled in reset register (bit2)
    if(vgus.reset_reg & 0x04)
    {
        uint8_t newly_set = vgus.irq_status & ~vgus.last_irq_status;
        if(newly_set)
        {
            if(raiseIRQ) raiseIRQ((uint8_t)vgus.irq);
        }
    }
    vgus.last_irq_status = vgus.irq_status;
}

#endif // SBEMU_GUS
