#pragma once

#include <concepts>
#include <type_traits>

#include "arch/arch.hpp"
#include "kernel/mm/allocator/allocator_trait.hpp"
#include "lib/type_utility.hpp"

namespace kernel::mem
{
    struct pagetable_base;

    template <typename T>
    concept PageTable = requires(T table,
                                 typename arch_traits<current_arch>::template pa_t<> pa,
                                 typename arch_traits<current_arch>::template va_t<> va) {
        typename T::allocator_t;

        requires std::derived_from<T, pagetable_base>;
        requires std::constructible_from<T, typename T::allocator_t>;
        requires std::default_initializable<T> || !std::default_initializable<typename T::allocator_t>;
        requires std::is_nothrow_copy_assignable_v<T>;
        requires std::is_nothrow_copy_constructible_v<T>;
        requires std::move_constructible<T> || !std::move_constructible<typename T::allocator_t>;
        requires std::movable<T> || !std::movable<typename T::allocator_t>;

        typename T::packed_perm_t;
        T::perm::R;
        T::perm::W;
        T::perm::X;
        T::perm::U;
        requires same_type_v<decltype(T::perm::R), decltype(T::perm::W), decltype(T::perm::X), decltype(T::perm::U)>;
        requires std::convertible_to<decltype(T::perm::R), typename T::packed_perm_t>;
        { T::perm::R | T::perm::W } noexcept -> std::same_as<typename T::packed_perm_t>;
        { typename T::packed_perm_t{} & T::perm::R } noexcept -> std::convertible_to<bool>;
        { typename T::packed_perm_t{} | T::perm::R } noexcept -> std::same_as<typename T::packed_perm_t>;

        { table.add_mapping(pa, va, typename T::packed_perm_t{}) } noexcept -> std::same_as<void>;
        { table.update_mapping(pa, va) } noexcept -> std::same_as<void>;
        { table.delete_mapping(va) } noexcept -> std::same_as<void>;
        { table.set_page_perm(va, typename T::packed_perm_t{}) } noexcept -> std::same_as<void>;
        { table.get_page_perm(va) } noexcept -> std::same_as<typename T::packed_perm_t>;
        { table.transform(va) } noexcept -> std::same_as<decltype(pa)>;
        { table.entry() } noexcept -> std::same_as<decltype(pa)>;
    };

    struct pagetable_base
    {
        void assign_from(this auto &&self, const PageTable auto &other) noexcept
        requires PageTable<std::remove_cvref_t<decltype(self)>>
        {
            static_assert(
                requires { self.assign_impl(other); }, "assign_impl function of derived class must be accessible");
            self.assign_impl(other);
        }

        inline virtual ~pagetable_base() = 0;
    };

    pagetable_base::~pagetable_base() = default;
}