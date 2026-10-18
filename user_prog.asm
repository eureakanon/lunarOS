; user_prog.asm
; ------------------------------------------------------------
; 用户程序（Ring 3）
;   - 通过 int 0x80 调用 sys_write / sys_exit
;   - 位置无关：拷到 0x00400000 后可直接执行
; ------------------------------------------------------------

[BITS 32]
section .userprog

global user_prog_start
global user_prog_end

user_prog_start:
    jmp .real_start

.msg1:
    db "Hello from user mode!", 10
.msg1_len equ $ - .msg1

.real_start:
    ; ---- 计算 .msg1 的运行时地址 ----
    call .get_pc
.get_pc:
    pop esi
    lea ecx, [esi + (.msg1 - .get_pc)]

    ; ---- sys_write(1, msg1, len) ----
    mov eax, 4
    mov ebx, 1
    mov edx, .msg1_len
    int 0x80

    ; ---- 计算 .msg2 的地址 ----
    call .get_pc2
.get_pc2:
    pop esi
    lea ecx, [esi + (.msg2 - .get_pc2)]

    ; ---- sys_write(1, msg2, len) ----
    mov eax, 4
    mov ebx, 1
    mov edx, .msg2_len
    int 0x80

    ; ---- sys_exit(42) ----
    mov eax, 1
    mov ebx, 42
    int 0x80

.hang:
    jmp .hang

.msg2:
    db "And this is the second syscall!", 10
.msg2_len equ $ - .msg2

user_prog_end:
