[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000
KERNEL_SECTORS equ 6

start:
    mov [BOOT_DRIVE], dl
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    mov si, MSG_LOADING
    call print_string

    mov al, [BOOT_DRIVE]
    call print_hex_byte

    mov bx, KERNEL_OFFSET
    mov dh, KERNEL_SECTORS
    mov dl, [BOOT_DRIVE]
    call disk_load

    mov si, MSG_LOADED
    call print_string

    call switch_to_pm

    jmp $

disk_load:
    push bx
    push cx
    push dx
    
    mov ah, 0x00
    int 0x13
    jc .error
    
    pop dx
    pop cx
    pop bx
    
    push bx
    push cx
    push dx
    
    mov ah, 0x02
    mov al, dh
    mov ch, 0
    mov cl, 2
    mov dh, 0
    
    int 0x13
    jc .error
    
    pop dx
    pop cx
    pop bx
    ret

.error:
    mov di, ax
    
    pop dx
    pop cx  
    pop bx
    
    mov si, DISK_ERROR_MSG
    call print_string
    
    mov si, MSG_ERROR_CODE
    call print_string
    mov ax, di
    mov al, ah
    call print_hex_byte
    
    jmp $

print_string:
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

print_hex_byte:
    push ax
    mov ah, 0x0e
    mov al, '0'
    int 0x10
    mov al, 'x'
    int 0x10
    pop ax
    
    push ax
    shr al, 4
    call .print_nibble
    pop ax
    and al, 0x0F
    call .print_nibble
    
    mov al, ' '
    mov ah, 0x0e
    int 0x10
    ret

.print_nibble:
    cmp al, 10
    jl .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov ah, 0x0e
    int 0x10
    ret

DISK_ERROR_MSG: db 'Disk read error!', 0
MSG_ERROR_CODE: db ' Error code: ', 0
MSG_LOADING: db 'Loading tetOS...', 13, 10, 0
MSG_LOADED: db 'Kernel loaded!', 13, 10, 0
BOOT_DRIVE: db 0

gdt_start:
    dq 0x0

gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0

gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

[BITS 16]
switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    jmp CODE_SEG:init_pm

[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov ebp, 0x90000
    mov esp, ebp
    
    call KERNEL_OFFSET
    
    jmp $

times 510-($-$$) db 0
dw 0xAA55
