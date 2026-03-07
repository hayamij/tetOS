#include "pmm.h"

static uint32_t* pmm_bitmap;
static uint32_t  pmm_total;
static uint32_t  pmm_used;

static void pmm_set_used(uint32_t frame) {
    pmm_bitmap[frame / 32] |= (1u << (frame % 32));
}

static void pmm_set_free(uint32_t frame) {
    pmm_bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static int pmm_is_used(uint32_t frame) {
    return (pmm_bitmap[frame / 32] >> (frame % 32)) & 1;
}

void pmm_init(uint32_t kernel_end_addr) {
    pmm_bitmap = (uint32_t*)kernel_end_addr;
    pmm_total  = PMM_TOTAL_FRAMES;
    pmm_used   = PMM_TOTAL_FRAMES;

    for (uint32_t i = 0; i < pmm_total / 32; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }

    uint32_t bitmap_size = (pmm_total / 8);
    uint32_t free_start  = (kernel_end_addr + bitmap_size + PMM_FRAME_SIZE - 1) & ~(PMM_FRAME_SIZE - 1);
    if (free_start < PMM_RESERVED_END) free_start = PMM_RESERVED_END;

    for (uint32_t addr = free_start; addr < PMM_MEMORY_SIZE; addr += PMM_FRAME_SIZE) {
        pmm_set_free(addr / PMM_FRAME_SIZE);
        pmm_used--;
    }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < pmm_total; i++) {
        if (!pmm_is_used(i)) {
            pmm_set_used(i);
            pmm_used++;
            return i * PMM_FRAME_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / PMM_FRAME_SIZE;
    if (frame >= pmm_total) return;
    pmm_set_free(frame);
    pmm_used--;
}

uint32_t pmm_free_frames(void) {
    return pmm_total - pmm_used;
}

uint32_t pmm_total_frames(void) {
    return pmm_total;
}
