// SBEMU sbemu/serial.c
// based on: https://github.com/MindlapseDemos/wip-dosdemo/blob/master/src/dos/logger.c
//           GPLv3 by John Tsiombikas

#include "serial.h"

#include <stdio.h>
#include <go32.h>
#include <sys/farptr.h>

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pc.h>

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

#define BDA_COMPORTS 0x0400 // BIOS Data Area (BDA) address
static unsigned short
get_com_port_address(int portno)
{
    unsigned short p = _farpeekw(_dos_ds, BDA_COMPORTS + (portno-1)*2);
    return p;
}

void
ser_print_com_ports()
{
    for (int n = 1; n <= 4; n++) {
        printf("COM%d: %4.4x\n", n, get_com_port_address(n));
    }
}

int
ser_setup(int stype, unsigned int sdev)
{
    unsigned int iobase = 0;
    if (sdev >= 1 && sdev <= 4) {
        iobase = get_com_port_address(sdev);
    } else {
        if (stype == 1 && sdev == 9) { // HW MIDI only
          iobase = 0;
          sdev = 0; // Prevent error
        }
        if (sdev > 0xff) { // Check for typos (less than 3 hexadecimal digits)
            iobase = sdev;
        } else {
            iobase = 0;
        }
    }

    switch (stype) {
        case 1: midi_iobase = iobase; break;
        default: dbg_iobase = iobase; break;
    }

    if (iobase == 0) {
        if (sdev != 0) {
            printf("Invalid COM port %d\n", sdev);
            return 1;
        } else {
            return 0;
        }
    }

    unsigned int baud = (stype == SBEMU_SERIAL_TYPE_FASTDBG) ? DIV_BAUD_FASTDBG : ((stype == SBEMU_SERIAL_TYPE_MIDI) ? DIV_BAUD_MIDI : DIV_BAUD_DBG);
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

    return 0;
}

void
ser_putbyte(int c)
{
  if (midi_iobase == 0) { // HW MIDI only
        return;
    }

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
