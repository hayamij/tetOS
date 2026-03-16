#ifndef EXEC_H
#define EXEC_H

#include "../types/types.h"
#include "../process/process.h"

int exec_elf_from_disk(const char *filename);

#endif
