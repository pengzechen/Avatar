
#include <pro.h>
#include <list.h>
#include <mem/aj_string.h>
#include <os_cfg.h>
#include <mem/mem.h>
#include <task/task.h>
#include <elf.h>
#include <list.h>
#include <thread.h>
static process_t g_pro_dec[MAX_TASKS];


process_t *alloc_process(char *name)
{
    static uint32_t pro_count = 1;
    for (int i=0; i<MAX_TASKS; i++) {
        if (g_pro_dec[i].process_id == 0) {
            process_t *pro = &g_pro_dec[i];
            pro->process_id = pro_count++;

            memcpy(pro->process_name, name, strlen(name));
            pro->process_name[PRO_MAX_NAME_LEN] = '\0';

            list_init(&pro->threads);
            return pro;
        }
    }
    return NULL;
}

void free_process(process_t *pro) 
{
    memset(pro, 0, sizeof(process_t));
}

// void copy_app_testapp(void)
// {
//     size_t size = (size_t)(__testapp_bin_end - __testapp_bin_start);
//     unsigned long *from = (unsigned long *)__testapp_bin_start;
//     unsigned long *to = (unsigned long *)0x90000000;
//     printf("Copy app image from %llx to %llx (%d bytes): 0x%llx / 0x%llx\n", from, to, size, from[0], from[1]);
//     memcpy(to, from, size);
//     printf("Copy end : 0x%llx / 0x%llx\n", to[0], to[1]);
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
        memcpy((char *)phys_to_virt(paddr), segment_start, curr_size);

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

void prepare_vm(process_t **process, void *elf_addr) 
{
    process_t * pro = *process;
    
    pro->pg_base = (void *)create_uvm();

    pro->entry = load_elf_file(pro, elf_addr, (pte_t *)pro->pg_base);   // map data 区的一块内存 将来优化这里
    printf("process entry: 0x%llx, process stack: 0x%llx\n", pro->entry, (uint64_t)pro->el1_stack + PAGE_SIZE);

    // 处理 EL1 的栈
    pro->el1_stack = kalloc_pages(2);
    list_node_t * iter = list_first(&get_task_manager()->task_list);   
    while (iter) {
        tcb_t *task = list_node_parent(iter, tcb_t, all_node);
        printf("map other task(%d) el1 stack: 0x%llx\n", task->id, task->sp);
        memory_create_map(pro->pg_base, task->sp, virt_to_phys(task->sp), 2, 1);  // 要把所有的task的el1栈都映射一下，不然 task_switch 结束的时候会page fault
        printf("map this task's el1 stack for task(%d), 0x%llx\n", task->id, (uint64_t)pro->el1_stack);
        memory_create_map((pte_t*)task->pgdir, (uint64_t)pro->el1_stack, virt_to_phys(pro->el1_stack), 2, 1);  // 帮另一个任务映射一下这个任务的栈
        iter = list_node_next(iter);
    }
    printf("map this task's el1 stack, 0x%llx\n", (uint64_t)pro->el1_stack);
    memory_create_map(pro->pg_base, (uint64_t)pro->el1_stack, virt_to_phys(pro->el1_stack), 2, 1);

    // 处理 EL0 的栈
    pro->el0_stack = kalloc_pages(1);
    printf("map this task's el0 stack, 0x%llx\n", (uint64_t)pro->el0_stack);
    memory_create_map(pro->pg_base, (uint64_t)pro->entry + 0x3000, virt_to_phys(pro->el0_stack), 1, 0);
}

void process_init(process_t *pro, void *elf_addr, uint32_t priority)
{
    prepare_vm(&pro, elf_addr);

    tcb_t *main_thread = create_task((void (*)())(void*)pro->entry, (uint64_t)pro->el1_stack + PAGE_SIZE * 2, priority);

    main_thread->pgdir = (uint64_t)pro->pg_base;
    main_thread->curr_pro = pro;

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
    // 这里 elf 物理地址释放的时候会有问题
    // 这里 el1 栈释放会重复释放
    destroy_uvm_4level(pro->pg_base);

    free_process(pro);
}

char * get_file_name (char * name) {
    char * s = name;

    // 定位到结束符
    while (*s != '\0') {
        s++;
    }

    // 反向搜索，直到找到反斜杆或者到文件开头
    while ((*s != '\\') && (*s != '/') && (s >= name)) {
        s--;
    }
    return s + 1;
}

int pro_execve(char *name, void *elf_addr) {
    tcb_t * curr = (tcb_t *)(void *)read_tpidr_el0();
    process_t * pro = curr->curr_pro;

    strcpy(pro->process_name, get_file_name(name));  // 先把名字换过来

    // 现在开始加载了，先准备应用页表，由于所有操作均在内核区中进行，所以可以直接先切换到新页表
    uint64_t old_page_dir = (uint64_t)pro->pg_base;
    
    // 设置新的页表并切过去
    prepare_vm(&pro, elf_addr);
    uint64_t new_page_dir = (uint64_t)pro->pg_base;
    asm volatile("msr ttbr0_el1, %[x]" : : [x] "r"(new_page_dir));
    dsb_sy();
    isb();
    tlbi_vmalle1();
    dsb_sy();
    isb();

    destroy_uvm_4level((pte_t*)(void*)old_page_dir);            // 再释放掉了原进程的内容空间
    return 0;
}