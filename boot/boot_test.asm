[BITS 16]
[ORG 0x7C00]

start:
    mov  ax, 0x0003         ; Set text mode 80x25
    int  0x10

    mov  ah, 0x0E           ; Print character BIOS call
    mov  al, 'H'            
    int  0x10
    
    mov  al, 'I'
    int  0x10

    jmp $                   ; Infinite loop

times 510-($-$$) db 0
dw 0xAA55
