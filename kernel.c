#include "types.h"
#include "output.h"
#include "desctbl.h"
#include "int.h"
#include "ioctl.h"
#include "keyboard.h"
#include "timer.h"
#include "tss.h"
#include "pmm.h"
#include "paging.h"
#include "proc.h"
#include "fs.h"
extern char __bss_start[], __bss_end[], _kernel_end[];
extern char _binary_user_hello_elf_start[];
extern char _binary_user_hello_elf_end[];

static void install_demo_elf(void)
{
    int size = (int)(_binary_user_hello_elf_end - _binary_user_hello_elf_start);
    if (size <= 0) return;

    /* 已存在就跳过 */
    int fd = fs_open("/hello.elf");
    if (fd >= 0) { fs_close(fd); return; }

    if (fs_create("/hello.elf",FS_TYPE_FILE) < 0) {
        puts("[install] create failed\n");
        return;
    }
    fd = fs_open("/hello.elf");
    if (fd < 0) return;
    int wrote=fs_write(fd, _binary_user_hello_elf_start, (uint32)size);
    fs_close(fd);
    if(wrote!=size)
    {
        puts("[install]short write: ");
        put_dec(wrote);
        puts("/");
        put_dec(size);
        putchar('\n');
        return;
    }
    puts("[install] /hello.elf = ");
    put_dec(size); puts(" bytes\n");
}
extern void shell_main(void);
static void shell_with_install(void)
{
    install_demo_elf();   // 进程上下文里做安装
    shell_main();         // 然后进入真正的 shell 循环
}
#define PHYS_END   0x02000000
/* 建立一个新的用户进程的地址空间并加载用户程序 */

__attribute__((section(".text.start")))
void kernel_main(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;
    __asm__ volatile ("movl $0x0007FFFC, %esp");

    clear_screen();
    puts("Simple OS v0.8 - fork & wait\n");
    puts("============================\n\n");

    init_gdtidt();
    init_pic();
    init_keyboard();
    init_timer();
    pmm_init((uint32)_kernel_end, PHYS_END);
    paging_init();
    fs_init();
    /* 演示期间关掉定时器抢占，方便观察 fork/wait 的语义 */
    io_out8(0x21, 0xfc);

    io_sti();
    proc_init();
    struct proc *sh = proc_alloc();
    sh->pid = 0;
    next_pid = 1;

    /* shell 跑在 kernel_pgd 里（内核线程不需要独立地址空间，
     * 但如果要 shell 也能 fork 用户进程，必须给它一个 pgd） */
    uint32 *shell_pgd = paging_new_proc_pgd();
    proc_start(sh, shell_with_install, shell_pgd);

    puts("Starting scheduler...\n\n");
    scheduler();
    for (;;) __asm__ volatile ("hlt");
}
