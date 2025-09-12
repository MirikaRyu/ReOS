export module contract.arch_base;

export namespace kernel::contract
{
    enum class archs
    {
        X64,
        RISCV,
        AARCH64,
        LOONGARCH,
    };

    template <archs current>
    struct generic_arch_traits
    {
        static_assert(false, "Unimplemented architecture");
    };
}