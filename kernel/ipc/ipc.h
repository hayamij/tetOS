#ifndef IPC_H
#define IPC_H

#include "../types/types.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

typedef struct {
    spinlock_t guard;
    volatile uint32_t locked;
    uint32_t owner_pid;
    uint32_t waiters[16];
    uint32_t waiter_count;
} mutex_t;

typedef struct {
    char buffer[256];
    uint32_t read_idx;
    uint32_t write_idx;
    uint32_t size;
    spinlock_t lock;
} pipe_t;

void spin_lock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

void pipe_create(pipe_t *p);
int pipe_read(pipe_t *p, char *buf, uint32_t len);
int pipe_write(pipe_t *p, const char *buf, uint32_t len);

int send_signal(uint32_t pid, uint32_t sig);
void handle_signal(uint32_t sig);

#define SIGKILL 9

#endif
