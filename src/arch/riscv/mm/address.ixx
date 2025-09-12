module;

#include <cstdint>

export module arch.riscv.address;

import contract.address;
import lib.assert;

using kernel::contract::pa_t;
using kernel::contract::va_t;

export namespace kernel::arch::riscv
{
    constexpr uint16_t PAGE_SIZE = 0x1000;                   // 4 KiB
    constexpr uint32_t MID_PAGE_SIZE = PAGE_SIZE * 512;      // 2 MiB
    constexpr uint32_t HUGE_PAGE_SIZE = MID_PAGE_SIZE * 512; // 1 GiB

    constexpr uint32_t USER_START = 1ull << 30; // 1 GiB
    constexpr uint64_t USER_END = 0x3fffffffff; // 256 GiB

    constexpr auto PHYS_TO_VIRT_OFFSET = 0xffffffc000000000;
    constexpr auto PHYS_VIRT_MAPPING_START = PHYS_TO_VIRT_OFFSET;
    constexpr auto PHYS_VIRT_MAPPING_END = 0xffffffe000000000;

    constexpr auto VMALLOC_START = PHYS_VIRT_MAPPING_END;
    constexpr auto VMALLOC_END = 0xfffffff400000000;

    constexpr auto KERNEL_START = 0xffffffff00000000;
    constexpr auto KERNEL_END = 0xffffffffffffffff;

    constexpr bool can_transform(pa_t pa) noexcept
    {
        // Mapping memory form 0x0 to 128GiB
        return pa.get() <= PHYS_VIRT_MAPPING_END - PHYS_VIRT_MAPPING_START;
    }

    bool can_transform(va_t va) noexcept
    {
        return PHYS_VIRT_MAPPING_START <= va.address() && va.address() <= PHYS_VIRT_MAPPING_END;
    }

    va_t to_va(pa_t pa) noexcept
    {
        lib::kassert(can_transform(pa), "Physical address is not mapped");

        return va_t{pa.get() + PHYS_TO_VIRT_OFFSET};
    }

    pa_t to_pa(va_t va) noexcept
    {
        lib::kassert(can_transform(va), "Virtual memory address out of range");

        return pa_t{va.address() - PHYS_TO_VIRT_OFFSET};
    }
}