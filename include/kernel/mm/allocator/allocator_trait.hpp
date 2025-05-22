#pragma once

#include <concepts>
#include <cstddef>

#include "arch/arch.hpp"
#include "kernel/mm/vm/address.hpp"

namespace kernel::mem
{
    struct allocator_base
    {
    };

    struct page_allocator_base : allocator_base
    {
    };

    struct slub_allocator_base : allocator_base
    {
    };

    struct vpage_allocator_base : allocator_base
    {
    };

    template <typename T>
    concept PageAllocator = requires(T allocator, typename arch_traits<current_arch>::template pa_t<> pa) {
        requires std::derived_from<T, page_allocator_base>;
        requires std::copyable<T>;
        requires std::movable<T>;

        { allocator.alloc_page() } noexcept -> std::same_as<decltype(pa)>;
        { allocator.alloc_page(size_t{}) } noexcept -> std::same_as<decltype(pa)>;
        { allocator.dealloc_page(pa, size_t{}) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept SlubAllocator = requires(T allocator, typename arch_traits<current_arch>::template va_t<> va) {
        requires std::derived_from<T, slub_allocator_base>;
        requires std::copyable<T>;
        requires std::movable<T>;

        { allocator.alloc(size_t{}) } noexcept -> std::constructible_from<decltype(va)>;
        { allocator.dealloc(va) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept VPageAllocator = requires(T allocator, typename arch_traits<current_arch>::template va_t<> va) {
        requires std::derived_from<T, vpage_allocator_base>;
        requires std::copyable<T>;
        requires std::movable<T>;

        { allocator.alloc_vpage(size_t{}) } noexcept -> std::constructible_from<decltype(va)>;
        { allocator.dealloc_vpage(va) } noexcept -> std::same_as<void>;
    };
}