#include "keyboard.h"
#include "../isr/isr.h"
#include "../io/io.h"

static const char scancode_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char scancode_shift_ascii[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static uint16_t key_buffer[KEY_BUFFER_SIZE];
static uint32_t buffer_read = 0;
static uint32_t buffer_write = 0;
static uint8_t shift_down = 0;
static uint8_t ctrl_down = 0;
static uint8_t alt_down = 0;
static uint8_t caps_lock = 0;
static uint8_t ext_prefix = 0;

static void push_key(uint16_t key) {
    uint32_t next = (buffer_write + 1) % KEY_BUFFER_SIZE;
    if (next == buffer_read) return;
    key_buffer[buffer_write] = key;
    buffer_write = next;
}

static uint16_t decode_extended(uint8_t scancode) {
    if (scancode == 0x48) return KEY_UP;
    if (scancode == 0x50) return KEY_DOWN;
    if (scancode == 0x4B) return KEY_LEFT;
    if (scancode == 0x4D) return KEY_RIGHT;
    if (scancode == 0x47) return KEY_HOME;
    if (scancode == 0x4F) return KEY_END;
    if (scancode == 0x49) return KEY_PAGEUP;
    if (scancode == 0x51) return KEY_PAGEDOWN;
    if (scancode == 0x52) return KEY_INSERT;
    if (scancode == 0x53) return KEY_DELETE;
    return 0;
}

static uint16_t decode_function(uint8_t scancode) {
    if (scancode == 0x3B) return KEY_F1;
    if (scancode == 0x3C) return KEY_F2;
    if (scancode == 0x3D) return KEY_F3;
    if (scancode == 0x3E) return KEY_F4;
    if (scancode == 0x3F) return KEY_F5;
    if (scancode == 0x40) return KEY_F6;
    if (scancode == 0x41) return KEY_F7;
    if (scancode == 0x42) return KEY_F8;
    if (scancode == 0x43) return KEY_F9;
    if (scancode == 0x44) return KEY_F10;
    if (scancode == 0x57) return KEY_F11;
    if (scancode == 0x58) return KEY_F12;
    return 0;
}

static uint16_t decode_ascii(uint8_t scancode) {
    if (scancode >= sizeof(scancode_ascii)) return 0;
    char c = shift_down ? scancode_shift_ascii[scancode] : scancode_ascii[scancode];
    if (c >= 'a' && c <= 'z' && (caps_lock ^ shift_down)) {
        c = (char)('A' + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z' && !(caps_lock ^ shift_down)) {
        c = (char)('a' + (c - 'A'));
    }
    return (uint16_t)(uint8_t)c;
}

static void keyboard_callback(struct registers* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) {
        ext_prefix = 1;
        return;
    }

    uint8_t released = (scancode & 0x80) ? 1 : 0;
    uint8_t code = (uint8_t)(scancode & 0x7F);

    if (ext_prefix) {
        ext_prefix = 0;
        if (code == 0x1D) {
            ctrl_down = released ? 0 : 1;
            return;
        }
        if (code == 0x38) {
            alt_down = released ? 0 : 1;
            return;
        }
        if (!released) {
            uint16_t key = decode_extended(code);
            if (key) push_key(key);
        }
        return;
    }

    if (code == 0x2A || code == 0x36) {
        shift_down = released ? 0 : 1;
        return;
    }
    if (code == 0x1D) {
        ctrl_down = released ? 0 : 1;
        return;
    }
    if (code == 0x38) {
        alt_down = released ? 0 : 1;
        return;
    }
    if (code == 0x3A && !released) {
        caps_lock = caps_lock ? 0 : 1;
        return;
    }
    if (released) return;

    uint16_t fn = decode_function(code);
    if (fn) {
        push_key(fn);
        return;
    }

    uint16_t key = decode_ascii(code);
    if (key) {
        if (key == '\n') key = KEY_ENTER;
        if (key == '\b') key = KEY_BACKSPACE;
        push_key(key);
    }
}

void keyboard_init(void) {
    register_irq_handler(1, keyboard_callback);
    
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
    
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x02);
}

int keyboard_has_input(void) {
    return buffer_read != buffer_write;
}

uint16_t keyboard_getkey(void) {
    while (!keyboard_has_input()) {
        __asm__ __volatile__("hlt");
    }
    uint16_t key = key_buffer[buffer_read];
    buffer_read = (buffer_read + 1) % KEY_BUFFER_SIZE;
    return key;
}

char keyboard_getchar(void) {
    while (1) {
        uint16_t key = keyboard_getkey();
        if (key < 0x100) return (char)key;
    }
}

int keyboard_shift_down(void) {
    return shift_down;
}

int keyboard_ctrl_down(void) {
    return ctrl_down;
}

int keyboard_alt_down(void) {
    return alt_down;
}

int keyboard_caps_on(void) {
    return caps_lock;
}
