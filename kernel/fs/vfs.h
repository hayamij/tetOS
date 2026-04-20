#ifndef VFS_H
#define VFS_H

#include "../types/types.h"

#define VFS_MAX_OPEN_FILES 32
#define VFS_O_RDONLY 0x1
#define VFS_O_WRONLY 0x2
#define VFS_O_RDWR   (VFS_O_RDONLY | VFS_O_WRONLY)
#define VFS_O_CREAT  0x4

typedef struct superblock {
    const char *name;
    uint32_t block_size;
    uint32_t total_blocks;
} superblock_t;

typedef struct vnode {
    uint32_t inode;
    uint32_t size;
    uint8_t type;
    superblock_t *sb;
} vnode_t;

typedef struct file {
    vnode_t vnode;
    uint32_t offset;
    uint32_t flags;
    uint8_t in_use;
} file_t;

void vfs_init(void);
int vfs_open(const char *path, uint32_t flags);
int vfs_read(int fd, void *buf, uint32_t len);
int vfs_write(int fd, const void *buf, uint32_t len);
int vfs_close(int fd);

#endif
