; ------------------------------------------------------------
; void swtch(struct context **old, struct context *new);
;   调用约定 (cdecl)：[esp+4] = old, [esp+8] = new
;
; struct context {
;     uint edi;   // 偏移 0
;     uint esi;   // 偏移 4
;     uint ebx;   // 偏移 8
;     uint ebp;   // 偏移 12
;     uint eip;   // 偏移 16
; };
;
; 工作原理（与 xv6 完全一致）：
;   1. 把当前的 callee-saved 寄存器 (ebp, ebx, esi, edi) 压栈
;   2. 把 esp 存入 *old
;   3. 用 new 覆盖 esp  -> 栈切换完成
;   4. 从新栈上弹出 callee-saved 寄存器
;   5. ret 弹出 eip，跳转到新进程的执行位置
; ------------------------------------------------------------

[BITS 32]

section .text
global swtch

swtch:
    mov     eax, [esp + 4]      ; eax = old (struct context **)
    mov     edx, [esp + 8]      ; edx = new (struct context *)

    ; ---- 保存当前上下文 ----
    ; 顺序必须与 struct context 的内存布局"反向"对应：
    ;   先 push 的在高地址，最后 push 的在 esp 处
    ;   所以先 push ebp、最后 push edi  =>  edi 落在 [esp+0]
    push    ebp
    push    ebx
    push    esi
    push    edi

    mov     [eax], esp          ; *old = esp（保存旧栈指针）
    mov     esp, edx            ; esp = new（切换到新栈）

    ; ---- 恢复新上下文 ----
    pop     edi
    pop     esi
    pop     ebx
    pop     ebp

    ret                         ; 弹出 eip，跳入新进程
