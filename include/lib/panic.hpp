#pragma once

#include <source_location>
#include <string_view>

[[noreturn]] inline void panic(std::string_view msg = {},
                               std::source_location loc = std::source_location::current()) noexcept
{
    (void)msg;
    (void)loc;

    while (true)
        ;
}