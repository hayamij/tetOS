#ifndef HEAP_H
#define HEAP_H

#include "../types/types.h"

void     heap_init(void);
void*    kmalloc(uint32_t size);
void     kfree(void* ptr);
uint32_t heap_used_bytes(void);

#endif
