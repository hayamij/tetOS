#include "../kernel/types/types.h"

extern int write(const char *buf, uint32_t len);
extern void exit(void);

void _start(void) {
    const char *msg = "Hello from user mode!\n";
    int len = 0;
    while (msg[len]) len++;
    
    write(msg, len);
    exit();
}
