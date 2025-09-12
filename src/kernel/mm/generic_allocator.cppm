module;

#include <cstddef>

module kernel.allocator;

import contract.address;

namespace kernel::mm
{
    using contract::va_t;
    using arch_mm = arch::arch_traits::mm;

    void *generic_allocator_t::allocate(size_t nbytes, [[maybe_unused]] size_t alignment) noexcept
    {
        if (nbytes <= SLAB_THRESHOLD) [[likely]]
            return slab_allocator_t::alloc_byte(nbytes).to<void>();
        else if (nbytes <= PAGE_THRESHOLD)
            return page_allocator_t::alloc_page(lib::div_ceil(nbytes, arch_mm::PAGE_SIZE)).template to<void>();
        else
            return vpage_allocator_t::alloc_vpage(lib::div_ceil(nbytes, arch_mm::PAGE_SIZE)).template to<void>();
    }

    void generic_allocator_t::deallocate(void *addr, size_t nbytes, [[maybe_unused]] size_t alignment) noexcept
    {
        if (nbytes <= SLAB_THRESHOLD) [[likely]]
            slab_allocator_t::dealloc_byte(va_t{addr}, nbytes);
        else if (nbytes <= PAGE_THRESHOLD)
            page_allocator_t::dealloc_page(va_t{addr}, lib::div_ceil(nbytes, arch_mm::PAGE_SIZE));
        else
            vpage_allocator_t::dealloc_vpage(va_t{addr});
    }
}