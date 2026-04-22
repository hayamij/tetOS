#ifndef VESA_H
#define VESA_H

#include "../types/types.h"

#define VESA_SUCCESS 0
#define VESA_FAILED -1

struct vesa_mode_info {
    uint16_t mode;
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
    uint32_t pitch;
    uint32_t framebuffer;
};

int vesa_get_boot_mode(struct vesa_mode_info *info);

struct vesa_controller_info {
    char signature[4];
    uint16_t version;
    uint32_t capabilities;
};

int vesa_init(void);
int vesa_get_mode_info(uint16_t mode, struct vesa_mode_info *info);
int vesa_set_mode(uint16_t mode);
int vesa_get_current_mode(void);

#endif
