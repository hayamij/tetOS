#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../types/types.h"

#define GRAPHICS_MAX_WIDTH  1280
#define GRAPHICS_MAX_HEIGHT 720

typedef uint32_t color_t;

extern uint32_t graphics_width;
extern uint32_t graphics_height;
extern uint32_t graphics_pitch;
extern uint8_t graphics_bpp;
extern uint8_t *graphics_framebuffer;

uint32_t graphics_get_width(void);
uint32_t graphics_get_height(void);
uint32_t graphics_get_pitch(void);
uint8_t graphics_get_bpp(void);

static inline color_t graphics_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

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
