#pragma once

#include <stdbool.h>

void
ser_setup(int sdev);

bool
ser_puts(const char *s);
