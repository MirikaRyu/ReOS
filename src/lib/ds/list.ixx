module;

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

export module lib.list;

import lib.assert;
import contract.allocator;

export namespace kernel::lib
{
    template <typename T, contract::UniversalAlloctor Alloc>
    struct forward_list_t
    {
    public:
        using value_type = T;
        using allocator_type = Alloc;

    private:
        struct node_base_t
        {
            node_base_t *next;
        };

        struct node_t : node_base_t
        {
            [[no_unique_address]] value_type value;
        };

        template <bool ConstIt>
        struct iterator_t
        {
            node_base_t *node_;

            constexpr operator iterator_t<true>(void) const noexcept
            {
                return {node_};
            }

            constexpr auto operator*(void) const noexcept
                -> std::conditional_t<ConstIt, std::add_const_t<value_type> &, value_type &>
            {
                return static_cast<node_t *>(node_)->value;
            }

            constexpr auto operator->(void) const noexcept
                -> std::conditional_t<ConstIt, std::add_const_t<value_type> *, value_type *>
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

        node_base_t head_;
        [[no_unique_address]] allocator_type allocator_;

    public:
        using iterator_type = iterator_t<false>;
        using const_iterator_type = iterator_t<true>;

        constexpr forward_list_t(void) noexcept
        requires std::default_initializable<Alloc>
            : head_{}, allocator_{}
        {
        }

        constexpr forward_list_t(const Alloc &allocator) noexcept
            : head_{}, allocator_{allocator}
        {
        }

        constexpr forward_list_t(const forward_list_t &other) noexcept
        requires std::copy_constructible<value_type>
            : head_{}, allocator_{other.allocator_}
        {
            auto it = before_begin();
            for (const auto &i : other)
                it = emplace_after(it, i);
        }

        constexpr forward_list_t(forward_list_t &&other) noexcept
            : head_{std::move(other.head_)}, allocator_{other.allocator_}
        {
            other.head_.next = nullptr;
        }

        constexpr forward_list_t &operator=(const forward_list_t &other) noexcept
        requires std::copy_constructible<value_type>
        {
            if (this == &other)
                return *this;

            clear();

            allocator_ = other.allocator_;
            auto it = before_begin();
            for (const auto &i : other)
                it = emplace_after(it, i);

            return *this;
        }

        constexpr forward_list_t &operator=(forward_list_t &&other) noexcept
        {
            if (this == &other)
                return *this;

            clear();

            head_.next = other.head_;
            other.head_.next = nullptr;
            allocator_ = std::move(other.allocator_);

            return *this;
        }

        constexpr ~forward_list_t() noexcept
        {
            clear();
        }

    public:
        template <typename U>
        constexpr auto begin(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator_type, iterator_type>
        {
            return {self.head_.next};
        }

        constexpr const_iterator_type cbegin(void) const noexcept
        {
            return {head_.next};
        }

        template <typename U>
        constexpr auto before_begin(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator_type, iterator_type>
        {
            return {const_cast<node_base_t *>(&self.head_)};
        }

        constexpr const_iterator_type cbefore_begin(void) const noexcept
        {
            return {const_cast<node_base_t *>(&head_)};
        }

        template <typename U>
        constexpr auto end(this U &&) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator_type, iterator_type>
        {
            return {nullptr};
        }

        constexpr const_iterator_type cend(void) const noexcept
        {
            return {nullptr};
        }

    public:
        template <typename... Args>
        requires std::constructible_from<value_type, Args...>
        constexpr iterator_type emplace_after(const_iterator_type position, Args &&...args) noexcept
        {
            auto memory = allocator_.allocate(sizeof(node_t));
            kassert(memory, "Memory allocation failed for forward_list_t");

            auto new_node = ::new (memory) node_t{{position.node_->next}, {std::forward<Args>(args)...}};
            position.node_->next = new_node;
            return {new_node};
        }

        constexpr iterator_type erase_after(const_iterator_type position) noexcept
        {
            kassert(position.node_->next, "Try to erase invalid node");

            std::destroy_at(&(*(++auto{position})));
            allocator_.deallocate(std::exchange(position.node_->next, position.node_->next->next), sizeof(node_t));
            return {position.node_->next};
        }

        constexpr void pop_front(void) noexcept
        {
            erase_after(before_begin());
        }

        constexpr void clear(void) noexcept
        {
            while (!empty())
                pop_front();
        }

        constexpr auto front(this auto &&self) noexcept
        {
            kassert(!self.empty(), "Call front on an empty forward list");
            return *(self.begin());
        }

        [[nodiscard]] constexpr bool empty(void) const noexcept
        {
            return head_.next == nullptr;
        }

    public:
        void swap(forward_list_t &other) noexcept
        {
            std::swap(head_, other.head_);
            std::swap(allocator_, other.allocator_);
        }
    };

    template <typename T, contract::UniversalAlloctor Alloc>
    void swap(forward_list_t<T, Alloc> &_1, forward_list_t<T, Alloc> &_2) noexcept
    {
        _1.swap(_2);
    }
}