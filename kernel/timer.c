#include "timer.h"
#include "isr.h"
#include "io.h"

static volatile uint32_t tick_count = 0;
static void (*sched_hook)(struct registers *) = NULL;

void timer_set_scheduler(void (*cb)(struct registers *)) {
    sched_hook = cb;
}

static void timer_callback(struct registers* regs) {
    tick_count++;
    if (sched_hook) sched_hook(regs);
}

void timer_init(uint32_t frequency) {
    isr_register_handler(32, timer_callback);
    
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
