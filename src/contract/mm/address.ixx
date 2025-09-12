module;

#include <bit>
#include <cstddef>
#include <cstdint>

export module contract.address;

export namespace kernel::contract
{
    struct pa_t
    {
    private:
        uint64_t address_;

    public:
        constexpr pa_t(void) noexcept
            : address_{}
        {
        }

        constexpr explicit pa_t(uint64_t address) noexcept
            : address_{address}
        {
        }

        [[nodiscard]] constexpr uint64_t get(void) const noexcept
        {
            return address_;
        }

    public:
        ptrdiff_t operator-(pa_t addr) const noexcept
        {
            return reinterpret_cast<std::byte *>(address_) - reinterpret_cast<std::byte *>(addr.address_);
        }

        pa_t operator-(ptrdiff_t offset) const noexcept
        {
            return pa_t{reinterpret_cast<uint64_t>(reinterpret_cast<std::byte *>(address_) - offset)};
        }

        pa_t operator+(ptrdiff_t offset) const noexcept
        {
            return pa_t{reinterpret_cast<uint64_t>(reinterpret_cast<std::byte *>(address_) + offset)};
        }

        friend pa_t operator+(ptrdiff_t offset, pa_t addr) noexcept
        {
            return addr + offset;
        }

    public:
        constexpr bool operator==(const pa_t &other) const noexcept
        {
            return address_ == other.address_;
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != 0;
        }

        [[nodiscard]] constexpr uint64_t align(void) const noexcept
        {
            return 1 << std::countr_zero(address_);
        }

        [[nodiscard]] constexpr bool is_align_to(uint64_t align) const noexcept
        {
            return address_ % align == 0;
        }
    };

    struct va_t
    {
    private:
        void *address_;

    public:
        constexpr va_t(void) noexcept
            : address_{}
        {
        }

        constexpr explicit va_t(void *address) noexcept
            : address_{address}
        {
        }

        explicit va_t(uint64_t address) noexcept
            : address_{reinterpret_cast<void *>(address)}
        {
        }

    public:
        template <typename T>
        constexpr explicit operator T *(void) const noexcept
        {
            return to<T>();
        }

        constexpr explicit operator bool(void) const noexcept
        {
            return address_ != nullptr;
        }

    public:
        template <typename T>
        [[nodiscard]] constexpr T *to(void) const noexcept
        {
            return static_cast<T *>(address_);
        }

        [[nodiscard]] uint64_t address(void) const noexcept
        {
            return reinterpret_cast<uint64_t>(address_);
        }

    public:
        ptrdiff_t operator-(va_t addr) const noexcept
        {
            return to<std::byte>() - addr.to<std::byte>();
        }

        va_t operator-(ptrdiff_t offset) const noexcept
        {
            return va_t{to<std::byte>() - offset};
        }

        va_t operator+(ptrdiff_t offset) const noexcept
        {
            return va_t{to<std::byte>() + offset};
        }

        friend va_t operator+(ptrdiff_t offset, va_t addr) noexcept
        {
            return addr + offset;
        }

    public:
        constexpr bool operator==(const va_t &other) const noexcept
        {
            return address_ == other.address_;
        }

        [[nodiscard]] uint64_t align(void) const noexcept
        {
            return 1 << std::countr_zero(address());
        }

        [[nodiscard]] bool is_align_to(uint64_t align) const noexcept
        {
            return address() % align == 0;
        }
    };
}