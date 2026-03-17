#ifndef BMP_H
#define BMP_H

#include "../types/types.h"

struct bmp_info {
    uint32_t width;
    uint32_t height;
    uint16_t bpp;
    uint32_t compression;
    uint32_t data_offset;
    uint32_t image_size;
};

int bmp_info_from_file(const char *name, uint16_t parent, struct bmp_info *out);
int bmp_draw_from_file(const char *name, uint16_t parent, uint32_t dst_x, uint32_t dst_y);

#endif
