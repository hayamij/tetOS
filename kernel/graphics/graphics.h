#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../types/types.h"

#define GRAPHICS_WIDTH 320
#define GRAPHICS_HEIGHT 200
#define GRAPHICS_MEMORY 0xA0000

typedef uint8_t color_t;

struct rgb_color {
    uint8_t r, g, b;
};

void graphics_init(void);
void graphics_clear(color_t color);
void graphics_set_pixel(uint32_t x, uint32_t y, color_t color);
color_t graphics_get_pixel(uint32_t x, uint32_t y);
void graphics_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, color_t color);
void graphics_draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, color_t color);
void graphics_draw_char(uint32_t x, uint32_t y, char c, color_t fg, color_t bg);
void graphics_draw_string(uint32_t x, uint32_t y, const char *str, color_t fg, color_t bg);
void graphics_present(void);

#endif
