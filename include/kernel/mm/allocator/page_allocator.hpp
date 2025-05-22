#pragma once

#include <cstdint>
#include <utility>

#include "arch/arch.hpp"
#include "lib/assert.hpp"
#include "lib/type_utility.hpp"

namespace kernel::mem
{
    // Fix me: the class is not capable for concurrency at present
    class page_allocator : public static_singleton<page_allocator>, no_copy_move
    {
    private:
        struct free_block_t
        {
            uint64_t page_count;
            free_block_t *next;
        };

        free_block_t *next_;

    public:
        using va_t = arch_traits<current_arch>::va_t<>;

        static constexpr auto &page_size = arch_traits<current_arch>::page_size;

    public:
        page_allocator(va_t start, va_t end) noexcept
        {
            kassert(start && end, "initializing page allocator with null pointer is invalid");
            kassert(start.is_align_by(page_size) && end.is_align_by(page_size), "managed memory boundary misaligned");
            kassert(end.address() > start.address(), "invalid memory range");

            auto diff_pages = (end.address() - start.address()) % page_size;
            next_ = ::new (start) free_block_t{.page_count = diff_pages, .next = nullptr};
        }

        [[nodiscard]] va_t alloc_page(void) noexcept
        {
            return alloc_page(1);
        }

        [[nodiscard]] va_t alloc_page(size_t page_counts) noexcept
        {
            // Fix me: use optional<va_t>
            if (next_ == nullptr || page_counts == 0)
                return va_t{nullptr};

            // find the first valid block
            free_block_t **visit_p = &next_;
            for (; (*visit_p)->page_count < page_counts; visit_p = &((*visit_p)->next))
                if ((*visit_p)->next == nullptr) [[unlikely]]
                    return va_t{nullptr};

            if ((*visit_p)->page_count == page_counts)
                return va_t{std::exchange(*visit_p, (*visit_p)->next)};
            else
                return va_t{std::exchange(
                    *visit_p,
                    ::new (*visit_p + page_counts * (page_size / sizeof(free_block_t)))
                        free_block_t{.page_count = (*visit_p)->page_count - page_counts, .next = (*visit_p)->next})};
        }

        void dealloc_page(va_t pages, size_t counts = 1) noexcept
        {
            kassert(pages.is_align_by(page_size), "try free misaligned memory address");

            if (pages == nullptr || counts == 0)
                return;

            // This will cause a countinuous physical memory being splitted
            next_ = ::new (pages) free_block_t{.page_count = counts, .next = next_};
        }

        ~page_allocator() noexcept
        {
            panic("Page allocator should not be destructed");
        }
    };
}