#include "ipc.h"
#include "../process/process.h"
#include "../string/string.h"

void spin_lock_init(spinlock_t *lock) {
    lock->locked = 0;
}

void spin_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ __volatile__("pause");
    }
}

void spin_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}

void mutex_init(mutex_t *m) {
    spin_lock_init(&m->guard);
    m->locked = 0;
    m->owner_pid = 0;
    m->waiter_count = 0;
    memset(m->waiters, 0, sizeof(m->waiters));
}

void mutex_lock(mutex_t *m) {
    process_t *cur = process_current();
    uint32_t pid = cur ? cur->pid : 0;

    for (;;) {
        spin_lock(&m->guard);
        if (!m->locked) {
            m->locked = 1;
            m->owner_pid = pid;
            spin_unlock(&m->guard);
            return;
        }

        if (m->waiter_count < 16) {
            m->waiters[m->waiter_count++] = pid;
        }
        spin_unlock(&m->guard);
        __asm__ __volatile__("hlt");
    }
}

void mutex_unlock(mutex_t *m) {
    spin_lock(&m->guard);
    m->locked = 0;
    m->owner_pid = 0;
    if (m->waiter_count > 0) {
        for (uint32_t i = 1; i < m->waiter_count; i++) {
            m->waiters[i - 1] = m->waiters[i];
        }
        m->waiter_count--;
    }
    spin_unlock(&m->guard);
}

void pipe_create(pipe_t *p) {
    memset(p->buffer, 0, sizeof(p->buffer));
    p->read_idx = 0;
    p->write_idx = 0;
    p->size = 0;
    spin_lock_init(&p->lock);
}

int pipe_read(pipe_t *p, char *buf, uint32_t len) {
    if (!p || !buf) return -1;

    spin_lock(&p->lock);
    uint32_t n = 0;
    while (n < len && p->size > 0) {
        buf[n++] = p->buffer[p->read_idx];
        p->read_idx = (p->read_idx + 1) % sizeof(p->buffer);
        p->size--;
    }
    spin_unlock(&p->lock);
    return (int)n;
}

int pipe_write(pipe_t *p, const char *buf, uint32_t len) {
    if (!p || !buf) return -1;

    spin_lock(&p->lock);
    uint32_t n = 0;
    while (n < len && p->size < sizeof(p->buffer)) {
        p->buffer[p->write_idx] = buf[n++];
        p->write_idx = (p->write_idx + 1) % sizeof(p->buffer);
        p->size++;
    }
    spin_unlock(&p->lock);
    return (int)n;
}

int send_signal(uint32_t pid, uint32_t sig) {
    if (sig == SIGKILL) {
        return process_kill(pid);
    }
    return 0;
}

void handle_signal(uint32_t sig) {
    if (sig == SIGKILL) {
        process_exit();
    }
}
