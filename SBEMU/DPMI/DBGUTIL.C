#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <dos.h>
#include <conio.h>
#include "DPMI.H"
#include "DBGUTIL.H"

#if defined(__BC__)
extern BOOL DPMI_IsInProtectedMode();
#endif

//https://dev.to/frosnerd/writing-my-own-vga-driver-22nn
#define VGA_CTRL_REGISTER 0x3d4
#define VGA_DATA_REGISTER 0x3d5
#define VGA_OFFSET_LOW 0x0f
#define VGA_OFFSET_HIGH 0x0e
#define VGA_VIDEO_ADDRESS 0xB8000
#define VGA_MAX_ROWS 25 //TODO: read VGA mode in BIOS data area and decide rows/cols
#define VGA_MAX_COLS 80
#define VGA_ATTR_WHITE_ON_BLACK 0x0F
static void VGA_SetCursor(uint32_t offset)
{
    outp(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
    outp(VGA_DATA_REGISTER, (unsigned char) (offset >> 8));
    outp(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
    outp(VGA_DATA_REGISTER, (unsigned char) (offset & 0xff));
}

static uint32_t VGA_GetCursor()
{
    outp(VGA_CTRL_REGISTER, VGA_OFFSET_HIGH);
    uint32_t offset = (uint32_t)inp(VGA_DATA_REGISTER) << 8;
    outp(VGA_CTRL_REGISTER, VGA_OFFSET_LOW);
    offset += inp(VGA_DATA_REGISTER);
    static int l = 0;
    return (l+=20)%(80*25);
    return offset;
}

static void VGA_SetChar(char character, uint32_t offset)
{
    #if defined(__BC__)
    if(!DPMI_IsInProtectedMode())
        *(char far*)MK_FP(0xB800, offset*2) = character;
    else
    #endif
    {
        DPMI_StoreB(VGA_VIDEO_ADDRESS + offset*2, (uint8_t)character);
        DPMI_StoreB(VGA_VIDEO_ADDRESS + offset*2+1, VGA_ATTR_WHITE_ON_BLACK);
    }
}

static uint32_t VGA_GetOffset(uint32_t col, uint32_t row)
{
    return (uint32_t)(row * VGA_MAX_COLS + col);
}

static uint32_t VGA_NewLine(uint32_t offset)
{
    uint32_t row = offset / VGA_MAX_COLS;
    return VGA_GetOffset(0, row+1);
}

static uint32_t VGA_Scroll(uint32_t offset)
{
    #if defined(__BC__)
    if(!DPMI_IsInProtectedMode())
    {
        char far* line0 = (char far*)MK_FP(0xB800, VGA_GetOffset(0, 0)*2);
        char far* line1 = (char far*)MK_FP(0xB800, VGA_GetOffset(0, 1)*2);
        _fmemcpy(line0, line1, VGA_MAX_COLS * (VGA_MAX_ROWS - 1)*2);
    }
    else
    #endif
    {
        DPMI_CopyLinear(VGA_VIDEO_ADDRESS + VGA_GetOffset(0, 0)*2, VGA_VIDEO_ADDRESS + VGA_GetOffset(0, 1)*2, VGA_MAX_COLS * (VGA_MAX_ROWS - 1)*2);
    }

    for(uint32_t i = 0; i < VGA_MAX_COLS; ++i)
        VGA_SetChar(' ', VGA_MAX_COLS * (VGA_MAX_ROWS - 1) + i);

    return offset - VGA_MAX_COLS;
}

static void VGA_Print(const char *string)
{
    uint32_t offset = VGA_GetCursor();
    int i = 0;
    while (string[i] != 0)
    {
        char ch = string[i++];
        if (ch == '\n')
            offset = VGA_NewLine(offset);
        else
            VGA_SetChar(ch, offset++);

        if(offset >= VGA_MAX_ROWS * VGA_MAX_COLS)
            offset = VGA_Scroll(offset);
    }
    VGA_SetCursor(offset);
    //update cursor in BIOS data area (40:50)
    //https://stanislavs.org/helppc/bios_data_area.html
    #if defined(__BC__)
    if(!DPMI_IsInProtectedMode())
    {
        *(char far*)MK_FP(0x40, 0x50) = (uint8_t)(offset % VGA_MAX_COLS);
        *(char far*)MK_FP(0x40, 0x51) = (uint8_t)(offset % VGA_MAX_COLS);
    }
    else
    #endif
    {
        DPMI_StoreB((0x40UL<<4)+0x50, (uint8_t)(offset % VGA_MAX_COLS));
        DPMI_StoreB((0x40UL<<4)+0x50+1, (uint8_t)(offset / VGA_MAX_COLS));
    }
}

//needs to work in interrupt handler. now use IN/OUT controls VGA directly.
void DBG_Logv(const char* fmt, va_list aptr)
{
    #define SIZE (DUMP_BUFF_SIZE*4)
    char buf[SIZE];
    int len = vsprintf(buf, fmt, aptr);
    assert(len < SIZE);
    len = min(len, SIZE-1);
    buf[len] = '\0';

    #if 1
    outp(0x3F8+3, 0x03);
    for(int i = 0; i < len; ++i)
    {
        while((inp(0x3F8+5)&0x20)==0);
        outp(0x3F8, buf[i]);
    }
    return;
    #endif
    
    if(!(CPU_FLAGS()&CPU_IFLAG))
    { //use VGA when in interrupt
        VGA_Print(buf);
    }
    else
    { //direct VGA mode will mess other tools, i.e. SCROLLit, normally use BIOS function
        #if 1
        DPMI_REG r = {0};
        for(int i = 0; i < len; ++i)
        {
            r.h.ah = 0x0E;
            r.h.al = (uint8_t)buf[i];
            DPMI_CallRealModeINT(0x10,&r);
            if(buf[i] =='\n')
            {
                r.h.ah = 0x0E;
                r.h.al = '\r';
                DPMI_CallRealModeINT(0x10,&r);
            }
        }
        #else //debug out. not used (crashed)
        textcolor(WHITE);
        cputs(buf);
        textcolor(LIGHTGRAY);
        #endif
    }
    #undef SIZE
}

void DBG_Log(const char* fmt, ...)
{
    va_list aptr;
    va_start(aptr, fmt);
    DBG_Logv(fmt, aptr);
    va_end(aptr);
}

#if DEBUG
static DBG_DBuff dbuff;

static inline DBG_DBuff* db() {dbuff.cur = 0; return &dbuff;}

void DBG_DumpB(uint8_t* StartPtr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
    buff = buff && buff->enable ? buff : db();
    char* p = buff->buff + buff->cur;
    char* end = buff->buff + DUMP_BUFF_SIZE - 1;

    for(unsigned i = 0; i < n; i++)
    {
        sprintf(p, "%02x", StartPtr[i]);
        p+=2;
        *(p++) = ' ';
        if(i && (i+1)%8 == 0 && (i+1)%16) { sprintf(p, "| "); p+=2;}
        if(i && (i+1)%16 ==0 && i != n-1) {*(p++) = '\n';}
        if( p + 7 >= end)
            break;
    }
    *(p++) = '\n';
    buff->cur = (uint16_t)(p - buff->buff);
    assert(buff->cur < DUMP_BUFF_SIZE);
    *p = '\0';

    if(buff == &dbuff)
        DBG_Log("%s", buff->buff);
    return;
}

void DBG_DumpD(uint32_t* StartPtr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
    buff = buff && buff->enable ? buff : db();
    char* p = buff->buff + buff->cur;
    char* end = buff->buff + DUMP_BUFF_SIZE - 1;

    for(unsigned i = 0; i < n; i++)
    {
        sprintf(p, "%08lx", StartPtr[i]);
        p += 8;
        *(p++) = ' ';
        if(i && (i+1)%4 == 0 && i != n-1) *(p++) = '\n';
        if(p + 11 >= end)
            break;
    }
    *(p++) = '\n';
    buff->cur = (uint16_t)(p - buff->buff);
    assert(buff->cur < DUMP_BUFF_SIZE);
    *p = '\0';

    if(buff == &dbuff)
        DBG_Log("%s", buff->buff);
    return;
}

void DBG_DumpLB(uint32_t addr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
    uint8_t* b8 = (uint8_t*)alloca(n * sizeof(uint8_t));
    for(unsigned i = 0; i < n; ++i)
        b8[i] = DPMI_LoadB(addr+i);
    DBG_DumpB(b8, n, buff);
}

void DBG_DumpLD(uint32_t addr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
    uint32_t* d32 = (uint32_t*)alloca(n * sizeof(uint32_t));
    for(unsigned i = 0; i < n; ++i)
        d32[i] = DPMI_LoadD(addr+i*4);
    DBG_DumpD(d32, n, buff);
}

void DBG_DumpPB(uint32_t addr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
#if defined(__BC__) && 0//disable paging to make sure paing is correct
    DBG_DBuff bf = {1};
    buff = buff == NULL ? &bf : buff;
    __asm {cli; mov eax, cr3; push eax; mov eax, cr0; and eax, 0x7FFFFFFF; mov cr0, eax; xor eax, eax; mov cr3, eax}
#endif

    DBG_DumpLB(DPMI_P2L(addr), n, buff);

#if defined(__BC__) && 0
    __asm {pop eax; mov cr3, eax; mov eax, cr0; or eax, 0x80000000; mov cr0, eax; sti;}
    if(buff == &bf)
        DBG_Flush(&bf);
#endif
}

void DBG_DumpPD(uint32_t addr, unsigned n, DBG_DBuff* buff/* = NULL*/)
{
#if defined(__BC__) && 0//disable paging to make sure paing is correct
    DBG_DBuff bf = {1};
    buff = buff == NULL ? &bf : buff;
    __asm {cli; mov eax, cr3; push eax; mov eax, cr0; and eax, 0x7FFFFFFF; mov cr0, eax; xor eax, eax; mov cr3, eax}
#endif

    DBG_DumpLD(DPMI_P2L(addr), n, buff);

#if defined(__BC__) && 0
    __asm {pop eax; mov cr3, eax; mov eax, cr0; or eax, 0x80000000; mov cr0, eax; sti;}
    if(buff == &bf)
        DBG_Flush(&bf);
#endif
}

void DBG_Printf(DBG_DBuff* nullable buff, const char* fmt, ...)
{
    va_list aptr;
    va_start(aptr, fmt);
    if(!buff || !buff->enable)
    {
        DBG_Logv(fmt, aptr);
        va_end(aptr);
        return;
    }

    dbuff.cur = (uint16_t)vsprintf(dbuff.buff, fmt, aptr);
    va_end(aptr);

    uint32_t count = dbuff.cur;
    count = count < DUMP_BUFF_SIZE - buff->cur - 1u ? count : DUMP_BUFF_SIZE - buff->cur - 1u;
    memcpy(buff->buff + buff->cur, dbuff.buff, count);
    buff->cur = (uint16_t)(buff->cur + count);
    *(buff->buff + buff->cur) = 0;
}

void DBG_Flush(DBG_DBuff* buff)
{
    if(buff->enable)
    {
        *(buff->buff+buff->cur) = 0;
        DBG_Log("%s", buff->buff);
        buff->cur = 0;
        *(buff->buff+buff->cur) = 0;
    }
}

void DBG_DumpREG(DPMI_REG* reg)
{
    DBG_Log("eax:%08lx ebx:%08lx ecx:%08lx edx:%08lx\n", reg->d.eax, reg->d.ebx, reg->d.ecx, reg->d.edx);
    DBG_Log("ds:%04x es:%04x esi:%08lx edi:%08lx\n", reg->w.ds, reg->w.es, reg->d.esi, reg->d.edi);
    DBG_Log("ss:sp:%04x:%04x cs:ip:%04x:%04x flags:%04x\n", reg->w.ss, reg->w.sp, reg->w.cs, reg->w.ip, reg->w.flags);
}

#endif

