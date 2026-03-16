#include "usermode.h"

void enter_user_mode(void (*entry)(void), uint32_t user_stack_top) {
    __asm__ __volatile__(
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl $0x23\n"
        "pushl %0\n"
        "pushf\n"
        "pop %%eax\n"
        "or $0x200, %%eax\n"
        "push %%eax\n"
        "pushl $0x1B\n"
        "pushl %1\n"
        "iret\n"
        :
        : "r"(user_stack_top), "r"(entry)
        : "eax", "memory"
    );
}