#include "appseed.h"
#include "../exec/elf.h"
#include "../fs/tetfs.h"
#include "../string/string.h"

extern const uint8_t _binary_build_user_hello_elf_start[];
extern const uint8_t _binary_build_user_hello_elf_end[];

int appseed_install(void) {
    if (!tetfs_is_mounted()) return -1;

    uint32_t len = (uint32_t)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);
    if (len < sizeof(elf_header_t)) return -1;
    if (*(const uint32_t *)_binary_build_user_hello_elf_start != ELF_MAGIC) return -1;

    int idx = tetfs_find("hello.elf", TETFS_ROOT_INODE);
    if (idx < 0)
        idx = tetfs_create("hello.elf", TETFS_ROOT_INODE, TETFS_TYPE_FILE);
    if (idx < 0) return -1;

    if (tetfs_write((uint16_t)idx, _binary_build_user_hello_elf_start, len) != 0) return -1;

    uint32_t disk_magic = 0;
    if (tetfs_read((uint16_t)idx, &disk_magic, 0, sizeof(disk_magic)) != (int)sizeof(disk_magic)) return -1;
    if (disk_magic != ELF_MAGIC) return -1;

    return 0;
}
