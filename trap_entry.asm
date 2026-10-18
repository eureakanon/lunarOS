; ------------------------------------------------------------
; trap_entry.asm
; 所有中断/异常的统一入口
; 用法：C 里把 isr_stub_table[i] 填到 IDT 的第 i 项
; ------------------------------------------------------------

[BITS 32]

section .text

extern trap

; ============================================================
; 为每个中断号生成 stub
;   - 有错误码的异常：CPU 已经压入 errcode，只 push trapno
;   - 无错误码：先 push 一个 dummy 0，再 push trapno
; ============================================================
%assign i 0
%rep 256
global isr%+i
isr%+i:
%if (i = 8) || (i = 10) || (i = 11) || (i = 12) || (i = 13) || (i = 14) || (i = 17)
    push dword i            ; CPU 已压 errcode
%else
    push dword 0            ; dummy errcode
    push dword i            ; trapno
%endif
    jmp alltraps
%assign i i+1
%endrep

; ============================================================
; 通用入口：构建完整 trapframe 后调用 C 的 trap()
; ============================================================
alltraps:
    pusha
    ; 1. 保存段寄存器
    push ds
    push es
    push fs
    push gs

    ; 2. 切到内核数据段（选择子 0x08）
    mov ax, 0x08
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 3. 把 trapframe 指针（= 当前 esp）传给 trap(tf)
    push esp
    call trap
    add esp, 4

    ; 4. 恢复段寄存器
    pop gs
    pop fs
    pop es
    pop ds
    popa
    ; 5. 跳过 trapno 和 errcode
    add esp, 8

    ; 6. 中断返回
    iret

; ============================================================
; 中断向量表（方便 C 里循环注册）
; ============================================================
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dd isr%+i
%assign i i+1
%endrep

; ============================================================
; GDT/IDT 加载辅助函数
; ============================================================
section .text
global load_gdtr
global load_idtr

; void load_gdtr(int limit, int addr);
;   利用栈上空间临时构造 6 字节的 GDTR 结构：
;     [esp+6..7] = limit 低 16 位
;     [esp+8..11] = addr
load_gdtr:
    mov ax, [esp+4]
    mov [esp+6], ax
    lgdt [esp+6]
    ret

; void load_idtr(int limit, int addr);
load_idtr:
    mov ax, [esp+4]
    mov [esp+6], ax
    lidt [esp+6]
    ret

global load_tr

load_tr:
    mov ax,[esp+4]
    ltr ax
    ret

global enter_user_mode
enter_user_mode:
    mov ecx, [esp + 4]         ; user_eip
    mov edx, [esp + 8]         ; user_esp

    ; ★ 关键：必须先把 ds/es/fs/gs 换成用户数据段 ★
    ; 因为 iret 到 Ring 3 后，段寄存器不会自动变，
    ; 而用 DPL=0 的段在 CPL=3 下访问数据会 GP
    mov ax, 0x1B               ; 用户数据段 | RPL=3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 构造 iret 帧（从 Ring 0 -> Ring 3，iret 弹出 5 个值）
    push 0x1B                  ; ss   = 用户数据 | RPL=3
    push edx                   ; esp  = 用户栈
    pushfd                     ; 取当前 eflags
    pop eax
    or  eax, 0x200             ; 确保 IF=1（用户态也能收中断）
    push eax                   ; eflags
    push 0x23                  ; cs   = 用户代码 | RPL=3
    push ecx                   ; eip  = 用户程序入口
    iret                       ; ★ 从此不再回返 ★

