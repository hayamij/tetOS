#ifndef VMM_H
#define VMM_H

#include "types.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

void vmm_init(void);
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

#endif
