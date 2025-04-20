ENTRY(_start)



SECTIONS
{
    . = 0x40080000;  

    .text : {
        *(.text)
    }

    . = ALIGN((1<<10));
    .rodata : {
        *(.rodata)
    }

    . = ALIGN((1<<10));
    .data : {
        *(.data)
    }

    __bss_start = .;
    .bss : {
        stack_start = .;
        *(.bss.kstack)    
        . = ALIGN(1 << 14);  
        stack_end = .;
        *(.bss)         
    }
    __bss_end = .;

    . = ALIGN((1 << 10));

    _end = .;
}
