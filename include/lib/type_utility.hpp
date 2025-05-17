#pragma once

#include <type_traits>

template <typename T, typename... Ts>
inline constexpr bool same_type_v = (std::is_same_v<T, Ts> && ...);

template <typename T, typename... Ts>
inline constexpr bool in_types_v = (std::is_same_v<T, Ts> || ...);

template <typename T, typename... Ts>
concept one_of = requires { requires in_types_v<T, Ts...>; };

struct no_copy
{
    no_copy(void) = default;
    no_copy(const no_copy &) = delete;
    no_copy(no_copy &&) = default;
    no_copy &operator=(const no_copy &) = delete;
    no_copy &operator=(no_copy &&) = default;
    ~no_copy() = default;
};

struct no_copy_move
{
    no_copy_move(void) = default;
    no_copy_move(const no_copy_move &) = delete;
    no_copy_move(no_copy_move &&) = delete;
    no_copy_move &operator=(const no_copy_move &) = delete;
    no_copy_move &operator=(no_copy_move &&) = delete;
    ~no_copy_move() = default;
};