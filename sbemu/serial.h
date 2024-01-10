#pragma once

#include <stdbool.h>

int
ser_setup(int stype, unsigned int sdev);

bool
ser_puts(const char *s);

void
ser_putbyte(int b);

void
ser_print_com_ports();
