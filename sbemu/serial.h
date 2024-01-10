#pragma once

#include <stdbool.h>

void
ser_setup(int stype, unsigned int sdev);

bool
ser_puts(const char *s);

void
ser_putbyte(int b);
