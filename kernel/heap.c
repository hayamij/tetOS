#include "heap.h"

#define HEAP_START 0x400000u
#define HEAP_SIZE  (4 * 1024 * 1024)
#define HEAP_MAGIC 0xC0FFEE00u

typedef struct block_header {
    uint32_t             magic;
    uint32_t             size;
    uint8_t              used;
    struct block_header* next;
} block_header_t;

static block_header_t* heap_head;

void heap_init(void) {
    heap_head        = (block_header_t*)HEAP_START;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size  = HEAP_SIZE - sizeof(block_header_t);
    heap_head->used  = 0;
    heap_head->next  = 0;
}

void* kmalloc(uint32_t size) {
    block_header_t* current = heap_head;
    while (current) {
        if (!current->used && current->size >= size) {
            if (current->size > size + sizeof(block_header_t) + 16) {
                block_header_t* split = (block_header_t*)((uint8_t*)current + sizeof(block_header_t) + size);
                split->magic  = HEAP_MAGIC;
                split->size   = current->size - size - sizeof(block_header_t);
                split->used   = 0;
                split->next   = current->next;
                current->next = split;
                current->size = size;
            }
            current->used = 1;
            return (void*)((uint8_t*)current + sizeof(block_header_t));
        }
        current = current->next;
    }
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header_t* header = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (header->magic != HEAP_MAGIC) return;
    header->used = 0;
    if (header->next && !header->next->used) {
        header->size += sizeof(block_header_t) + header->next->size;
        header->next  = header->next->next;
    }
}

uint32_t heap_used_bytes(void) {
    uint32_t used = 0;
    block_header_t* current = heap_head;
    while (current) {
        if (current->used) used += current->size;
        current = current->next;
    }
    return used;
}
