#include "stdio.h"
#include "../vga/vga.h"
#include "../string/string.h"
#include "../io/serial.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

static void kprintf_uint(uint32_t value, int base) {
    char buf[32];
    int i = 0;
    
    if (value == 0) {
        vga_putchar('0');
        serial_putchar('0');
        return;
    }
    
    while (value > 0) {
        int digit = value % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }
    
    while (i > 0) {
        char c = buf[--i];
        vga_putchar(c);
        serial_putchar(c);
    }
}

static void kprintf_int(int32_t value) {
    if (value < 0) {
        vga_putchar('-');
        serial_putchar('-');
        value = -value;
    }
    kprintf_uint((uint32_t)value, 10);
}

static void vprintk(const char* fmt, va_list args) {
    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i + 1] != '\0') {
            i++;
            switch (fmt[i]) {
                case 'd':
                case 'i':
                    kprintf_int(va_arg(args, int32_t));
                    break;
                case 'u':
                    kprintf_uint(va_arg(args, uint32_t), 10);
                    break;
                case 'x':
                    kprintf_uint(va_arg(args, uint32_t), 16);
                    break;
                case 's':
                {
                    const char *s = va_arg(args, const char*);
                    vga_write(s);
                    serial_puts(s);
                    break;
                }
                case 'c':
                {
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    serial_putchar(c);
                    break;
                }
                case '%':
                    vga_putchar('%');
                    serial_putchar('%');
                    break;
                default:
                    vga_putchar('%');
                    vga_putchar(fmt[i]);
                    serial_putchar('%');
                    serial_putchar(fmt[i]);
                    break;
            }
        } else {
            vga_putchar(fmt[i]);
            serial_putchar(fmt[i]);
        }
    }
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
