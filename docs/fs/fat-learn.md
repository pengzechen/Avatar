在FAT32文件系统中，FAT表存储在磁盘的特定位置，让我详细说明：

## FAT表在磁盘中的位置

### 磁盘布局结构
```
扇区0-31:     保留区域 (引导扇区、FSInfo等)
扇区32-X:     第一个FAT表
扇区X+1-Y:    第二个FAT表 (备份)
扇区Y+1-:     数据区域 (文件和目录数据)
```

### 具体位置计算

````c path=include/fs/fat32_disk.h mode=EXCERPT
#define FAT32_RESERVED_SECTORS      32                  // 保留扇区数
#define FAT32_NUM_FATS              2                   // FAT表数量
````

````c path=include/fs/fat32_disk.h mode=EXCERPT
/* 计算得出的磁盘布局参数 */
#define FAT32_FAT_SIZE_SECTORS      ((FAT32_CLUSTERS_COUNT * 4 + FAT32_SECTOR_SIZE - 1) / FAT32_SECTOR_SIZE)
#define FAT32_DATA_START_SECTOR     (FAT32_RESERVED_SECTORS + FAT32_NUM_FATS * FAT32_FAT_SIZE_SECTORS)
````

### FAT表位置信息存储

FAT表的位置信息记录在引导扇区中：

````c path=include/fs/fat32_types.h mode=EXCERPT
typedef struct __attribute__((packed)) {
    // ...
    uint16_t    reserved_sectors;       // 保留扇区数（包括引导扇区）
    uint8_t     num_fats;               // FAT表数量（通常为2）
    uint32_t    fat_size_32;            // FAT表大小（扇区数）
    // ...
} fat32_boot_sector_t;
````

### 运行时计算FAT表位置

````c path=fs/fat32_boot.c mode=EXCERPT
fat32_error_t fat32_boot_calculate_layout(const fat32_boot_sector_t *boot_sector, 
                                          fat32_fs_info_t *fs_info)
{
    // 计算FAT表起始扇区
    fs_info->fat_start_sector = boot_sector->reserved_sectors;
    
    // 计算数据区起始扇区
    fs_info->data_start_sector = boot_sector->reserved_sectors + 
                                 boot_sector->num_fats * boot_sector->fat_size_32;
}
````

## FAT表项的具体位置计算

当需要访问特定簇的FAT表项时，使用以下函数计算位置：

````c path=include/fs/fat32_fat.h mode=EXCERPT
/**
 * @brief 计算FAT表项所在的扇区
 */
static inline uint32_t fat32_fat_get_entry_sector(const fat32_fs_info_t *fs_info, 
                                                  uint32_t cluster_num) {
    return fs_info->fat_start_sector + (cluster_num * 4) / FAT32_SECTOR_SIZE;
}

/**
 * @brief 计算FAT表项在扇区中的偏移
 */
static inline uint32_t fat32_fat_get_entry_offset(uint32_t cluster_num) {
    return (cluster_num * 4) % FAT32_SECTOR_SIZE;
}
````

## 双FAT表机制

FAT32通常维护两个相同的FAT表副本：

````c path=fs/fat32_fat.c mode=EXCERPT
// 写入所有FAT表副本
for (uint8_t fat_num = 0; fat_num < fs_info->boot_sector.num_fats; fat_num++) {
    uint32_t fat_start = fs_info->fat_start_sector + fat_num * fs_info->boot_sector.fat_size_32;
    uint32_t target_sector = fat_start + (fat_sector - fs_info->fat_start_sector);
    
    result = fat32_disk_write_sectors(disk, target_sector, 1, sector_buffer);
}
````

## 具体示例

在这个64MB的虚拟磁盘中：

1. **保留区域**: 扇区0-31 (32个扇区)
2. **第一个FAT表**: 从扇区32开始
3. **第二个FAT表**: 紧接着第一个FAT表
4. **数据区**: FAT表之后的所有扇区

### FAT表大小计算
- 每个FAT表项：4字节
- 每扇区：512字节
- 每扇区可存储：512/4 = 128个FAT表项
- 总簇数决定FAT表需要多少扇区

## 访问流程

1. **读取引导扇区**获取FAT表位置信息
2. **计算FAT表起始扇区** = 保留扇区数
3. **定位具体表项**：
   - 扇区号 = FAT起始扇区 + (簇号×4)/512
   - 扇区内偏移 = (簇号×4) % 512
4. **读取扇区数据**并提取4字节FAT表项

这种设计确保了FAT表的位置是固定且可预测的，同时通过双表机制提供了数据冗余保护。
