
#ifndef _ELF
#define _ELF

#pragma pack(1)

// ELF Header
#define EI_NIDENT       16
#define ELF_MAGIC       0x7F

#define ET_EXEC         2   // 可执行文件
#define EM_AARCH64      183	/* ARM 64-bit architecture (AARCH64) */
#define PT_LOAD         1   // 可加载类型

// ELF 头部和程序头部定义（AArch64版）
typedef struct {
    uint8_t  e_ident[16];    // ELF标识
    uint16_t e_type;         // 文件类型
    uint16_t e_machine;      // 机器架构
    uint32_t e_version;      // ELF版本
    uint64_t e_entry;        // 程序入口地址
    uint64_t e_phoff;        // 程序头表的偏移
    uint64_t e_shoff;        // 节头表的偏移
    uint32_t e_flags;        // 处理器特定标志
    uint16_t e_ehsize;       // ELF头的大小
    uint16_t e_phentsize;    // 程序头条目的大小
    uint16_t e_phnum;        // 程序头表的条目数
    uint16_t e_shentsize;    // 节头表条目的大小
    uint16_t e_shnum;        // 节头表的条目数
    uint16_t e_shstrndx;     // 节头表字符串表的索引
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;         // 段类型
    uint32_t p_flags;        // 段标志
    uint64_t p_offset;       // 段在文件中的偏移
    uint64_t p_vaddr;        // 段在内存中的虚拟地址
    uint64_t p_paddr;        // 段在物理内存中的地址
    uint64_t p_filesz;       // 段在文件中的大小
    uint64_t p_memsz;        // 段在内存中的大小
    uint64_t p_align;        // 段对齐
} Elf64_Phdr;


#pragma pack()


#endif // _ELF