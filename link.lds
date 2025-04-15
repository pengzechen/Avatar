/* link.ld */
ENTRY(_start)



SECTIONS
{
    /* 设置内核加载地址  */
    /* . = 0x70200000;   */
    . = 0xffff000040080000;  

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        . = ALIGN((1 << 10));
        __guset_bin_start = .;
        *(.data.guset_bin)
        __guset_bin_end = .;
        . = ALIGN((1 << 10));

        __guset_dtb_start = .;
        *(.data.guest_dtb)
        __guset_dtb_end = .;
        . = ALIGN((1 << 10));

        __guset_fs_start = .;
        *(.data.guest_fs)
        __guset_fs_end = .;
        . = ALIGN((1 << 10));

        /* Automatically generated lds file */
        __shell_bin_start = .;
        *(.data.shell_bin)
        __shell_bin_end = .;
        . = ALIGN((1 << 10));

        __testapp_bin_start = .;
        *(.data.testapp_bin)
        __testapp_bin_end = .;
        . = ALIGN((1 << 10));

        /*  bitmap buffer   */
        /* ================ */
        __bitmap_buffer_start = .;     
        *(.data.bitmap_buffer)        
        __bitmap_buffer_end = .;      
        . = ALIGN((1 << 10));

        *(.data)
    }

    . = ALIGN((1<<10));
    __bss_start = .;
    .bss : {
        *(.bss)
    }
    __bss_end = .;

    . = ALIGN((1 << 14));
    ept_L1 = .;
    __heap_flag = .;
}
