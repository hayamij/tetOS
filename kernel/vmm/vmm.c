#include "vmm.h"
#include "../pmm/pmm.h"
#include "../string/string.h"

static uint32_t* page_directory;

void vmm_init(void) {
    page_directory = (uint32_t*)pmm_alloc_frame();
    if (!page_directory) return;
    memset(page_directory, 0, PMM_FRAME_SIZE);

    uint32_t* pt;
    for (uint32_t mb = 0; mb < 8; mb++) {
        pt = (uint32_t*)pmm_alloc_frame();
        if (!pt) return;
        for (int i = 0; i < 1024; i++) {
            pt[i] = (uint32_t)((mb * 1024 + i) * PMM_FRAME_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
        }
        page_directory[mb] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    for (int i = 8; i < 1024; i++) {
        page_directory[i] = 0x00000002;
    }

    uint32_t cr0;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"((uint32_t)page_directory));
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_idx] & PAGE_PRESENT)) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) return;
        uint32_t* tbl = (uint32_t*)frame;
        memset(tbl, 0, PMM_FRAME_SIZE);
        page_directory[dir_idx] = frame | PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t* table = (uint32_t*)(page_directory[dir_idx] & ~0xFFFu);
    table[table_idx] = (phys & ~0xFFFu) | (flags & 0xFFFu) | PAGE_PRESENT;
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

uint32_t* vmm_create_address_space(void) {
    uint32_t *new_dir = (uint32_t*)pmm_alloc_frame();
    if (!new_dir) return 0;
    memset(new_dir, 0, PMM_FRAME_SIZE);
    vmm_copy_kernel_space(new_dir);
    return new_dir;
}

void vmm_copy_kernel_space(uint32_t *new_dir) {
    uint32_t i;
    for (i = 0; i < 1024; i++) {
        new_dir[i] = page_directory[i];
    }
}

void vmm_map_page_in_dir(uint32_t *dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    if (!(dir[dir_idx] & PAGE_PRESENT)) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) return;
        uint32_t* tbl = (uint32_t*)frame;
        memset(tbl, 0, PMM_FRAME_SIZE);
        dir[dir_idx] = frame | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    uint32_t* table = (uint32_t*)(dir[dir_idx] & ~0xFFFu);
    table[table_idx] = (phys & ~0xFFFu) | (flags & 0xFFFu) | PAGE_PRESENT;
}

uint32_t vmm_get_kernel_pd(void) {
    return (uint32_t)page_directory;
}
