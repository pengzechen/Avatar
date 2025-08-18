这是一个很好的问题！目录作为文件存储时，它自己的文件名信息存储在**父目录**中。

## 目录文件名的存储位置

### 根目录的特殊情况
根目录是个例外：
- 根目录没有父目录
- 根目录没有自己的目录项
- 根目录的起始簇号固定为2，在引导扇区中定义

````c path=include/fs/fat32_disk.h mode=EXCERPT
#define FAT32_ROOT_CLUSTER          2                   // 根目录起始簇号
````

### 子目录的文件名存储
对于子目录，它的文件名存储在父目录的目录项中：

```
父目录 (作为文件)
├── 目录项1: "MYDIR      " attr=0x10 cluster=100  ← 子目录的文件名在这里
├── 目录项2: "FILE1   TXT" attr=0x00 cluster=200
└── ...

子目录 (簇100开始的文件内容)
├── 目录项1: ".          " attr=0x10 cluster=100  ← 指向自己
├── 目录项2: "..         " attr=0x10 cluster=2    ← 指向父目录
├── 目录项3: "SUBFILE TXT" attr=0x00 cluster=300
└── ...
```

## 具体实现示例

### 创建目录时的文件名处理
````c path=fs/fat32_dir.c mode=EXCERPT
fat32_error_t fat32_dir_create_directory(fat32_disk_t *disk,
                                         fat32_fs_info_t *fs_info,
                                         uint32_t parent_cluster,
                                         const char *dirname,
                                         uint32_t *new_dir_cluster)
{
    // 1. 分配新目录的簇
    uint32_t dir_cluster;
    result = fat32_fat_allocate_cluster(disk, fs_info, &dir_cluster);
    
    // 2. 在父目录中创建目录项 - 这里存储目录的文件名
    uint32_t entry_index;
    result = fat32_dir_create_entry(disk, fs_info, parent_cluster, dirname,
                                   FAT32_ATTR_DIRECTORY, dir_cluster, 0, &entry_index);
````

### 目录项创建过程
````c path=fs/fat32_dir.c mode=EXCERPT
// 创建新的目录项
fat32_dir_entry_t new_entry;
memset(&new_entry, 0, sizeof(new_entry));

// 转换文件名 - 目录的文件名存储在这里
result = fat32_dir_convert_to_short_name(filename, new_entry.name);

// 设置目录项属性
new_entry.attr = attr;  // FAT32_ATTR_DIRECTORY for directories
fat32_dir_set_first_cluster(&new_entry, first_cluster);
````

## 目录的自引用机制

### "." 和 ".." 目录项
虽然目录的文件名存储在父目录中，但目录内部通常包含特殊的目录项：

```c
// "." 目录项 - 指向自己
dir_entry.name = ".          ";  // 当前目录
dir_entry.attr = FAT32_ATTR_DIRECTORY;
dir_entry.first_cluster = current_dir_cluster;

// ".." 目录项 - 指向父目录  
dir_entry.name = "..         ";  // 父目录
dir_entry.attr = FAT32_ATTR_DIRECTORY;
dir_entry.first_cluster = parent_dir_cluster;
```

## 实际的目录结构示例

假设我们有这样的目录结构：
```
/ (根目录，簇2)
├── documents/ (簇100)
│   ├── file1.txt (簇200)
│   └── photos/ (簇300)
│       └── pic1.jpg (簇400)
└── music/ (簇500)
```

### 根目录的内容 (簇2)
```
偏移0:  "DOCUMENTS  " attr=0x10 cluster=100  ← documents目录的文件名
偏移32: "MUSIC      " attr=0x10 cluster=500  ← music目录的文件名
偏移64: [空闲目录项]
```

### documents目录的内容 (簇100)
```
偏移0:  ".          " attr=0x10 cluster=100  ← 指向自己
偏移32: "..         " attr=0x10 cluster=2    ← 指向根目录
偏移64: "FILE1   TXT" attr=0x00 cluster=200  ← file1.txt的文件名
偏移96: "PHOTOS     " attr=0x10 cluster=300  ← photos目录的文件名
```

### photos目录的内容 (簇300)
```
偏移0:  ".          " attr=0x10 cluster=300  ← 指向自己
偏移32: "..         " attr=0x10 cluster=100  ← 指向documents目录
偏移64: "PIC1    JPG" attr=0x00 cluster=400  ← pic1.jpg的文件名
```

## 查找目录文件名的方法

如果你想知道某个目录的文件名，需要：

1. **已知目录簇号，查找文件名**：
   - 遍历所有可能的父目录
   - 在父目录中查找指向该簇号的目录项
   - 从目录项中获取文件名

2. **通过路径解析**：
   - 从根目录开始
   - 逐级解析路径中的每个组件
   - 每一级都在当前目录中查找下一级的目录项

## 总结

- **目录的文件名存储在父目录的目录项中**
- **根目录没有文件名，是文件系统的起点**
- **目录内部可能包含"."和".."特殊目录项用于导航**
- **目录作为文件，其内容是一系列32字节的目录项**

这种设计保持了文件系统结构的一致性：每个文件（包括目录文件）的元数据都存储在其父目录中，形成了清晰的层次结构。
