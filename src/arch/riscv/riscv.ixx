module;

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string_view>

export module arch.riscv;

import arch.riscv.mmu;
import arch.riscv.panic;
import arch.riscv.address;
import arch.riscv.page_table;
import contract.address;
import contract.arch_base;
import contract.allocator;

namespace riscv = kernel::arch::riscv;
using kernel::contract::pa_t;
using kernel::contract::va_t;

export namespace kernel::contract
{
    template <>
    struct generic_arch_traits<archs::RISCV>
    {
        struct mm
        {
            template <PageAllocator Alloc>
            using pagetable_t = riscv::pagetable_t<Alloc>;

            static constexpr uint16_t PAGE_SIZE = riscv::PAGE_SIZE;
            static constexpr uint32_t MID_PAGE_SIZE = riscv::MID_PAGE_SIZE;
            static constexpr uint32_t HUGE_PAGE_SIZE = riscv::HUGE_PAGE_SIZE;

            static constexpr uint32_t USER_START = riscv::USER_START;
            static constexpr uint64_t USER_END = riscv::USER_END;

            static constexpr auto PHYS_TO_VIRT_OFFSET = riscv::PHYS_TO_VIRT_OFFSET;
            static constexpr auto PHYS_VIRT_MAPPING_START = riscv::PHYS_VIRT_MAPPING_START;
            static constexpr auto PHYS_VIRT_MAPPING_END = riscv::PHYS_VIRT_MAPPING_END;

            static constexpr auto VMALLOC_START = riscv::VMALLOC_START;
            static constexpr auto VMALLOC_END = riscv::VMALLOC_END;

            static constexpr auto KERNEL_START = riscv::KERNEL_START;
            static constexpr auto KERNEL_END = riscv::KERNEL_END;

            static constexpr bool can_transform(pa_t pa) noexcept
            {
                return riscv::can_transform(pa);
            }

            static bool can_transform(va_t va) noexcept
            {
                return riscv::can_transform(va);
            }

            static va_t to_va(pa_t pa) noexcept
            {
                return riscv::to_va(pa);
            }

            static pa_t to_pa(va_t va) noexcept
            {
                return riscv::to_pa(va);
            }

            static void tlb_flush(void) noexcept
            {
                riscv::tlb_flush();
            }

            static void tlb_flush(va_t va) noexcept
            {
                riscv::tlb_flush(va);
            }

            static void remote_tlb_flush(void) noexcept
            {
                riscv::remote_tlb_flush();
            }

            static void remote_tlb_flush(va_t start, size_t len) noexcept
            {
                riscv::remote_tlb_flush(start, len);
            }

            static pa_t get_pagetable_base(void) noexcept
            {
                return riscv::get_pagetable_base();
            }

            static void set_pagetable_base(pa_t l2_address) noexcept
            {
                riscv::set_pagetable_base(l2_address);
            }
        };

        struct task
        {
            static bool is_interrupt_on(void) noexcept
            {
                // return riscv::is_interrupt_on();
                return true;
            }

            static void interrupt_on(void) noexcept
            {
                // riscv::interrupt_on();
            }

            static void interrupt_off(void) noexcept
            {
                // riscv::interrupt_off();
            }
        };

        struct lib
        {
            [[noreturn]] static void panic_handler(void *args) noexcept
            {
                riscv::panic_handler(args);
            }
        };
    };
}