/* link_vmm.ld */
ENTRY(_start)


SECTIONS
{
    /* 设置内核加载地址  */
    /* . = 0x70200000;   */
    . = 0x40080000;  
    PROVIDE(__kernal_start = .);
    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        INCLUDE "boot/users.lds"
        *(.data)
    }

    . = ALIGN((1<<10));
    __bss_start = .;
    .bss : {
        *(.bss.bitmap_buffer)
        *(.bss)
    }
    __bss_end = .;

    . = ALIGN((1 << 14));
    ept_L1 = .;
    PROVIDE(__heap_flag = .);
}
