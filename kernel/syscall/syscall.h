#ifndef SYSCALL_H
#define SYSCALL_H

#include "../types/types.h"

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_FORK   2
#define SYS_EXEC   3
#define SYS_GETPID 7
#define SYS_OPEN   8
#define SYS_READ   9
#define SYS_CLOSE  10
#define SYS_MALLOC 11
#define SYS_FREE   12

void syscall_init(void);

#endif