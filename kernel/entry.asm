[BITS 32]
[EXTERN kernel_main]
[EXTERN bss_start]
[EXTERN bss_end]

[GLOBAL _start]

_start:
    ; Kernel stack must stay outside .bss (backbuffer) and outside heap (0x400000+).
    ; Use the gap below heap start so graphics writes cannot corrupt return addresses.
    mov esp, 0x3FF000
    mov ebp, esp
    
    ; Zero out BSS section
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    
    call kernel_main
    jmp $
