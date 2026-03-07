#ifndef ISR_H
#define ISR_H

#include "types.h"

struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

typedef void (*isr_handler_t)(struct registers*);

void isr_init(void);
void isr_register_handler(uint8_t n, isr_handler_t handler);

#endif
