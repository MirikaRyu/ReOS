module;

#include <concepts>
#include <cstddef>
#include <cstdint>

export module contract.allocator;

import contract.address;

template <typename T>
concept Allocator_Basic_Trait = requires {
    requires std::copyable<T>;
    requires std::movable<T>;
    requires std::equality_comparable<T>;
};

export namespace kernel::contract
{
    template <typename T>
    concept UniversalAlloctor = Allocator_Basic_Trait<T> && requires(T allocator, size_t nbytes, size_t alignment) {
        { allocator.allocate(nbytes) } noexcept -> std::same_as<void *>;
        { allocator.allocate(nbytes, alignment) } noexcept -> std::same_as<void *>;
        { allocator.deallocate(allocator.allocate(nbytes), nbytes) } noexcept -> std::same_as<void>;
        { allocator.deallocate(allocator.allocate(nbytes), nbytes, alignment) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept PageAllocator = Allocator_Basic_Trait<T> && requires(T allocator, size_t npages, va_t va) {
        { allocator.PAGE_SIZE } -> std::convertible_to<uint64_t>;

        { allocator.alloc_page() } noexcept -> std::same_as<va_t>;
        { allocator.alloc_page(npages) } noexcept -> std::same_as<va_t>;
        { allocator.dealloc_page(va, npages) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept SlabAllocator = Allocator_Basic_Trait<T> && requires(T allocator, size_t nbytes, va_t va) {
        { allocator.MAX_SLAB_SIZE } -> std::convertible_to<uint64_t>;

        { allocator.alloc_byte(nbytes) } noexcept -> std::same_as<va_t>;
        { allocator.dealloc_byte(va, nbytes) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept VPageAllocator = Allocator_Basic_Trait<T> && requires(T allocator, size_t npages, va_t va) {
        { allocator.VPAGE_SIZE } -> std::convertible_to<uint64_t>;

        { allocator.alloc_vpage(npages) } noexcept -> std::same_as<va_t>;
        { allocator.dealloc_vpage(va) } noexcept -> std::same_as<void>;
    };

    template <typename T>
    concept Allocator = UniversalAlloctor<T> || PageAllocator<T> || SlabAllocator<T> || VPageAllocator<T>;

    template <typename T>
    concept BasicAllocator = Allocator<T> && !UniversalAlloctor<T>;
}