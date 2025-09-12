export module arch;

import config;
import contract.arch_base;

export import arch.riscv;

export namespace kernel::arch
{
    using arch_traits = contract::generic_arch_traits<CURRENT_ARCH>;
}