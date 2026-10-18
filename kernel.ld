ENTRY(kernel_main)

SECTIONS
{
    . = 0x00280000;

    .text : {
        *(.text.start)
        *(.text)
        *(.userprog)         /* ★ 用户程序跟在内核代码后面，bin 不会变大 */
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data) }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}
