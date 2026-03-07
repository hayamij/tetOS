#include "keyboard.h"
#include "isr.h"
#include "io.h"

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static char key_buffer[KEY_BUFFER_SIZE];
static uint32_t buffer_read = 0;
static uint32_t buffer_write = 0;

static void keyboard_callback(struct registers* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);
    
    if (scancode & 0x80) {
        return;
    }
    
    if (scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            key_buffer[buffer_write] = c;
            buffer_write = (buffer_write + 1) % KEY_BUFFER_SIZE;
        }
    }
}

void keyboard_init(void) {
    isr_register_handler(33, keyboard_callback);
    
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
    
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x02);
}

int keyboard_has_input(void) {
    return buffer_read != buffer_write;
}

char keyboard_getchar(void) {
    while (!keyboard_has_input()) {
        __asm__ __volatile__("hlt");
    }
    char c = key_buffer[buffer_read];
    buffer_read = (buffer_read + 1) % KEY_BUFFER_SIZE;
    return c;
}
