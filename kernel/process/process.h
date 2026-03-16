#ifndef PROCESS_H
#define PROCESS_H

#include "../types/types.h"
#include "../isr/isr.h"

#define PROC_READY    0
#define PROC_RUNNING  1
#define PROC_DEAD     2
#define PROC_UNUSED   0xFF

#define MAX_PROCS     16
#define PROC_STACK    4096
#define SCHED_QUANTUM 5
#define MAX_FD        16
#define USER_STACK_BASE 0x08000000
#define USER_STACK_SIZE 0x1000

struct fd_entry {
    uint16_t inode_idx;
    uint32_t offset;
};

typedef struct process {
    uint32_t  pid;
    uint8_t   state;
    uint32_t  esp;
    uint8_t  *stack;
    char      name[32];
    struct process *next;
    uint32_t  user_page_dir;
    struct fd_entry user_fd[MAX_FD];
    uint8_t   user_fd_used[MAX_FD];
} process_t;

void       process_init(void);
process_t *process_create(void (*entry)(void), const char *name);
process_t *process_create_user(void (*entry)(void), const char *name);
void       process_exit(void);
int        process_kill(uint32_t pid);
void       schedule(struct registers *regs);
process_t *process_current(void);
int        process_list(process_t **out, int max);

/* Written by schedule(), read by irq_common_stub in isr_stub.asm */
extern volatile uint32_t sched_switch_esp;

#endif
