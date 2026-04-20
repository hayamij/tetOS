#include "debug.h"
#include "../stdio/stdio.h"
#include "../vga/vga.h"

void debug_stacktrace(void) {
    uint32_t *ebp;
    __asm__ __volatile__("mov %%ebp, %0" : "=r"(ebp));

    kprintf("Stacktrace:\n");
    for (int i = 0; ebp && i < 8; i++) {
        uint32_t ret = ebp[1];
        if (ret == 0) break;
        kprintf("  #%u 0x%x\n", (uint32_t)i, ret);
        ebp = (uint32_t *)ebp[0];
    }
}

void panic(const char *msg, uint32_t eip, uint32_t err_code) {
    vga_write_color("\n[KERNEL PANIC] ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    kprintf("%s\n", msg ? msg : "(null)");
    if (eip || err_code) {
        kprintf("EIP=0x%x ERR=0x%x\n", eip, err_code);
    }
    debug_stacktrace();
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}
