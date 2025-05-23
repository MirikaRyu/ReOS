#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "arch/arch.hpp"
#include "lib/assert.hpp"
#include "lib/panic.hpp"
#include "lib/type_utility.hpp"
#include "lib/utility.hpp"
#include "page_allocator.hpp"

namespace kernel::mem
{
    class slab_allocator : public static_singleton<slab_allocator>, no_copy_move
    {
    public:
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

        using va_t = arch_traits<current_arch>::va_t<>;

    private:
        struct free_obj_t
        {
            free_obj_t *next;
        };

        free_obj_t *start_[idx(slab_size::count)];

    private:
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
            default:
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

    public:
        slab_allocator(void) noexcept
            : start_{}
        {
        }

        [[nodiscard]] va_t alloc(size_t size) noexcept
        {
            if (size == 0) [[unlikely]]
                return va_t{nullptr};
            else if (size > enum_to_size[idx(slab_size::KiB2)]) [[unlikely]]
                panic("try to allocate too much memory from slab allocator");

            const auto slab_idx = idx(size_to_enum(std::bit_ceil(size)));

            if (start_[slab_idx] == nullptr) [[unlikely]]
            {
                if (const auto va = page_allocator::instance().alloc_page().to_va()) [[likely]]
                    start_[slab_idx] = ::new (va) free_obj_t{nullptr};
                else
                    return va_t{nullptr};

                for (auto j = 1; j < arch_traits<current_arch>::page_size / enum_to_size[slab_idx]; ++j)
                {
                    start_[slab_idx][j - 1].next = start_[slab_idx] + j;
                    ::new (start_[slab_idx] + j) free_obj_t{nullptr};
                }
            }

            return va_t{std::exchange(start_[slab_idx], start_[slab_idx]->next)};
        }

        void dealloc(va_t addr, size_t size) noexcept
        {
            if (!addr || size == 0 || size > enum_to_size[idx(slab_size::KiB2)]) [[unlikely]]
                return;

            const auto slab_size = std::bit_ceil(size);
            const auto slab_idx = idx(size_to_enum(slab_size));

            kassert(addr.is_align_by(slab_size), "try to free misaligned memory address");

            start_[slab_idx] = ::new (addr) free_obj_t{start_[slab_idx]};
        }
    };
}