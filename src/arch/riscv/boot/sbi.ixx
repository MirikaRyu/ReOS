module;

#include <cstdint>

export module arch.riscv.sbi;

export namespace kernel::arch::riscv
{
    int64_t sbi_call(int32_t id_extension,
                     int32_t id_function = {},
                     uint64_t arg0 = {},
                     uint64_t arg1 = {},
                     uint64_t arg2 = {},
                     uint64_t arg3 = {},
                     uint64_t arg4 = {},
                     uint64_t arg5 = {}) noexcept
    {
        int64_t ret_err{};
        int64_t ret_val{};

        asm volatile("mv a7, %[id_extension]\n"
                     "mv a6, %[id_function]\n"
                     "mv a0, %[arg0]\n"
                     "mv a1, %[arg1]\n"
                     "mv a2, %[arg2]\n"
                     "mv a3, %[arg3]\n"
                     "mv a4, %[arg4]\n"
                     "mv a5, %[arg5]\n"
                     "ecall\n"
                     "mv %[ret_err], a0\n"
                     "mv %[ret_val], a1\n"
                     : [ret_err] "=r"(ret_err), [ret_val] "=r"(ret_val)
                     : [id_extension] "r"(id_extension),
                       [id_function] "r"(id_function),
                       [arg0] "r"(arg0),
                       [arg1] "r"(arg1),
                       [arg2] "r"(arg2),
                       [arg3] "r"(arg3),
                       [arg4] "r"(arg4),
                       [arg5] "r"(arg5)
                     : "memory", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7");

        return ret_err < 0 ? ret_err : ret_val;
    }
}