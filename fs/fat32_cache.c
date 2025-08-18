/**
 * @file fat32_cache.c
 * @brief FAT32缓存管理实现
 * 
 * 本文件实现了FAT32文件系统的缓存管理功能。
 * 简化实现，主要提供基本的缓存接口。
 */

#include "fs/fat32_cache.h"
#include "fs/fat32_boot.h"
#include "lib/avatar_string.h"
#include "lib/avatar_assert.h"
#include "mem/mem.h"
#include "io.h"

/* ============================================================================
 * 全局变量
 * ============================================================================ */

static fat32_cache_manager_t g_cache_manager;

/* ============================================================================
 * 私有函数声明
 * ============================================================================ */

static fat32_cache_block_t *
fat32_cache_find_block(fat32_cache_manager_t *cache_mgr, uint32_t cluster_num);
static fat32_cache_block_t *
fat32_cache_alloc_block(fat32_cache_manager_t *cache_mgr);
static fat32_error_t
fat32_cache_load_cluster(fat32_cache_block_t   *block,
                         fat32_disk_t          *disk,
                         const fat32_fs_info_t *fs_info,
                         uint32_t               cluster_num);
static fat32_error_t
fat32_cache_write_back_block(fat32_cache_block_t   *block,
                             fat32_disk_t          *disk,
                             const fat32_fs_info_t *fs_info);

/* ============================================================================
 * 缓存管理函数实现
 * ============================================================================ */

fat32_error_t
fat32_cache_init(fat32_cache_manager_t *cache_mgr)
{
    avatar_assert(cache_mgr != NULL);

    if (cache_mgr->initialized) {
        return FAT32_OK;
    }

    // 初始化缓存管理器
    memset(cache_mgr, 0, sizeof(fat32_cache_manager_t));

    // 为每个缓存块分配内存
    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        cache_mgr->blocks[i].data = (uint8_t *) kalloc_pages(FAT32_CACHE_BLOCK_SIZE / PAGE_SIZE);
        if (cache_mgr->blocks[i].data == NULL) {
            // 分配失败，清理已分配的内存
            for (int j = 0; j < i; j++) {
                kfree_pages(cache_mgr->blocks[j].data, FAT32_CACHE_BLOCK_SIZE / PAGE_SIZE);
            }
            return FAT32_ERROR_DISK_ERROR;
        }

        cache_mgr->blocks[i].cluster_num = 0;
        cache_mgr->blocks[i].status      = FAT32_CACHE_FREE;
        cache_mgr->blocks[i].access_time = 0;
        cache_mgr->blocks[i].in_use      = 0;
    }

    cache_mgr->access_counter = 0;
    cache_mgr->initialized    = 1;

    logger("FAT32: Cache manager initialized with %d blocks\n", FAT32_CACHE_SIZE);
    return FAT32_OK;
}

fat32_error_t
fat32_cache_cleanup(fat32_cache_manager_t *cache_mgr,
                    fat32_disk_t          *disk,
                    const fat32_fs_info_t *fs_info)
{
    avatar_assert(cache_mgr != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_OK;
    }

    // 刷新所有脏缓存块
    fat32_error_t result = fat32_cache_flush(cache_mgr, disk, fs_info);
    if (result != FAT32_OK) {
        logger("FAT32: Warning - Failed to flush cache during cleanup\n");
    }

    // 释放所有缓存块内存
    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        if (cache_mgr->blocks[i].data != NULL) {
            kfree_pages(cache_mgr->blocks[i].data, FAT32_CACHE_BLOCK_SIZE / PAGE_SIZE);
            cache_mgr->blocks[i].data = NULL;
        }
    }

    memset(cache_mgr, 0, sizeof(fat32_cache_manager_t));

    logger("FAT32: Cache manager cleaned up\n");
    return FAT32_OK;
}

fat32_error_t
fat32_cache_read_cluster(fat32_cache_manager_t *cache_mgr,
                         fat32_disk_t          *disk,
                         const fat32_fs_info_t *fs_info,
                         uint32_t               cluster_num,
                         void                  *buffer)
{
    avatar_assert(cache_mgr != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }

    // 查找缓存块
    fat32_cache_block_t *block = fat32_cache_find_block(cache_mgr, cluster_num);

    if (block == NULL) {
        // 缓存未命中，分配新的缓存块
        block = fat32_cache_alloc_block(cache_mgr);
        if (block == NULL) {
            // 缓存已满，直接从磁盘读取（简化处理）
            uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);
            return fat32_disk_read_sectors(disk,
                                           first_sector,
                                           fs_info->sectors_per_cluster,
                                           buffer);
        }

        // 从磁盘加载数据到缓存
        fat32_error_t result = fat32_cache_load_cluster(block, disk, fs_info, cluster_num);
        if (result != FAT32_OK) {
            block->status = FAT32_CACHE_FREE;
            return result;
        }
    }

    // 更新访问时间
    block->access_time = ++cache_mgr->access_counter;

    // 复制数据到用户缓冲区
    memcpy(buffer, block->data, fs_info->bytes_per_cluster);

    return FAT32_OK;
}

fat32_error_t
fat32_cache_write_cluster(fat32_cache_manager_t *cache_mgr,
                          fat32_disk_t          *disk,
                          const fat32_fs_info_t *fs_info,
                          uint32_t               cluster_num,
                          const void            *buffer)
{
    avatar_assert(cache_mgr != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);
    avatar_assert(buffer != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }

    // 查找缓存块
    fat32_cache_block_t *block = fat32_cache_find_block(cache_mgr, cluster_num);

    if (block == NULL) {
        // 缓存未命中，分配新的缓存块
        block = fat32_cache_alloc_block(cache_mgr);
        if (block == NULL) {
            // 缓存已满，直接写入磁盘（简化处理）
            uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);
            return fat32_disk_write_sectors(disk,
                                            first_sector,
                                            fs_info->sectors_per_cluster,
                                            buffer);
        }

        block->cluster_num = cluster_num;
        block->in_use      = 1;
    }

    // 复制数据到缓存块
    memcpy(block->data, buffer, fs_info->bytes_per_cluster);

    // 标记为脏块
    fat32_cache_mark_dirty(block);

    // 更新访问时间
    block->access_time = ++cache_mgr->access_counter;

    return FAT32_OK;
}

fat32_error_t
fat32_cache_flush(fat32_cache_manager_t *cache_mgr,
                  fat32_disk_t          *disk,
                  const fat32_fs_info_t *fs_info)
{
    avatar_assert(cache_mgr != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_OK;
    }

    fat32_error_t result = FAT32_OK;

    // 遍历所有缓存块，写回脏块
    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        fat32_cache_block_t *block = &cache_mgr->blocks[i];

        if (block->in_use && fat32_cache_is_dirty_block(block)) {
            fat32_error_t write_result = fat32_cache_write_back_block(block, disk, fs_info);
            if (write_result != FAT32_OK) {
                logger("FAT32: Failed to write back cache block for cluster %u\n",
                       block->cluster_num);
                result = write_result;  // 记录错误但继续处理其他块
            } else {
                fat32_cache_mark_clean(block);
            }
        }
    }

    return result;
}

fat32_error_t
fat32_cache_flush_cluster(fat32_cache_manager_t *cache_mgr,
                          fat32_disk_t          *disk,
                          const fat32_fs_info_t *fs_info,
                          uint32_t               cluster_num)
{
    avatar_assert(cache_mgr != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_OK;
    }

    fat32_cache_block_t *block = fat32_cache_find_block(cache_mgr, cluster_num);
    if (block != NULL && fat32_cache_is_dirty_block(block)) {
        fat32_error_t result = fat32_cache_write_back_block(block, disk, fs_info);
        if (result == FAT32_OK) {
            fat32_cache_mark_clean(block);
        }
        return result;
    }

    return FAT32_OK;
}

fat32_error_t
fat32_cache_invalidate_cluster(fat32_cache_manager_t *cache_mgr, uint32_t cluster_num)
{
    avatar_assert(cache_mgr != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_OK;
    }

    fat32_cache_block_t *block = fat32_cache_find_block(cache_mgr, cluster_num);
    if (block != NULL) {
        block->status      = FAT32_CACHE_FREE;
        block->in_use      = 0;
        block->cluster_num = 0;
    }

    return FAT32_OK;
}

fat32_error_t
fat32_cache_get_stats(const fat32_cache_manager_t *cache_mgr,
                      uint32_t                    *total_blocks,
                      uint32_t                    *used_blocks,
                      uint32_t                    *dirty_blocks)
{
    avatar_assert(cache_mgr != NULL);

    if (!cache_mgr->initialized) {
        return FAT32_ERROR_DISK_ERROR;
    }

    uint32_t used = 0, dirty = 0;

    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        const fat32_cache_block_t *block = &cache_mgr->blocks[i];
        if (block->in_use) {
            used++;
            if (fat32_cache_is_dirty_block(block)) {
                dirty++;
            }
        }
    }

    if (total_blocks != NULL) {
        *total_blocks = FAT32_CACHE_SIZE;
    }

    if (used_blocks != NULL) {
        *used_blocks = used;
    }

    if (dirty_blocks != NULL) {
        *dirty_blocks = dirty;
    }

    return FAT32_OK;
}

/* ============================================================================
 * 全局缓存管理器访问函数实现
 * ============================================================================ */

fat32_cache_manager_t *
fat32_get_cache_manager(void)
{
    return &g_cache_manager;
}

fat32_error_t
fat32_init_global_cache(void)
{
    return fat32_cache_init(&g_cache_manager);
}

fat32_error_t
fat32_cleanup_global_cache(fat32_disk_t *disk, const fat32_fs_info_t *fs_info)
{
    return fat32_cache_cleanup(&g_cache_manager, disk, fs_info);
}

/* ============================================================================
 * 私有函数实现
 * ============================================================================ */

static fat32_cache_block_t *
fat32_cache_find_block(fat32_cache_manager_t *cache_mgr, uint32_t cluster_num)
{
    avatar_assert(cache_mgr != NULL);

    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        fat32_cache_block_t *block = &cache_mgr->blocks[i];
        if (block->in_use && block->cluster_num == cluster_num) {
            return block;
        }
    }

    return NULL;
}

static fat32_cache_block_t *
fat32_cache_alloc_block(fat32_cache_manager_t *cache_mgr)
{
    avatar_assert(cache_mgr != NULL);

    // 首先查找空闲块
    for (int i = 0; i < FAT32_CACHE_SIZE; i++) {
        fat32_cache_block_t *block = &cache_mgr->blocks[i];
        if (fat32_cache_is_free_block(block)) {
            return block;
        }
    }

    // 没有空闲块，使用简单的LRU策略找到最久未访问的块
    fat32_cache_block_t *lru_block   = &cache_mgr->blocks[0];
    uint32_t             oldest_time = lru_block->access_time;

    for (int i = 1; i < FAT32_CACHE_SIZE; i++) {
        fat32_cache_block_t *block = &cache_mgr->blocks[i];
        if (block->access_time < oldest_time) {
            oldest_time = block->access_time;
            lru_block   = block;
        }
    }

    // 如果LRU块是脏块，需要先写回（简化实现中忽略错误）
    if (fat32_cache_is_dirty_block(lru_block)) {
        logger("FAT32: Warning - Evicting dirty cache block for cluster %u\n",
               lru_block->cluster_num);
        // 在实际实现中应该写回磁盘
    }

    // 重置块状态
    lru_block->status      = FAT32_CACHE_FREE;
    lru_block->cluster_num = 0;
    lru_block->in_use      = 0;

    return lru_block;
}

static fat32_error_t
fat32_cache_load_cluster(fat32_cache_block_t   *block,
                         fat32_disk_t          *disk,
                         const fat32_fs_info_t *fs_info,
                         uint32_t               cluster_num)
{
    avatar_assert(block != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, cluster_num);

    // 从磁盘读取数据
    fat32_error_t result =
        fat32_disk_read_sectors(disk, first_sector, fs_info->sectors_per_cluster, block->data);
    if (result != FAT32_OK) {
        return result;
    }

    // 设置块状态
    block->cluster_num = cluster_num;
    block->status      = FAT32_CACHE_CLEAN;
    block->in_use      = 1;

    return FAT32_OK;
}

static fat32_error_t
fat32_cache_write_back_block(fat32_cache_block_t   *block,
                             fat32_disk_t          *disk,
                             const fat32_fs_info_t *fs_info)
{
    avatar_assert(block != NULL);
    avatar_assert(disk != NULL);
    avatar_assert(fs_info != NULL);

    if (!block->in_use) {
        return FAT32_ERROR_INVALID_PARAM;
    }

    // 计算簇对应的扇区
    uint32_t first_sector = fat32_boot_cluster_to_sector(fs_info, block->cluster_num);

    // 写入磁盘
    return fat32_disk_write_sectors(disk, first_sector, fs_info->sectors_per_cluster, block->data);
}
