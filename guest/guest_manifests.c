#include "guest_manifest.h"

// Linux Guest配置
static const guest_manifest_t linux_manifest = {.type         = GUEST_TYPE_LINUX,
                                                .name         = "Linux",
                                                .bin_loadaddr = 0x70200000UL,
                                                .dtb_loadaddr = 0x70000000UL,
                                                .fs_loadaddr  = 0x78000000UL,
                                                .smp_num      = 1,
                                                .files = {.kernel_path  = "/GUESTS/LINUX/LINUX.BIN",
                                                          .dtb_path     = "/GUESTS/LINUX/LINUX.DTB",
                                                          .initrd_path  = "/GUESTS/LINUX/INITRD.GZ",
                                                          .needs_dtb    = true,
                                                          .needs_initrd = true}};

// NimbOS Guest配置
static const guest_manifest_t nimbos_manifest = {
    .type         = GUEST_TYPE_NIMBOS,
    .name         = "NimbOS",
    .bin_loadaddr = 0x50200000UL,
    .dtb_loadaddr = 0x50000000UL,  // 不使用，但保留
    .fs_loadaddr  = 0x58000000UL,  // 不使用，但保留
    .smp_num      = 1,
    .files        = {.kernel_path  = "/GUESTS/NIMBOS/NIMBOS.BIN",
                     .dtb_path     = NULL,
                     .initrd_path  = NULL,
                     .needs_dtb    = false,
                     .needs_initrd = false}};

// TestOS Guest配置
static const guest_manifest_t testos_manifest = {
    .type         = GUEST_TYPE_TESTOS,
    .name         = "TestOS",
    .bin_loadaddr = 0x60200000UL,
    .dtb_loadaddr = 0x60000000UL,  // 不使用，但保留
    .fs_loadaddr  = 0x68000000UL,  // 不使用，但保留
    .smp_num      = 1,
    .files        = {.kernel_path  = "/GUESTS/TESTOS/TESTOS.BIN",
                     .dtb_path     = NULL,
                     .initrd_path  = NULL,
                     .needs_dtb    = false,
                     .needs_initrd = false}};

// Guest配置数组
const guest_manifest_t guest_manifests[] = {linux_manifest, nimbos_manifest, testos_manifest};

const uint32_t guest_manifest_count = sizeof(guest_manifests) / sizeof(guest_manifests[0]);

// 辅助函数实现
const char *
guest_load_error_to_string(guest_load_error_t error)
{
    switch (error) {
        case GUEST_LOAD_SUCCESS:
            return "Success";
        case GUEST_LOAD_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case GUEST_LOAD_ERROR_KERNEL_LOAD_FAILED:
            return "Kernel load failed";
        case GUEST_LOAD_ERROR_DTB_LOAD_FAILED:
            return "DTB load failed";
        case GUEST_LOAD_ERROR_INITRD_LOAD_FAILED:
            return "Initrd load failed";
        case GUEST_LOAD_ERROR_NO_MEMORY:
            return "No memory available";
        case GUEST_LOAD_ERROR_FILE_SYSTEM_ERROR:
            return "File system error";
        default:
            return "Unknown error";
    }
}

const char *
guest_type_to_string(guest_type_t type)
{
    switch (type) {
        case GUEST_TYPE_LINUX:
            return "Linux";
        case GUEST_TYPE_NIMBOS:
            return "NimbOS";
        case GUEST_TYPE_TESTOS:
            return "TestOS";
        default:
            return "Unknown";
    }
}
