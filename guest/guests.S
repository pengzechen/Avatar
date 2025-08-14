
/* VM 0 BIN（强定义，必须存在） */
.section .data.guest_bin.0
    // .incbin "../guest/testos/build/guest0_kernel.bin"
    .incbin "../guest/linux/linux.bin"

/* VM 0 DTB（弱定义，可选） */
.section .data.guest_dtb.0, "a"
    .weak __guest_dtb_start_0
    .weak __guest_dtb_end_0
    .incbin "../guest/linux/linux.dtb"
__guest_dtb_start_0:
__guest_dtb_end_0:

/* VM 0 FS（弱定义，可选） */
.section .data.guest_fs.0, "a"
    .weak __guest_fs_start_0
    .weak __guest_fs_end_0
    .incbin "../guest/linux/initrd.gz"
__guest_fs_start_0:
__guest_fs_end_0:

/* VM 1 BIN（强定义，必须存在） */
.section .data.guest_bin.1
    // .incbin "../guest/testos/build/guest1_kernel.bin"
    .incbin "../guest/nimbos/kernel/target/aarch64/debug/nimbos.bin"

/* VM 1 DTB（弱定义，可选） */
.section .data.guest_dtb.1, "a"
    //.incbin "../guest/vm1/kernel.dtb"
    .weak __guest_dtb_start_1
    .weak __guest_dtb_end_1
__guest_dtb_start_1:
__guest_dtb_end_1:

/* VM 1 FS（弱定义，可选） */
.section .data.guest_fs.1, "a"
    //.incbin "../guest/vm1/fs.img"
    .weak __guest_fs_start_1
    .weak __guest_fs_end_1
__guest_fs_start_1:
__guest_fs_end_1:
