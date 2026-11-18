; boot.asm
; ------------------------------------------------------------
; Linux 0.12 风格的引导扇区
;   - 无文件系统，按原始扇区读
;   - 把 setup 加载到 0x8000 后跳转
; ------------------------------------------------------------

[BITS 16]
    ORG 0x7C00

SETUP_SEG   EQU 0x8000      ; setup 加载地址（物理）
SETUP_START EQU 2           ; setup 起始扇区（1 基）
SETUP_LEN   EQU 8           ; setup 占用扇区数（4KB）

start:
    jmp     short init
    nop
    ; 注意：这里没有 BPB / FAT12 头，整个扇区都是我们的代码

init:
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00
    sti

    ; ---- 把 setup 从软盘读到 0x8000 ----
    mov     bx, SETUP_SEG
    mov     ah, 0x02
    mov     al, SETUP_LEN
    mov     ch, 0               ; 柱面 0
    mov     cl, SETUP_START     ; 起始扇区号
    mov     dh, 0               ; 磁头 0
    mov     dl, 0x80            ; A 驱动器
    int     0x13
    jc      disk_error

    ; ---- 跳转到 setup ----
    jmp     SETUP_SEG

disk_error:
    mov     si, err_msg
.print:
    lodsb
    or      al, al
    jz      .halt
    mov     ah, 0x0E
    mov     bx, 0x0007
    int     0x10
    jmp     .print
.halt:
    hlt
    jmp     .halt

err_msg:
    db      0x0D, 0x0A, "Boot sector: disk read error!", 0x0D, 0x0A, 0

    times   510 - ($ - $$) db 0
    dw      0xAA55
