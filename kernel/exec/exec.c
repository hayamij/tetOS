#include "exec.h"
#include "elf.h"
#include "../fs/tetfs.h"
#include "../vmm/vmm.h"
#include "../pmm/pmm.h"
#include "../heap/heap.h"
#include "../string/string.h"
#include "../process/process.h"
#include "../stdio/stdio.h"

int exec_elf_from_disk(const char *filename) {
    int idx = tetfs_find(filename, TETFS_ROOT_INODE);
    if (idx < 0) return -1;

    tetfs_inode_t node;
    tetfs_read_inode((uint16_t)idx, &node);
    if (node.size < sizeof(elf_header_t)) return -1;

    elf_header_t *hdr = (elf_header_t *)kmalloc(node.size);
    if (!hdr) return -1;

    int nread = tetfs_read((uint16_t)idx, (char *)hdr, 0, node.size);
    if (nread <= 0) {
        kfree(hdr);
        return -1;
    }

    if (*(uint32_t *)hdr->e_ident != ELF_MAGIC) {
        kfree(hdr);
        return -1;
    }

    uint32_t *user_dir = vmm_create_address_space();
    if (!user_dir) {
        kfree(hdr);
        return -1;
    }

    uint32_t phoff = hdr->e_phoff;
    uint16_t phnum = hdr->e_phnum;
    uint16_t phsize = hdr->e_phentsize;

    for (uint16_t i = 0; i < phnum; i++) {
        elf_program_header_t *ph = (elf_program_header_t *)
            ((uint8_t *)hdr + phoff + i * phsize);

        if (ph->p_type != PT_LOAD) continue;

        uint32_t virt = ph->p_vaddr;
        uint32_t offset = ph->p_offset;
        uint32_t fsize = ph->p_filesz;
        uint32_t msize = ph->p_memsz;

        uint8_t *data = (uint8_t *)hdr + offset;

        for (uint32_t j = 0; j < msize; j += 0x1000) {
            uint32_t frame = pmm_alloc_frame();
            if (!frame) {
                kfree(hdr);
                return -1;
            }

            memset((void *)frame, 0, 0x1000);

            uint32_t copy_sz = (fsize > j) ? ((fsize - j < 0x1000) ? (fsize - j) : 0x1000) : 0;
            if (copy_sz > 0) {
                memcpy((void *)frame, data + j, copy_sz);
            }

            uint32_t flags = PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
            vmm_map_page_in_dir(user_dir, virt + j, frame, flags);
        }
    }

    process_t *p = process_create_user((void *)hdr->e_entry, filename);
    if (!p) {
        kfree(hdr);
        return -1;
    }

    p->user_page_dir = (uint32_t)user_dir;

    kfree(hdr);
    return p->pid;
}
