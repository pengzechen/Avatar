
#include <pro.h>
#include <list.h>
#include <mem/aj_string.h>
#include <os_cfg.h>
#include <mem/mem.h>
#include <task/task.h>
#include <elf.h>
#include <list.h>

static process_t g_pro_dec[MAX_TASKS];
static uint32_t pro_count = 0;

process_t *alloc_process(char *name)
{
    process_t *pro = &g_pro_dec[pro_count++];
    pro->process_id = pro_count;

    memcpy(pro->process_name, name, strlen(name));
    pro->process_name[PRO_MAX_NAME_LEN] = '\0';

    list_init(&pro->threads);

    return pro;
}

// void copy_app_testapp(void)
// {
//     size_t size = (size_t)(__testapp_bin_end - __testapp_bin_start);
//     unsigned long *from = (unsigned long *)__testapp_bin_start;
//     unsigned long *to = (unsigned long *)0x90000000;
//     printf("Copy app image from %x to %x (%d bytes): 0x%x / 0x%x\n", from, to, size, from[0], from[1]);
//     memcpy(to, from, size);
//     printf("Copy end : 0x%x / 0x%x\n", to[0], to[1]);
// }

static int load_phdr(const char *elf_file_addr, Elf64_Phdr *phdr, pte_t *page_dir)
{
    // 生成的ELF文件要求是页边界对齐的
    assert((phdr->p_vaddr & (PAGE_SIZE - 1)) == 0);

    // 分配空间
    int err = memory_alloc_page(page_dir, phdr->p_vaddr, phdr->p_memsz, 0);
    if (err < 0)
    {
        printf("no memory");
        return -1;
    }

    // 从ELF文件的内存地址中读取段数据
    const char *segment_start = elf_file_addr + phdr->p_offset;
    uint64_t vaddr = phdr->p_vaddr;
    uint64_t size = phdr->p_filesz;

    // 复制文件数据到相应的内存地址
    while (size > 0)
    {
        int curr_size = (size > PAGE_SIZE) ? PAGE_SIZE : size;

        uint64_t paddr = memory_get_paddr(page_dir, vaddr);

        // 从文件数据（即内存）中读取当前的段内容
        memcpy((char *)paddr, segment_start, curr_size);

        // 更新剩余数据量和当前虚拟地址
        size -= curr_size;
        vaddr += curr_size;
        segment_start += curr_size; // 移动文件数据指针
    }

    return 0;
}

static uint64_t load_elf_file(process_t *pro, const char *elf_file_addr, pte_t *page_dir)
{
    Elf64_Ehdr *elf_hdr = (Elf64_Ehdr *)elf_file_addr;
    Elf64_Phdr elf_phdr;

    // ELF头部校验
    if ((elf_hdr->e_ident[0] != 0x7f) || (elf_hdr->e_ident[1] != 'E') || (elf_hdr->e_ident[2] != 'L') || (elf_hdr->e_ident[3] != 'F'))
    {
        printf("check elf indent failed.");
        return 0;
    }

    // 确保是可执行文件并且是AArch64架构
    if ((elf_hdr->e_type != 2) || (elf_hdr->e_machine != EM_AARCH64) || (elf_hdr->e_entry == 0))
    {
        printf("check elf type or entry failed.");
        return 0;
    }

    // 必须有程序头
    if ((elf_hdr->e_phentsize == 0) || (elf_hdr->e_phoff == 0))
    {
        printf("no program header");
        return 0;
    }

    uint64_t e_phoff = elf_hdr->e_phoff;
    // 遍历程序头，加载段
    for (int i = 0; i < elf_hdr->e_phnum; i++, e_phoff += elf_hdr->e_phentsize)
    {
        // 读取程序头
        elf_phdr = *(Elf64_Phdr *)(elf_file_addr + e_phoff);

        // 确保该段是可加载类型，并且地址在用户空间范围内
        if (elf_phdr.p_type != 1)
        {
            continue;
        }

        // 加载该段
        int err = load_phdr(elf_file_addr, &elf_phdr, page_dir);
        if (err < 0)
        {
            printf("load program header failed");
            return 0;
        }

        // 设置堆的起始和结束地址
        pro->heap_start = (void*)(elf_phdr.p_vaddr + elf_phdr.p_memsz);
        pro->heap_end = pro->heap_start;
    }

    return elf_hdr->e_entry;
}

void process_init(process_t *pro, void *elf_addr)
{

    pro->el1_stack = kalloc_page();

    pro->pg_base = (void *)create_uvm();

    pro->entry = load_elf_file(pro, elf_addr, (pte_t *)pro->pg_base);

    printf("process entry: 0x%x, process stack: 0x%x\n", pro->entry, (uint64_t)pro->el1_stack + PAGE_SIZE);

    tcb_t *main_thread = create_task((void (*)())(void*)pro->entry, (uint64_t)pro->el1_stack + PAGE_SIZE, (1 << 0));

    main_thread->pgdir = (uint64_t)pro->pg_base;

    list_insert_last(&pro->threads, &main_thread->process);
}

void run_process(process_t *pro)
{
    // 遍历所有的 thread 把他们的状态设置为就绪
    list_node_t *iter = list_first(&pro->threads);
    while (iter)
    {
        tcb_t *task = list_node_parent(iter, tcb_t, process);
        task_set_ready(task);
        iter = list_node_next(iter);
    }
}

void exit_process(process_t *pro)
{
    // 释放内存，把状态设置为退出
    destroy_uvm_4level(pro->pg_base);
    kfree_page(pro->el1_stack);

    pro->process_name[0] = '\0';
}
