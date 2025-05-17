#include <cstdint>
#include <string_view>

#include "arch/riscv/boot/sbi.hpp"
#include "arch/riscv/kernel/mm/address.hpp"
#include "arch/riscv/kernel/mm/allocator.hpp"
#include "arch/riscv/kernel/mm/pagetable.hpp"

void uart_puts(std::string_view s) noexcept
{
    for (const auto &ch : s)
        sbi_call(1, 0, ch);
}

extern "C" [[noreturn]] void main(void) noexcept
{
    extern void __libc_init_array(void);
    __libc_init_array();

    uart_puts("Hello world");

    // riscv pagetable test
    riscv::mem::page_allocator_tmp alloc{riscv::mem::pa_t<>{0x1999}};
    riscv::mem::pagetable_t pgt{alloc};
    riscv::mem::pagetable_t pgt2{alloc};
    pgt2 = pgt;
    pgt.assign_from(pgt2);
    (void)pgt.entry();
    (void)pgt.transform(riscv::mem::va_t<>{0x1983});
    pgt.add_mapping(riscv::mem::pa_t<>{32856928}, riscv::mem::va_t<>{0x89898}, decltype(pgt)::perm::R);
    pgt.delete_mapping(riscv::mem::va_t<>{0x89898});
    pgt.set_page_perm(riscv::mem::va_t<>{0x89898}, decltype(pgt)::perm::R | decltype(pgt)::perm::W);
    pgt.update_mapping(riscv::mem::pa_t<>{32856928}, riscv::mem::va_t<>{0x89898});
    (void)pgt.get_page_perm(riscv::mem::va_t<>{0x89898});

    while (true)
        ;
}