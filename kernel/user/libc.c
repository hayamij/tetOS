#include "libc.h"
#include "../syscall/syscall.h"

static int syscall(int eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a" (ret)
        : "a" (eax), "b" (ebx), "c" (ecx), "d" (edx)
        : "memory"
    );
    return ret;
}

void write_char(char c) {
    write(&c, 1);
}

int write(const char *buf, uint32_t len) {
    return syscall(SYS_WRITE, (uint32_t)buf, len, 0);
}

int open(const char *filename) {
    return syscall(SYS_OPEN, (uint32_t)filename, 0, 0);
}

int read(int fd, char *buf, uint32_t len) {
    return syscall(SYS_READ, fd, (uint32_t)buf, len);
}

int close(int fd) {
    return syscall(SYS_CLOSE, fd, 0, 0);
}

void* malloc(uint32_t size) {
    return (void*)syscall(SYS_MALLOC, size, 0, 0);
}

void free(void *ptr) {
    syscall(SYS_FREE, (uint32_t)ptr, 0, 0);
}

void exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
}
