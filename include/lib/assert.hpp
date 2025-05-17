#pragma once

#include "lib/panic.hpp"
#include <concepts>
#include <source_location>
#include <string_view>

template <typename T>
struct with_loc_t
{
    T val;
    std::source_location loc;

    template <typename U>
    requires std::constructible_from<T, U>
    constexpr with_loc_t(U &&val, std::source_location loc = std::source_location::current()) noexcept
        : val{val}, loc{loc}
    {
    }
};

constexpr inline void kassert(with_loc_t<bool> e, std::string_view msg = {}) noexcept
{
    if (!e.val) [[unlikely]]
        panic(msg, e.loc);
}