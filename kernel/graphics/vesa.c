#include "vesa.h"

struct vbe_mode_info {
    uint16_t attributes;
    uint8_t wina_attributes;
    uint8_t winb_attributes;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t wina_segment;
    uint16_t winb_segment;
    uint32_t win_func_ptr;
    uint16_t bytes_per_scanline;
    uint16_t xresolution;
    uint16_t yresolution;
    uint8_t char_width;
    uint8_t char_height;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved;
    uint8_t red_mask_size;
    uint8_t red_field_position;
    uint8_t green_mask_size;
    uint8_t green_field_position;
    uint8_t blue_mask_size;
    uint8_t blue_field_position;
    uint8_t reserved_mask_size;
    uint8_t reserved_field_position;
    uint8_t direct_color_attributes;
    uint32_t phys_base_ptr;
    uint32_t off_screen_mem_offset;
    uint16_t off_screen_mem_size;
} __attribute__((packed));

#define VBE_MODE_INFO_ADDR 0x0500

int vesa_get_boot_mode(struct vesa_mode_info *info) {
    if (!info) return VESA_FAILED;

    const struct vbe_mode_info *b = (const struct vbe_mode_info *)VBE_MODE_INFO_ADDR;

    if (!(b->attributes & 0x01)) return VESA_FAILED;
    if (!(b->attributes & 0x80)) return VESA_FAILED;
    if (b->xresolution == 0 || b->yresolution == 0) return VESA_FAILED;
    if (b->bpp != 24 && b->bpp != 32) return VESA_FAILED;
    if (b->phys_base_ptr == 0) return VESA_FAILED;
    if (b->bytes_per_scanline == 0) return VESA_FAILED;

    info->mode        = 0;
    info->width       = b->xresolution;
    info->height      = b->yresolution;
    info->bpp         = b->bpp;
    info->pitch       = b->bytes_per_scanline;
    info->framebuffer = b->phys_base_ptr;
    return VESA_SUCCESS;
}
