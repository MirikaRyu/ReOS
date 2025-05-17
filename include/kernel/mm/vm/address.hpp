#pragma once

#include <bit>
#include <cstddef>
#include <memory>

namespace kernel::mem
{
    template <typename T = void>
    struct va_t
    {
    private:
        T *address_;

    public:
        constexpr va_t(T *address) noexcept
            : address_{address}
        {
        }

        explicit va_t(size_t address) noexcept
            : address_{reinterpret_cast<T *>(address)}
        {
        }

        constexpr operator T *(void) const noexcept
        {
            return address_;
        }
    };

    struct pa_t
    {
    };
}