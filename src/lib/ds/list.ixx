module;

#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

export module lib.list;

import lib.assert;
import contract.allocator;
import contract.container;

export namespace kernel::lib
{
    template <contract::ContainerType T, contract::UniversalAlloctor Alloc>
    struct forward_list
    {
    public:
        using value_type = T;
        using allocator_type = Alloc;

        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using reference = value_type &;
        using const_reference = const value_type &;
        using pointer = value_type *;
        using const_pointer = const value_type *;

    private:
        struct node_base_t
        {
            node_base_t *next;
        };

        struct node_t : node_base_t
        {
            [[no_unique_address]] value_type value;
        };

        template <bool IsConst>
        struct iterator_t
        {
            using value_type = forward_list::value_type;
            using difference_type = ptrdiff_t;

            node_base_t *node_;

            constexpr operator iterator_t<true>(void) const noexcept
            {
                return {node_};
            }

            constexpr auto operator*(void) const noexcept -> std::conditional_t<IsConst, const_reference, reference>
            {
                return static_cast<node_t *>(node_)->value;
            }

            constexpr auto operator->(void) const noexcept -> std::conditional_t<IsConst, const_pointer, pointer>
            {
                return &(static_cast<node_t *>(node_)->value);
            }

            constexpr iterator_t &operator++(void) noexcept
            {
                node_ = node_->next;
                return *this;
            }

            constexpr iterator_t operator++(int) noexcept
            {
                return iterator_t{std::exchange(node_, node_->next)};
            }

            constexpr bool operator==(const iterator_t &other) const noexcept
            {
                return node_ == other.node_;
            }
        };

        [[no_unique_address]] allocator_type allocator_;

        node_base_t head_;

    public:
        using iterator = iterator_t<false>;
        using const_iterator = iterator_t<true>;

    public:
        explicit constexpr forward_list(const Alloc &allocator = {}) noexcept
            : allocator_{allocator}, head_{}
        {
        }

        template <contract::ContainerCompatibleRange<value_type> R>
        explicit constexpr forward_list(R &&rg, const allocator_type &allocator = {}) noexcept
            : forward_list{allocator}
        {
            prepend_range(rg);
        }

        constexpr forward_list(const forward_list &other) noexcept
            : forward_list{other, other.allocator_}
        {
        }

        constexpr forward_list(forward_list &&other) noexcept
            : allocator_{std::move(other.allocator_)}, head_{std::exchange(other.head_, {nullptr})}
        {
        }

        constexpr ~forward_list() noexcept
        {
            clear();
        }

        constexpr forward_list &operator=(const forward_list &other) noexcept
        requires std::is_nothrow_copy_constructible_v<value_type>
        {
            if (this == &other)
                return *this;

            // Using a slower way to copy elements
            clear();
            allocator_ = other.allocator_;
            prepend_range(other);

            return *this;
        }

        constexpr forward_list &operator=(forward_list &&other) noexcept
        {
            if (this == &other)
                return *this;

            clear();

            allocator_ = std::move(other.allocator_);
            head_.next = std::exchange(other.head_, {nullptr});

            return *this;
        }

    public:
        constexpr auto front(this auto &&self) noexcept
        {
            kassert(!self.empty(), "Call front on an empty forward list");
            return *(self.begin());
        }

    public:
        template <typename U>
        constexpr auto before_begin(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator, iterator>
        {
            return {const_cast<node_base_t *>(&self.head_)};
        }

        constexpr const_iterator cbefore_begin(void) const noexcept
        {
            return {const_cast<node_base_t *>(&head_)};
        }

        template <typename U>
        constexpr auto begin(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator, iterator>
        {
            return {self.head_.next};
        }

        constexpr const_iterator cbegin(void) const noexcept
        {
            return {head_.next};
        }

        template <typename U>
        constexpr auto end(this U &&) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator, iterator>
        {
            return {nullptr};
        }

        constexpr const_iterator cend(void) const noexcept
        {
            return {nullptr};
        }

    public:
        constexpr bool empty(void) const noexcept
        {
            return head_.next == nullptr;
        }

    public:
        constexpr void clear(void) noexcept
        {
            while (!empty())
                pop_front();
        }

        template <typename... Args>
        requires std::constructible_from<value_type, Args...>
        constexpr iterator emplace_after(const_iterator position, Args &&...args) noexcept
        {
            auto memory = allocator_.allocate(sizeof(node_t));
            kassert(memory, "Memory allocation failed for forward_list");

            auto new_node = ::new (memory) node_t{{position.node_->next}, {std::forward<Args>(args)...}};
            position.node_->next = new_node;
            return {new_node};
        }

        constexpr iterator erase_after(const_iterator position) noexcept
        {
            kassert(position.node_->next, "Try to erase invalid node");

            std::destroy_at(&(*(++auto{position})));
            allocator_.deallocate(std::exchange(position.node_->next, position.node_->next->next), sizeof(node_t));
            return {position.node_->next};
        }

        constexpr iterator erase_after(const_iterator first, const_iterator last) noexcept
        {
            while (++auto{first} != last)
            {
                std::destroy_at(&(*(++auto{first})));
                allocator_.deallocate(std::exchange(first.node_->next, first.node_->next->next), sizeof(node_t));
            }

            return {last.node};
        }

        template <contract::ContainerCompatibleRange<value_type> R>
        requires std::is_nothrow_copy_constructible_v<value_type>
        constexpr void prepend_range(R &&rg) noexcept
        {
            auto it = before_begin();
            for (const auto &i : rg)
                it = emplace_after(it, i);
        }

        constexpr void pop_front(void) noexcept
        {
            erase_after(before_begin());
        }

        constexpr void swap(forward_list &other) noexcept
        {
            std::swap(head_, other.head_);
            std::swap(allocator_, other.allocator_);
        }
    };

    template <std::ranges::input_range R, contract::UniversalAlloctor Alloc>
    forward_list(R &&, Alloc = Alloc{}) -> forward_list<std::ranges::range_value_t<R>, Alloc>;

    template <contract::ContainerType T, contract::UniversalAlloctor Alloc>
    constexpr void swap(forward_list<T, Alloc> &_1, forward_list<T, Alloc> &_2) noexcept
    {
        _1.swap(_2);
    }
}