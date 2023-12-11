// https://github.com/MindlapseDemos/wip-dosdemo/blob/master/src/dos/logger.c
// GPLv3 by John Tsiombikas

#include "serial.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pc.h>

#define UART1_BASE	0x3f8
#define UART2_BASE	0x2f8

#define UART_DATA	0
#define UART_DIVLO	0
#define UART_DIVHI	1
#define UART_FIFO	2
#define UART_LCTL	3
#define UART_MCTL	4
#define UART_LSTAT	5

#define DIV_9600			(115200 / 9600)
#define DIV_38400			(115200 / 38400)
#define LCTL_8N1			0x03
#define LCTL_DLAB			0x80
#define MCTL_DTR_RTS_OUT2	0x0b
#define LST_TRIG_EMPTY		0x20

static unsigned int iobase = 0;

void
ser_setup(int sdev)
{
    switch (sdev) {
        case 1: iobase = UART1_BASE; break;
        case 2: iobase = UART2_BASE; break;
        default: iobase = 0; break;
    }

    if (iobase == 0) {
        return;
    }

    /* set clock divisor */
    outp(iobase | UART_LCTL, LCTL_DLAB);
    outp(iobase | UART_DIVLO, DIV_9600 & 0xff);
    outp(iobase | UART_DIVHI, DIV_9600 >> 8);

    /* set format 8n1 */
    outp(iobase | UART_LCTL, LCTL_8N1);

    /* assert RTS and DTR */
    outp(iobase | UART_MCTL, MCTL_DTR_RTS_OUT2);
}

static void
ser_putchar(int c)
{
    if (c == '\n') {
        ser_putchar('\r');
    }

    while((inp(iobase | UART_LSTAT) & LST_TRIG_EMPTY) == 0);
    outp(iobase | UART_DATA, c);
}

bool
ser_puts(const char *s)
{
    if (iobase == 0) {
        return false;
    }

    while(*s) {
        ser_putchar(*s++);
    }

    return true;
}
