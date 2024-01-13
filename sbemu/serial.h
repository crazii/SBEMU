#pragma once

#include <stdbool.h>

#define SBEMU_SERIAL_TYPE_DBG      0
#define SBEMU_SERIAL_TYPE_MIDI     1
#define SBEMU_SERIAL_TYPE_FASTDBG  2

int
ser_setup(int stype, unsigned int sdev);

bool
ser_puts(const char *s);

void
ser_putbyte(int b);

void
ser_print_com_ports();
