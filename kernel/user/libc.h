#ifndef LIBC_H
#define LIBC_H

#include "../types/types.h"

void write_char(char c);
int write(const char *buf, uint32_t len);
int open(const char *filename);
int read(int fd, char *buf, uint32_t len);
int close(int fd);
void* malloc(uint32_t size);
void free(void *ptr);
void exit(void);

#endif
