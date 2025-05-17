#pragma once

#include <cstddef>
#include <cstdint>

#include "kernel/mm/allocator/allocator_trait.hpp"
#include "lib/assert.hpp"

#include "address.hpp"
#include "lib/panic.hpp"
#include "lib/type_utility.hpp"

namespace riscv::mem
{
    class page_allocator_tmp final : public kernel::mem::page_allocator_base
    {
    private:
        uint64_t start_;
        size_t page_count_;

    public:
        explicit page_allocator_tmp(pa_t<> start /* 4K aligned */) noexcept
            : start_{start.get()}, page_count_{}
        {
            kassert(start && (start.get() % 4096 == 0));
        }

        [[nodiscard]] pa_t<> alloc_page(void) noexcept
        {
            return pa_t<>{start_ + (page_count_++) * 4096};
        }

        [[nodiscard]] pa_t<> alloc_page([[maybe_unused]] size_t size) noexcept
        {
            return alloc_page();
        }

        void dealloc_page([[maybe_unused]] va_t<> addr) noexcept
        {
        }

        [[nodiscard]] pa_t<> begin(void) const noexcept
        {
            return pa_t<>{start_};
        }

        [[nodiscard]] pa_t<> end(void) const noexcept
        {
            return pa_t<>{start_ + page_count_ * 4096};
        }
    };

    class page_allocator final : public kernel::mem::page_allocator_base
    {
    };

    class vpage_allocator final : public kernel::mem::vpage_allocator_base
    {
    };

    class slub_allocator final : public kernel::mem::slub_allocator_base
    {
    };
}