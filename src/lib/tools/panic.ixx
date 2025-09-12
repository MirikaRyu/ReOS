module;

#include <source_location>
#include <string_view>

export module lib.panic;

export namespace kernel::lib
{
    [[noreturn]] void panic(std::string_view msg = {},
                            std::source_location loc = std::source_location::current()) noexcept;

    using panic_handler_t = void (*)(void *args) noexcept;
    void panic_handler_set(panic_handler_t func, void *args) noexcept;
    void panic_handler_unset(void) noexcept;
}

namespace kernel::lib
{
    panic_handler_t panic_handler_ = nullptr;
    void *args_ = nullptr;

    void panic(std::string_view msg, std::source_location loc) noexcept
    {
        (void)msg;
        (void)loc;

        if (panic_handler_)
            panic_handler_(args_);

        while (true)
            ;
    }

    void panic_handler_set(panic_handler_t func, void *args) noexcept
    {
        if (!panic_handler_)
        {
            panic_handler_ = func;
            args_ = args;
        }
    }

    void panic_handler_unset(void) noexcept
    {
        panic_handler_ = nullptr;
    }
}