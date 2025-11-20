module;

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <tuple>
#include <utility>

export module arch.riscv.page_table;

import arch.riscv.address;
import contract.address;
import contract.allocator;
import contract.page_table;
import lib.panic;
import lib.assert;
import lib.utility;

using kernel::contract::pa_t;
using kernel::contract::va_t;
using kernel::contract::perms;
using kernel::contract::page_level;
using kernel::contract::perm_result_t;

export namespace kernel::arch::riscv
{
    template <contract::PageAllocator Alloc>
    class pagetable_t final
    {
    public:
        using allocator_t = Alloc;

    private:
        struct pte_t
        {
            uint64_t val;

            constexpr pte_t(void) noexcept
                : val{}
            {
            }

            constexpr bool valid(void) const noexcept
            {
                return val & (1ULL << 0);
            }

            constexpr bool read(void) const noexcept
            {
                return val & (1ULL << 1);
            }

            constexpr bool write(void) const noexcept
            {
                return val & (1ULL << 2);
            }

            constexpr bool exec(void) const noexcept
            {
                return val & (1ULL << 3);
            }

            constexpr bool user(void) const noexcept
            {
                return val & (1ULL << 4);
            }

            constexpr bool global(void) const noexcept
            {
                return val & (1ULL << 5);
            }

            constexpr bool accessed(void) const noexcept
            {
                return val & (1ULL << 6);
            }

            constexpr bool dirty(void) const noexcept
            {
                return val & (1ULL << 7);
            }

            // Get the `shared` bit from the pte.
            // Note that the bit means differently in L2, L1 level, refer to `set_shared` for more info.
            constexpr bool shared(void) const noexcept
            {
                return val & (1ULL << 8);
            }

            constexpr uint64_t ppn(void) const noexcept
            {
                return (val >> 10) & ((1ULL << 44) - 1);
            }

            constexpr bool is_leaf(void) const noexcept
            {
                return read() || write() || exec();
            }

        public:
            constexpr void set_valid(bool x) noexcept
            {
                set_bit(0, x);
            }

            constexpr void set_read(bool x) noexcept
            {
                set_bit(1, x);
            }

            constexpr void set_write(bool x) noexcept
            {
                set_bit(2, x);
            }

            constexpr void set_exec(bool x) noexcept
            {
                set_bit(3, x);
            }

            constexpr void set_user(bool x) noexcept
            {
                set_bit(4, x);
            }

            constexpr void set_global(bool x) noexcept
            {
                set_bit(5, x);
            }

            constexpr void set_accessed(bool x) noexcept
            {
                set_bit(6, x);
            }

            constexpr void set_dirty(bool x) noexcept
            {
                set_bit(7, x);
            }

            // Set the shared status of a page or mark a pte share the underlaying subtree.
            // If the bit appears in L2, it means that the subtree is borrowed from other page_table.
            // If the bit appears in L1, it means that the page is pinned in memory.
            // The bit should not appear in L0 level.
            // If the subtree is borrowed, then it should not be released by the page_table.
            // If the page is pinned,
            //      then this bit must appear in the first entry of the page
            //      and the pte should not be remapped to other addresses,
            //      and the page won't be released until the end of page_table's lifetime.
            constexpr void set_shared(bool x) noexcept
            {
                set_bit(8, x);
            }

            constexpr void set_ppn(uint64_t v) noexcept
            {
                val &= ~(((1ULL << 44) - 1) << 10);
                val |= (v & ((1ULL << 44) - 1)) << 10;
            }

        private:
            constexpr void set_bit(int pos, bool x) noexcept
            {
                if (x)
                    val |= (1ULL << pos);
                else
                    val &= ~(1ULL << pos);
            }
        };

        struct page_t
        {
            static constexpr auto PTE_COUNTS = 512u;
            pte_t entries[PTE_COUNTS];
        };

        enum class ppn_level : uint8_t
        {
            L0,
            L1,
            L2,
        };

        using ppn_t = uint64_t;

        static constexpr va_t ppn_to_va(ppn_t ppn) noexcept
        {
            return to_va(pa_t{ppn << 12u});
        }

        static constexpr ppn_t pa_to_ppn(pa_t addr) noexcept
        {
            return addr.get() >> 12u;
        }

        static constexpr page_t &ppn_to_page(ppn_t ppn) noexcept
        {
            lib::kassert(ppn, "Null ppn dereference");

            return *(ppn_to_va(ppn).to<page_t>());
        }

        static constexpr pte_t &ppn_get_pte(ppn_t ppn, uint16_t idx) noexcept
        {
            return ppn_to_page(ppn).entries[idx];
        }

        static constexpr uint16_t get_pte_idx(va_t va, ppn_level level) noexcept
        {
            const uint64_t mask = 0x1ffull << (12u + lib::idx(level) * 9u);
            return static_cast<uint16_t>((va.address() & mask) >> (12u + lib::idx(level) * 9u));
        }

        static constexpr perm_result_t make_perms(const pte_t &pte) noexcept
        {
            return static_cast<perms>(pte.user() << 3u) | static_cast<perms>(pte.exec() << 2u) |
                   static_cast<perms>(pte.write() << 1u) | static_cast<perms>(pte.read());
        }

    private:
        [[no_unique_address]] Alloc allocator_;
        uint64_t root_pte_; // Take the root ppn as a pte struct to tranverse more easily

        ppn_t root_ppn(void) const noexcept
        {
            return std::bit_cast<pte_t>(root_pte_).ppn();
        }

        [[nodiscard]] ppn_t do_page_alloc(void) noexcept
        {
            if (const va_t raw_page = allocator_.alloc_page()) [[likely]]
            {
                const auto page = ::new (raw_page.to<void>()) page_t{};
                return pa_to_ppn(to_pa(va_t{page}));
            }
            else
            {
                lib::panic("Pagetable memory allocation failed");
                std::unreachable();
            }
        }

        template <std::invocable<pte_t &, ppn_level> Func>
        void do_pagetable_walk(this auto &&self, va_t addr, Func &&func) noexcept
        {
            if (!self.root_ppn())
                return;

            ppn_t ppn_next{self.root_ppn()};
            for (const auto lv : {ppn_level::L2, ppn_level::L1, ppn_level::L0})
            {
                pte_t &pte = ppn_get_pte(ppn_next, get_pte_idx(addr, lv));
                ppn_next = pte.ppn();

                lib::kassert(pte.valid(), "Invalid virtual address");

                if (pte.is_leaf())
                {
                    func(pte, lv);
                    return;
                }
            }

            lib::panic("Corrupted page table");
            std::unreachable();
        }

        void do_pagetable_copy(const auto &other_pagetable) noexcept
        {
            [obj = this](this auto &&self, pte_t *local_pte, ppn_t other_root)
            {
                if (other_root == 0)
                    return;

                auto valid_entry_map =
                    ppn_to_page(other_root).entries | std::views::enumerate |
                    std::views::filter([](const auto &idx_pte) { return std::get<1>(idx_pte).valid(); });
                for (const auto &[idx, pte] : valid_entry_map)
                {
                    if (local_pte->ppn() == 0)
                        local_pte->set_ppn(obj->do_page_alloc());

                    if (pte.is_leaf())
                        ppn_get_pte(local_pte->ppn(), idx) = pte;
                    else
                        self(&ppn_get_pte(local_pte->ppn(), idx), ppn_get_pte(other_root, idx).ppn()),
                            ppn_get_pte(local_pte->ppn(), idx).set_valid(true);
                }
            }(reinterpret_cast<pte_t *>(&root_pte_), pa_to_ppn(other_pagetable.entry()));
        }

        void do_pagetable_free(ppn_t tree_root, uint8_t _iter = 0) noexcept
        {
            if (tree_root == 0)
                return;

            for (pte_t &pte :
                 ppn_to_page(tree_root).entries |
                     std::views::filter([&](const pte_t &pte)
                                        { return pte.valid() && !pte.is_leaf() && (_iter != 0 || !pte.shared()); }))
                do_pagetable_free(pte.ppn(), _iter + 1);

            allocator_.dealloc_page(ppn_to_va(tree_root), 1); // Trival, no need to call destructor
        }

    public:
        pagetable_t(void) noexcept
        requires std::is_default_constructible_v<Alloc>
            : pagetable_t{Alloc{}}
        {
        }

        explicit pagetable_t(const Alloc &alloc) noexcept
            : allocator_{alloc}, root_pte_{}
        {
        }

        pagetable_t(const pagetable_t &other) noexcept
            : pagetable_t{other.allocator_}
        {
            do_pagetable_copy(other);
        }

        pagetable_t(pagetable_t &&other) noexcept
            : allocator_{std::move(other.allocator_)}, root_pte_{std::move(other.root_pte_)}
        {
            other.root_pte_ = {};
        }

        pagetable_t &operator=(const pagetable_t &other) noexcept
        {
            return assign(other);
        }

        pagetable_t &operator=(pagetable_t &&other) noexcept
        {
            if (this == &other)
                return *this;

            do_pagetable_free(root_ppn());
            allocator_ = std::move(other.allocator_);
            root_pte_ = std::move(other.root_pte_);
            other.root_pte_ = {};

            return *this;
        }

        template <contract::PageAllocator Alloc2>
        pagetable_t &assign(const pagetable_t<Alloc2> &other) noexcept
        {
            if constexpr (std::is_same_v<Alloc, Alloc2>)
                if (this == &other)
                    return *this;

            do_pagetable_free(root_ppn());
            do_pagetable_copy(other);

            return *this;
        }

        friend void swap(pagetable_t &_1, pagetable_t &_2) noexcept
        {
            std::ranges::swap(_1.allocator_, _2.allocator_);
            std::ranges::swap(_1.root_pte_, _2.root_pte_);
        }

        ~pagetable_t() noexcept
        {
            do_pagetable_free(root_ppn());
        }

    public:
        void add_mapping(va_t from, pa_t to, perm_result_t perm, page_level level = page_level::BASE) noexcept
        {
            lib::kassert(from.is_align_to(page_size(level)), "Virtual address misaligned");
            lib::kassert(to.is_align_to(page_size(level)), "Physical address misaligned");

            auto is_table_empty = [](this auto &&self, ppn_t root)
            {
                if (root == 0)
                    return true;

                for (const pte_t &pte :
                     ppn_to_page(root).entries | std::views::filter([](const pte_t &pte) { return pte.valid(); }))
                    if (pte.is_leaf() || !self(pte.ppn())) // `a valid leaf page` or `next level not empty`
                        return false;

                return true;
            };

            ppn_t ppn_next{root_pte_ ? root_ppn() : (root_pte_ = (do_page_alloc() << 10u), root_ppn())};
            for (const auto lv : {ppn_level::L2, ppn_level::L1, ppn_level::L0})
            {
                pte_t &pte = ppn_get_pte(ppn_next, get_pte_idx(from, lv));

                if (lib::idx(lv) == lib::idx(level)) // Reach the target pagetable level
                {
                    if (pte.valid()) // Check if the target is occupied
                    {
                        if (pte.is_leaf() || !is_table_empty(pte.ppn())) [[unlikely]]
                        {
                            lib::panic("Try to map the same va or some addresses in the hugepage are mapped");
                            return;
                        }

                        do_pagetable_free(pte.ppn()); // No mapping in subtree, free pages
                    }

                    pte.set_ppn(pa_to_ppn(to));
                    pte.set_user(perm & perms::U);
                    pte.set_exec(perm & perms::X);
                    pte.set_write(perm & perms::W);
                    pte.set_read(perm & perms::R);
                    pte.set_valid(true);

                    break;
                }

                if (!pte.valid())
                {
                    pte.set_ppn(do_page_alloc());
                    pte.set_valid(true);
                }

                if (pte.is_leaf()) [[unlikely]]
                {
                    lib::panic("Try to map address in an active hugepage");
                    return;
                }

                ppn_next = pte.ppn();
            }
        }

        void del_mapping(va_t addr) noexcept
        {
            do_pagetable_walk(addr, [](pte_t &entry, auto) { entry.set_valid(false); });
        }

        void set_page_perm(va_t addr, perm_result_t perm) noexcept
        {
            do_pagetable_walk(addr,
                              [&](pte_t &entry, auto)
                              {
                                  entry.set_user(perm & perms::U);
                                  entry.set_exec(perm & perms::X);
                                  entry.set_write(perm & perms::W);
                                  entry.set_read(perm & perms::R);
                              });
        }

        [[nodiscard]] perm_result_t get_page_perm(va_t addr) const noexcept
        {
            perm_result_t ret = perms::R;
            do_pagetable_walk(addr, [&ret](const pte_t &entry, auto) { ret = make_perms(entry); });

            return ret;
        }

        pagetable_t shared_copy(void) noexcept
        {
            pagetable_t result{allocator_};

            if (root_ppn())
            {
                result.root_pte_ = do_page_alloc() << 10u;
                std::copy_n(&ppn_to_page(root_ppn()), 1, &ppn_to_page(result.root_ppn()));

                for (pte_t &entry :
                     ppn_to_page(result.root_ppn()).entries |
                         std::views::filter([](const pte_t &pte) { return pte.valid() && !pte.is_leaf(); }))
                {
                    entry.set_shared(true);
                    ppn_get_pte(entry.ppn(), 0).set_shared(true);
                }
            }

            return result;
        }

        void shared_mark(va_t start, va_t end) noexcept
        {
            lib::kassert(start.is_align_to(page_size(page_level::HUGE)), "Start address misaligned");
            lib::kassert(end.is_align_to(page_size(page_level::HUGE)), "End address misaligned");
            lib::kassert(end - start > 0, "Invalid address range");

            if (!root_ppn())
                root_pte_ = do_page_alloc() << 10;

            for (const auto idx : std::views::iota(get_pte_idx(start, ppn_level::L2), get_pte_idx(end, ppn_level::L2)))
            {
                pte_t &pte = ppn_get_pte(root_ppn(), idx);
                if (!pte.is_leaf())
                {
                    if (!pte.valid())
                    {
                        pte.set_ppn(do_page_alloc());
                        pte.set_valid(true);
                    }

                    ppn_get_pte(pte.ppn(), idx).set_shared(true);
                }
            }
        }

        void shared_attach(const pagetable_t &other, va_t start, va_t end) noexcept
        {
            lib::kassert(start.is_align_to(page_size(page_level::HUGE)), "Start address misaligned");
            lib::kassert(end.is_align_to(page_size(page_level::HUGE)), "End address misaligned");
            lib::kassert(end - start > 0, "Invalid address range");

            if (!other.root_ppn())
                return;

            for (const auto idx : std::views::iota(get_pte_idx(start, ppn_level::L2), get_pte_idx(end, ppn_level::L2)))
            {
                const pte_t &other_pte = ppn_get_pte(other.root_ppn(), idx);
                if (other_pte.valid() && !other_pte.is_leaf() && ppn_get_pte(other_pte.ppn(), 0).shared())
                {
                    if (!root_ppn()) [[unlikely]]
                        root_pte_ = do_page_alloc() << 10;

                    pte_t &local_pte = ppn_get_pte(root_ppn(), idx);
                    if (!local_pte.valid())
                    {
                        local_pte.set_ppn(other_pte.ppn());
                        local_pte.set_user(false);
                        local_pte.set_exec(false);
                        local_pte.set_write(false);
                        local_pte.set_read(false);
                        local_pte.set_shared(true);
                        local_pte.set_valid(true);
                    }
                }
            }
        }

        void shared_detach(va_t start, va_t end) noexcept
        {
            lib::kassert(start.is_align_to(page_size(page_level::HUGE)), "Start address misaligned");
            lib::kassert(end.is_align_to(page_size(page_level::HUGE)), "End address misaligned");
            lib::kassert(end - start > 0, "Invalid address range");

            if (!root_ppn())
                return;

            for (const auto idx : std::views::iota(get_pte_idx(start, ppn_level::L2), get_pte_idx(end, ppn_level::L2)))
            {
                pte_t &pte = ppn_get_pte(root_ppn(), idx);
                if (pte.valid() && pte.shared() && !pte.is_leaf())
                {
                    pte.set_shared(false);
                    pte.set_valid(false);
                }
            }
        }

        [[nodiscard]] pa_t transform(va_t addr) const noexcept
        {
            uint64_t ret{};

            do_pagetable_walk(addr,
                              [&](const pte_t &pte, ppn_level lv)
                              {
                                  /* Make bit mask for corresponding page level */
                                  /* BASE -> low 12 */
                                  /* MIDDLE -> low 12 + 9 */
                                  /* HUGE -> low 12 + 9 * 2 */
                                  const auto mask = (1ull << (12u + lib::idx(lv) * 9u)) - 1;

                                  ret = (pte.ppn() << 12u) + (addr.address() & mask);
                              });

            return pa_t{ret};
        }

        [[nodiscard]] static constexpr size_t page_size(page_level level) noexcept
        {
            switch (level)
            {
            case page_level::BASE:
                return PAGE_SIZE;
            case page_level::MIDDLE:
                return MID_PAGE_SIZE;
            case page_level::HUGE:
                return HUGE_PAGE_SIZE;
            [[unlikely]] default:
                lib::panic("Invalid page level");
                std::unreachable();
            }
        }

        [[nodiscard]] pa_t entry(void) const noexcept
        {
            return to_pa(ppn_to_va(root_ppn()));
        }
    };
}