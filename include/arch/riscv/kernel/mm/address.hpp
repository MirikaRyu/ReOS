#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "lib/assert.hpp"

namespace riscv::mem
{
    inline constexpr auto direct_virt_mem_offset = 0xffffffc000000000;
    inline constexpr auto direct_virt_mem_start = direct_virt_mem_offset;
    inline constexpr auto direct_virt_mem_end = 0xffffffe000000000;

    template <typename T = void>
    struct pa_t
    {
    private:
        uint64_t address_;

    public:
        constexpr explicit pa_t(uint64_t address) noexcept
            : address_{address}
        {
        }

        [[nodiscard]] constexpr uint64_t get(void) const noexcept
        {
            return address_;
        }

    public:
        explicit operator T *(void) noexcept
        {
            return reinterpret_cast<T *>(address_);
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != 0;
        }

    public:
        constexpr ptrdiff_t operator-(pa_t addr) const noexcept
        requires(!std::is_void_v<T>)
        {
            return address_ - addr.address_;
        }

        constexpr pa_t operator+(ptrdiff_t offset) const noexcept
        requires(!std::is_void_v<T>)
        {
            return pa_t{address_ + offset};
        }

        friend constexpr pa_t operator+(ptrdiff_t offset, pa_t addr) noexcept
        requires(!std::is_void_v<T>)
        {
            return addr + offset;
        }

    public:
        [[nodiscard]] bool can_transform(void) const noexcept
        {
            return reinterpret_cast<uint64_t>(address_) <= direct_virt_mem_end - direct_virt_mem_start;
        }

        [[nodiscard]] constexpr uint64_t align(void) const noexcept
        {
            return 1 << std::countr_zero(address_);
        }

        [[nodiscard]] constexpr bool is_align_by(uint64_t align) const noexcept
        {
            return address_ % align == 0;
        }
    };

    template <typename T = void>
    struct va_t
    {
    private:
        T *address_;

    public:
        constexpr explicit va_t(T *address) noexcept
            : address_{address}
        {
        }

        explicit va_t(uint64_t address) noexcept
            : address_{reinterpret_cast<T *>(address)}
        {
        }

        explicit va_t(pa_t<T> p_address) noexcept
        {
            address_ =
                reinterpret_cast<T *>(reinterpret_cast<uint64_t>(static_cast<T *>(p_address)) + direct_virt_mem_offset);

            kassert(can_transform(), "Physical address could not be mapped");
        }

    public:
        explicit operator pa_t<T>(void) const noexcept
        {
            kassert(can_transform(), "Virtual memory address out of range");

            return pa_t<T>{reinterpret_cast<uint64_t>(address_) - direct_virt_mem_offset};
        }

        constexpr operator T *(void) const noexcept
        {
            return address_;
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != nullptr;
        }

        constexpr T *operator->(void) const noexcept
        requires(!std::is_void_v<T>)
        {
            return address_;
        }

        constexpr decltype(auto) operator*(void) const noexcept
        requires(!std::is_void_v<T>)
        {
            return (*address_);
        }

        constexpr ptrdiff_t operator-(va_t addr) const noexcept
        requires(!std::is_void_v<T>)
        {
            return address_ - addr.address_;
        }

        constexpr va_t operator+(ptrdiff_t offset) const noexcept
        requires(!std::is_void_v<T>)
        {
            return va_t{address_ + offset};
        }

        friend constexpr va_t operator+(ptrdiff_t offset, va_t addr) noexcept
        requires(!std::is_void_v<T>)
        {
            return addr + offset;
        }

    public:
        enum ppn_level : uint8_t
        {
            L0,
            L1,
            L2,
        };

        [[nodiscard]] uint16_t get_pte_idx(ppn_level level) const noexcept
        {
            const auto &idx = static_cast<uint8_t>(level);
            kassert(idx < 3);

            const auto mask = 0x1ffu << (12u + idx * 9u);
            return static_cast<uint16_t>((reinterpret_cast<uint64_t>(address_) & mask) >> (12u + idx * 9u));
        }

        [[nodiscard]] uint16_t get_page_offset(void) const noexcept
        {
            return static_cast<uint16_t>((reinterpret_cast<uint64_t>(address_) & 0xfffu));
        }

    public:
        [[nodiscard]] bool can_transform(void) const noexcept
        {
            return reinterpret_cast<uint64_t>(address_) >= direct_virt_mem_start &&
                   reinterpret_cast<uint64_t>(address_) <= direct_virt_mem_end;
        }

        [[nodiscard]] uint64_t align(void) const noexcept
        {
            return 1 << std::countr_zero(reinterpret_cast<uint64_t>(address_));
        }

        [[nodiscard]] bool is_align_by(uint64_t align) const noexcept
        {
            return reinterpret_cast<uint64_t>(address_) % align == 0;
        }

        [[nodiscard]] uint64_t address(void) const noexcept
        {
            return reinterpret_cast<uint64_t>(address_);
        }
    };

    template <typename T>
    pa_t(va_t<T>) -> pa_t<T>;
}