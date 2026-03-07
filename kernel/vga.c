#include "vga.h"
#include "io.h"

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static uint32_t vga_index = 0;
static uint8_t vga_current_color = 0x0F;

static void vga_update_cursor(void) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(vga_index & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((vga_index >> 8) & 0xFF));
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static inline uint8_t vga_color_byte(uint8_t fg, uint8_t bg) {
    return fg | bg << 4;
}

void vga_init(void) {
    vga_current_color = vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_current_color);
    }
    vga_index = 0;
    vga_update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_current_color = vga_color_byte(fg, bg);
}

void vga_newline(void) {
    uint32_t col = vga_index % VGA_WIDTH;
    
    if (col != 0) {
        vga_index += VGA_WIDTH - col;
    }
    
    if (vga_index >= VGA_WIDTH * VGA_HEIGHT) {
        for (uint32_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (uint32_t i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buffer[i] = vga_entry(' ', vga_current_color);
        }
        vga_index = VGA_WIDTH * (VGA_HEIGHT - 1);
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_newline();
        return;
    }
    
    if (c == '\r') {
        uint32_t row = vga_index / VGA_WIDTH;
        vga_index = row * VGA_WIDTH;
        return;
    }
    
    if (c == '\b') {
        if (vga_index > 0) {
            vga_index--;
            vga_buffer[vga_index] = vga_entry(' ', vga_current_color);
        }
        return;
    }
    
    vga_buffer[vga_index] = vga_entry(c, vga_current_color);
    vga_index++;
    
    if (vga_index >= VGA_WIDTH * VGA_HEIGHT) {
        vga_newline();
    }
    
    vga_update_cursor();
}

void vga_write(const char* str) {
    for (uint32_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void vga_write_color(const char* str, uint8_t fg, uint8_t bg) {
    uint8_t old_color = vga_current_color;
    vga_set_color(fg, bg);
    vga_write(str);
    vga_current_color = old_color;
}
