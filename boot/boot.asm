[BITS 16]
[ORG 0x0600]            ; assembled for 0x0600; BIOS loads us at 0x7C00

KERNEL_OFFSET  equ 0x1000
KERNEL_SECTORS equ 100

; -------------------------------------------------------
; BIOS loads the MBR at 0x7C00.  We copy ourselves to
; 0x0600 first, so the kernel can load into 0x1000-0x8800
; without stomping on our code.  Real-mode stack lives at 0x9000:0xFFFC.
; -------------------------------------------------------
start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ax, 0x9000
    mov  ss, ax
    mov  sp, 0xFFFC

    ; Copy 512 bytes: 0x7C00 -> 0x0600
    mov  si, 0x7C00
    mov  di, 0x0600
    mov  cx, 256        ; 256 words = 512 bytes
    rep  movsw

    ; Far-jump into the relocated copy.
    ; ORG is 0x0600, so all labels already hold the right addresses.
    jmp  0x0000:relocated

relocated:
    sti
    mov  [BOOT_DRIVE], dl

    mov  si, MSG_LOADING
    call print_string

    mov  dh, KERNEL_SECTORS
    mov  dl, [BOOT_DRIVE]
    call disk_load

    mov  si, MSG_LOADED
    call print_string

    call switch_to_pm

    jmp $

; -------------------------------------------------------
; disk_load  --  INT 13h extended LBA read (AH=42h)
; IN:  dh = sector count, dl = drive number
; -------------------------------------------------------
disk_load:
    xor  ax, ax
    mov  al, dh
    mov  [.dap + 2], ax     ; write sector count word into DAP
    mov  ah, 0x42
    mov  si, .dap
    int  0x13
    jc   .error
    ret

.error:
    push ax
    mov  si, DISK_ERROR_MSG
    call print_string
    mov  si, MSG_ERROR_CODE
    call print_string
    pop  ax
    mov  al, ah
    call print_hex_byte
    jmp  $

; Disk Address Packet (16 bytes)
.dap:
    db 0x10             ; packet size
    db 0x00             ; reserved
    dw 0                ; sector count  <- filled at runtime
    dw KERNEL_OFFSET    ; destination offset  (0x1000)
    dw 0x0000           ; destination segment (0x0000)
    dd 1                ; LBA lo = 1  (sector right after MBR)
    dd 0                ; LBA hi = 0

; -------------------------------------------------------
; print_string  --  SI -> null-terminated string
; -------------------------------------------------------
print_string:
    mov  ah, 0x0e
.loop:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .loop
.done:
    ret

; -------------------------------------------------------
; print_hex_byte  --  AL -> "0xHH "
; -------------------------------------------------------
print_hex_byte:
    push ax
    mov  ah, 0x0e
    mov  al, '0'
    int  0x10
    mov  al, 'x'
    int  0x10
    pop  ax

    push ax
    shr  al, 4
    call .nibble
    pop  ax
    and  al, 0x0F
    call .nibble

    mov  al, ' '
    mov  ah, 0x0e
    int  0x10
    ret

.nibble:
    cmp  al, 10
    jl   .digit
    add  al, 'A' - 10
    jmp  .print
.digit:
    add  al, '0'
.print:
    mov  ah, 0x0e
    int  0x10
    ret

; -------------------------------------------------------
; Data
; -------------------------------------------------------
DISK_ERROR_MSG: db 'Disk read error!', 0
MSG_ERROR_CODE: db ' Err: ', 0
MSG_LOADING:    db 'Loading tetOS v2...', 13, 10, 0
MSG_LOADED:     db 'Kernel loaded!',   13, 10, 0
BOOT_DRIVE:     db 0

; -------------------------------------------------------
; GDT
; -------------------------------------------------------
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

; -------------------------------------------------------
; Protected-mode switch
; -------------------------------------------------------
[BITS 16]
switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov  eax, cr0
    or   eax, 0x1
    mov  cr0, eax
    jmp  CODE_SEG:init_pm

[BITS 32]
init_pm:
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  word [0xB8000], 0x0A50
    mov  ebp, 0x90000
    mov  esp, ebp
    call KERNEL_OFFSET
    jmp  $

times 510-($-$$) db 0
dw 0xAA55
