module;

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

export module lib.vector;

import lib.assert;
import contract.allocator;
import contract.container;

export namespace kernel::lib
{
    template <contract::ContainerType T, contract::UniversalAlloctor Alloc>
    struct vector
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

        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
        [[no_unique_address]] allocator_type alloc_;

        T *data_;
        size_t size_;
        size_t capacity_;

    public:
        explicit constexpr vector(const allocator_type &allocator = {}) noexcept
            : alloc_{allocator}, data_{}, size_{}, capacity_{}
        {
        }

        template <contract::ContainerCompatibleRange<value_type> R>
        requires std::is_nothrow_copy_constructible_v<value_type>
        explicit constexpr vector(R &&rg, const allocator_type &allocator = {}) noexcept
            : vector{allocator}
        {
            if constexpr (std::ranges::forward_range<R> || std::ranges::sized_range<R>)
            {
                const auto count = std::ranges::distance(rg);
                reserve(count);
                std::uninitialized_copy(std::ranges::begin(rg), std::ranges::end(rg), data_);
                size_ = count;
            }
            else
            {
                for (auto &&elem : rg)
                    emplace_back(elem);
            }
        }

        constexpr vector(const vector &other) noexcept
        requires std::is_nothrow_copy_constructible_v<value_type>
            : vector{other, other.alloc_}
        {
        }

        constexpr vector(vector &&other) noexcept
            : alloc_{std::move(other.alloc_)},
              data_{std::exchange(other.data_, nullptr)},
              size_{std::exchange(other.size_, 0)},
              capacity_{std::exchange(other.capacity_, 0)}
        {
        }

        constexpr ~vector() noexcept
        {
            clear();
            alloc_.deallocate(data_, sizeof(value_type) * capacity_);
        }

        constexpr vector &operator=(const vector &other) noexcept
        requires std::is_nothrow_copy_constructible_v<value_type>
        {
            if (this == &other)
                return *this;

            if (other.size_ > capacity_)
            {
                const auto tmp_data = static_cast<pointer>(other.alloc_.allocate(other.size_ * sizeof(value_type)));
                kassert(tmp_data, "Memory allocation failed for vector");

                clear();
                alloc_.deallocate(std::exchange(data_, tmp_data),
                                  sizeof(value_type) * std::exchange(capacity_, other.size_));

                std::uninitialized_copy_n(other.data_, other.size_, tmp_data);
                size_ = other.size_;
            }
            else
            {
                clear();
                std::uninitialized_copy_n(other.data_, other.size_, data_);
                size_ = other.size_;
            }
            alloc_ = other.alloc_;

            return *this;
        }

        constexpr vector &operator=(vector &&other) noexcept
        {
            if (this == &other)
                return *this;

            clear();
            alloc_.deallocate(data_, sizeof(value_type) * capacity_);

            alloc_ = std::move(other.alloc_);
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);

            return *this;
        }

    public:
        template <typename U>
        constexpr auto operator[](this U &&self, size_type pos) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_reference, reference>
        {
            return self.data_[pos];
        }

        template <typename U>
        constexpr auto front(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_reference, reference>
        {
            return *self.data_;
        }

        template <typename U>
        constexpr auto back(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_reference, reference>
        {
            return *(self.end() - 1);
        }

        template <typename U>
        constexpr auto data(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_pointer, pointer>
        {
            return self.data_;
        }

    public:
        template <typename U>
        constexpr auto begin(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator, iterator>
        {
            return self.data_;
        }

        constexpr const_iterator cbegin(void) const noexcept
        {
            return data_;
        }

        template <typename U>
        constexpr auto end(this U &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<U>>, const_iterator, iterator>
        {
            return self.data_ + self.size_;
        }

        constexpr const_iterator cend(void) const noexcept
        {
            return data_ + size_;
        }

        template <typename U>
        constexpr auto rbegin(this U &&self) noexcept
        {
            return std::conditional_t<std::is_const_v<std::remove_reference_t<U>>,
                                      const_reverse_iterator,
                                      reverse_iterator>{self.end()};
        }

        constexpr auto crbegin(void) const noexcept
        {
            return const_reverse_iterator{cend()};
        }

        template <typename U>
        constexpr auto rend(this U &&self) noexcept
        {
            return std::conditional_t<std::is_const_v<std::remove_reference_t<U>>,
                                      const_reverse_iterator,
                                      reverse_iterator>{self.begin()};
        }

        constexpr auto crend(void) const noexcept
        {
            return const_reverse_iterator{cbegin()};
        }

    public:
        [[nodiscard]] bool empty(void) const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] size_type size(void) const noexcept
        {
            return size_;
        }

        void reserve(size_type new_capacity) noexcept
        requires std::is_nothrow_move_constructible_v<value_type>
        {
            if (new_capacity <= capacity_)
                return;

            const auto tmp_data = static_cast<pointer>(alloc_.allocate(new_capacity * sizeof(value_type)));
            kassert(tmp_data, "Memory allocation failed for vector");

            std::uninitialized_move_n(data_, size_, tmp_data);
            std::destroy_n(data_, size_);
            alloc_.deallocate(std::exchange(data_, tmp_data),
                              sizeof(value_type) * std::exchange(capacity_, new_capacity));
        }

        [[nodiscard]] size_type capacity(void) const noexcept
        {
            return capacity_;
        }

        void shrink_to_fit(void) noexcept
        requires std::is_nothrow_move_constructible_v<value_type>
        {
            if (capacity_ == size_)
                return;

            const auto tmp_data = static_cast<pointer>(alloc_.allocate(size_ * sizeof(value_type)));
            kassert(tmp_data, "Memory allocation failed for vector");

            std::uninitialized_move_n(data_, size_, tmp_data);
            std::destroy_n(data_, size_);
            alloc_.deallocate(std::exchange(data_, tmp_data), sizeof(value_type) * std::exchange(capacity_, size_));
        }

    public:
        void clear(void) noexcept
        {
            std::destroy_n(data_, size_);
            size_ = 0;
        }

        template <typename... Args>
        requires std::constructible_from<value_type, Args...>
        constexpr iterator emplace(const_iterator pos, Args &&...args) noexcept
        requires std::is_nothrow_move_constructible_v<value_type> && std::is_nothrow_move_assignable_v<value_type>
        {
            iterator ret{};
            const iterator mut_pos = begin() + (pos - cbegin());

            if (size_ >= capacity_)
            {
                const auto new_capacity = capacity_ < 2 ? 2 : capacity_ + capacity_ / 2;
                const auto tmp_data = static_cast<pointer>(alloc_.allocate(new_capacity * sizeof(value_type)));
                kassert(tmp_data, "Memory allocation failed for vector");

                const auto last = std::uninitialized_move(begin(), mut_pos, tmp_data);
                std::construct_at(last, std::forward<Args>(args)...);
                ret = last;
                std::uninitialized_move(mut_pos, end(), last + 1);

                std::destroy_n(data_, size_);
                alloc_.deallocate(std::exchange(data_, tmp_data),
                                  sizeof(value_type) * std::exchange(capacity_, new_capacity));
            }
            else
            {
                if (pos != cend())
                {
                    // Dirty works:
                    // Construct tailing element to prevent calling move assign operator on raw memory in move_backward
                    std::construct_at(end(), std::move(*(end() - 1)));
                    std::move_backward(mut_pos, end() - 1, end());

                    value_type tmp{std::forward<Args>(args)...};
                    *mut_pos = std::move(tmp);
                }
                else
                    std::construct_at(mut_pos, std::forward<Args>(args)...);

                ret = mut_pos;
            }
            ++size_;

            return ret;
        }

        constexpr iterator erase(const_iterator pos) noexcept
        requires std::is_nothrow_move_assignable_v<value_type>
        {
            const iterator mut_pos = begin() + (pos - cbegin());

            std::move(mut_pos + 1, end(), mut_pos);
            pop_back();

            return mut_pos;
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept
        requires std::is_nothrow_move_assignable_v<value_type>
        {
            const iterator mut_first = begin() + (first - cbegin());

            if (first < last)
            {
                const iterator mut_last = begin() + (last - cbegin());

                std::move(mut_last, end(), mut_first);
                std::destroy(mut_first + (end() - mut_last), end());
                size_ -= last - first;
            }

            return mut_first;
        }

        template <typename... Args>
        constexpr reference emplace_back(Args &&...args) noexcept
        {
            return *emplace(end(), std::forward<Args>(args)...);
        }

        void pop_back(void) noexcept
        {
            std::destroy_at(data_ + --size_);
        }

        constexpr void swap(vector &other) noexcept
        {
            std::swap(alloc_, other.alloc_);
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
            std::swap(capacity_, other.capacity_);
        }
    };

    template <std::ranges::input_range R, contract::UniversalAlloctor Alloc>
    vector(R &&, Alloc = Alloc{}) -> vector<std::ranges::range_value_t<R>, Alloc>;

    template <contract::ContainerType T, contract::UniversalAlloctor Alloc>
    void swap(vector<T, Alloc> &_1, vector<T, Alloc> &_2) noexcept
    {
        _1.swap(_2);
    }
}