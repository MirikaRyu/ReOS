#pragma once

#include <concepts>
#include <cstdint>
#include <ranges>
#include <utility>

#include "kernel/mm/paging/page_table.hpp"

#include "address.hpp"
#include "lib/assert.hpp"

namespace riscv::mem
{
    template <kernel::mem::PageAllocator Alloc>
    class pagetable_t final : public kernel::mem::pagetable_base
    {
        template <kernel::mem::PageAllocator Alloc2>
        friend class pagetable_t;

        friend struct kernel::mem::pagetable_base;

    public:
        using allocator_t = Alloc;

        enum struct perm : uint8_t
        {
            R = 0b1,
            W = 0b10,
            X = 0b100,
            U = 0b1000,
            G = 0b10000,
        };

        struct packed_perm_t
        {
            uint8_t packed_perm;

            constexpr packed_perm_t(void) noexcept
                : packed_perm{}
            {
            }

            constexpr packed_perm_t(perm val) noexcept
                : packed_perm{static_cast<uint8_t>(val)}
            {
            }

            constexpr packed_perm_t operator|(packed_perm_t other) const noexcept
            {
                packed_perm_t ret{};
                ret.packed_perm = static_cast<uint8_t>(packed_perm | other.packed_perm);
                return ret;
            }

            constexpr bool operator&(perm other) noexcept
            {
                return static_cast<uint8_t>(other) & packed_perm;
            }
        };

        friend constexpr packed_perm_t operator|(perm op1, perm op2) noexcept
        {
            return packed_perm_t{op1} | op2;
        }

    private:
        struct pte_t
        {
            uint64_t zero : 10;
            uint64_t ppn : 44;
            uint64_t rsw : 2;
            uint64_t bit_dirty : 1;
            uint64_t bit_accessed : 1;
            uint64_t bit_global : 1;
            uint64_t bit_user : 1;
            uint64_t bit_exec : 1;
            uint64_t bit_write : 1;
            uint64_t bit_read : 1;
            uint64_t bit_valid : 1;
        };

        struct page_t
        {
            static constexpr auto pte_counts = 512u;
            pte_t entries[pte_counts];
        };

        using ppn_t = uint64_t;

        template <typename T = page_t>
        [[nodiscard]] constexpr static pa_t<T> ppn_to_addr(ppn_t ppn) noexcept
        {
            return pa_t<T>{ppn << 12u};
        }

        template <typename T>
        [[nodiscard]] constexpr static ppn_t addr_to_ppn(pa_t<T> addr) noexcept
        {
            return addr.get() >> 12u;
        }

    private:
        Alloc allocator_;
        ppn_t root_;

        [[nodiscard]] constexpr static packed_perm_t make_pack_perm(const pte_t &pte) noexcept
        {
            packed_perm_t ret{};
            ret.packed_perm = (pte.bit_global << 4u) + (pte.bit_user << 3u) + (pte.bit_exec << 2u) +
                              (pte.bit_write << 1u) + pte.bit_read;
            return ret;
        }

        [[nodiscard]] ppn_t do_page_alloc(void) noexcept
        {
            return addr_to_ppn(pa_t(va_t(::new (va_t(allocator_.alloc_page())) page_t{})));
        }

        template <std::invocable<pte_t &> Func>
        packed_perm_t do_mapping_pte_mod(this auto &&self, va_t<> addr, Func &&func) noexcept
        {
            ppn_t ppn_next{self.root_};

            for (const auto &i : {2, 1, 0})
            {
                auto &&pte = va_t(ppn_to_addr(ppn_next))->entries[addr.get_pte_idx(va_t<>::ppn_level(i))];

                kassert(bool(pte.bit_valid), "Invalid virtual address or corrupted pagetable");

                if (i == 0)
                {
                    auto ret = make_pack_perm(pte);

                    func(pte);

                    return ret;
                }
                ppn_next = pte.ppn;
            }

            std::unreachable();
        }

        void do_pagetable_copy(const auto &other_pagetable) noexcept
        {
            const auto &pgtbl_l2 = va_t(ppn_to_addr(other_pagetable.root_))->entries;
            for (const auto &pte_l2_idx : std::views::iota(0u, page_t::pte_counts) |
                                              std::views::filter([&](auto i) { return pgtbl_l2[i].bit_valid; }))
            {
                const uint64_t vpn_2 = pte_l2_idx << 30u;

                const auto &pgtbl_l1 = va_t(ppn_to_addr(pgtbl_l2[pte_l2_idx].ppn))->entries;
                for (const auto &pte_l1_idx : std::views::iota(0u, page_t::pte_counts) |
                                                  std::views::filter([&](auto i) { return pgtbl_l1[i].bit_valid; }))
                {
                    const uint64_t vpn_21 = vpn_2 + (pte_l1_idx << 21u);

                    const auto &pgtbl_l0 = va_t(ppn_to_addr(pgtbl_l1[pte_l1_idx].ppn))->entries;
                    for (const auto &pte_l0_idx : std::views::iota(0u, page_t::pte_counts) |
                                                      std::views::filter([&](auto i) { return pgtbl_l0[i].bit_valid; }))
                    {
                        const uint64_t vpn_210 = vpn_21 + (pte_l0_idx << 12u);

                        add_mapping(ppn_to_addr(pgtbl_l0[pte_l0_idx].ppn),
                                    va_t{vpn_210 << 12u},
                                    make_pack_perm(pgtbl_l0[pte_l0_idx]));
                    }
                }
            }
        }

        void do_pagetable_free(void) noexcept
        {
            auto valid_pte = [](const pte_t &pte) -> bool { return pte.bit_valid; };

            for (auto &pte_l2 : va_t(ppn_to_addr(root_))->entries | std::views::filter(valid_pte))
            {
                for (auto &pte_l1 : va_t(ppn_to_addr(pte_l2.ppn))->entries | std::views::filter(valid_pte))
                    allocator_.dealloc_page(va_t(ppn_to_addr(pte_l1.ppn))); // Trival, no need to destruct

                allocator_.dealloc_page(va_t(ppn_to_addr(pte_l2.ppn)));
            }

            allocator_.dealloc_page(va_t(ppn_to_addr(root_)));
        }

        template <kernel::mem::PageAllocator Alloc2>
        pagetable_t &assign_impl(const pagetable_t<Alloc2> &other) noexcept
        {
            if constexpr (std::is_same_v<Alloc, Alloc2>)
                if (this == &other)
                    return *this;

            do_pagetable_free();
            root_ = do_page_alloc();

            do_pagetable_copy(other);

            return *this;
        }

    public:
        pagetable_t(void) noexcept
        requires std::is_default_constructible_v<Alloc>
            : pagetable_t{Alloc{}}
        {
        }

        explicit pagetable_t(const Alloc &alloc) noexcept
            : allocator_{alloc}, root_{do_page_alloc()}
        {
        }

        ~pagetable_t() override
        {
            if (root_ != 0)
                do_pagetable_free();
        }

    public:
        pagetable_t(const pagetable_t &other) noexcept
            : pagetable_t{other, other.allocator_}
        {
        }

        template <kernel::mem::PageAllocator Alloc2>
        explicit pagetable_t(const pagetable_t<Alloc2> &other) noexcept
        requires std::is_default_constructible_v<Alloc>
            : pagetable_t{other, Alloc{}}
        {
        }

        template <kernel::mem::PageAllocator Alloc2>
        pagetable_t(const pagetable_t<Alloc2> &other, const Alloc &alloc) noexcept
            : pagetable_t{alloc}
        {
            do_pagetable_copy(other);
        }

        pagetable_t &operator=(const pagetable_t &other) noexcept
        {
            return assign_impl(other);
        }

        template <kernel::mem::PageAllocator Alloc2>
        pagetable_t &operator=(const pagetable_t<Alloc2> &other) noexcept
        {
            return assign_impl(other);
        }

        pagetable_t(pagetable_t &&other) noexcept
        requires std::is_move_constructible_v<Alloc>
            : allocator_{std::move(other.allocator_)}, root_{std::move(other.root_)}
        {
            other.root_ = {};
        }

        pagetable_t &operator=(pagetable_t &&other) noexcept
        requires std::is_move_assignable_v<Alloc>
        {
            if (this == &other)
                return *this;

            allocator_ = std::move(other.allocator_);
            root_ = std::move(other.root_);
            other.root_ = {};

            return *this;
        }

    public:
        void add_mapping(pa_t<> from, va_t<> to, packed_perm_t perm) noexcept
        {
            kassert(perm.packed_perm > 0 && perm.packed_perm < 0b11111, "Invalid permission bits");
            kassert(from.get() % 4096 == 0, "Physical address misaligned");
            kassert(pa_t(to).get() % 4096 == 0, "Virtual address misaligned");

            ppn_t ppn_next{root_};

            for (const auto &i : {2, 1, 0})
            {
                auto &&pte = va_t(ppn_to_addr(ppn_next))->entries[to.get_pte_idx(va_t<>::ppn_level(i))];

                if (!pte.bit_valid)
                {
                    pte.bit_valid = 1;

                    if (i == 0)
                    {
                        pte.ppn = addr_to_ppn(from);
                        pte.bit_global = perm & perm::G;
                        pte.bit_user = perm & perm::U;
                        pte.bit_exec = perm & perm::X;
                        pte.bit_write = perm & perm::W;
                        pte.bit_read = perm & perm::R;

                        break;
                    }

                    pte.ppn = do_page_alloc();
                }

                ppn_next = pte.ppn;
            }
        }

        void update_mapping(pa_t<> new_pa, va_t<> va) noexcept
        {
            do_mapping_pte_mod(va, [&](pte_t &pte) { pte.ppn = addr_to_ppn(new_pa); });
        }

        void delete_mapping(va_t<> addr) noexcept
        {
            do_mapping_pte_mod(addr, [](pte_t &entry) { entry.bit_valid = 0; });
        }

        void set_page_perm(va_t<> addr, packed_perm_t perm) noexcept
        {
            kassert(perm.packed_perm > 0 && perm.packed_perm < 0b11111, "Invalid permission bits");

            do_mapping_pte_mod(addr,
                               [&](pte_t &entry)
                               {
                                   entry.bit_global = perm & perm::G;
                                   entry.bit_user = perm & perm::U;
                                   entry.bit_exec = perm & perm::X;
                                   entry.bit_write = perm & perm::W;
                                   entry.bit_read = perm & perm::R;
                               });
        }

        [[nodiscard]] packed_perm_t get_page_perm(va_t<> addr) const noexcept
        {
            return do_mapping_pte_mod(addr, [](auto) {});
        }

        [[nodiscard]] pa_t<> transform(va_t<> addr) const noexcept
        {
            uint64_t ppn_pa{};

            do_mapping_pte_mod(addr, [&](const pte_t &pte) { ppn_pa = pte.ppn; });

            return pa_t{(ppn_pa << 12u) + addr.get_page_offset()};
        }

        [[nodiscard]] pa_t<> entry(void) const noexcept
        {
            return ppn_to_addr(root_);
        }
    };
}