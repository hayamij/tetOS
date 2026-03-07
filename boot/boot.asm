[BITS 16]
[ORG 0x7C00]

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x00
    mov al, 0x03
    int 0x10

    mov si, msg_hello
    call print_string

    mov si, msg_teto
    call print_string

    jmp $

print_string:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x0F
    int 0x10
    jmp .loop
.done:
    popa
    ret

msg_hello:
    db 13, 10, 10
    db "    === tetOS v0.0.1 ===", 13, 10
    db "     Hello from tetOS!", 13, 10, 10, 0

msg_teto:
    db "          .-.          ", 13, 10
    db "     .--/      \--.     ", 13, 10
    db "  ( ( ) | ^  ^ | ( ( )   ", 13, 10  
    db "  ( ~ ) |   v  | ( ~ )   ", 13, 10
    db "     (@)\______/(@) ", 13, 10
    db 13, 10
    db " Booting complete. System halted.", 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
