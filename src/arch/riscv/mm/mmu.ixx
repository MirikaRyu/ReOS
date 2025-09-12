module;

#include <cstddef>
#include <cstdint>

export module arch.riscv.mmu;

import arch.riscv.sbi;
import contract.address;

using kernel::contract::pa_t;
using kernel::contract::va_t;

export namespace kernel::arch::riscv
{
    void tlb_flush(void) noexcept
    {
        asm volatile("sfence.vma");
    }

    void tlb_flush(va_t va) noexcept
    {
        asm volatile("sfence.vma %0" ::"r"(va.address()) : "memory");
    }

    void remote_tlb_flush(void) noexcept
    {
        // (SBI-rfence EID, SFENCE.VMA FID, hart base, hart mask, start, size)
        sbi_call(0x52464E43, 1, 0, -1, 0, 0);
    }

    void remote_tlb_flush(va_t start, size_t len) noexcept
    {
        // (SBI-rfence EID, SFENCE.VMA FID, hart base, hart mask, start, size)
        sbi_call(0x52464E43, 1, 0, -1, start.address(), len);
    }

    pa_t get_pagetable_base(void) noexcept
    {
        uint64_t satp{};
        asm volatile("csrr %0, satp" : "=r"(satp));

        return pa_t{satp << 12u};
    }

    void set_pagetable_base(pa_t l2_address) noexcept
    {
        asm volatile("csrw satp, %0" ::"r"((l2_address.get() >> 12u) | (0b1000ul << 60u)));
    }
};
