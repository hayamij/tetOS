[BITS 32]
[EXTERN kernel_main]
[EXTERN bss_start]
[EXTERN bss_end]

[GLOBAL _start]

_start:
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kernel_main
    jmp $
