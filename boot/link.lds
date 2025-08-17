/* link.ld */
ENTRY(_start)



SECTIONS
{
    /* 设置内核加载地址  */
    /* . = 0x70200000;   */
    . = 0xffff000040080000;  
    PROVIDE(__kernal_start = .);
    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        INCLUDE "boot/users.lds"

        . = ALIGN(64);
        __virtio_start = .;
        . += 0x1000000; /* 16MB VirtIO */
        __virtio_end = .;

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
