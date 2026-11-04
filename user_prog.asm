; user_prog.asm
[BITS 32]
section .userprog

global user_prog_start
global user_prog_end

user_prog_start:
    jmp .real

.msg_start:  db "[start] about to fork", 10
.msg_start_len equ $ - .msg_start

.msg_child:  db "[child]  hello from child", 10
.msg_child_len equ $ - .msg_child

.msg_parent: db "[parent] child has exited, done", 10
.msg_parent_len equ $ - .msg_parent

.real:
    ; ---- 打印 "[start]" ----
    call .get_pc1
.get_pc1:
    pop esi
    lea esi, [esi + (.msg_start - .get_pc1)]
    mov ecx, esi
    mov edx, .msg_start_len
    mov eax, 4
    mov ebx, 1
    int 0x80

    ; ---- fork ----
    mov eax, 2
    int 0x80
    test eax, eax
    jz .child

    ; ---- 父进程：wait ----
    mov eax, 3
    int 0x80

    ; ---- 打印 "[parent] ..." ----
    call .get_pc2
.get_pc2:
    pop esi
    lea esi, [esi + (.msg_parent - .get_pc2)]
    mov ecx, esi
    mov edx, .msg_parent_len
    mov eax, 4
    mov ebx, 1
    int 0x80

    jmp .exit0

.child:
    ; ---- 打印 "[child] ..." ----
    call .get_pc3
.get_pc3:
    pop esi
    lea esi, [esi + (.msg_child - .get_pc3)]
    mov ecx, esi
    mov edx, .msg_child_len
    mov eax, 4
    mov ebx, 1
    int 0x80

.exit0:
    mov eax, 1
    xor ebx, ebx
    int 0x80

.hang:
    jmp .hang

user_prog_end:
