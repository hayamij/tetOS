[BITS 16]
[ORG 0x0000]

; -------------------------------------------------------
; Stage-2 bootloader. Loaded by stage1 at CS=0x2000, IP=0x0000
; (linear 0x20000). Safely above 0x1000..~0xE000 where the kernel
; will land.
;
; Responsibilities:
;   1. Pick a VBE mode (1280x720x32 preferred, fallback 1024x768).
;      Mode info block is copied to linear 0x00000500 for the kernel.
;   2. Load kernel binary from LBA 17 to linear 0x1000 using INT 13h
;      LBA reads in 64-sector chunks.
;   3. Switch to protected mode and jmp into the kernel entry.
; -------------------------------------------------------

STAGE2_LINEAR     equ 0x20000
KERNEL_OFFSET     equ 0x1000
KERNEL_LBA        equ 17
; Max kernel space must stay below STAGE2_LINEAR (0x20000). Kernel starts at
; 0x1000, so we can safely load up to (0x20000 - 0x1000) / 512 = 0xF8 (248)
; sectors. Use 240 (120 KB) to leave a small guard band.
KERNEL_TOTAL      equ 240
CHUNK_SECTORS     equ 32               ; 32 sectors = 16 KB per chunk

VBE_INFO_OFF      equ 0x0800           ; absolute offset in seg 0
VBE_TMP_MODE_OFF  equ 0x0C00
VBE_MODE_INFO_OFF equ 0x0500

PREF_WIDTH        equ 1280
PREF_HEIGHT       equ 720
FALLBACK_WIDTH    equ 1024
FALLBACK_HEIGHT   equ 768

; -------------------------------------------------------
; Entry point. CS=0x2000, IP=0, DL=boot drive.
; -------------------------------------------------------
stage2_start:
    cli
    mov  ax, cs
    mov  ds, ax
    mov  ax, 0x9000
    mov  ss, ax
    mov  sp, 0xFFFC
    sti

    mov  [BOOT_DRIVE], dl

    mov  si, MSG_STAGE2
    call print_string

    call load_kernel
    mov  si, MSG_LOADED
    call print_string

    call vbe_select_mode
    mov  si, MSG_VBE_OK
    call print_string

    call switch_to_pm
    jmp  $

; -------------------------------------------------------
; load_kernel -- chunked LBA read of KERNEL_TOTAL sectors into 0x1000
; Destination tracked via DAP segment:offset. Start segment:offset
; = 0x0000:0x1000. Every 64 sectors advances offset by 0x8000.
; -------------------------------------------------------
load_kernel:
    pusha
    ; Destination expressed as segment:offset. Start at linear 0x1000 using
    ; segment 0x0100:offset 0x0000 (0x0100<<4 = 0x1000). Each chunk of N
    ; sectors advances segment by N*32 (since one segment unit = 16 bytes,
    ; so 512 bytes per sector = 32 segment units). Offset stays at 0 so
    ; there is no 64 KB wrap bug.
    mov  word [DAP_COUNT],    0
    mov  word [DAP_OFFSET],   0
    mov  word [DAP_SEGMENT],  (KERNEL_OFFSET >> 4)
    mov  dword [DAP_LBA_LO],  KERNEL_LBA
    mov  dword [DAP_LBA_HI],  0

    mov  cx, KERNEL_TOTAL
.loop:
    test cx, cx
    je   .done

    mov  ax, CHUNK_SECTORS
    cmp  ax, cx
    jbe  .chunk_ok
    mov  ax, cx
.chunk_ok:
    mov  [DAP_COUNT], ax

    mov  ah, 0x42
    mov  dl, [BOOT_DRIVE]
    mov  si, DAP
    int  0x13
    jc   .error

    mov  ax, [DAP_COUNT]
    sub  cx, ax

    ; Advance destination segment by ax * 32 (512 bytes / 16 per seg unit).
    mov  bx, ax
    shl  bx, 5
    add  [DAP_SEGMENT], bx

    movzx eax, word [DAP_COUNT]
    add  [DAP_LBA_LO], eax
    jmp  .loop

.done:
    popa
    ret

.error:
    mov  si, DISK_ERR_MSG
    call print_string
    jmp  $

; -------------------------------------------------------
; vbe_select_mode -- enumerate VBE modes, pick best
; -------------------------------------------------------
vbe_select_mode:
    pusha
    push es

    ; Write VBE2 signature + call 4F00h
    xor  ax, ax
    mov  es, ax
    mov  di, VBE_INFO_OFF
    mov  byte [es:di + 0], 'V'
    mov  byte [es:di + 1], 'B'
    mov  byte [es:di + 2], 'E'
    mov  byte [es:di + 3], '2'
    mov  ax, 0x4F00
    int  0x10
    cmp  ax, 0x004F
    jne  .fail

    mov  ax, [es:di + 0x0E]          ; video_modes offset
    mov  dx, [es:di + 0x10]          ; video_modes segment
    mov  [VBE_MODES_OFF], ax
    mov  [VBE_MODES_SEG], dx

    mov  word [BEST_MODE],     0xFFFF
    mov  word [FALLBACK_MODE], 0xFFFF
    mov  word [ANY_MODE],      0xFFFF

.enum_loop:
    mov  ax, [VBE_MODES_SEG]
    mov  es, ax
    mov  bx, [VBE_MODES_OFF]
    mov  cx, [es:bx]
    cmp  cx, 0xFFFF
    je   .enum_done
    add  word [VBE_MODES_OFF], 2

    push cx
    xor  ax, ax
    mov  es, ax
    mov  di, VBE_TMP_MODE_OFF
    mov  ax, 0x4F01
    int  0x10
    pop  cx
    cmp  ax, 0x004F
    jne  .enum_loop

    xor  ax, ax
    mov  es, ax
    mov  di, VBE_TMP_MODE_OFF

    mov  ax, [es:di + 0]             ; mode attributes
    test ax, 0x01
    jz   .enum_loop
    test ax, 0x10
    jz   .enum_loop
    test ax, 0x80
    jz   .enum_loop

    mov  al, [es:di + 25]            ; bpp
    cmp  al, 24
    je   .bpp_ok
    cmp  al, 32
    je   .bpp_ok
    jmp  .enum_loop
.bpp_ok:

    mov  ax, [es:di + 18]            ; xres
    mov  bx, [es:di + 20]            ; yres

    cmp  word [ANY_MODE], 0xFFFF
    jne  .check_pref
    mov  [ANY_MODE], cx

.check_pref:
    cmp  ax, PREF_WIDTH
    jne  .check_fallback
    cmp  bx, PREF_HEIGHT
    jne  .check_fallback
    mov  [BEST_MODE], cx
    jmp  .enum_loop

.check_fallback:
    cmp  ax, FALLBACK_WIDTH
    jne  .enum_loop
    cmp  bx, FALLBACK_HEIGHT
    jne  .enum_loop
    mov  [FALLBACK_MODE], cx
    jmp  .enum_loop

.enum_done:
    mov  cx, [BEST_MODE]
    cmp  cx, 0xFFFF
    jne  .have_mode
    mov  cx, [FALLBACK_MODE]
    cmp  cx, 0xFFFF
    jne  .have_mode
    mov  cx, [ANY_MODE]
    cmp  cx, 0xFFFF
    je   .fail

.have_mode:
    mov  [SELECTED_MODE], cx

    ; Final ModeInfoBlock into 0:VBE_MODE_INFO_OFF
    xor  ax, ax
    mov  es, ax
    mov  di, VBE_MODE_INFO_OFF
    mov  ax, 0x4F01
    int  0x10
    cmp  ax, 0x004F
    jne  .fail

    ; Activate mode with LFB (bit 14)
    mov  ax, 0x4F02
    mov  bx, [SELECTED_MODE]
    or   bx, 0x4000
    int  0x10
    cmp  ax, 0x004F
    jne  .fail

    pop  es
    popa
    ret

.fail:
    pop  es
    popa
    mov  si, MSG_VBE_FAIL
    call print_string
    jmp  $

; -------------------------------------------------------
; print_string
; -------------------------------------------------------
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

; -------------------------------------------------------
; Data
; -------------------------------------------------------
DAP:
    db 0x10
    db 0x00
DAP_COUNT:      dw 0
DAP_OFFSET:     dw 0
DAP_SEGMENT:    dw 0
DAP_LBA_LO:     dd 0
DAP_LBA_HI:     dd 0

VBE_MODES_OFF:   dw 0
VBE_MODES_SEG:   dw 0
BEST_MODE:       dw 0xFFFF
FALLBACK_MODE:   dw 0xFFFF
ANY_MODE:        dw 0xFFFF
SELECTED_MODE:   dw 0xFFFF

BOOT_DRIVE:    db 0
MSG_STAGE2:    db 'tetOS stage2, loading kernel...', 13, 10, 0
MSG_LOADED:    db 'Kernel loaded. Selecting VBE mode...', 13, 10, 0
MSG_VBE_OK:    db 'VBE mode set. Entering protected mode.', 13, 10, 0
DISK_ERR_MSG:  db 'stage2 disk read error', 13, 10, 0
MSG_VBE_FAIL:  db 'VBE mode select failed', 13, 10, 0

; -------------------------------------------------------
; GDT -- base addresses are 0 (flat). Mode switch below uses a
; 32-bit far jump so we can reach the kernel at linear 0x1000.
; -------------------------------------------------------
align 8
gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + STAGE2_LINEAR        ; linear base of the GDT

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; -------------------------------------------------------
; switch_to_pm -- enable PE, far jmp (32-bit) to flat init_pm
; -------------------------------------------------------
[BITS 16]
switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov  eax, cr0
    or   eax, 0x1
    mov  cr0, eax
    jmp  dword CODE_SEG:(init_pm + STAGE2_LINEAR)

[BITS 32]
init_pm:
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ebp, 0x90000
    mov  esp, ebp
    ; Use an indirect call so the target is absolute. A plain
    ;   call KERNEL_OFFSET
    ; would be encoded as a PC-relative call; since the linker placed this
    ; code at virtual offset 0 but it actually runs at linear 0x20000, the
    ; relative call would land at KERNEL_OFFSET + 0x20000 instead of the
    ; kernel entry at linear 0x1000.
    mov  eax, KERNEL_OFFSET
    call eax
    jmp  $

; pad stage2 to exactly 16 * 512 = 8192 bytes
times (16*512)-($-$$) db 0
