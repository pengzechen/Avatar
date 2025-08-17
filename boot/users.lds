/* users.lds - Common user application and VM sections */

        . = ALIGN((1 << 10));
        /* VM 0 */
        __guest_bin_start_0 = .;
        *(.data.guest_bin.0)
        __guest_bin_end_0 = .;

        __guest_dtb_start_0 = .;
        *(.data.guest_dtb.0)
        __guest_dtb_end_0 = .;

        __guest_fs_start_0 = .;
        *(.data.guest_fs.0)
        __guest_fs_end_0 = .;

        /* VM 1 */
        __guest_bin_start_1 = .;
        *(.data.guest_bin.1)
        __guest_bin_end_1 = .;

        __guest_dtb_start_1 = .;
        *(.data.guest_dtb.1)
        __guest_dtb_end_1 = .;

        __guest_fs_start_1 = .;
        *(.data.guest_fs.1)
        __guest_fs_end_1 = .;
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
