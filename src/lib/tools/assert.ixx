module;

#include <source_location>
#include <string_view>

export module lib.assert;

import lib.panic;

namespace kernel::lib
{
    struct bool_location_t
    {
        bool val;
        std::source_location loc;

        template <typename T>
        requires requires(T t) { bool(t); }
        constexpr bool_location_t(T &&bool_v, std::source_location location = std::source_location::current()) noexcept
            : val{bool(bool_v)}, loc{location}
        {
        }

        constexpr operator bool(void) const noexcept
        {
            return val;
        }
    };
}

export namespace kernel::lib
{
    constexpr void kassert(bool_location_t e, std::string_view msg = {}) noexcept
    {
        if (!e) [[unlikely]]
            panic(msg, e.loc);
    }
}