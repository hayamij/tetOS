[BITS 32]
[EXTERN kernel_main]
[EXTERN bss_start]
[EXTERN bss_end]

[GLOBAL _start]

_start:
    ; Setup protected mode stack (before BSS zeroing which might affect memory)
    ; Use a safe location in high memory for kernel stack
    mov esp, 0x90000            ; Stack at 0x90000 = 576 KB (safe zone before kernel)
    mov ebp, esp
    
    ; Zero out BSS section
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    
    call kernel_main
    jmp $
