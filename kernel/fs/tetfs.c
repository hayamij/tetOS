#include "tetfs.h"
#include "vfs.h"
#include "../ata/ata.h"
#include "../string/string.h"

static int fs_mounted = 0;
static tetfs_fd_t fd_table[TETFS_MAX_FD];

static int read_super(tetfs_super_t *s) {
    uint8_t sector[512];
    if (ata_read(TETFS_LBA_SUPER, 1, sector) != 0) return -1;
    memcpy(s, sector, sizeof(tetfs_super_t));
    return 0;
}

static int write_super(const tetfs_super_t *s) {
    uint8_t sector[512];
    memset(sector, 0, sizeof(sector));
    memcpy(sector, s, sizeof(tetfs_super_t));
    return ata_write(TETFS_LBA_SUPER, 1, sector);
}

static int read_inode_sector(uint32_t lba, uint8_t *buf) {
    return ata_read(lba, 1, buf);
}
static int write_inode_sector(uint32_t lba, const uint8_t *buf) {
    return ata_write(lba, 1, buf);
}

int tetfs_read_inode(uint16_t idx, tetfs_inode_t *out) {
    if (idx >= TETFS_MAX_INODES) return -1;
    uint8_t buf[512];
    uint32_t lba = TETFS_LBA_INODES + (idx / 8);
    if (read_inode_sector(lba, buf) != 0) return -1;
    memcpy(out, buf + (idx % 8) * TETFS_INODE_SIZE, TETFS_INODE_SIZE);
    return 0;
}

static int write_inode(uint16_t idx, const tetfs_inode_t *in) {
    if (idx >= TETFS_MAX_INODES) return -1;
    uint8_t buf[512];
    uint32_t lba = TETFS_LBA_INODES + (idx / 8);
    if (read_inode_sector(lba, buf) != 0) return -1;
    memcpy(buf + (idx % 8) * TETFS_INODE_SIZE, in, TETFS_INODE_SIZE);
    return write_inode_sector(lba, buf);
}

static int read_bitmap(uint8_t *bm) {
    if (ata_read(TETFS_LBA_BITMAP,     1, bm)       != 0) return -1;
    if (ata_read(TETFS_LBA_BITMAP + 1, 1, bm + 512) != 0) return -1;
    return 0;
}

static int write_bitmap(const uint8_t *bm) {
    if (ata_write(TETFS_LBA_BITMAP,     1, bm)       != 0) return -1;
    if (ata_write(TETFS_LBA_BITMAP + 1, 1, bm + 512) != 0) return -1;
    return 0;
}

static int bitmap_get(const uint8_t *bm, uint16_t bit) {
    return (bm[bit / 8] >> (bit % 8)) & 1;
}

static void bitmap_set(uint8_t *bm, uint16_t bit, int val) {
    if (val) bm[bit / 8] |=  (uint8_t)(1 << (bit % 8));
    else     bm[bit / 8] &= ~(uint8_t)(1 << (bit % 8));
}

static int alloc_blocks(uint8_t *bm, uint16_t count) {
    uint16_t start = 0, run = 0, i;
    for (i = 0; i < TETFS_MAX_BLOCKS; i++) {
        if (!bitmap_get(bm, i)) {
            if (run == 0) start = i;
            if (++run >= count) {
                uint16_t j;
                for (j = start; j < start + count; j++)
                    bitmap_set(bm, j, 1);
                return start;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}

int tetfs_format(void) {
    uint8_t zero[512];
    uint32_t i;

    memset(zero, 0, 512);

    tetfs_super_t s;
    memset(&s, 0, sizeof(s));
    s.magic          = TETFS_MAGIC;
    s.version        = TETFS_VERSION;
    s.total_blocks   = TETFS_MAX_BLOCKS;
    s.free_blocks    = TETFS_MAX_BLOCKS;
    s.inode_count    = TETFS_MAX_INODES;
    s.first_data_lba = TETFS_LBA_DATA;
    if (write_super(&s) != 0) return -1;

    for (i = 0; i < 4; i++)
        if (ata_write(TETFS_LBA_INODES + i, 1, zero) != 0) return -1;

    for (i = 0; i < 2; i++)
        if (ata_write(TETFS_LBA_BITMAP + i, 1, zero) != 0) return -1;

    tetfs_inode_t root;
    memset(&root, 0, sizeof(root));
    root.type        = TETFS_TYPE_DIR;
    root.name[0]     = '/';
    root.name[1]     = '\0';
    root.parent      = 0xFFFF;
    if (write_inode(0, &root) != 0) return -1;

    fs_mounted = 1;
    return 0;
}

int tetfs_mount(void) {
    tetfs_super_t s;
    if (read_super(&s) != 0)          return -1;
    if (s.magic != TETFS_MAGIC)       return -1;
    if (s.version != TETFS_VERSION)   return -1;
    fs_mounted = 1;
    return 0;
}

int tetfs_is_mounted(void) {
    return fs_mounted;
}

int tetfs_find(const char *name, uint16_t parent) {
    uint16_t i;
    tetfs_inode_t node;
    for (i = 1; i < TETFS_MAX_INODES; i++) {
        if (tetfs_read_inode(i, &node) != 0) continue;
        if (node.type == TETFS_TYPE_FREE)    continue;
        if (node.parent == parent && strcmp(node.name, name) == 0)
            return i;
    }
    return -1;
}

int tetfs_create(const char *name, uint16_t parent, uint8_t type) {
    if (tetfs_find(name, parent) >= 0) return -1;

    uint16_t i;
    tetfs_inode_t node;
    for (i = 1; i < TETFS_MAX_INODES; i++) {
        if (tetfs_read_inode(i, &node) != 0) continue;
        if (node.type != TETFS_TYPE_FREE) continue;

        memset(&node, 0, sizeof(node));
        node.type   = type;
        node.parent = parent;
        int j;
        for (j = 0; j < 50 && name[j]; j++) node.name[j] = name[j];
        node.name[j] = '\0';
        return write_inode(i, &node) == 0 ? i : -1;
    }
    return -1;
}

int tetfs_delete(uint16_t idx) {
    if (idx == TETFS_ROOT_INODE) return -1;
    tetfs_inode_t node;
    if (tetfs_read_inode(idx, &node) != 0)   return -1;
    if (node.type == TETFS_TYPE_FREE)         return -1;

    if (node.block_count > 0) {
        uint8_t bm[1024];
        if (read_bitmap(bm) != 0) return -1;
        uint16_t i;
        for (i = 0; i < node.block_count; i++)
            bitmap_set(bm, (uint16_t)(node.start_block + i), 0);
        if (write_bitmap(bm) != 0) return -1;
    }

    memset(&node, 0, sizeof(node));
    node.type = TETFS_TYPE_FREE;
    return write_inode(idx, &node);
}

int tetfs_write(uint16_t idx, const void *buf, uint32_t len) {
    tetfs_inode_t node;
    if (tetfs_read_inode(idx, &node) != 0) return -1;
    if (node.type != TETFS_TYPE_FILE)      return -1;
    if (len == 0) { node.size = 0; return write_inode(idx, &node); }

    uint16_t blocks_needed = (uint16_t)((len + TETFS_BLOCK_SIZE - 1) / TETFS_BLOCK_SIZE);
    if (blocks_needed > TETFS_MAX_FILE_BLOCKS) return -1;

    uint8_t bm[1024];
    if (read_bitmap(bm) != 0) return -1;

    if (node.block_count > 0) {
        uint16_t i;
        for (i = 0; i < node.block_count; i++)
            bitmap_set(bm, (uint16_t)(node.start_block + i), 0);
    }

    int start = alloc_blocks(bm, blocks_needed);
    if (start < 0) return -1;
    if (write_bitmap(bm) != 0) return -1;

    uint8_t sector[512];
    uint16_t i;
    for (i = 0; i < blocks_needed; i++) {
        uint32_t off   = (uint32_t)i * TETFS_BLOCK_SIZE;
        uint32_t chunk = len - off;
        if (chunk > TETFS_BLOCK_SIZE) chunk = TETFS_BLOCK_SIZE;
        memset(sector, 0, 512);
        memcpy(sector, (const uint8_t *)buf + off, chunk);
        if (ata_write((uint32_t)(TETFS_LBA_DATA + start + i), 1, sector) != 0)
            return -1;
    }

    node.start_block = (uint16_t)start;
    node.block_count = blocks_needed;
    node.size        = len;
    return write_inode(idx, &node);
}

int tetfs_read(uint16_t idx, void *buf, uint32_t offset, uint32_t len) {
    tetfs_inode_t node;
    if (tetfs_read_inode(idx, &node) != 0) return -1;
    if (node.type != TETFS_TYPE_FILE)      return -1;
    if (offset >= node.size)               return 0;
    if (offset + len > node.size)
        len = node.size - offset;

    uint8_t sector[512];
    uint32_t done = 0, pos = offset;
    while (done < len) {
        uint16_t blk      = (uint16_t)(pos / TETFS_BLOCK_SIZE);
        uint32_t blk_off  = pos % TETFS_BLOCK_SIZE;
        uint32_t chunk    = TETFS_BLOCK_SIZE - blk_off;
        if (chunk > len - done) chunk = len - done;
        if (blk >= node.block_count) break;
        if (ata_read((uint32_t)(TETFS_LBA_DATA + node.start_block + blk), 1, sector) != 0)
            return -1;
        memcpy((uint8_t *)buf + done, sector + blk_off, chunk);
        done += chunk;
        pos  += chunk;
    }
    return (int)done;
}

int tetfs_list(uint16_t parent, tetfs_inode_t *out, int max) {
    int n = 0;
    uint16_t i;
    for (i = 0; i < TETFS_MAX_INODES && n < max; i++) {
        tetfs_inode_t node;
        if (tetfs_read_inode(i, &node) != 0)  continue;
        if (node.type == TETFS_TYPE_FREE)      continue;
        if (i == TETFS_ROOT_INODE)             continue;
        if (node.parent == parent)
            out[n++] = node;
    }
    return n;
}

int tetfs_count_children(uint16_t parent) {
    int n = 0;
    uint16_t i;
    tetfs_inode_t node;
    for (i = 0; i < TETFS_MAX_INODES; i++) {
        if (tetfs_read_inode(i, &node) != 0) continue;
        if (node.type == TETFS_TYPE_FREE)    continue;
        if (i == TETFS_ROOT_INODE)           continue;
        if (node.parent == parent)           n++;
    }
    return n;
}

void fs_init(void) {
    memset(fd_table, 0, sizeof(fd_table));
    vfs_init();
    tetfs_mount();
}

int fs_open(const char *name, int write) {
    if (!fs_mounted) return -1;

    int idx = tetfs_find(name, TETFS_ROOT_INODE);

    if (idx < 0) {
        if (!write) return -1;
        idx = tetfs_create(name, TETFS_ROOT_INODE, TETFS_TYPE_FILE);
        if (idx < 0) return -1;
    }

    int fd;
    for (fd = 0; fd < TETFS_MAX_FD; fd++) {
        if (!fd_table[fd].used) {
            fd_table[fd].used   = 1;
            fd_table[fd].inode  = (uint16_t)idx;
            fd_table[fd].offset = 0;
            return fd;
        }
    }
    return -1;
}

int fs_read(int fd, void *buf, uint32_t len) {
    if (fd < 0 || fd >= TETFS_MAX_FD || !fd_table[fd].used) return -1;
    tetfs_fd_t *f = &fd_table[fd];
    int n = tetfs_read(f->inode, buf, f->offset, len);
    if (n > 0) f->offset += (uint32_t)n;
    return n;
}

int fs_write(int fd, const void *buf, uint32_t len) {
    if (fd < 0 || fd >= TETFS_MAX_FD || !fd_table[fd].used) return -1;
    return tetfs_write(fd_table[fd].inode, buf, len);
}

void fs_close(int fd) {
    if (fd >= 0 && fd < TETFS_MAX_FD)
        fd_table[fd].used = 0;
}
