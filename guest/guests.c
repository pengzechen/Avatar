#include "guests.h"

// 定义 guest 配置数组
guest_config_t guest_configs[] = {
    {
        .image = {
            .bin_start = __guest_bin_start_0,
            .bin_end   = __guest_bin_end_0,
            .dtb_start = __guest_dtb_start_0,
            .dtb_end   = __guest_dtb_end_0,
            .fs_start  = __guest_fs_start_0,
            .fs_end    = __guest_fs_end_0,
        },
        .bin_loadaddr = VM0_BIN_LOADADDR,
        .dtb_loadaddr = VM0_DTB_LOADADDR,
        .fs_loadaddr  = VM0_FS_LOADADDR,
        .smp_num      = VM0_SMP_NUM,
    },
    {
        .image = {
            .bin_start = __guest_bin_start_1,
            .bin_end   = __guest_bin_end_1,
            .dtb_start = __guest_dtb_start_1,
            .dtb_end   = __guest_dtb_end_1,
            .fs_start  = __guest_fs_start_1,
            .fs_end    = __guest_fs_end_1,
        },
        .bin_loadaddr = VM1_BIN_LOADADDR,
        .dtb_loadaddr = VM1_DTB_LOADADDR,
        .fs_loadaddr  = VM1_FS_LOADADDR,
        .smp_num      = VM1_SMP_NUM,
    }
};
