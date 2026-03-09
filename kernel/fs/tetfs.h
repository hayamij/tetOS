#ifndef TETFS_H
#define TETFS_H

#include "../types/types.h"

/* ======================================================
 * tetFS — simple flat filesystem for tetOS
 *
 * Disk layout (kernel occupies LBA 1-50):
 *   LBA  51      : Superblock
 *   LBA  52-55   : Inode table  (4 sectors × 8 inodes = 32 inodes, 64 B each)
 *   LBA  56-57   : Block bitmap (2 sectors = 1024 B = tracks 8192 blocks)
 *   LBA  58+     : Data blocks  (512 B each)
 * ====================================================== */

#define TETFS_MAGIC           0x74455453u
#define TETFS_VERSION         1

#define TETFS_LBA_SUPER       61
#define TETFS_LBA_INODES      62
#define TETFS_LBA_BITMAP      66
#define TETFS_LBA_DATA        68

#define TETFS_INODE_SIZE      64
#define TETFS_MAX_INODES      32
#define TETFS_BLOCK_SIZE      512
#define TETFS_MAX_BLOCKS      1024
#define TETFS_MAX_FILE_BLOCKS 16
#define TETFS_MAX_FD          8

#define TETFS_ROOT_INODE      0

#define TETFS_TYPE_FREE       0
#define TETFS_TYPE_FILE       1
#define TETFS_TYPE_DIR        2

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_count;
    uint32_t first_data_lba;
    uint8_t  reserved[512 - 24];
} __attribute__((packed)) tetfs_super_t;

typedef struct {
    uint8_t  type;
    char     name[51];
    uint32_t size;
    uint16_t start_block;
    uint16_t block_count;
    uint16_t parent;
    uint8_t  reserved[2];
} __attribute__((packed)) tetfs_inode_t;

typedef struct {
    uint8_t  used;
    uint16_t inode;
    uint32_t offset;
} tetfs_fd_t;

int  tetfs_format(void);
int  tetfs_mount(void);
int  tetfs_is_mounted(void);

int  tetfs_find(const char *name, uint16_t parent);
int  tetfs_create(const char *name, uint16_t parent, uint8_t type);
int  tetfs_delete(uint16_t idx);

int  tetfs_write(uint16_t idx, const void *buf, uint32_t len);
int  tetfs_read(uint16_t idx, void *buf, uint32_t offset, uint32_t len);

int  tetfs_list(uint16_t parent, tetfs_inode_t *out, int max);
int  tetfs_read_inode(uint16_t idx, tetfs_inode_t *out);
int  tetfs_count_children(uint16_t parent);

void fs_init(void);
int  fs_open(const char *name, int write);
int  fs_read(int fd, void *buf, uint32_t len);
int  fs_write(int fd, const void *buf, uint32_t len);
void fs_close(int fd);

#endif
