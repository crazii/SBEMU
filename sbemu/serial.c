// https://github.com/MindlapseDemos/wip-dosdemo/blob/master/src/dos/logger.c
// GPLv3 by John Tsiombikas

#include "serial.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pc.h>

#define UART1_BASE	0x3f8
#define UART2_BASE	0x2f8
#define UART3_BASE	0x3e8
#define UART4_BASE	0x2e8
#define UART8_BASE	0xD040 // COM3 (Moschip 9935CV PCI card)
#define UART9_BASE	0xD050 // COM2 (Moschip 9935CV PCI card)

#define UART_DATA	0
#define UART_DIVLO	0
#define UART_DIVHI	1
#define UART_FIFO	2
#define UART_LCTL	3
#define UART_MCTL	4
#define UART_LSTAT	5

#define DIV_9600			(115200 / 9600)
#define DIV_38400			(115200 / 38400)
#define DIV_115200			(115200 / 115200)
#define DIV_BAUD_MIDI                   DIV_38400
#define DIV_BAUD_DBG                    DIV_9600
#define DIV_BAUD_FASTDBG                DIV_115200
#define LCTL_8N1			0x03
#define LCTL_DLAB			0x80
//#define MCTL_DTR_RTS_OUT2	0x0b // Enable Interrupts
#define MCTL_DTR_RTS_OUT2	0x03 // Disable Interrupts
#define LST_TRIG_EMPTY		0x20

static unsigned int dbg_iobase = 0;
static unsigned int midi_iobase = 0;

void
ser_setup(int stype, unsigned int sdev)
{
    unsigned int iobase = 0;
    switch (sdev) {
        case 1: iobase = UART1_BASE; break;
        case 2: iobase = UART2_BASE; break;
        case 3: iobase = UART3_BASE; break;
        case 4: iobase = UART4_BASE; break;
        case 8: iobase = UART8_BASE; break;
        case 9: iobase = UART9_BASE; break;
        default: iobase = sdev; break;
    }

    switch (stype) {
        case 1: midi_iobase = iobase; break;
        default: dbg_iobase = iobase; break;
    }

    if (iobase == 0) {
        return;
    }

    unsigned int baud = (stype == 2) ? DIV_BAUD_FASTDBG : ((stype == 1) ? DIV_BAUD_MIDI : DIV_BAUD_DBG);
    /* set clock divisor */
    outp(iobase | UART_LCTL, LCTL_DLAB);
    outp(iobase | UART_DIVLO, baud & 0xff);
    outp(iobase | UART_DIVHI, baud >> 8);

    /* set format 8n1 */
    outp(iobase | UART_LCTL, LCTL_8N1);

    /* assert RTS and DTR */
    outp(iobase | UART_MCTL, MCTL_DTR_RTS_OUT2);

    // How to read output:
    // stty -F /dev/ttyS0 9600 cs8 -cstopb -parenb && cat /dev/ttyS0
    // or: stty -F /dev/ttyS0 115200 cs8 -cstopb -parenb && cat /dev/ttyS0
}

void
ser_putbyte(int c)
{
    // if (midi_iobase == 0) { // No need to check this
    //   return;
    // }

    while((inp(midi_iobase | UART_LSTAT) & LST_TRIG_EMPTY) == 0);
    outp(midi_iobase | UART_DATA, c);
}

static void
ser_putchar(int c)
{
    if (c == '\n') {
        ser_putchar('\r');
    }

    while((inp(dbg_iobase | UART_LSTAT) & LST_TRIG_EMPTY) == 0);
    outp(dbg_iobase | UART_DATA, c);
}

bool
ser_puts(const char *s)
{
    if (dbg_iobase == 0) {
        return false;
    }

    while(*s) {
        ser_putchar(*s++);
    }

    return true;
}
