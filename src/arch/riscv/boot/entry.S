# /* Setup initial memory mapping and basic execution environment */

.set load_address, 0x80200000
.set kernel_start_va, 0xffffffff00000000
.set physical_mem_map_va, 0xffffffc000000000

.section .text.entry
.global entry
entry:
    /* Generate L2 and L0 pagetable */
    call map_kernel
    call map_linear

    /* Load physical address of pagetable */
    la t0, init_pgtbl_L2

    /* Generate value for stap */
    srli t0, t0, 12
    li t1, 8 << 60
    or t0, t0, t1

    csrw satp, t0
    sfence.vma

    /* Calculate offset: t1 */
    li t1, kernel_start_va
    li t0, load_address
    sub t1, t1, t0

    /* Setup stack pointer */
    la sp, init_stack_top
    add sp, sp, t1
    
    /* Jump to main() */
    la t0, main
    add t0, t0, t1
    jr t0

map_kernel:
    /* Make pte[508] points to init_pgtbl_L1 */
    la t1, init_pgtbl_L1
    srli t1, t1, 12 // make ppn
    slli t1, t1, 10 
    or t1, t1, 0b1 // V

    la t2, init_pgtbl_L2
    li t3, 508 * 8
    add t2, t2, t3 // calculate [508]

    sd t1, 0(t2)

    /* Fill init_pgtbl_L1 */
    li t0, 0 // i = 0
    li t1, 512 // pte counts
    li t2, load_address // base physical address
    map_kernel_loop:
        bge t0, t1, map_kernel_loop_done // i < 512 ?
        
        // calculate pte
        li t4, 0x200000 // 2MiB
        mul t3, t0, t4
        add t3, t3, t2
        srli t3, t3, 12 // ppn
        slli t3, t3, 10
        or t3, t3, 0b11001111 // ADXRWV

        // store t3: pte
        li t5, 0x8 // one pte
        mul t4, t0, t5
        la t5, init_pgtbl_L1
        add t4, t4, t5 // &pte[i]
        sd t3, 0(t4)

        addi t0, t0, 1 // i++
        j map_kernel_loop

    map_kernel_loop_done:
        ret

map_linear:
    la t0, init_pgtbl_L2
    
    /* Calculate mapping start index */
    li t1, physical_mem_map_va
    li t2, 0x1ff << 30
    and t1, t1, t2
    srli t1, t1, 30

    li t2, 0 // i = 0
    li t3, 0x4 // map size
    li t4, 0x8 // one pte

    map_linear_loop:
        bge t2, t3, map_linear_loop_done

        mv t5, t2
        slli t5, t5, 28
        or t5, t5, 0b11001111 // ADXRWV
        mul t6, t1, t4 // idx * size
        add t6, t6, t0
        sd t5, 0(t6)
        addi t1, t1, 1 // idx++

        addi t2, t2, 1 // i++
        j map_linear_loop

    map_linear_loop_done:
        ret

.section .init_pgtbl, "a"
.align 12 // The physical address of pagetable has to be 4K aligned
init_pgtbl_L2:
    /* 512 entries for a pagetable entry, only use the second and last 4 entries*/
    .quad 0
    .quad 0
    
    // identical mapping to make sure that we can execute code after load new pagetable
    .quad ((0x80000000 >> 12) << 10) | 0b11001111 // ADXRWV

    // contains linear mapping and kernel mapping :
    // linear mapping for physical memory is filled later by function: map_linear
    // 0xffffffc000000000 -> pte[256] = 0x0 (start)
    // kernel mapping maps 1GiB only, have to ensure that final pagetable can be built up within 1GiB space
    // 0xffffffff00000000 -> pte[508] = 0x80200000
    .zero 8 * 509

init_pgtbl_L1:
    /* L1 page table for mapping kernel as 2MiB pages to 0xffffffff00000000 */
    /* Filled by function: map_kernel */
    .zero 8 * 512

.section .init_stack, "aw"
.align 12
init_stack:
    /* 8KiB for hart0 to do init stuff */
    .space 1024 * 8
init_stack_top: