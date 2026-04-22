[BITS 16]
[ORG 0x7C00]

; -------------------------------------------------------
; Stage-1 MBR (512 B). Its job: load stage-2 from LBA 1
; (16 sectors = 8 KB) into segment 0x2000 (physical 0x20000) and
; jump there. That keeps stage-2 far above the 0x1000..0xE000 area
; that the kernel will later occupy.
; -------------------------------------------------------

STAGE2_LBA        equ 1
STAGE2_SECTORS    equ 16
STAGE2_SEG        equ 0x2000           ; physical 0x20000
STAGE2_OFF        equ 0x0000

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti

    mov  [BOOT_DRIVE], dl

    mov  si, MSG_STAGE1
    call print_string

    mov  ah, 0x42
    mov  dl, [BOOT_DRIVE]
    mov  si, DAP
    int  0x13
    jc   .error

    ; Hand control to stage2 in far form.
    jmp  STAGE2_SEG:STAGE2_OFF

.error:
    mov  si, MSG_DISK_ERR
    call print_string
    jmp  $

print_string:
    mov  ah, 0x0e
.ps_loop:
    lodsb
    test al, al
    jz   .ps_done
    int  0x10
    jmp  .ps_loop
.ps_done:
    ret

DAP:
    db 0x10
    db 0x00
    dw STAGE2_SECTORS
    dw STAGE2_OFF
    dw STAGE2_SEG
    dd STAGE2_LBA
    dd 0

BOOT_DRIVE:    db 0
MSG_STAGE1:    db 'tetOS stage1', 13, 10, 0
MSG_DISK_ERR:  db 'stage1 disk error', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
