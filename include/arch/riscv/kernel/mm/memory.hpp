#pragma once

#include <cstdint>

namespace riscv::mem
{
    [[maybe_unused]] constexpr uint16_t page_size = 4096;
    [[maybe_unused]] constexpr uint32_t mid_page_size = page_size * 512;
    [[maybe_unused]] constexpr uint32_t big_page_size = mid_page_size * 512;
}