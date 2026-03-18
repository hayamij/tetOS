#include "serial.h"
#include "io.h"

#define SERIAL_PORT 0x3F8  // COM1

#define UART_DATA 0
#define UART_IER 1
#define UART_FCR 2
#define UART_LCR 3
#define UART_LSR 5

#define LSR_THRE 0x20

void serial_init(void) {
    // Disable interrupts
    outb(SERIAL_PORT + UART_IER, 0x00);
    
    // Enable DLAB (Divisor Latch Access Bit)
    outb(SERIAL_PORT + UART_LCR, 0x80);
    
    // Set baud rate to 115200 (divisor = 1)
    outb(SERIAL_PORT + UART_DATA, 0x01);    // Low byte
    outb(SERIAL_PORT + UART_IER, 0x00);     // High byte
    
    // Disable DLAB, set 8 data bits, 1 stop bit, no parity
    outb(SERIAL_PORT + UART_LCR, 0x03);
    
    // Enable FIFO
    outb(SERIAL_PORT + UART_FCR, 0xC7);
    
    // Set RTS and DTR
    outb(SERIAL_PORT + 4, 0x0B);
}

void serial_putchar(char c) {
    while ((inb(SERIAL_PORT + UART_LSR) & LSR_THRE) == 0);
    outb(SERIAL_PORT + UART_DATA, (uint8_t)c);
}

void serial_puts(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_putchar(str[i]);
    }
}
