#include "syscall.h"
#include "../isr/isr.h"
#include "../vga/vga.h"
#include "../process/process.h"
#include "../fs/tetfs.h"
#include "../heap/heap.h"

static void syscall_handler(struct registers *regs) {
    uint32_t i;
    const char *buf;
    process_t *p;
    int fd;

    if (regs->eax == SYS_EXIT) {
        process_exit();
        return;
    }

    if (regs->eax == SYS_WRITE) {
        buf = (const char *)regs->ebx;
        for (i = 0; i < regs->ecx; i++)
            vga_putchar(buf[i]);
        regs->eax = regs->ecx;
        return;
    }

    if (regs->eax == SYS_GETPID) {
        p = process_current();
        regs->eax = p ? p->pid : 0;
        return;
    }

    if (regs->eax == SYS_OPEN) {
        const char *filename = (const char *)regs->ebx;
        p = process_current();
        if (!p) {
            regs->eax = -1;
            return;
        }

        for (fd = 0; fd < MAX_FD; fd++) {
            if (!p->user_fd_used[fd]) break;
        }
        if (fd >= MAX_FD) {
            regs->eax = -1;
            return;
        }

        int idx = tetfs_find(filename, TETFS_ROOT_INODE);
        if (idx < 0) {
            regs->eax = -1;
            return;
        }

        p->user_fd[fd].inode_idx = (uint16_t)idx;
        p->user_fd[fd].offset = 0;
        p->user_fd_used[fd] = 1;
        regs->eax = fd;
        return;
    }

    if (regs->eax == SYS_READ) {
        fd = (int)regs->ebx;
        char *buf = (char *)regs->ecx;
        uint32_t len = regs->edx;

        p = process_current();
        if (!p || fd < 0 || fd >= MAX_FD || !p->user_fd_used[fd]) {
            regs->eax = -1;
            return;
        }

        int nread = tetfs_read(p->user_fd[fd].inode_idx, buf, p->user_fd[fd].offset, len);
        if (nread < 0) {
            regs->eax = -1;
            return;
        }

        p->user_fd[fd].offset += nread;
        regs->eax = nread;
        return;
    }

    if (regs->eax == SYS_CLOSE) {
        fd = (int)regs->ebx;
        p = process_current();
        if (!p || fd < 0 || fd >= MAX_FD) {
            regs->eax = -1;
            return;
        }

        p->user_fd_used[fd] = 0;
        regs->eax = 0;
        return;
    }

    if (regs->eax == SYS_MALLOC) {
        uint32_t size = regs->ebx;
        void *ptr = kmalloc(size);
        if (!ptr) {
            regs->eax = 0;
            return;
        }
        regs->eax = (uint32_t)ptr;
        return;
    }

    if (regs->eax == SYS_FREE) {
        void *ptr = (void *)regs->ebx;
        kfree(ptr);
        regs->eax = 0;
        return;
    }

    regs->eax = (uint32_t)-1;
}

void syscall_init(void) {
    isr_register_handler(0x80, syscall_handler);
}