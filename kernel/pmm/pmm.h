#ifndef PMM_H
#define PMM_H

#include "../types/types.h"

#define PMM_FRAME_SIZE    4096
#define PMM_MEMORY_SIZE   (128 * 1024 * 1024)
#define PMM_TOTAL_FRAMES  (PMM_MEMORY_SIZE / PMM_FRAME_SIZE)
/*
 * Reserve everything below 0xC00000 (12 MB) for the kernel, boot stack and
 * the graphics backbuffer at 0x00800000 (3.6 MB for 1280x720x32).
 */
#define PMM_RESERVED_END  0xC00000

void     pmm_init(uint32_t kernel_end_addr);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t addr);
uint32_t alloc_frame(void);
void     free_frame(uint32_t addr);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

#endif
