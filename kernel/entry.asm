[BITS 32]
[EXTERN kernel_main]

[GLOBAL _start]

_start:
    call kernel_main
    jmp $
