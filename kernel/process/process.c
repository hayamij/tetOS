#include "process.h"
#include "../heap/heap.h"
#include "../string/string.h"
#include "../vmm/vmm.h"

static process_t  process_table[MAX_PROCS];
static process_t *run_queue       = NULL;
static process_t *current_process = NULL;
static uint32_t   next_pid        = 0;

/* Set by schedule() when a context switch is needed.
   Checked and cleared by irq_common_stub in isr_stub.asm. */
volatile uint32_t sched_switch_esp = 0;
volatile uint32_t sched_switch_cr3 = 0;

static void reset_process_slot(process_t *p) {
    if (p->stack) {
        kfree(p->stack);
        p->stack = NULL;
    }
    p->pid = 0;
    p->state = PROC_UNUSED;
    p->esp = 0;
    p->name[0] = '\0';
    p->next = NULL;
}

static void copy_name(char *dst, const char *src) {
    int i;
    for (i = 0; i < 31 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

void process_init(void) {
    uint32_t i;
    for (i = 0; i < MAX_PROCS; i++)
        process_table[i].state = PROC_UNUSED;

    /* Process 0 = kernel/shell (already running, using boot stack) */
    process_t *p = &process_table[0];
    p->pid   = next_pid++;
    p->state = PROC_RUNNING;
    p->esp   = 0;       /* set on first preemption */
    p->stack = NULL;    /* uses the boot stack */
    p->next  = p;
    copy_name(p->name, "shell");

    run_queue       = p;
    current_process = p;
}

process_t *process_create(void (*entry)(void), const char *name) {
    /* Find an unused slot (skip slot 0 = shell) */
    uint32_t i;
    process_t *p = NULL;
    for (i = 1; i < MAX_PROCS; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            p = &process_table[i];
            break;
        }
    }
    if (!p) return NULL;

    /* Allocate kernel stack */
    uint8_t *stack = (uint8_t *)kmalloc(PROC_STACK);
    if (!stack) return NULL;

    p->pid   = next_pid++;
    p->state = PROC_READY;
    p->stack = stack;
    copy_name(p->name, name);

    /*
     * Build a fake IRQ interrupt frame on the new stack so that when
     * irq_common_stub restores it (pop ds / popa / add esp,8 / iret),
     * the CPU jumps straight to `entry` with interrupts enabled.
     *
     * Layout (ESP pointing at lowest address = frame bottom):
     *   [esp+ 0] ds       = 0x10
     *   [esp+ 4] edi      = 0   \
     *   [esp+ 8] esi      = 0    |
     *   [esp+12] ebp      = 0    |  pusha block
     *   [esp+16] esp_val  = 0    |  (popa ignores this slot)
     *   [esp+20] ebx      = 0    |
     *   [esp+24] edx      = 0    |
     *   [esp+28] ecx      = 0    |
     *   [esp+32] eax      = 0   /
     *   [esp+36] int_no   = 32
     *   [esp+40] err_code = 0
     *   [esp+44] eip      = entry     \
     *   [esp+48] cs       = 0x08       | iret pops these
     *   [esp+52] eflags   = 0x202     /  (IF=1)
     */
    uint32_t *sp = (uint32_t *)(stack + PROC_STACK);
    *(--sp) = 0x202;           /* eflags: IF=1, reserved bit 1 */
    *(--sp) = 0x08;            /* cs: kernel code */
    *(--sp) = (uint32_t)entry; /* eip: process entry point */
    *(--sp) = 0;               /* err_code */
    *(--sp) = 32;              /* int_no = 32 (IRQ0) */
    *(--sp) = 0;               /* eax */
    *(--sp) = 0;               /* ecx */
    *(--sp) = 0;               /* edx */
    *(--sp) = 0;               /* ebx */
    *(--sp) = 0;               /* esp (saved by pusha, ignored by popa) */
    *(--sp) = 0;               /* ebp */
    *(--sp) = 0;               /* esi */
    *(--sp) = 0;               /* edi */
    *(--sp) = 0x10;            /* ds: kernel data */

    p->esp = (uint32_t)sp;     /* frame bottom = what irq_common_stub expects */

    /* Insert right after current process in the circular list */
    p->next = current_process->next;
    current_process->next = p;

    return p;
}

process_t *process_create_user(void (*entry)(void), uint32_t user_stack_top, const char *name) {
    uint32_t i;
    process_t *p = NULL;
    for (i = 1; i < MAX_PROCS; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            p = &process_table[i];
            break;
        }
    }
    if (!p) return NULL;

    uint8_t *stack = (uint8_t *)kmalloc(PROC_STACK);
    if (!stack) return NULL;

    p->pid   = next_pid++;
    p->state = PROC_READY;
    p->stack = stack;
    copy_name(p->name, name);
    p->user_page_dir = 0;

    for (i = 0; i < MAX_FD; i++) {
        p->user_fd[i].inode_idx = 0;
        p->user_fd[i].offset = 0;
        p->user_fd_used[i] = 0;
    }

    uint32_t *sp = (uint32_t *)(stack + PROC_STACK);
    *(--sp) = 0x23;
    *(--sp) = user_stack_top;
    *(--sp) = 0x202;
    *(--sp) = 0x1B;
    *(--sp) = (uint32_t)entry;
    *(--sp) = 0;
    *(--sp) = 32;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0x23;

    p->esp = (uint32_t)sp;

    p->next = current_process->next;
    current_process->next = p;

    return p;
}

void process_exit(void) {
    current_process->state = PROC_DEAD;
    while (1) {
        __asm__ volatile("sti; hlt");
    }
}

int process_kill(uint32_t pid) {
    process_t *p;
    process_t *prev;

    if (!run_queue) return -1;
    if (!current_process) return -1;
    if (pid == current_process->pid) return -2;

    prev = run_queue;
    p = run_queue;
    do {
        if (p->pid == pid) break;
        prev = p;
        p = p->next;
    } while (p != run_queue);

    if (p->pid != pid) return -1;
    if (p == current_process) return -2;

    prev->next = p->next;
    if (run_queue == p)
        run_queue = p->next;

    reset_process_slot(p);
    return 0;
}

void schedule(struct registers *regs) {
    static uint32_t quantum_count = 0;

    if (!current_process) return;

    int force = (current_process->state == PROC_DEAD);
    if (force) {
        process_t *dead = current_process;
        process_t *prev = dead;
        process_t *candidate = dead->next;

        while (prev->next != dead)
            prev = prev->next;

        while (candidate != dead && candidate->state != PROC_READY)
            candidate = candidate->next;

        if (candidate == dead) {
            dead->state = PROC_RUNNING;
            return;
        }

        prev->next = dead->next;
        if (run_queue == dead)
            run_queue = dead->next;

        reset_process_slot(dead);

        candidate->state = PROC_RUNNING;
        current_process = candidate;
        sched_switch_esp = candidate->esp;
        sched_switch_cr3 = candidate->user_page_dir ? candidate->user_page_dir : vmm_get_kernel_pd();
        quantum_count = 0;
        return;
    }

    if (!force) {
        if (++quantum_count < SCHED_QUANTUM) return;
        quantum_count = 0;
    }

    current_process->esp = (uint32_t)regs;

    if (current_process->state == PROC_RUNNING)
        current_process->state = PROC_READY;

    process_t *candidate = current_process->next;
    while (candidate != current_process && candidate->state != PROC_READY)
        candidate = candidate->next;

    if (candidate == current_process) {
        if (current_process->state == PROC_READY)
            current_process->state = PROC_RUNNING;
        return;
    }

    candidate->state  = PROC_RUNNING;
    current_process   = candidate;
    sched_switch_esp  = candidate->esp;
    sched_switch_cr3  = candidate->user_page_dir ? candidate->user_page_dir : vmm_get_kernel_pd();
}

process_t *process_current(void) {
    return current_process;
}

int process_list(process_t **out, int max) {
    int n = 0;
    process_t *p = run_queue;
    if (!p) return 0;
    do {
        if (n < max) out[n++] = p;
        p = p->next;
    } while (p != run_queue);
    return n;
}
