#include "framebuffer.h"
#include "../heap/heap.h"
#include "../string/string.h"

struct framebuffer *gfx_fb = NULL;

int gfx_framebuffer_init(uint16_t width, uint16_t height, uint8_t bpp) {
    if (gfx_fb) {
        kfree(gfx_fb->buffer);
        kfree(gfx_fb);
    }
    
    gfx_fb = (struct framebuffer *)kmalloc(sizeof(struct framebuffer));
    if (!gfx_fb) return -1;
    
    uint32_t pitch = width * (bpp / 8);
    uint32_t size = pitch * height;
    
    gfx_fb->buffer = (uint8_t *)kmalloc(size);
    if (!gfx_fb->buffer) {
        kfree(gfx_fb);
        gfx_fb = NULL;
        return -1;
    }
    
    gfx_fb->width = width;
    gfx_fb->height = height;
    gfx_fb->bpp = bpp;
    gfx_fb->pitch = pitch;
    gfx_fb->dirty_region = NULL;
    
    memset(gfx_fb->buffer, 0, size);
    return 0;
}

void gfx_framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!gfx_fb || x >= gfx_fb->width || y >= gfx_fb->height) return;
    
    uint32_t offset = y * gfx_fb->pitch + x * (gfx_fb->bpp / 8);
    
    if (gfx_fb->bpp == 8) {
        gfx_fb->buffer[offset] = (uint8_t)color;
    } else if (gfx_fb->bpp == 16) {
        *(uint16_t *)(gfx_fb->buffer + offset) = (uint16_t)color;
    } else if (gfx_fb->bpp == 32) {
        *(uint32_t *)(gfx_fb->buffer + offset) = color;
    }
}

uint32_t gfx_framebuffer_get_pixel(uint32_t x, uint32_t y) {
    if (!gfx_fb || x >= gfx_fb->width || y >= gfx_fb->height) return 0;
    
    uint32_t offset = y * gfx_fb->pitch + x * (gfx_fb->bpp / 8);
    
    if (gfx_fb->bpp == 8) {
        return gfx_fb->buffer[offset];
    } else if (gfx_fb->bpp == 16) {
        return *(uint16_t *)(gfx_fb->buffer + offset);
    } else if (gfx_fb->bpp == 32) {
        return *(uint32_t *)(gfx_fb->buffer + offset);
    }
    
    return 0;
}

void gfx_framebuffer_fill(uint32_t color) {
    if (!gfx_fb) return;
    
    uint32_t size = gfx_fb->pitch * gfx_fb->height;
    
    if (gfx_fb->bpp == 8) {
        memset(gfx_fb->buffer, (uint8_t)color, size);
    } else {
        for (uint32_t i = 0; i < gfx_fb->width * gfx_fb->height; i++) {
            gfx_framebuffer_set_pixel(i % gfx_fb->width, i / gfx_fb->width, color);
        }
    }
}

void gfx_framebuffer_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
}

void gfx_framebuffer_flush(void) {
}
