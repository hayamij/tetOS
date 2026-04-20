#ifndef DEBUG_H
#define DEBUG_H

#include "../types/types.h"

void panic(const char *msg, uint32_t eip, uint32_t err_code);
void debug_stacktrace(void);

#define KASSERT(expr) do { \
    if (!(expr)) { \
        panic("assertion failed: " #expr, 0, 0); \
    } \
} while (0)

#endif
