/* users.lds - Common user application sections */
/* Guest VM sections removed - now using dynamic loading from filesystem */

        . = ALIGN((1 << 10));

        /* Automatically generated lds file */
        __add_bin_start = .;
        *(.data.add_bin)
        __add_bin_end = .;
        . = ALIGN((1 << 10));

        __sub_bin_start = .;
        *(.data.sub_bin)
        __sub_bin_end = .;
        . = ALIGN((1 << 10));

        __testapp_bin_start = .;
        *(.data.testapp_bin)
        __testapp_bin_end = .;
        . = ALIGN((1 << 10));
