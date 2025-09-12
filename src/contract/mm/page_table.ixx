module;

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

export module contract.page_table;

import contract.address;
import contract.allocator;

export namespace kernel::contract
{
    enum class page_level : uint8_t
    {
        BASE,
        MIDDLE,
        HUGE,
    };

    enum class perms : uint8_t
    {
        R = 0b1,
        W = 0b10,
        X = 0b100,
        U = 0b1000,
    };

    struct perm_result_t
    {
    private:
        uint8_t perm_result_;

        constexpr perm_result_t(uint8_t val) noexcept
            : perm_result_{val}
        {
        }

    public:
        constexpr perm_result_t(perms perm) noexcept
            : perm_result_{static_cast<uint8_t>(perm)}
        {
        }

        constexpr bool operator&(perms perm) const noexcept
        {
            return static_cast<uint8_t>(perm) & perm_result_;
        }

        constexpr perm_result_t operator|(perms perm) const noexcept
        {
            return {static_cast<uint8_t>(static_cast<uint8_t>(perm) | perm_result_)};
        }

        constexpr bool operator==(perm_result_t res) const noexcept
        {
            return perm_result_ == res.perm_result_;
        }
    };

    constexpr bool operator&(perms perm, perm_result_t res) noexcept
    {
        return res & perm;
    }

    constexpr perm_result_t operator|(perms perm, perm_result_t res) noexcept
    {
        return res | perm;
    }
}

export namespace kernel::contract
{
    template <typename T>
    concept PageTable = requires(T table, pa_t pa, va_t va, perm_result_t perm_pack, page_level level) {
        typename T::allocator_t;
        requires PageAllocator<typename T::allocator_t>;

        requires std::default_initializable<T> || !std::default_initializable<typename T::allocator_t>;
        requires std::constructible_from<T, typename T::allocator_t>;
        requires std::copyable<T>;
        requires std::movable<T>;

        // Requires the ability to assign between the instances of different allocators
        // For C++26
        // { table.assign([: substitute(template_of(^^table), {^^dummy_allocator_t}) :]) } noexcept
        //      -> std::same_as<T&>;
        { table.assign(table) } noexcept -> std::same_as<T &>;

        { table.add_mapping(pa, va, perm_pack) } noexcept -> std::same_as<void>;
        { table.add_mapping(pa, va, perm_pack, level) } noexcept -> std::same_as<void>;
        { table.delete_mapping(va) } noexcept -> std::same_as<void>;
        { table.set_page_perm(va, perm_pack) } noexcept -> std::same_as<void>;
        { table.get_page_perm(va) } noexcept -> std::same_as<perm_result_t>;
        { table.transform(va) } noexcept -> std::same_as<pa_t>;
        { table.entry() } noexcept -> std::same_as<pa_t>;
        { table.page_size(level) } noexcept -> std::same_as<size_t>;
    };
}