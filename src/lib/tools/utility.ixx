module;

#include <concepts>
#include <type_traits>
#include <utility>

export module lib.utility;

import contract.lock;
import lib.type_utility;

export namespace kernel::lib
{
    template <typename T>
    requires std::is_enum_v<T>
    constexpr auto idx(const T &val) noexcept
    {
        return std::to_underlying(val);
    }

    constexpr auto div_ceil(std::integral auto dividend, std::integral auto divisor) noexcept
    {
        return dividend / divisor + (dividend % divisor == 0 ? 0 : 1);
    }

    template <contract::Lock T>
    class [[nodiscard]] lock_guard : no_copy_move
    {
    private:
        T &lk_;

    public:
        lock_guard(T &lk) noexcept
            : lk_{lk}
        {
            lk_.lock();
        }

        ~lock_guard()
        {
            lk_.unlock();
        }
    };
}