ENTRY(kernel_main)

SECTIONS
{
    . = 0x00280000;

    .text : {
        *(.text.start)
        *(.text)
        *(.userprog)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data) }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }

    _kernel_end = .;
}