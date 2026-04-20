#include "timer.h"
#include "../isr/isr.h"
#include "../io/io.h"

static volatile uint32_t tick_count = 0;
static void (*sched_hook)(struct registers *) = NULL;
static uint8_t sched_hook_enabled = 0;

void timer_set_scheduler(void (*cb)(struct registers *)) {
    sched_hook = cb;
    sched_hook_enabled = (cb != 0);
}

static void timer_callback(struct registers* regs) {
    tick_count++;
    if (sched_hook_enabled && sched_hook) sched_hook(regs);
}

void timer_init(uint32_t frequency) {
    tick_count = 0;
    sched_hook = 0;
    sched_hook_enabled = 0;

    register_irq_handler(0, timer_callback);
    
    uint32_t divisor = 1193180 / frequency;
    
    outb(0x43, 0x36);
    
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    
    outb(0x40, low);
    outb(0x40, high);
    
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x01);
}

uint32_t timer_ticks(void) {
    return tick_count;
}

void timer_wait(uint32_t ticks) {
    uint32_t end_tick = tick_count + ticks;
    while (tick_count < end_tick) {
        __asm__ __volatile__("hlt");
    }
}
