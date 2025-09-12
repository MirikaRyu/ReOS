module;

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

export module lib.type_utility;

import lib.assert;

export namespace kernel::lib
{
    template <typename T, typename... Ts>
    constexpr bool same_type_v = (std::is_same_v<T, Ts> && ...);

    template <typename T, typename... Ts>
    constexpr bool in_types_v = (std::is_same_v<T, Ts> || ...);

    template <typename T, typename... Ts>
    concept OneOf = requires { requires in_types_v<T, Ts...>; };

    template <typename T>
    concept CompleteType = requires { sizeof(T); };

    struct no_copy
    {
        no_copy(void) = default;
        no_copy(const no_copy &) = delete;
        no_copy(no_copy &&) = default;
        no_copy &operator=(const no_copy &) = delete;
        no_copy &operator=(no_copy &&) = default;
        ~no_copy() = default;
    };

    struct no_copy_move
    {
        no_copy_move(void) = default;
        no_copy_move(const no_copy_move &) = delete;
        no_copy_move(no_copy_move &&) = delete;
        no_copy_move &operator=(const no_copy_move &) = delete;
        no_copy_move &operator=(no_copy_move &&) = delete;
        ~no_copy_move() = default;
    };

    template <typename T>
    requires std::is_class_v<T>
    struct static_singleton
    {
    private:
        inline static T *obj_{nullptr};

        consteval virtual void base_prevent_construct(void) const noexcept = 0;

        struct dummy_t final : T
        {
            using T::T;

        private:
            consteval void base_prevent_construct(void) const noexcept override {};
        };

    public:
        template <typename... Args>
        requires std::constructible_from<dummy_t, Args...>
        static void construct(Args &&...args) noexcept
        {
            static std::byte storage[sizeof(dummy_t)];
            if (obj_)
                obj_->~T();
            obj_ = ::new (storage) dummy_t{std::forward<Args>(args)...};
        }

        static T &instance(void) noexcept
        {
            kassert(obj_, "Try to get object before construction");
            return *obj_;
        }

        inline virtual ~static_singleton() = 0;
    };

    template <typename T>
    requires std::is_class_v<T>
    kernel::lib::static_singleton<T>::~static_singleton() = default;
}