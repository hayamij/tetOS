#include "vfs.h"
#include "tetfs.h"
#include "../string/string.h"

static file_t vfs_files[VFS_MAX_OPEN_FILES];
static superblock_t root_sb;

void vfs_init(void) {
    memset(vfs_files, 0, sizeof(vfs_files));
    root_sb.name = "tetfs";
    root_sb.block_size = TETFS_BLOCK_SIZE;
    root_sb.total_blocks = TETFS_MAX_BLOCKS;
}

static int alloc_file_slot(void) {
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!vfs_files[i].in_use) return i;
    }
    return -1;
}

int vfs_open(const char *path, uint32_t flags) {
    if (!path || !path[0]) return -1;

    int inode = tetfs_find(path, TETFS_ROOT_INODE);
    if (inode < 0 && (flags & VFS_O_CREAT)) {
        inode = tetfs_create(path, TETFS_ROOT_INODE, TETFS_TYPE_FILE);
    }
    if (inode < 0) return -1;

    tetfs_inode_t node;
    if (tetfs_read_inode((uint16_t)inode, &node) != 0) return -1;

    int fd = alloc_file_slot();
    if (fd < 0) return -1;

    vfs_files[fd].in_use = 1;
    vfs_files[fd].flags = flags;
    vfs_files[fd].offset = 0;
    vfs_files[fd].vnode.inode = (uint32_t)inode;
    vfs_files[fd].vnode.size = node.size;
    vfs_files[fd].vnode.type = node.type;
    vfs_files[fd].vnode.sb = &root_sb;

    return fd;
}

int vfs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !vfs_files[fd].in_use) return -1;
    if (!buf || len == 0) return 0;

    file_t *f = &vfs_files[fd];
    int n = tetfs_read((uint16_t)f->vnode.inode, buf, f->offset, len);
    if (n > 0) f->offset += (uint32_t)n;
    return n;
}

int vfs_write(int fd, const void *buf, uint32_t len) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !vfs_files[fd].in_use) return -1;
    if (!buf) return -1;

    file_t *f = &vfs_files[fd];
    if (!(f->flags & VFS_O_WRONLY) && !(f->flags & VFS_O_RDWR)) return -1;

    int n = tetfs_write((uint16_t)f->vnode.inode, buf, len);
    if (n >= 0) {
        f->vnode.size = len;
        f->offset = len;
    }
    return n;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !vfs_files[fd].in_use) return -1;
    memset(&vfs_files[fd], 0, sizeof(file_t));
    return 0;
}
