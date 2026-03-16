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

static struct vesa_mode_info current_mode_info = {0};

int vesa_init(void) {
    return VESA_SUCCESS;
}

int vesa_get_mode_info(uint16_t mode, struct vesa_mode_info *info) {
    if (!info) return VESA_FAILED;
    
    info->mode = mode;
    
    if (mode == 0x118) {
        info->width = 1024;
        info->height = 768;
        info->bpp = 32;
        info->pitch = 1024 * 4;
        info->framebuffer = 0;
    } else if (mode == 0x115) {
        info->width = 800;
        info->height = 600;
        info->bpp = 32;
        info->pitch = 800 * 4;
        info->framebuffer = 0;
    } else if (mode == 0x111) {
        info->width = 640;
        info->height = 480;
        info->bpp = 32;
        info->pitch = 640 * 4;
        info->framebuffer = 0;
    } else {
        return VESA_FAILED;
    }
    
    return VESA_SUCCESS;
}

int vesa_set_mode(uint16_t mode) {
    struct vesa_mode_info info;
    
    if (vesa_get_mode_info(mode, &info) != VESA_SUCCESS) {
        return VESA_FAILED;
    }
    
    __asm__ __volatile__(
        "mov $0x4F02, %%ax\n"
        "mov %0, %%bx\n"
        "int $0x10\n"
        : : "r" (mode)
        : "ax", "bx"
    );
    
    current_mode_info = info;
    return VESA_SUCCESS;
}

int vesa_get_current_mode(void) {
    return current_mode_info.mode;
}
