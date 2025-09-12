module;

#include <cstddef>
#include <cstdint>
#include <utility>

export module arch.riscv.allocator;

import arch.riscv.address;
import contract.address;
import lib.panic;
import lib.assert;
import lib.type_utility;

using kernel::contract::va_t;

export namespace kernel::arch::riscv
{
    class early_page_allocator_resource final : lib::no_copy_move
    {
        friend class early_page_allocator_t;

    private:
        const uint64_t start_;
        size_t page_count_;

    public:
        explicit early_page_allocator_resource(va_t start) noexcept
            : start_{start.address()}, page_count_{}
        {
            lib::kassert(start && start.is_align_to(PAGE_SIZE));
        }
    };

    class early_page_allocator_t final
    {
    private:
        early_page_allocator_resource *resource_;

    public:
        static constexpr auto PAGE_SIZE = riscv::PAGE_SIZE;

        explicit early_page_allocator_t(early_page_allocator_resource &early_resource) noexcept
            : resource_{&early_resource}
        {
        }

        [[nodiscard]] va_t alloc_page(void) noexcept
        {
            return va_t{resource_->start_ + (resource_->page_count_++) * PAGE_SIZE};
        }

        [[nodiscard]] va_t alloc_page(size_t) noexcept
        {
            lib::panic("Allocate multiple pages is not available in this allocator");
            std::unreachable();
        }

        void dealloc_page(va_t, size_t) noexcept
        {
        }

        [[nodiscard]] va_t begin(void) const noexcept
        {
            return va_t{resource_->start_};
        }

        [[nodiscard]] va_t end(void) const noexcept
        {
            return va_t{resource_->start_ + resource_->page_count_ * PAGE_SIZE};
        }

    public:
        constexpr bool operator==(const early_page_allocator_t &) const noexcept
        {
            return true;
        }
    };
}