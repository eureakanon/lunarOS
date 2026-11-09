[BITS 32]
section .userprog

global user_prog_start
global user_prog_end

user_prog_start:
    mov eax, 2              ; fork
    int 0x80
    test eax, eax
    jz .child

    ; 父进程写栈页上的同一个虚拟地址
    mov dword [0x40002FF0], 'P'
    jmp .print

.child:
    mov dword [0x40002FF0], 'C'

.print:
    mov eax, 4              ; write(1, 0x40002FF0, 1)
    mov ebx, 1
    mov ecx, 0x40002FF0
    mov edx, 1
    int 0x80

    mov eax, 1              ; exit(0)
    xor ebx, ebx
    int 0x80

.hang:
    jmp .hang

user_prog_end:
