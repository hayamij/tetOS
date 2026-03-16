#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../types/types.h"

struct framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    uint8_t *buffer;
    uint8_t *dirty_region;
};

extern struct framebuffer *gfx_fb;

int gfx_framebuffer_init(uint16_t width, uint16_t height, uint8_t bpp);
void gfx_framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t gfx_framebuffer_get_pixel(uint32_t x, uint32_t y);
void gfx_framebuffer_fill(uint32_t color);
void gfx_framebuffer_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void gfx_framebuffer_flush(void);

#endif
