#pragma once

#include "arch/riscv/kernel/mm/address.hpp"

enum class archs
{
    X64,
    RISCV,
    AARCH64,
    LOONGARCH,
};

[[maybe_unused]] inline constexpr auto current_arch = archs::RISCV;

template <archs current>
struct arch_traits;

template <>
struct arch_traits<archs::RISCV>
{
    template <typename T = void>
    using pa_t = riscv::mem::pa_t<T>;

    template <typename T = void>
    using va_t = riscv::mem::va_t<T>;
};