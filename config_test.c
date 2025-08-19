#include "os_cfg.h"
#include "vmm/vmm_cfg.h"
#include "guest/guest_manifest.h"
#include "io.h"

// 外部声明
extern const guest_manifest_t guest_manifests[];
extern const uint32_t         guest_manifest_count;
extern const char *
guest_type_to_string(guest_type_t type);

/**
 * 配置验证和测试函数
 * 用于验证所有配置参数的合理性和一致性
 */

void
validate_timer_config(void)
{
    logger("=== Timer Configuration Validation ===\n");
    logger("Timer Frequency: %d Hz\n", TIMER_FREQUENCY_HZ);
    logger("Timer Interval: %d ms\n", TIMER_TICK_INTERVAL_MS);
    logger("Timer TVAL Value: %d\n", TIMER_TVAL_VALUE);

    // 验证定时器配置的合理性
    if (TIMER_TICK_INTERVAL_MS < 1 || TIMER_TICK_INTERVAL_MS > 1000) {
        logger("WARNING: Timer interval %d ms may be too extreme\n", TIMER_TICK_INTERVAL_MS);
    }

    // 验证计算是否正确
    uint32_t expected_tval = TIMER_FREQUENCY_HZ * TIMER_TICK_INTERVAL_MS / 1000;
    if (TIMER_TVAL_VALUE != expected_tval) {
        logger("ERROR: Timer TVAL calculation mismatch! Expected: %d, Got: %d\n",
               expected_tval,
               TIMER_TVAL_VALUE);
    } else {
        logger("Timer TVAL calculation: CORRECT\n");
    }
}

void
validate_memory_config(void)
{
    logger("=== Memory Configuration Validation ===\n");
    logger("Kernel RAM Start: 0x%lx\n", KERNEL_RAM_START);
    logger("Kernel RAM Size: 0x%lx (%lu MB)\n", KERNEL_RAM_SIZE, KERNEL_RAM_SIZE / (1024 * 1024));
    logger("Kernel RAM End: 0x%lx\n", KERNEL_RAM_END);
    logger("Page Size: %d bytes\n", PAGE_SIZE);
    logger("Heap Offset: 0x%lx (%lu MB)\n", HEAP_OFFSET, HEAP_OFFSET / (1024 * 1024));

    // 验证内存配置的合理性
    if (KERNEL_RAM_SIZE < 64 * 1024 * 1024) {  // 64MB
        logger("WARNING: Kernel RAM size may be too small\n");
    }

    if (PAGE_SIZE != 4096) {
        logger("WARNING: Non-standard page size detected\n");
    }

    // 验证地址对齐
    if (KERNEL_RAM_START % PAGE_SIZE != 0) {
        logger("ERROR: Kernel RAM start address not page-aligned\n");
    }
}

void
validate_smp_config(void)
{
    logger("=== SMP Configuration Validation ===\n");
    logger("SMP Number: %d\n", SMP_NUM);
    logger("Stack Size: %d KB\n", STACK_SIZE / 1024);
    logger("Secondary CPU Count: %d\n", SECONDARY_CPU_COUNT);
    logger("CPU Startup Wait Loops: %d\n", CPU_STARTUP_WAIT_LOOPS);

    // 验证SMP配置的合理性
    if (SMP_NUM < 1 || SMP_NUM > 8) {
        logger("WARNING: SMP_NUM %d may be out of reasonable range\n", SMP_NUM);
    }

    if (STACK_SIZE < 8192) {  // 8KB
        logger("WARNING: Stack size may be too small\n");
    }
}

void
validate_vm_config(void)
{
    logger("=== VM Configuration Validation ===\n");
    logger("Max VMs: %d\n", VM_NUM_MAX);
    logger("Max VCPUs: %d\n", VCPU_NUM_MAX);
    logger("VM Stack Size: %d KB\n", VM_STACK_SIZE / 1024);

    // 显示可用的guest配置
    logger("Available Guest Manifests: %u\n", guest_manifest_count);
    for (uint32_t i = 0; i < guest_manifest_count; i++) {
        const guest_manifest_t *manifest = &guest_manifests[i];
        logger("  Guest %u: %s (%s)\n", i, manifest->name, guest_type_to_string(manifest->type));
        logger("    Binary Load Address: 0x%lx\n", manifest->bin_loadaddr);
        logger("    DTB Load Address: 0x%lx\n", manifest->dtb_loadaddr);
        logger("    FS Load Address: 0x%lx\n", manifest->fs_loadaddr);
        logger("    SMP Number: %u\n", manifest->smp_num);
    }

    // 验证VM配置的合理性
    if (VM_NUM_MAX > 8) {
        logger("WARNING: Too many VMs may impact performance\n");
    }

    if (VCPU_NUM_MAX > 16) {
        logger("WARNING: Too many VCPUs may impact performance\n");
    }

    // 验证guest配置地址不重叠
    for (uint32_t i = 0; i < guest_manifest_count; i++) {
        const guest_manifest_t *manifest = &guest_manifests[i];

        // 检查DTB和Binary地址是否重叠（假设Binary最大16MB）
        if (manifest->dtb_loadaddr >= manifest->bin_loadaddr &&
            manifest->dtb_loadaddr < manifest->bin_loadaddr + 0x1000000) {
            logger("WARNING: Guest %s DTB and Binary addresses may overlap\n", manifest->name);
        }

        // 检查FS和Binary地址是否重叠
        if (manifest->fs_loadaddr >= manifest->bin_loadaddr &&
            manifest->fs_loadaddr < manifest->bin_loadaddr + 0x1000000) {
            logger("WARNING: Guest %s FS and Binary addresses may overlap\n", manifest->name);
        }
    }
}

void
validate_device_config(void)
{
    logger("=== Device Configuration Validation ===\n");
    logger("UART0 Base: 0x%lx\n", UART0_BASE_ADDR);
    logger("GICD Base: 0x%lx\n", GICD_BASE_ADDR);
    logger("GICC Base: 0x%lx\n", GICC_BASE_ADDR);
    logger("GICH Base: 0x%lx\n", GICH_BASE_ADDR);
    logger("GICV Base: 0x%lx\n", GICV_BASE_ADDR);
    logger("Timer Vector: %d\n", TIMER_VECTOR);
    logger("PL011 Interrupt: %d\n", PL011_INT);

    // 验证设备地址的合理性
    if (GICC_BASE_ADDR <= GICD_BASE_ADDR) {
        logger("ERROR: GICC base address should be after GICD\n");
    }
}

void
validate_all_configs(void)
{
    logger("\n========================================\n");
    logger("Configuration Validation Report\n");
    logger("========================================\n");

    validate_timer_config();
    logger("\n");

    validate_memory_config();
    logger("\n");

    validate_smp_config();
    logger("\n");

    validate_vm_config();
    logger("\n");

    validate_device_config();
    logger("\n");

    logger("========================================\n");
    logger("Configuration validation completed\n");
    logger("========================================\n\n");
}

void
print_config_summary(void)
{
    logger("=== Configuration Summary ===\n");
    logger("System: %d CPU(s), %lu MB RAM\n", SMP_NUM, KERNEL_RAM_SIZE / (1024 * 1024));
    logger("Timer: %d ms interval (%d Hz)\n",
           TIMER_TICK_INTERVAL_MS,
           1000 / TIMER_TICK_INTERVAL_MS);
    logger("VMs: Max %d VMs with %d VCPUs each\n", VM_NUM_MAX, VCPU_NUM_MAX);
    logger("Tasks: Max %d tasks\n", MAX_TASKS);
    logger("==============================\n\n");
}
