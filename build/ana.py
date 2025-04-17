data = """
ffff000040080000 l    d  .text	0000000000000000 .text
ffff00004008c918 l    d  .rodata	0000000000000000 .rodata
ffff00004008e468 l    d  .data	0000000000000000 .data
ffff0000400b4000 l    d  .bss	0000000000000000 .bss
0000000000000000 l    d  .debug_line	0000000000000000 .debug_line
0000000000000000 l    d  .debug_line_str	0000000000000000 .debug_line_str
0000000000000000 l    d  .debug_info	0000000000000000 .debug_info
0000000000000000 l    d  .debug_abbrev	0000000000000000 .debug_abbrev
0000000000000000 l    d  .debug_aranges	0000000000000000 .debug_aranges
0000000000000000 l    d  .debug_str	0000000000000000 .debug_str
0000000000000000 l    d  .comment	0000000000000000 .comment
0000000000000000 l    d  .debug_frame	0000000000000000 .debug_frame
0000000000000000 l    d  .debug_rnglists	0000000000000000 .debug_rnglists
0000000000000000 l    df *ABS*	0000000000000000 boot.s.o
ffff0000400d0000 l       .bss	0000000000000000 _stack_top_el3
ffff000040080108 l       .text	0000000000000000 from_el3_to_el1
ffff0000400800a0 l       .text	0000000000000000 from_el2
ffff000040080058 l       .text	0000000000000000 primary_entry
ffff0000400800f0 l       .text	0000000000000000 stack_test
ffff0000400cc000 l       .bss	0000000000000000 _stack_bottom_el3
ffff0000400d0000 l       .bss	0000000000000000 _stack_bottom
ffff0000400d4000 l       .bss	0000000000000000 _stack_bottom_second
0000000000000000 l    df *ABS*	0000000000000000 guest.s.o
0000000000000000 l    df *ABS*	0000000000000000 main.c
ffff0000400801d8 l     F .text	000000000000001c get_current_cpu_id
ffff0000400801f4 l     F .text	000000000000001c spinlock_init
0000000000000000 l    df *ABS*	0000000000000000 smp.c
ffff0000400804ec l     F .text	000000000000003c hvc_call
0000000000000000 l    df *ABS*	0000000000000000 main_hyper.c
ffff0000400806dc l     F .text	000000000000001c write_vtcr_el2
ffff0000400806f8 l     F .text	0000000000000018 read_sctlr_el2
ffff000040080710 l     F .text	0000000000000018 read_hcr_el2
ffff000040080728 l     F .text	0000000000000018 read_vttbr_el2
ffff00004008079c l     F .text	000000000000003c guest_trap_init
ffff0000400e8000 l     O .bss	0000000000002000 guest1_el2_stack
ffff0000400ea000 l     O .bss	0000000000002000 guest2_el2_stack
0000000000000000 l    df *ABS*	0000000000000000 exception.s.o
0000000000000000 l    df *ABS*	0000000000000000 exception_el3.s.o
0000000000000000 l    df *ABS*	0000000000000000 exception.c
ffff000040082858 l     F .text	0000000000000018 read_esr_el1
ffff000040082870 l     F .text	000000000000001c get_current_cpu_id
0000000000000000 l    df *ABS*	0000000000000000 exception_el3.c
ffff000040082abc l     F .text	0000000000000018 read_esr_el3
0000000000000000 l    df *ABS*	0000000000000000 exception_el2.c
ffff000040082c28 l     F .text	0000000000000018 read_esr_el2
ffff000040082c40 l     F .text	0000000000000018 read_hpfar_el2
ffff000040082c58 l     F .text	0000000000000018 read_far_el2
0000000000000000 l    df *ABS*	0000000000000000 exception_el2.s.o
0000000000000000 l    df *ABS*	0000000000000000 syscall.c
0000000000000000 l    df *ABS*	0000000000000000 gic.c
ffff000040084140 l     F .text	0000000000000024 write8
ffff000040084164 l     F .text	0000000000000020 read32
ffff000040084184 l     F .text	0000000000000024 write32
0000000000000000 l    df *ABS*	0000000000000000 io.c
ffff0000400ed008 l     O .bss	0000000000000008 uart_op
ffff00004008d0c8 l     O .rodata	0000000000000010 early_ops
ffff00004008d0d8 l     O .rodata	0000000000000010 advance_ops
0000000000000000 l    df *ABS*	0000000000000000 uart_pl011.c
ffff000040084b90 l     F .text	000000000000001c spinlock_init
ffff0000400ed820 l     O .bss	0000000000000004 lock
0000000000000000 l    df *ABS*	0000000000000000 uart_pl011_early.c
ffff000040084f68 l     F .text	000000000000001c spinlock_init
ffff0000400ed828 l     O .bss	0000000000000004 lock
0000000000000000 l    df *ABS*	0000000000000000 printf.c
ffff0000400b3940 l     O .data	0000000000000010 digits
ffff0000400850c8 l     F .text	000000000000006c addchar
ffff000040085134 l     F .text	0000000000000120 print_str
ffff000040085254 l     F .text	00000000000001a4 print_int
ffff0000400853f8 l     F .text	00000000000001f4 print_unsigned
ffff0000400855ec l     F .text	00000000000000b4 fmtnum
0000000000000000 l    df *ABS*	0000000000000000 mmu.s.o
0000000000000000 l    df *ABS*	0000000000000000 page.c
ffff0000400ef000 l     O .bss	0000000000001000 pt0
ffff0000400f0000 l     O .bss	0000000000001000 pt1
ffff0000400f1000 l     O .bss	0000000000001000 high_pt0
ffff0000400f2000 l     O .bss	0000000000001000 high_pt1
ffff0000400f3000 l     O .bss	0000000000000001 mmu_enable_flag
0000000000000000 l    df *ABS*	0000000000000000 bitmap.c
ffff00004008d348 l     O .rodata	000000000000000b __func__.1
ffff00004008d358 l     O .rodata	000000000000000d __func__.0
0000000000000000 l    df *ABS*	0000000000000000 mem.c
ffff000040086a54 l     F .text	0000000000000018 read_ttbr0_el1
ffff000040086a6c l     F .text	0000000000000018 read_tpidr_el0
ffff0000400f3008 l     O .bss	0000000000000050 g_alloc
ffff0000400f3058 l     O .bss	0000000000000008 kpage_dir
ffff000040086c54 l     F .text	0000000000000094 addr_alloc_page
ffff000040086ce8 l     F .text	00000000000000dc addr_free_page
ffff0000400b3958 l     O .data	0000000000000008 heap_start.13
ffff00004008da00 l     O .rodata	000000000000000f __func__.12
ffff00004008da10 l     O .rodata	0000000000000010 max_entries.11
ffff00004008da20 l     O .rodata	0000000000000010 max_entries.10
ffff000040087f78 l     F .text	00000000000000cc get_available_page_count
ffff00004008da30 l     O .rodata	0000000000000014 __func__.9
ffff00004008da48 l     O .rodata	0000000000000010 __func__.8
ffff00004008da58 l     O .rodata	0000000000000014 __func__.7
ffff00004008da70 l     O .rodata	0000000000000020 __func__.6
ffff00004008da90 l     O .rodata	0000000000000019 __func__.5
ffff00004008dab0 l     O .rodata	0000000000000017 __func__.4
ffff00004008dac8 l     O .rodata	0000000000000014 __func__.3
ffff00004008dae0 l     O .rodata	0000000000000015 __func__.2
ffff00004008daf8 l     O .rodata	0000000000000014 __func__.1
ffff00004008db10 l     O .rodata	000000000000001c __func__.0
0000000000000000 l    df *ABS*	0000000000000000 ept.c
ffff000040088eec l     F .text	000000000000001c flush_tlb
ffff000040088f08 l     F .text	000000000000001c __clean_and_invalidate_dcache_one
ffff000040088f24 l     F .text	0000000000000068 clean_and_invalidate_dcache_va_range
ffff000040088f8c l     F .text	0000000000000018 read_par
ffff000040088fa4 l     F .text	000000000000001c write_par
ffff000040088fc0 l     F .text	000000000000001c write_ats1cpr
ffff000040088fdc l     F .text	000000000000003c isInMemory
ffff0000400893d0 l     F .text	000000000000003c gva_to_ipa_par
ffff000040089524 l     F .text	000000000000014c handle_mmio
0000000000000000 l    df *ABS*	0000000000000000 string.c
0000000000000000 l    df *ABS*	0000000000000000 timer.c
ffff000040089cb4 l     F .text	0000000000000018 read_cntfrq_el0
ffff000040089ccc l     F .text	000000000000001c write_cntp_ctl_el0
ffff000040089ce8 l     F .text	000000000000001c write_cntp_tval_el0
ffff0000400f3070 l     O .bss	0000000000000008 test_num
0000000000000000 l    df *ABS*	0000000000000000 task.c
ffff000040089e28 l     F .text	000000000000001c spinlock_init
ffff000040089e44 l     F .text	000000000000002c list_node_init
ffff000040089e70 l     F .text	0000000000000018 list_node_next
ffff000040089e88 l     F .text	0000000000000018 list_count
ffff000040089ea0 l     F .text	0000000000000018 list_first
ffff000040089eb8 l     F .text	000000000000001c get_current_cpu_id
ffff000040089ed4 l     F .text	0000000000000018 read_tpidr_el0
ffff000040089eec l     F .text	000000000000001c write_tpidr_el0
ffff000040089f08 l     F .text	0000000000000018 read_tpidr_el2
ffff000040089f20 l     F .text	000000000000001c write_tpidr_el2
ffff0000400fc308 l     O .bss	0000000000000004 lock
ffff0000400fc310 l     O .bss	0000000000000004 print_lock
ffff0000400fc318 l     O .bss	0000000000000468 task_manager
ffff00004008aab0 l     F .text	0000000000000184 task_next_run
ffff0000400fd000 l     O .bss	0000000000002000 idle_task_stack
0000000000000000 l    df *ABS*	0000000000000000 pro.c
ffff00004008ad54 l     F .text	0000000000000018 list_node_next
ffff00004008ad6c l     F .text	0000000000000018 list_first
ffff0000400ff000 l     O .bss	0000000000002600 g_pro_dec
ffff000040101600 l     O .bss	0000000000000004 pro_count
ffff00004008ae34 l     F .text	000000000000014c load_phdr
ffff00004008de48 l     O .rodata	000000000000000a __func__.0
ffff00004008af80 l     F .text	00000000000001ec load_elf_file
0000000000000000 l    df *ABS*	0000000000000000 context.s.o
0000000000000000 l    df *ABS*	0000000000000000 mutex.c
ffff00004008b540 l     F .text	0000000000000018 list_count
ffff00004008b558 l     F .text	0000000000000018 read_tpidr_el0
0000000000000000 l    df *ABS*	0000000000000000 spinlock.s.o
0000000000000000 l    df *ABS*	0000000000000000 vcpu.c
0000000000000000 l    df *ABS*	0000000000000000 vgic.c
ffff000040109608 l     O .bss	00000000000057a0 _vgic
ffff00004010eda8 l     O .bss	0000000000000001 _vgic_num
0000000000000000 l    df *ABS*	0000000000000000 vm.c
ffff0000401131f0 l     O .bss	0000000000000004 _vgic_num
ffff0000401131f4 l     O .bss	0000000000000004 _state_num
0000000000000000 l    df *ABS*	0000000000000000 hyper_ctx.s.o
0000000000000000 l    df *ABS*	0000000000000000 list.c
ffff00004008c620 l     F .text	0000000000000024 list_is_empty
ffff000040082bf8 g     F .text	0000000000000030 invalid_exception_el3
ffff000040089720 g     F .text	000000000000002c strcpy
ffff000040086750 g     F .text	0000000000000110 bitmap_clear
ffff000040084914 g     F .text	000000000000001c gic_apr
ffff0000400b3950 g     O .data	0000000000000008 mutex_test_num
ffff00004008c840 g     F .text	00000000000000d4 list_delete
ffff0000400ec000 g     O .bss	0000000000001000 g_handler_vec
ffff000040091890 g       .data	0000000000000000 __guset_fs_end
ffff00004008a350 g     F .text	00000000000000ac print_current_task_list
ffff000040080570 g     F .text	000000000000016c start_secondary_cpus
ffff00004008c70c g     F .text	0000000000000094 list_insert_last
ffff000040086044 g     F .text	000000000000009c printf
ffff0000400842c4 g     F .text	0000000000000030 gicc_init
ffff0000400e4000 g     O .bss	0000000000000004 inited_cpu_num
ffff00004008a93c g     F .text	000000000000009c task_set_ready
ffff000040081000 g       .text	0000000000000000 exception_vector_base
ffff000040114000 g       .bss	0000000000000000 ept_L1
ffff0000400849b4 g     F .text	0000000000000028 gic_clear_np_int
ffff00004008002c g       .text	0000000000000000 second_entry
ffff000040084454 g     F .text	0000000000000028 gic_write_dir
ffff000040080880 g     F .text	00000000000000a8 copy_dtb
ffff0000400863c4 g     F .text	0000000000000028 setup_cache
ffff00004008acf4 g     F .text	0000000000000060 sys_sleep_tick
ffff000040082000 g       .text	0000000000000000 exception_vector_base_el3
ffff000040087c9c g     F .text	0000000000000138 mutex_test_add
ffff00004008b730 g       .text	0000000000000000 spin_trylock
ffff000040089a4c g     F .text	00000000000000d8 memmove
ffff000040086340 g       .text	0000000000000000 init_mmu_el2
ffff000040085f50 g     F .text	000000000000008c snprintf
ffff00004008509c g     F .text	000000000000002c uart_early_getc
ffff000040089b90 g     F .text	0000000000000124 atol
ffff000040086218 g     F .text	00000000000000bc binstr
ffff0000400840d8 g     F .text	0000000000000024 _putc
ffff0000400e4008 g     O .bss	0000000000000004 lock
ffff00004008c49c g       .text	0000000000000000 get_all_sysregs
ffff000040089d48 g     F .text	0000000000000068 timer_init_second
ffff000040084b40 g     F .text	0000000000000028 print_info
ffff0000400840fc g     F .text	0000000000000020 _getc
ffff0000400ed414 g     O .bss	0000000000000004 rx_write_idx
ffff000040083800 g       .text	0000000000000000 exception_vector_base_el2
ffff0000400842f4 g     F .text	0000000000000104 gic_virtual_init
ffff0000400a24e8 g       .data	0000000000000000 __shell_bin_end
ffff000040088390 g     F .text	0000000000000128 test_find_contiguous_free_pages
ffff000040080928 g     F .text	00000000000000a8 copy_fs
ffff000040084a08 g     F .text	0000000000000064 io_init
ffff000040087040 g     F .text	000000000000026c memory_create_map
ffff000040084a6c g     F .text	0000000000000028 getc
ffff0000400878ec g     F .text	00000000000001bc _copy_page_table
ffff00004008994c g     F .text	0000000000000070 memcpy
ffff0000400ee000 g     O .bss	0000000000000008 cacheline_bytes
ffff00004008b7e4 g     F .text	00000000000000f4 vgicd_write
ffff000040087490 g     F .text	0000000000000154 create_uvm
ffff000040084048 g       .text	0000000000000000 guest_entry
ffff000040082840 g       .text	0000000000000000 trigger_exception
ffff0000400849dc g     F .text	000000000000002c io_early_init
ffff00004008a154 g     F .text	0000000000000024 set_tcb_pgdir
ffff000040084ac4 g     F .text	000000000000007c uart_putstr
ffff000040082a18 g     F .text	0000000000000074 handle_irq_exception
ffff0000400807d8 g     F .text	00000000000000a8 copy_guest
ffff000040082a8c g     F .text	0000000000000030 invalid_exception
ffff000040086940 g     F .text	0000000000000060 bitmap_find_first_free
ffff0000400856a0 g     F .text	00000000000008b0 vsnprintf
ffff00004008654c g     F .text	000000000000000c get_kpgdir
ffff00004008b458 g     F .text	0000000000000044 exit_process
ffff00004008409c g     F .text	000000000000003c _debug
ffff00004008939c g     F .text	0000000000000034 get_ept_entry
ffff00004008a930 g     F .text	000000000000000c get_task_manager
ffff0000400863a0 g     F .text	0000000000000024 get_cache_line_size
ffff00004008c2a4 g     F .text	000000000000001c v_timer_handler
ffff000040084f84 g     F .text	00000000000000c0 uart_early_init
ffff000040088044 g     F .text	00000000000000a8 assert_bitmap_state
ffff0000400845d4 g     F .text	0000000000000074 gic_disable_int
ffff00004010edb0 g     O .bss	0000000000004440 _state
ffff00004008a7a8 g     F .text	0000000000000144 el1_idle_init
ffff00004008a774 g     F .text	0000000000000034 get_idle_sp_top
ffff000040084d08 g     F .text	00000000000000a0 uart_advance_putc
ffff0000400ed410 g     O .bss	0000000000000004 rx_read_idx
ffff0000400868dc g     F .text	0000000000000064 bitmap_test
ffff00004008484c g     F .text	0000000000000064 gic_make_virtual_software_sgi
ffff0000400f7900 g     O .bss	0000000000004a00 g_cpu_dec
ffff000040087dd4 g     F .text	0000000000000138 mutex_test_minus
ffff00004008a65c g     F .text	000000000000002c vm_out
ffff0000401131f8 g       .bss	0000000000000000 __bss_end
ffff00004008a3fc g     F .text	0000000000000124 schedule
ffff000040082e80 g     F .text	0000000000000084 handle_irq_exception_el2
ffff00004008c3f8 g       .text	0000000000000000 save_sysregs
ffff000040087430 g     F .text	0000000000000060 memory_free_page
ffff00004008a090 g     F .text	00000000000000c4 create_task
ffff000040089018 g     F .text	0000000000000040 apply_ept
ffff00004008411c g     F .text	0000000000000024 _sleep_tick
ffff0000400864fc g     F .text	0000000000000030 enable_mmu
ffff0000400841a8 g     F .text	00000000000000ac gic_test_init
ffff000040080278 g     F .text	000000000000008c test_types
ffff00004008b768 g     F .text	000000000000002c get_vgic
ffff000040084648 g     F .text	0000000000000088 gic_get_enable
ffff000040088904 g     F .text	0000000000000114 test_uvm_alloc_free
ffff000040080424 g     F .text	000000000000004c kernel_main
ffff000040084930 g     F .text	000000000000001c gic_elsr0
ffff000040089db0 g     F .text	0000000000000078 timer_init
ffff0000400d4000 g       .bss	0000000000000000 _stack_top
ffff0000400ed000 g     O .bss	0000000000000004 _gicv2
ffff000040086de4 g     F .text	0000000000000030 kfree_page
ffff0000400ed010 g     O .bss	0000000000000400 rx_buffer
ffff0000400b4000 g     O .bss	0000000000018000 bitmap_buffer
ffff0000400ed418 g     O .bss	0000000000000400 tx_buffer
ffff00004008c678 g     F .text	0000000000000094 list_insert_first
ffff0000400896b0 g     F .text	0000000000000070 strcat
ffff000040088a18 g     F .text	00000000000001d4 test_copydata_to_uvm
ffff0000400862d4 g     F .text	000000000000003c print_binstr
ffff000040085fdc g     F .text	0000000000000068 vprintf
ffff000040082c70 g     F .text	000000000000004c advance_pc
ffff00004008acac g     F .text	0000000000000048 task_set_wakeup
ffff000040086a84 g     F .text	0000000000000140 mark_kernel_memory_allocated
ffff000040080210 g     F .text	0000000000000068 test_mem
ffff0000400809d0 g     F .text	000000000000007c mmio_map_gicd
ffff000040084254 g     F .text	0000000000000070 gic_init
ffff0000400f3060 g     O .bss	0000000000000008 ept_L2_root
ffff000040088c94 g     F .text	000000000000006c validate_page_table
ffff00004008b4e8 g       .text	0000000000000000 get_el
ffff000040089b24 g     F .text	000000000000006c memchr
ffff000040084710 g     F .text	0000000000000040 gic_set_ipriority
ffff000040087b20 g     F .text	0000000000000128 copydata_to_uvm
ffff000040080000 g       .text	0000000000000000 _start
ffff000040084c88 g     F .text	0000000000000080 uart_advance_getc
ffff000040089058 g     F .text	0000000000000344 guest_ept_init
ffff000040089858 g     F .text	0000000000000094 strstr
ffff000040084750 g     F .text	0000000000000040 gic_set_icenabler
ffff00004008494c g     F .text	0000000000000040 gic_write_lr
ffff000040101608 g     O .bss	0000000000008000 cpu_sysregs
ffff00004008b5a4 g     F .text	00000000000000a8 mutex_lock
ffff00004008b9d4 g     F .text	00000000000007c4 intc_handler
ffff000040082bc4 g     F .text	0000000000000034 handle_irq_exception_el3
ffff000040080ac8 g     F .text	0000000000000194 hyper_main
ffff0000400866d4 g     F .text	000000000000007c bitmap_set_range
ffff0000400b3140 g       .data	0000000000000000 __testapp_bin_end
ffff0000400ed81c g     O .bss	0000000000000004 tx_write_idx
ffff00004008974c g     F .text	0000000000000090 strncmp
ffff000040087aa8 g     F .text	000000000000003c destroy_uvm_4level
ffff0000400846d0 g     F .text	0000000000000040 gic_set_isenabler
ffff000040087c48 g     F .text	0000000000000054 memory_copy_uvm_4level
ffff00004008c2dc g     F .text	000000000000005c vm_init
ffff000040081840 g       .text	0000000000000000 el0_tesk_entry
ffff000040086bc4 g     F .text	0000000000000090 alloctor_init
ffff00004008288c g     F .text	0000000000000154 handle_sync_exception
ffff00004008ac34 g     F .text	0000000000000078 task_set_sleep
ffff000040088d00 g     F .text	00000000000001e4 test_memory_copy_uvm_4level
ffff000040082cbc g     F .text	00000000000001c4 handle_sync_exception_el2
ffff000040087790 g     F .text	000000000000015c _destroy_page_table
ffff0000400829ec g     F .text	000000000000002c irq_install
ffff0000400843f8 g     F .text	000000000000001c gic_read_iar
ffff0000400899bc g     F .text	0000000000000090 memcmp
ffff00004008498c g     F .text	0000000000000028 gic_set_np_int
ffff000040087f0c g     F .text	000000000000006c mutex_test_print
ffff00004008b570 g     F .text	0000000000000034 mutex_init
ffff00004008aa68 g     F .text	0000000000000048 can_run_on_core
ffff000040091890 g       .data	0000000000000000 __guset_dtb_start
ffff00004008c198 g     F .text	000000000000010c vgic_inject
ffff000040088664 g     F .text	00000000000002a0 test_memory_create_map
ffff0000400844e4 g     F .text	00000000000000f0 gic_enable_int
ffff0000400b3c00 g       .bss	0000000000000000 __bss_start
ffff00004008b16c g     F .text	000000000000027c process_init
ffff0000400898ec g     F .text	0000000000000060 memset
ffff00004008a630 g     F .text	000000000000002c vm_in
ffff00004008940c g     F .text	000000000000005c gva_to_ipa
ffff00004008a2c4 g     F .text	000000000000008c schedule_init_local
ffff00004008b794 g     F .text	0000000000000050 virtual_gic_register_int
ffff00004008a720 g     F .text	0000000000000008 idle_task_el1
ffff00004008b3e8 g     F .text	0000000000000070 run_process
ffff000040089468 g     F .text	00000000000000bc data_abort_handler
ffff000040080470 g     F .text	000000000000007c second_kernel_main
ffff000040091890 g       .data	0000000000000000 __guset_dtb_end
ffff000040084a94 g     F .text	0000000000000030 putc
ffff0000400872ac g     F .text	0000000000000014 current_page_dir
ffff000040084bac g     F .text	00000000000000dc uart_advance_init
ffff0000400897dc g     F .text	0000000000000028 strcmp
ffff0000400875e4 g     F .text	00000000000001ac _destroy_page_table_vm
ffff00004008a244 g     F .text	0000000000000080 schedule_init
ffff000040082ad4 g     F .text	00000000000000f0 handle_sync_exception_el3
ffff000040084b68 g     F .text	0000000000000028 print_warn
ffff000040084790 g     F .text	0000000000000068 gic_make_virtual_hardware_interrupt
ffff00004008c644 g     F .text	0000000000000034 list_init
ffff000040088bec g     F .text	0000000000000038 mock_addr_alloc_page
ffff000040088ee4 g     F .text	0000000000000008 kmem_test
ffff00004008a9d8 g     F .text	0000000000000090 task_set_block
ffff000040087324 g     F .text	000000000000010c memory_alloc_page
ffff000040085044 g     F .text	0000000000000058 uart_early_putc
ffff00004008652c g     F .text	0000000000000020 enable_mmu_el2
ffff00004008c338 g       .text	0000000000000000 set_stage2_pgd
ffff000040088294 g     F .text	00000000000000fc test_find_free_page
ffff00004008617c g     F .text	000000000000009c error
ffff000040091890 g       .data	0000000000000000 __shell_bin_start
ffff0000400863ec g     F .text	0000000000000110 init_page_table
ffff0000400865cc g     F .text	0000000000000108 bitmap_set
ffff00004008b64c g     F .text	00000000000000c8 mutex_unlock
ffff00004008b758 g       .text	0000000000000000 spin_unlock
ffff0000400fc300 g     O .bss	0000000000000004 task_count
ffff000040091890 g       .data	0000000000000000 __guset_fs_start
ffff000040114000 g       .bss	0000000000000000 __heap_flag
ffff000040086310 g       .text	0000000000000000 init_mmu
ffff0000400b3140 g     O .data	0000000000000800 syscall_table
ffff000040082ff0 g     F .text	000000000000002c current_spxel_irq
ffff000040091890 g       .data	0000000000000000 __guset_bin_end
ffff0000400860e0 g     F .text	000000000000009c warning
ffff000040084414 g     F .text	0000000000000018 gic_iar_irqnr
ffff0000400a24e8 g       .data	0000000000000000 __testapp_bin_start
ffff0000400880ec g     F .text	00000000000001a8 test_alloc_free
ffff0000400f3068 g     O .bss	0000000000000008 ept_L3_root
ffff000040086558 g     F .text	0000000000000074 bitmap_init
ffff00004008b714 g       .text	0000000000000000 spin_lock
ffff00004008c2c0 g     F .text	000000000000001c fake_console
ffff00004008ad84 g     F .text	00000000000000b0 alloc_process
ffff000040086860 g     F .text	000000000000007c bitmap_clear_range
ffff000040080190 g       .text	0000000000000000 test_guest
ffff000040082f04 g     F .text	00000000000000ec invalid_exception_el2
ffff00004008a8ec g     F .text	0000000000000044 task_manager_init
ffff00004008442c g     F .text	0000000000000028 gic_write_eoir
ffff000040080304 g     F .text	0000000000000120 main_entry
ffff000040080740 g     F .text	000000000000005c vtcr_init
ffff00004008a520 g     F .text	0000000000000110 timer_tick_schedule
ffff0000400869a0 g     F .text	00000000000000b4 bitmap_find_contiguous_free
ffff00004008b49c g       .text	0000000000000000 switch_context
ffff00004008e468 g       .data	0000000000000000 __guset_bin_start
ffff0000400872c0 g     F .text	0000000000000064 memory_get_paddr
ffff0000400e4000 g       .bss	0000000000000000 _stack_top_second
ffff00004008b8d8 g     F .text	00000000000000fc vgicd_read
ffff0000400ed818 g     O .bss	0000000000000004 tx_read_idx
ffff00004008447c g     F .text	0000000000000040 gic_ipi_send_single
ffff000040086dc4 g     F .text	0000000000000020 kalloc_page
ffff0000400847f8 g     F .text	0000000000000054 gic_make_virtual_software_interrupt
ffff0000400829e0 g     F .text	000000000000000c get_g_handler_vec
ffff0000400848b0 g     F .text	0000000000000030 gic_read_lr
ffff000040089670 g     F .text	0000000000000040 strlen
ffff0000400848e0 g     F .text	000000000000001c gic_lr_read_pri
ffff000040080528 g     F .text	0000000000000048 thread_info_init
ffff00004008b4f4 g       .text	0000000000000000 switch_context_el2
ffff000040089804 g     F .text	0000000000000054 strchr
ffff000040087ae4 g     F .text	000000000000003c destory_4level
ffff00004008c354 g       .text	0000000000000000 restore_sysregs
ffff00004008c7a0 g     F .text	00000000000000a0 list_delete_first
ffff0000400884b8 g     F .text	00000000000001ac test_create_uvm_find_pte
ffff0000400f4000 g     O .bss	0000000000003900 g_task_dec
ffff0000400848fc g     F .text	0000000000000018 gic_lr_read_vid
ffff000040089f3c g     F .text	0000000000000154 alloc_tcb
ffff00004008a728 g     F .text	000000000000004c get_idle
ffff000040080a4c g     F .text	000000000000007c mmio_map_gicc
ffff0000400844bc g     F .text	0000000000000028 cpu_num
ffff000040089d04 g     F .text	0000000000000044 handle_timer_interrupt
ffff000040088c24 g     F .text	0000000000000070 validate_memory_content
ffff00004008a688 g     F .text	0000000000000060 save_cpu_ctx
ffff00004008a6e8 g     F .text	0000000000000038 switch_context_el
ffff000040086e14 g     F .text	000000000000022c find_pte
ffff000040084da8 g     F .text	00000000000001c0 uart_interrupt_handler
ffff00004008a178 g     F .text	00000000000000cc craete_vm_task
"""

lines = data.strip().splitlines()
# 用空格或多个空格分隔，取第一个元素作为地址进行排序
sorted_lines = sorted(lines, key=lambda line: int(line.strip().split()[0], 16))

# 输出结果
for line in sorted_lines:
    print(line)
