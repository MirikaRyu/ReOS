#pragma once

#include <type_traits>
#include <utility>

template <typename T>
requires std::is_enum_v<T>
constexpr auto idx(const T &val) noexcept
{
    return std::to_underlying(val);
}