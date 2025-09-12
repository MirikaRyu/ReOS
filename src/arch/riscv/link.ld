OUTPUT_ARCH(riscv)
ENTRY(entry)

kernel_load = 0x80200000;
kernel_start = 0xffffffff00000000;

SECTIONS
{
    /* 
        Start address is set to a virtual address (refer to memory layout), 
        so the kernel will work only after the initial pagetable is set in entry.S      
    */
    . = kernel_start;

    .text ALIGN(4K) : AT(kernel_load)
    {
        text_start = .;

        /* Run entry() in entry.S to setup the initial pagetable and prepare for main() */
        KEEP(*(.text.entry))
        
        *(.text)
        *(.text.*)

        /* Support for C++ static ctors */
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))

        text_end = .;
    }

    .rodata ALIGN(4K) :
    {
        rodata_start = .;

        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)

        rodata_end = .;
    }

    .data ALIGN(4K) :
    {
        data_start = .;

        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)

        data_end = .;
    }

    .bss ALIGN(4K) :
    {
        bss_start = .;

        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)

        bss_end = .;
    }

    /* Make sure the kernel can fit into multiple pages */
    . = ALIGN(4K);

    /* Initial stack and pagetable for init hart comes here */
    .init_stuff ALIGN(4K) :
    {
        init_stuff_start = .;

        KEEP(*(.init_pgtbl))
        KEEP(*(.init_stack))

        init_stuff_end = .;
    }

    kernel_end = .;
}