
#ifndef _H_GUETSTS_H
#define _H_GUETSTS_H

#include "aj_types.h"
#include "vmm/vmm_cfg.h"

typedef struct {
    void *bin_start;
    void *bin_end;
    void *dtb_start;
    void *dtb_end;
    void *fs_start;
    void *fs_end;
} guest_image_t;

typedef struct {
    guest_image_t image;      // 原有的 guest_image_t 结构体

    uint64_t bin_loadaddr;   // bin 加载地址
    uint64_t dtb_loadaddr;   // dtb 加载地址
    uint64_t fs_loadaddr;    // fs 加载地址

    int32_t smp_num;              // 虚拟 CPU 数量
} guest_config_t;


extern char __guest_bin_start_0[], __guest_bin_end_0[];
extern char __guest_dtb_start_0[], __guest_dtb_end_0[];
extern char __guest_fs_start_0[], __guest_fs_end_0[];

extern char __guest_bin_start_1[], __guest_bin_end_1[];
extern char __guest_dtb_start_1[], __guest_dtb_end_1[];
extern char __guest_fs_start_1[], __guest_fs_end_1[];

#define VM0_BIN_LOADADDR  0x70200000UL
#define VM0_DTB_LOADADDR  0x70000000UL
#define VM0_FS_LOADADDR   0x78000000UL
#define VM0_SMP_NUM       1

#define VM1_BIN_LOADADDR  0x50200000UL
#define VM1_DTB_LOADADDR  0x50000000UL
#define VM1_FS_LOADADDR   0x58000000UL
#define VM1_SMP_NUM       1


// 声明数组，实际定义在 guest/guests.c 中
extern guest_config_t guest_configs[];

#endif // _H_GUETSTS_H