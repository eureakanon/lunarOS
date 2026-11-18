; setup.asm
; ------------------------------------------------------------
; 实模式准备 + 进入保护模式 + 搬运 kernel
; 由 boot.asm 加载到 0x8000 执行（CS=0, IP=0x8000）
; ------------------------------------------------------------

[BITS 16]
    ORG 0x8000

; ---- 常量 ----
KERNEL_TMP      EQU 0x00010000      ; 实模式下 kernel 的临时位置
KERNEL_SEG      EQU 0x1000          ; 对应段值（0x1000:0000 = 0x10000）
KERNEL_START    EQU 10              ; kernel 起始扇区（1 基）
KERNEL_SECTORS  EQU 64              ; 读取的扇区数（32KB）
KERNEL_BASE     EQU 0x00280000      ; kernel 保护模式下的最终地址

entry:
    cli
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     sp, 0x7C00
    sti

    ; ---- 1. 读取 kernel 到 0x10000 ----
    mov     ax, KERNEL_SEG
    mov     es, ax
    xor     bx, bx
    mov     ah, 0x02
    mov     al, KERNEL_SECTORS
    mov     ch, 0
    mov     cl, KERNEL_START
    mov     dh, 0
    mov     dl, 0x80
    int     0x13
    jc      read_error

    ; ---- 2. 设置 80x25 彩色文本模式 ----
    mov     ax, 0x0003
    int     0x10

    ; ---- 3. 屏蔽 PIC ----
    mov     al, 0xFF
    out     0x21, al
    nop
    out     0xA1, al

    ; ---- 4. 打开 A20 地址线 ----
    call    wait_kbd
    mov     al, 0xD1
    out     0x64, al
    call    wait_kbd
    mov     al, 0xDF
    out     0x60, al
    call    wait_kbd

    ; ---- 5. 装载 GDT，进入保护模式 ----
    lgdt    [GDTR0]
    mov     eax, cr0
    and     eax, 0x7FFFFFFF         ; 关闭分页
    or      eax, 0x00000001         ; 打开保护模式 (PE)
    mov     cr0, eax
    jmp     DWORD 2*8:pipeline_flush

; ---- 实模式辅助过程 ----
wait_kbd:
    in      al, 0x64
    and     al, 0x02
    jnz     wait_kbd
    ret

read_error:
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
    db      0x0D, 0x0A, "Setup: failed to load kernel!", 0x0D, 0x0A, 0

; ============================================================
; 保护模式代码（32 位）
; ============================================================
[BITS 32]
pipeline_flush:
    mov     ax, 1*8
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    ; ---- 6. 把 kernel 从 0x10000 搬到 0x100000 ----
    mov     esi, KERNEL_TMP
    mov     edi, KERNEL_BASE
    mov     ecx, KERNEL_SECTORS * 512 / 4
    call    memcpy

    ; ---- 7. 设置栈并跳转到 kernel ----
    mov     esp, 0x0007FFFC
    jmp     DWORD 2*8:KERNEL_BASE

memcpy:
    mov     eax, [esi]
    add     esi, 4
    mov     [edi], eax
    add     edi, 4
    sub     ecx, 1
    jnz     memcpy
    ret

; ============================================================
; GDT
; ============================================================
    ALIGN 16
GDT0:
    times   8 db 0                  ; 空描述符（选择子 0x00）

    ; 数据段（选择子 0x08）：基址 0，限制 4GB，可读可写
    dw      0xFFFF
    dw      0x0000
    db      0x00
    db      0x92
    db      0xCF
    db      0x00

    ; 代码段（选择子 0x10）：基址 0，限制 4GB，可读可执行
    dw      0xFFFF
    dw      0x0000
    db      0x00
    db      0x9A
    db      0xCF
    db      0x00

GDTR0:
    dw      8*3 - 1
    dd      GDT0
