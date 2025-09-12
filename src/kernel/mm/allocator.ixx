module;

#include <concepts>
#include <cstddef>

export module kernel.allocator;

import arch;
import contract.address;
import contract.allocator;
import lib.utility;

using kernel::contract::va_t;
using kernel::contract::PageAllocator;
using kernel::contract::BasicAllocator;
using kernel::contract::VPageAllocator;
using arch_mm = kernel::arch::arch_traits::mm;

export namespace kernel::mm
{
    class page_allocator_t
    {
    public:
        static constexpr auto PAGE_SIZE = arch_mm::PAGE_SIZE;

        [[nodiscard]] static va_t alloc_page(size_t page_counts = 1) noexcept;
        static void dealloc_page(va_t pages, size_t counts = 1) noexcept;

    public:
        constexpr bool operator==(const page_allocator_t &) const noexcept
        {
            return true;
        }
    };

    class slab_allocator_t
    {
    public:
        static constexpr auto MAX_SLAB_SIZE = 2048u;

        [[nodiscard]] static va_t alloc_byte(size_t size) noexcept;
        static void dealloc_byte(va_t addr, size_t size) noexcept;

    public:
        constexpr bool operator==(const slab_allocator_t &) const noexcept
        {
            return true;
        }
    };

    class vpage_allocator_t
    {
    public:
        static constexpr auto VPAGE_SIZE = arch_mm::PAGE_SIZE;

        [[nodiscard]] static va_t alloc_vpage(size_t count) noexcept;
        static void dealloc_vpage(va_t vaddr) noexcept;

    public:
        constexpr bool operator==(const vpage_allocator_t &) const noexcept
        {
            return true;
        }
    };

    class generic_allocator_t
    {
    public:
        [[nodiscard]] static void *allocate(size_t nbytes, size_t alignment = 8) noexcept;
        static void deallocate(void *addr, size_t nbytes, size_t alignment = 8) noexcept;

    public:
        constexpr bool operator==(const generic_allocator_t &) const noexcept
        {
            return true;
        }

    private:
        static constexpr auto SLAB_THRESHOLD = 2048u;
        static constexpr auto PAGE_THRESHOLD = 2 * arch_mm::PAGE_SIZE;
    };

    template <BasicAllocator Alloc>
    class adaptor_allocator_t
    {
    private:
        [[no_unique_address]] Alloc allocator_;

    public:
        adaptor_allocator_t(void) noexcept
        requires std::default_initializable<Alloc>
            : allocator_{}
        {
        }

        adaptor_allocator_t(const Alloc &alloc) noexcept
            : allocator_{alloc}
        {
        }

        [[nodiscard]] void *allocate(size_t nbytes, size_t alignment = 8) noexcept
        {
            (void)alignment;

            if constexpr (PageAllocator<Alloc>)
                return allocator_.alloc_page(lib::div_ceil(nbytes, arch_mm::PAGE_SIZE)).template to<void>();
            else if constexpr (VPageAllocator<Alloc>)
                return allocator_.alloc_vpage(lib::div_ceil(nbytes, arch_mm::PAGE_SIZE)).template to<void>();
            else
                return allocator_.alloc_byte(nbytes).template to<void>();
        }

        void deallocate(void *addr, size_t nbytes, size_t alignment = 8) noexcept
        {
            (void)alignment;

            if constexpr (PageAllocator<Alloc>)
                allocator_.dealloc_page(va_t{addr}, lib::div_ceil(nbytes, arch_mm::PAGE_SIZE));
            else if constexpr (VPageAllocator<Alloc>)
                allocator_.dealloc_page(va_t{addr});
            else
                allocator_.dealloc_byte(va_t{addr}, nbytes);
        }

    public:
        constexpr bool operator==(const adaptor_allocator_t &) const noexcept
        {
            return true;
        }
    };

    void allocator_init(va_t mem_start, va_t mem_end) noexcept;
    void allocator_pagetable_init(arch_mm::pagetable_t<page_allocator_t> &kernel_pgtbl) noexcept;
}