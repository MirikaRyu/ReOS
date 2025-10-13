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
    /* Paging levels a page_table should support */
    enum class page_level : uint8_t
    {
        BASE,
        MIDDLE,
        HUGE,
    };

    /* Page permissions a page_table should support */
    enum class perms : uint8_t
    {
        R = 0b1,
        W = 0b10,
        X = 0b100,
        U = 0b1000,
    };

    /* A strong type wrapper of multiple `perms` */
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
    /* The features every page_table should implement */
    template <typename T>
    concept PageTable = requires(T table, pa_t pa, va_t va, perm_result_t perm_pack, page_level level) {
        // Requires to expose allocator type and which must be a PageAllocator
        typename T::allocator_t;
        requires PageAllocator<typename T::allocator_t>;

        // Should be constructible from allocator
        // Should be default initializable if the allocator is default initializable
        // The copy constructor/assign operator are deep copy functions that clone all non-empty page_table pages
        requires std::default_initializable<T> || !std::default_initializable<typename T::allocator_t>;
        requires std::constructible_from<T, typename T::allocator_t>;
        requires std::copyable<T>;
        requires std::movable<T>;

        // The ability to assign between the instances of different allocators
        { table.assign(table) } noexcept -> std::same_as<T &>;

        // The ability to add and delete mapping
        // The mapped address must be aligned to the page size of the corresponding page_table level
        // The default page_table level is `page_level::BASE`
        // Memory retrieve of empty page_table entry is not required when deleting a mapping
        // Address for `del_mapping` must be a mapped valid virtual address
        { table.add_mapping(pa, va, perm_pack) } noexcept -> std::same_as<void>;
        { table.add_mapping(pa, va, perm_pack, level) } noexcept -> std::same_as<void>;
        { table.del_mapping(va) } noexcept -> std::same_as<void>;

        // The ability to get and set page permission
        // The address must be valid and aligned to the size of the page it refers to
        { table.set_page_perm(va, perm_pack) } noexcept -> std::same_as<void>;
        { table.get_page_perm(va) } noexcept -> std::same_as<perm_result_t>;

        // The ability to query the physical address of a virtual address
        // The virtual address must be valid
        { table.transform(va) } noexcept -> std::same_as<pa_t>;

        // The ability to share parts of the page_table
        // `shared_copy` should create a new page_table that shares the same lower-level subtrees with the origin
        // `shared_mark` should create (if not exist) and pin smallest page_table subtrees covering [va_start, va_end)
        // `shared_attach` should share the subtrees from the other over [va_start, va_end),
        //                 and only the valid mapping with shareable subtrees will be attached to
        //                 Two tables will link to the same subtrees of attached addresses and pin them in memory,
        //                 and the page_table was attached to must have a lifetime longer than the other
        // `shared_deatch` should remove the valid mappings in [va_start, va_end) and unshared mappings are not affected
        //                 Unpinning the page_table or retrieving the memory is not required
        // All the virtual address must be aligned to the size of `page_level::HUGE`
        { table.shared_copy() } noexcept -> std::same_as<T>;
        { table.shared_mark(va, va) } noexcept -> std::same_as<void>;
        { table.shared_attach(table, va, va) } noexcept -> std::same_as<void>;
        { table.shared_detach(va, va) } noexcept -> std::same_as<void>;

        // The ability to query the page size of the specific page_table level, recommend to be static
        { table.page_size(level) } noexcept -> std::same_as<size_t>;

        // The ability to get physical address of the page_table's root
        // The ownership of the page_table is NOT released
        { table.entry() } noexcept -> std::same_as<pa_t>;
    };
}