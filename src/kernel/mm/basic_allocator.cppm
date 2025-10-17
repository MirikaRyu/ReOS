module;

#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

module kernel.allocator;

import arch;
import contract.address;
import contract.page_table;
import kernel.spin;
import lib.list;
import lib.panic;
import lib.assert;
import lib.utility;
import lib.type_utility;

namespace kernel::mm
{
    using contract::va_t;
    using contract::perms;
    using arch_mm = arch::arch_traits::mm;
    using kpagetable_t = arch_mm::pagetable_t<page_allocator_t>;

    namespace details
    {
        class page_allocator_impl : public lib::static_singleton<page_allocator_impl>
        {
        private:
            struct free_block_t
            {
                uint64_t page_count;
                free_block_t *next;
            };

            free_block_t *next_;
            sync::spinlock_t lock_{};

        public:
            page_allocator_impl(va_t start, va_t end) noexcept
            {
                lib::kassert(start && end, "Initializing page allocator with null pointer is invalid");
                lib::kassert(start.is_align_to(arch_mm::PAGE_SIZE) && end.is_align_to(arch_mm::PAGE_SIZE),
                             "Managed memory boundary misaligned");
                lib::kassert(end.address() > start.address(), "Invalid memory range");

                uint64_t diff_pages = (end - start) / arch_mm::PAGE_SIZE;
                next_ = ::new (start.to<void>()) free_block_t{.page_count = diff_pages, .next = nullptr};
            }

            ~page_allocator_impl() noexcept
            {
                lib::panic("Page allocator should not be destructed");
            }

        public:
            [[nodiscard]] va_t alloc_page(size_t page_counts) noexcept
            {
                lib::lock_guard guard{lock_};

                if (next_ == nullptr || page_counts == 0)
                    return {};

                // Find the first valid block
                free_block_t **visit_p = &next_;
                for (; (*visit_p)->page_count < page_counts; visit_p = &((*visit_p)->next))
                    if ((*visit_p)->next == nullptr) [[unlikely]]
                        return {};

                if ((*visit_p)->page_count == page_counts)
                    return va_t{std::exchange(*visit_p, (*visit_p)->next)};
                else
                    return va_t{std::exchange(
                        *visit_p,
                        ::new (*visit_p + page_counts * (arch_mm::PAGE_SIZE / sizeof(free_block_t))) free_block_t{
                            .page_count = (*visit_p)->page_count - page_counts,
                            .next = (*visit_p)->next,
                        })};
            }

            void dealloc_page(va_t pages, size_t counts) noexcept
            {
                lib::kassert(pages.is_align_to(arch_mm::PAGE_SIZE), "Try to free misaligned memory address");

                if (!pages || counts == 0)
                    return;

                lib::lock_guard guard{lock_};

                // This will cause a countinuous physical memory being splitted
                next_ = ::new (pages.to<void>()) free_block_t{.page_count = counts, .next = next_};
            }
        };

        class slab_allocator_impl : public lib::static_singleton<slab_allocator_impl>
        {
        private:
            enum struct slab_size
            {
                B8,
                B16,
                B32,
                B64,
                B96,
                B128,
                B192,
                B256,
                B512,
                KiB1,
                KiB2,
                count,
            };

            static constexpr slab_size size_to_enum(size_t size) noexcept
            {
                switch (size)
                {
                case 8:
                    return slab_size{0};
                case 16:
                    return slab_size{1};
                case 32:
                    return slab_size{2};
                case 64:
                    return slab_size{3};
                case 96:
                    return slab_size{4};
                case 128:
                    return slab_size{5};
                case 192:
                    return slab_size{6};
                case 256:
                    return slab_size{7};
                case 512:
                    return slab_size{8};
                case 1024:
                    return slab_size{9};
                case 2048:
                    return slab_size{10};
                [[unlikely]] default:
                    lib::panic("Invalid slab size");
                    std::unreachable();
                }
            }

            static constexpr uint16_t enum_to_size[] = {
                8,
                16,
                32,
                64,
                96,
                128,
                192,
                256,
                512,
                1024,
                2048,
            };

            struct free_obj_t
            {
                free_obj_t *next;
            };

            free_obj_t *start_[lib::idx(slab_size::count)];
            sync::spinlock_t lock_;

        public:
            slab_allocator_impl(void) noexcept
                : start_{}
            {
            }

            ~slab_allocator_impl() noexcept
            {
                lib::panic("Slab allocator should not be destructed");
            }


        public:
            [[nodiscard]] va_t alloc_byte(size_t size) noexcept
            {
                if (size == 0) [[unlikely]]
                    return {};
                else if (size > enum_to_size[lib::idx(slab_size::KiB2)]) [[unlikely]]
                    lib::panic("Try to allocate too much memory from slab allocator");

                const auto slab_idx = lib::idx(size_to_enum(std::bit_ceil(size)));

                lib::lock_guard guard{lock_};

                if (start_[slab_idx] == nullptr) [[unlikely]]
                {
                    if (const auto va = page_allocator_t{}.alloc_page()) [[likely]]
                        start_[slab_idx] = ::new (va.to<void>()) free_obj_t{nullptr};
                    else
                        return {};

                    free_obj_t *builder{start_[slab_idx]};
                    for (auto i = 1; i < arch_mm::PAGE_SIZE / enum_to_size[slab_idx]; ++i)
                    {
                        builder->next =
                            ::new (reinterpret_cast<std::byte *>(start_[slab_idx]) + i * enum_to_size[slab_idx])
                                free_obj_t{nullptr};
                        builder = builder->next;
                    }
                }

                return va_t{std::exchange(start_[slab_idx], start_[slab_idx]->next)};
            }

            void dealloc_byte(va_t addr, size_t size) noexcept
            {
                if (!addr || size == 0 || size > enum_to_size[lib::idx(slab_size::KiB2)]) [[unlikely]]
                    return;

                const auto slab_sz = std::bit_ceil(size);
                const auto slab_idx = lib::idx(size_to_enum(slab_sz));

                lib::kassert(addr.is_align_to(slab_sz), "Try to free misaligned memory address");

                lib::lock_guard guard{lock_};

                start_[slab_idx] = ::new (addr.to<void>()) free_obj_t{start_[slab_idx]};
            }
        };

        class vpage_allocator_impl : public lib::static_singleton<vpage_allocator_impl>
        {
        private:
            struct region_t
            {
                va_t start;
                size_t page_count;
            };

            lib::forward_list<region_t, adaptor_allocator_t<slab_allocator_t>> occupy_list_;
            kpagetable_t *pagetable_;
            sync::spinlock_t lock_{};

            va_t do_alloc(va_t vaddr, size_t count, auto it) noexcept
            {
                for (size_t i = 0; i < count; i++)
                {
                    auto page = page_allocator_t::alloc_page();
                    if (!page) [[unlikely]]
                    {
                        for (size_t j = 0; j < i; j++)
                            pagetable_->del_mapping(vaddr + j * arch_mm::PAGE_SIZE); // Not visited, don't flush
                        return {};
                    }

                    pagetable_->add_mapping(
                        vaddr + i * arch_mm::PAGE_SIZE, arch_mm::to_pa(page), perms::R | perms::W | perms::X);
                }

                occupy_list_.emplace_after(it, vaddr, count);

                return vaddr;
            }

        public:
            vpage_allocator_impl(kpagetable_t &table) noexcept
                : occupy_list_{}, pagetable_{&table}
            {
            }

            ~vpage_allocator_impl() noexcept
            {
                lib::panic("VPage allocator resource should not be destructed");
            }

            [[nodiscard]] va_t alloc_vpage(size_t count) noexcept
            {
                lib::lock_guard guard{lock_};

                const int64_t size_count = count * arch_mm::PAGE_SIZE;

                auto start = va_t{arch_mm::VMALLOC_START};
                auto before_it = occupy_list_.before_begin();
                for (auto it = occupy_list_.begin(); it != occupy_list_.end(); ++it)
                {
                    if (it->start - start >= size_count)
                        return do_alloc(start, count, before_it);

                    start = it->start + it->page_count * arch_mm::PAGE_SIZE;
                    before_it = it;
                }

                if (va_t{arch_mm::VMALLOC_END} - start >= size_count)
                    return do_alloc(start, count, before_it);
                else
                    return {};
            }

            void dealloc_vpage(va_t vaddr) noexcept
            {
                if (!vaddr) [[unlikely]]
                    return;

                {
                    lib::lock_guard guard{lock_};

                    for (auto it = occupy_list_.before_begin(); ++auto{it} != occupy_list_.end(); ++it)
                    {
                        if (auto next = ++auto{it}; next->start == vaddr)
                        {
                            for (size_t i = 0; i < next->page_count; i++)
                            {
                                const auto addr = vaddr + i * arch_mm::PAGE_SIZE;

                                page_allocator_t::dealloc_page(arch_mm::to_va(pagetable_->transform(addr)));
                                pagetable_->del_mapping(addr);
                                arch_mm::tlb_flush(addr);
                            }

                            arch_mm::remote_tlb_flush(vaddr, next->page_count * arch_mm::PAGE_SIZE);

                            occupy_list_.erase_after(it);
                            return;
                        }
                    }
                }

                lib::panic("Unmapped virtual address");
                std::unreachable();
            }
        };
    }

    va_t page_allocator_t::alloc_page(size_t page_counts) noexcept
    {
        return details::page_allocator_impl::instance().alloc_page(page_counts);
    }

    void page_allocator_t::dealloc_page(va_t pages, size_t counts) noexcept
    {
        details::page_allocator_impl::instance().dealloc_page(pages, counts);
    }

    va_t slab_allocator_t::alloc_byte(size_t size) noexcept
    {
        return details::slab_allocator_impl::instance().alloc_byte(size);
    }

    void slab_allocator_t::dealloc_byte(va_t addr, size_t size) noexcept
    {
        details::slab_allocator_impl::instance().dealloc_byte(addr, size);
    }

    va_t vpage_allocator_t::alloc_vpage(size_t count) noexcept
    {
        return details::vpage_allocator_impl::instance().alloc_vpage(count);
    }

    void vpage_allocator_t::dealloc_vpage(va_t vaddr) noexcept
    {
        details::vpage_allocator_impl::instance().dealloc_vpage(vaddr);
    }

    void allocator_init(va_t mem_start, va_t mem_end) noexcept
    {
        details::page_allocator_impl::construct(mem_start, mem_end);
        details::slab_allocator_impl::construct();
    }

    void allocator_pagetable_init(arch_mm::pagetable_t<page_allocator_t> &kernel_pgtbl) noexcept
    {
        details::vpage_allocator_impl::construct(kernel_pgtbl);
    }
}