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

        using va_t = arch_traits<current_arch>::va_t<>;

    public:
        using pa_t = arch_traits<current_arch>::pa_t<>;

        static constexpr auto &page_size = arch_traits<current_arch>::page_size;

    public:
        page_allocator(pa_t start, pa_t end) noexcept
        {
            kassert(start && end, "initializing page allocator with null pointer is invalid");
            kassert(start.is_align_by(page_size) && end.is_align_by(page_size), "managed memory boundary misaligned");
            kassert(end.get() > start.get(), "invalid memory range");

            auto diff_pages = (end.get() - start.get()) % page_size;
            next_ = ::new (va_t{start}) free_block_t{.page_count = diff_pages, .next = nullptr};
        }

        [[nodiscard]] pa_t alloc_page(void) noexcept
        {
            return alloc_page(1);
        }

        [[nodiscard]] pa_t alloc_page(size_t page_counts) noexcept
        {
            // Fix me: use optional<pa_t>
            if (next_ == nullptr || page_counts == 0)
                return pa_t{va_t{nullptr}};

            // find the first valid block
            free_block_t **visit_p = &next_;
            for (; (*visit_p)->page_count < page_counts; visit_p = &((*visit_p)->next))
                if ((*visit_p)->next == nullptr) [[unlikely]]
                    return pa_t{va_t{nullptr}};

            if ((*visit_p)->page_count == page_counts)
                return pa_t{va_t{std::exchange(*visit_p, (*visit_p)->next)}};
            else
                return pa_t{va_t{std::exchange(
                    *visit_p,
                    ::new (*visit_p + page_counts * (page_size / sizeof(free_block_t)))
                        free_block_t{.page_count = (*visit_p)->page_count - page_counts, .next = (*visit_p)->next})}};
        }

        void dealloc_page(pa_t pages, size_t counts = 1) noexcept
        {
            kassert(pages.is_align_by(page_size), "try free misaligned memory address");

            if (!pages || counts == 0)
                return;

            // This will cause a countinuous physical memory being splitted
            next_ = ::new (va_t{pages}) free_block_t{.page_count = counts, .next = next_};
        }

        ~page_allocator() noexcept
        {
            panic("Page allocator should not be destructed");
        }
    };
}