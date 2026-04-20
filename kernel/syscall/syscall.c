#include "syscall.h"
#include "../isr/isr.h"
#include "../vga/vga.h"
#include "../stdio/stdio.h"
#include "../process/process.h"
#include "../fs/vfs.h"
#include "../heap/heap.h"

#define SYSCALL_MAX 64
#define USER_PTR_MIN 0x00001000u
#define USER_PTR_MAX 0xBFFFFFFFu

typedef int32_t (*syscall_fn_t)(struct registers *regs);

static syscall_fn_t syscall_table[SYSCALL_MAX];

static int validate_user_ptr(const void *ptr, uint32_t len) {
    uint32_t p = (uint32_t)ptr;
    if (p < USER_PTR_MIN || p > USER_PTR_MAX) return 0;
    if (len == 0) return 1;
    if ((uint32_t)(p + len - 1) < p) return 0;
    if ((uint32_t)(p + len - 1) > USER_PTR_MAX) return 0;
    return 1;
}

static int32_t sys_exit(struct registers *regs) {
    (void)regs;
    process_exit();
    return 0;
}

static int32_t sys_write(struct registers *regs) {
    const char *buf = (const char *)regs->ebx;
    uint32_t len = regs->ecx;
    if (!validate_user_ptr(buf, len)) return -1;

    for (uint32_t i = 0; i < len; i++) {
        vga_putchar(buf[i]);
    }
    return (int32_t)len;
}

static int32_t sys_open(struct registers *regs) {
    const char *path = (const char *)regs->ebx;
    if (!validate_user_ptr(path, 1)) return -1;
    return vfs_open(path, VFS_O_RDONLY);
}

static int32_t sys_read(struct registers *regs) {
    int fd = (int)regs->ebx;
    void *buf = (void *)regs->ecx;
    uint32_t len = regs->edx;
    if (!validate_user_ptr(buf, len)) return -1;
    return vfs_read(fd, buf, len);
}

static int32_t sys_close(struct registers *regs) {
    int fd = (int)regs->ebx;
    return vfs_close(fd);
}

static int32_t sys_getpid(struct registers *regs) {
    (void)regs;
    process_t *p = process_current();
    return p ? (int32_t)p->pid : 0;
}

static int32_t sys_fork(struct registers *regs) {
    (void)regs;
    return process_fork_stub();
}

static int32_t sys_exec(struct registers *regs) {
    const char *path = (const char *)regs->ebx;
    if (!validate_user_ptr(path, 1)) return -1;
    return process_exec_stub(path);
}

static int32_t sys_malloc(struct registers *regs) {
    return (int32_t)kmalloc(regs->ebx);
}

static int32_t sys_free(struct registers *regs) {
    void *ptr = (void *)regs->ebx;
    kfree(ptr);
    return 0;
}

static void syscall_handler(struct registers *regs) {
    uint32_t num = regs->eax;
    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        regs->eax = (uint32_t)-1;
        return;
    }

    regs->eax = (uint32_t)syscall_table[num](regs);
}

void syscall_init(void) {
    for (uint32_t i = 0; i < SYSCALL_MAX; i++) syscall_table[i] = 0;

    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_EXEC] = sys_exec;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;

    isr_register_handler(0x80, syscall_handler);
    printk("[syscall] table initialized\n");
}
