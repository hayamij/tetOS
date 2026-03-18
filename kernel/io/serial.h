#ifndef SERIAL_H
#define SERIAL_H

#include "../types/types.h"

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char* str);

#endif
