export module arch.riscv.panic;

export namespace kernel::arch::riscv
{
    [[noreturn]] void panic_handler(void *) noexcept
    {
        while (true)
            asm volatile("wfi");
    }
}