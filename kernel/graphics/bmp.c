#include "bmp.h"
#include "../fs/tetfs.h"
#include "../heap/heap.h"
#include "graphics.h"

typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} __attribute__((packed)) bmp_file_header_t;

typedef struct {
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_ppm;
    int32_t y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
} __attribute__((packed)) bmp_info_header_t;

int bmp_info_from_file(const char *name, uint16_t parent, struct bmp_info *out) {
    if (!out || !name || !name[0]) return -1;

    int idx = tetfs_find(name, parent);
    if (idx < 0) return -2;

    tetfs_inode_t node;
    if (tetfs_read_inode((uint16_t)idx, &node) != 0) return -3;
    if (node.type != TETFS_TYPE_FILE) return -4;
    if (node.size < sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t)) return -5;

    bmp_file_header_t fh;
    bmp_info_header_t ih;

    if (tetfs_read((uint16_t)idx, &fh, 0, sizeof(fh)) != (int)sizeof(fh)) return -6;
    if (fh.signature != 0x4D42) return -7;

    if (tetfs_read((uint16_t)idx, &ih, sizeof(fh), sizeof(ih)) != (int)sizeof(ih)) return -8;
    if (ih.header_size < 40) return -9;
    if (ih.width <= 0 || ih.height == 0) return -10;

    out->width = (uint32_t)ih.width;
    out->height = (ih.height < 0) ? (uint32_t)(-ih.height) : (uint32_t)ih.height;
    out->bpp = ih.bpp;
    out->compression = ih.compression;
    out->data_offset = fh.data_offset;
    out->image_size = ih.image_size;

    return 0;
}

static uint8_t rgb_to_332(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
}

int bmp_draw_from_file(const char *name, uint16_t parent, uint32_t dst_x, uint32_t dst_y) {
    if (!name || !name[0]) return -1;

    int idx = tetfs_find(name, parent);
    if (idx < 0) return -2;

    tetfs_inode_t node;
    if (tetfs_read_inode((uint16_t)idx, &node) != 0) return -3;
    if (node.type != TETFS_TYPE_FILE) return -4;

    bmp_file_header_t fh;
    bmp_info_header_t ih;
    if (tetfs_read((uint16_t)idx, &fh, 0, sizeof(fh)) != (int)sizeof(fh)) return -5;
    if (tetfs_read((uint16_t)idx, &ih, sizeof(fh), sizeof(ih)) != (int)sizeof(ih)) return -6;

    if (fh.signature != 0x4D42) return -7;
    if (ih.header_size < 40) return -8;
    if (ih.width <= 0 || ih.height == 0) return -9;
    if (ih.compression != 0) return -10;
    if (ih.bpp != 8 && ih.bpp != 24) return -11;

    uint32_t width = (uint32_t)ih.width;
    uint32_t height = (ih.height < 0) ? (uint32_t)(-ih.height) : (uint32_t)ih.height;
    uint32_t row_size = ((width * ih.bpp + 31) / 32) * 4;
    uint8_t top_down = (ih.height < 0) ? 1 : 0;

    uint8_t *row = (uint8_t *)kmalloc(row_size);
    if (!row) return -12;

    uint8_t palette_map[256];
    if (ih.bpp == 8) {
        uint8_t palette[1024];
        uint32_t palette_off = sizeof(bmp_file_header_t) + ih.header_size;
        if (tetfs_read((uint16_t)idx, palette, palette_off, sizeof(palette)) != (int)sizeof(palette)) {
            kfree(row);
            return -13;
        }
        for (uint32_t i = 0; i < 256; i++) {
            uint8_t b = palette[i * 4 + 0];
            uint8_t g = palette[i * 4 + 1];
            uint8_t r = palette[i * 4 + 2];
            palette_map[i] = rgb_to_332(r, g, b);
        }
    }

    for (uint32_t y = 0; y < height; y++) {
        uint32_t src_y = top_down ? y : (height - 1 - y);
        uint32_t file_off = fh.data_offset + src_y * row_size;
        if (tetfs_read((uint16_t)idx, row, file_off, row_size) != (int)row_size) {
            kfree(row);
            return -14;
        }

        if (dst_y + y >= GRAPHICS_HEIGHT) continue;

        for (uint32_t x = 0; x < width; x++) {
            if (dst_x + x >= GRAPHICS_WIDTH) break;

            uint8_t color;
            if (ih.bpp == 8) {
                color = palette_map[row[x]];
            } else {
                uint32_t px = x * 3;
                uint8_t b = row[px + 0];
                uint8_t g = row[px + 1];
                uint8_t r = row[px + 2];
                color = rgb_to_332(r, g, b);
            }
            graphics_set_pixel(dst_x + x, dst_y + y, color);
        }
    }

    kfree(row);
    graphics_present();
    return 0;
}
