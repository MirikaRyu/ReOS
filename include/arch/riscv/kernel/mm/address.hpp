#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

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
        template <typename U>
        constexpr operator pa_t<U>(void) const noexcept
        requires std::convertible_to<T *, U *>
        {
            return pa_t<U>{address_};
        }

        explicit operator T *(void) noexcept
        {
            return reinterpret_cast<T *>(address_);
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != 0;
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
            kassert(reinterpret_cast<uint64_t>(p_address.get()) + direct_virt_mem_offset >= direct_virt_mem_start &&
                        reinterpret_cast<uint64_t>(p_address.get()) + direct_virt_mem_offset <= direct_virt_mem_end,
                    "Physical address could not be mapped");

            address_ =
                reinterpret_cast<T *>(reinterpret_cast<uint64_t>(static_cast<T *>(p_address)) + direct_virt_mem_offset);
        }

    public:
        explicit operator pa_t<T>(void) const noexcept
        {
            kassert(reinterpret_cast<uint64_t>(address_) >= direct_virt_mem_start &&
                        reinterpret_cast<uint64_t>(address_) <= direct_virt_mem_end,
                    "Virtual memory address out of range");

            return pa_t<T>{reinterpret_cast<uint64_t>(address_) - direct_virt_mem_offset};
        }

        template <typename U>
        constexpr operator va_t<U>(void) const noexcept
        requires std::convertible_to<T *, U *>
        {
            return va_t<U>{static_cast<U *>(address_)};
        }

        constexpr operator T *(void) const noexcept
        {
            return address_;
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != nullptr;
        }

        constexpr T *operator->(void) noexcept
        requires(!std::same_as<T, void>)
        {
            return address_;
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
    };

    template <typename T>
    pa_t(va_t<T>) -> pa_t<T>;
}