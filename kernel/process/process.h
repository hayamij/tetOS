#ifndef PROCESS_H
#define PROCESS_H

#include "../types/types.h"
#include "../isr/isr.h"

/* Process states */
#define PROC_READY    0
#define PROC_RUNNING  1
#define PROC_DEAD     2
#define PROC_UNUSED   0xFF

#define MAX_PROCS     16
#define PROC_STACK    4096
#define SCHED_QUANTUM 5     /* timer ticks per time slice (50ms @ 100Hz) */

typedef struct process {
    uint32_t  pid;
    uint8_t   state;
    uint32_t  esp;        /* saved kernel stack pointer (frame bottom) */
    uint8_t  *stack;      /* NULL for process 0 (uses boot stack) */
    char      name[32];
    struct process *next; /* circular run queue */
} process_t;

void       process_init(void);
process_t *process_create(void (*entry)(void), const char *name);
void       process_exit(void);
void       schedule(struct registers *regs);
process_t *process_current(void);
int        process_list(process_t **out, int max);

/* Written by schedule(), read by irq_common_stub in isr_stub.asm */
extern volatile uint32_t sched_switch_esp;

#endif
