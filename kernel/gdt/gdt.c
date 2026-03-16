#include "gdt.h"
#include "../string/string.h"

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;
static struct tss_entry tss;
static uint8_t kernel_stack[4096];

extern void gdt_flush(uint32_t);

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

static void tss_init(void) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    gdt_set_gate(5, base, limit, 0x89, 0x00);
    memset(&tss, 0, sizeof(struct tss_entry));

    tss.ss0 = 0x10;
    tss.esp0 = (uint32_t)(kernel_stack + sizeof(kernel_stack));

    tss.cs = 0x0B;
    tss.ss = 0x13;
    tss.ds = 0x13;
    tss.es = 0x13;
    tss.fs = 0x13;
    tss.gs = 0x13;
    tss.iomap_base = sizeof(struct tss_entry);
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gp.base = (uint32_t)&gdt;
    
    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    tss_init();
    
    gdt_flush((uint32_t)&gp);
}
