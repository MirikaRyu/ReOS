#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "address.hpp"
#include "kernel/mm/allocator/allocator_trait.hpp"
#include "lib/assert.hpp"
#include "lib/panic.hpp"
#include "lib/type_utility.hpp"

namespace riscv::mem
{
    class page_allocator_tmp final : public kernel::mem::page_allocator_base, no_copy_move
    {
        friend class page_allocator_tmp_proxy;

    private:
        const uint64_t start_;
        size_t page_count_;

    public:
        explicit page_allocator_tmp(pa_t<> start) noexcept
            : start_{start.get()}, page_count_{}
        {
            kassert(start && start.is_align_by(page_size));
        }
    };

    class page_allocator_tmp_proxy
    {
    private:
        page_allocator_tmp *alloc_;

    public:
        explicit page_allocator_tmp_proxy(page_allocator_tmp &tmp_allocator) noexcept
            : alloc_{&tmp_allocator}
        {
        }

        [[nodiscard]] pa_t<> alloc_page(void) noexcept
        {
            return pa_t<>{alloc_->start_ + (alloc_->page_count_++) * page_size};
        }

        [[nodiscard]] pa_t<> alloc_page(size_t) noexcept
        {
            panic("allocate multiple pages is not available in this allocator");
            std::unreachable();
        }

        void dealloc_page(pa_t<>) noexcept
        {
        }

        [[nodiscard]] pa_t<> begin(void) const noexcept
        {
            return pa_t<>{alloc_->start_};
        }

        [[nodiscard]] pa_t<> end(void) const noexcept
        {
            return pa_t<>{alloc_->start_ + alloc_->page_count_ * page_size};
        }
    };
}