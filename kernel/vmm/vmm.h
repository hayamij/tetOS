#ifndef VMM_H
#define VMM_H

#include "../types/types.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

typedef struct {
    uint32_t entries[1024];
} page_table_t;

typedef struct {
    uint32_t entries[1024];
} page_directory_t;

void vmm_init(void);
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void unmap_page(uint32_t virt);
uint32_t* vmm_create_address_space(void);
void vmm_copy_kernel_space(uint32_t *new_dir);
void vmm_map_page_in_dir(uint32_t *dir, uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t vmm_get_kernel_pd(void);

#endif
