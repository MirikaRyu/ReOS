module;

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

export module lib.string;

import lib.assert;
import contract.allocator;
import contract.container;

template <typename T>
concept StringViewLike = std::convertible_to<T, std::string_view>;

constexpr auto LARGE_MIN_SIZE = 3 * sizeof(char *); // Takes 3 size_t spaces
constexpr auto SMALL_MAX_SIZE = 1u << 7;            // Only 7 bits to count free space

export namespace kernel::lib
{
    template <size_t Reserve, contract::UniversalAlloctor Alloc>
    requires(LARGE_MIN_SIZE <= Reserve && Reserve <= SMALL_MAX_SIZE)
    struct string
    {
    public:
        using value_type = char;
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
        static constexpr auto SSO_CAPACITY = Reserve - 1;

        template <size_type Size, typename = void>
        struct large_t
        {
            char *data;
            size_type size;
            size_type capacity;

            uint8_t padding__[Size - (LARGE_MIN_SIZE + 1)];

            uint8_t : 7;
            uint8_t is_large : 1;
        };

        template <typename Dummy>
        struct large_t<LARGE_MIN_SIZE + 1, Dummy>
        {
            char *data;
            size_type size;
            size_type capacity;

            uint8_t : 7;
            uint8_t is_large : 1;
        };

        template <typename Dummy>
        struct large_t<LARGE_MIN_SIZE, Dummy>
        {
            char *data;
            size_type size;
            size_type capacity : sizeof(size_type) * 8 - 1;
            size_type is_large : 1;
        };

        struct alignas(large_t<Reserve>) small_t
        {
            char data[SSO_CAPACITY];
            uint8_t available : 7;
            uint8_t is_large : 1;
        };

        union rep_t {
            large_t<Reserve> large;
            small_t small;
        };

        [[no_unique_address]] allocator_type alloc_;
        rep_t rep_;

    private:
        constexpr bool is_large(void) const noexcept
        {
            // Fix me: There is no standard way to tell whether an expression is constant or not
            if (std::is_constant_evaluated() && __builtin_constant_p(rep_.small.is_large))
                return rep_.small.is_large;
            else
                return rep_.large.is_large;
        }

    public:
        constexpr explicit string(const allocator_type &allocator = {}) noexcept
            : alloc_{allocator}
        {
            rep_.small = {
                .data{},
                .available{SSO_CAPACITY},
                .is_large{false},
            };
        }

        constexpr string(const char *str, const allocator_type &allocator = {}) noexcept
            : string{std::string_view{str}, allocator}
        {
        }

        template <contract::ContainerCompatibleRange<char> R>
        constexpr string(R &&rg, const allocator_type &allocator = {}) noexcept
            : string{allocator}
        {
            if constexpr (std::ranges::forward_range<R> || std::ranges::sized_range<R>)
            {
                const auto count = std::ranges::distance(rg);
                reserve(count);

                const auto next_it = std::uninitialized_copy(std::ranges::begin(rg), std::ranges::end(rg), data());
                if (is_large())
                {
                    rep_.large.size = count;
                    ::new (next_it) char{};
                }
                else
                {
                    rep_.small.available -= count;
                    if (count != SSO_CAPACITY)
                        ::new (next_it) char{};
                }
            }
            else
            {
                for (auto &&elem : rg)
                    push_back(elem);
            }
        }

        constexpr string(const string &other) noexcept
            : string{other, other.alloc_}
        {
        }

        constexpr string(string &&other) noexcept
            : alloc_{std::move(other.alloc_)},
              rep_{std::exchange(other.rep_, {.small = {.data{}, .available{SSO_CAPACITY}, .is_large{false}}})}
        {
        }

        constexpr ~string() noexcept
        {
            if (is_large())
                alloc_.deallocate(rep_.large.data, rep_.large.capacity + 1);
        }

        constexpr string &operator=(const StringViewLike auto &str) noexcept
        {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(str)>, string>)
                if (&str == this)
                    return *this;

            const auto sv = std::string_view{str};
            if (is_large() || sv.size() > SSO_CAPACITY)
            {
                reserve(sv.size());
                rep_.large.size = sv.size();
                ::new (std::uninitialized_copy(sv.begin(), sv.end(), data())) char{};
            }
            else
            {
                rep_.small.available = SSO_CAPACITY - sv.size();
                const auto end_it = std::uninitialized_copy(sv.begin(), sv.end(), data());
                if (rep_.small.available)
                    ::new (end_it) char{};
            }

            return *this;
        }

        constexpr string &operator=(const string &str) noexcept
        {
            if (&str == this)
                return *this;

            if (is_large() && (alloc_ != str.alloc_))
            {
                string tmp{str.alloc_};
                swap(tmp);
            }

            return operator=(std::string_view{str});
        }

        constexpr string &operator=(string &&str) noexcept
        {
            string tmp{std::move(str)};
            swap(tmp);

            return *this;
        }

    public:
        template <typename T>
        constexpr decltype(auto) operator[](this T &&self, size_type pos)
        {
            return self.data()[pos];
        }

        template <typename T>
        constexpr auto front(this T &&self)
        {
            return *self.begin();
        }

        template <typename T>
        constexpr auto back(this T &&self)
        {
            return *(self.end() - 1);
        }

        template <typename T>
        constexpr auto data(this T &&self)
        {
            return self.begin();
        }

        constexpr const char *c_str(void) const noexcept
        {
            return data();
        }

        constexpr operator std::string_view(void) const noexcept
        {
            return {c_str(), size()};
        }

    public:
        template <typename T>
        constexpr auto begin(this T &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const_iterator, iterator>
        {
            return self.is_large() ? self.rep_.large.data : self.rep_.small.data;
        }

        constexpr const_iterator cbegin(void) const noexcept
        {
            return begin();
        }

        template <typename T>
        constexpr auto end(this T &&self) noexcept
            -> std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const_iterator, iterator>
        {
            return self.begin() + self.size();
        }

        constexpr const_iterator cend(void) const noexcept
        {
            return end();
        }

        template <typename T>
        constexpr auto rbegin(this T &&self) noexcept
        {
            return std::conditional_t<std::is_const_v<std::remove_reference_t<T>>,
                                      const_reverse_iterator,
                                      reverse_iterator>{self.end()};
        }

        constexpr auto crbegin(void) const noexcept
        {
            return const_reverse_iterator{cend()};
        }

        template <typename T>
        constexpr auto rend(this T &&self) noexcept
        {
            return std::conditional_t<std::is_const_v<std::remove_reference_t<T>>,
                                      const_reverse_iterator,
                                      reverse_iterator>{self.begin()};
        }

        constexpr auto crend(void) const noexcept
        {
            return const_reverse_iterator{cbegin()};
        }

    public:
        constexpr bool empty(void) const noexcept
        {
            return size() == 0;
        }

        constexpr size_type size(void) const noexcept
        {
            return is_large() ? rep_.large.size : SSO_CAPACITY - rep_.small.available;
        }

        constexpr void reserve(size_type new_capacity) noexcept
        {
            if (new_capacity <= capacity())
                return;

            const auto tmp_data = static_cast<pointer>(alloc_.allocate(new_capacity + 1));
            kassert(tmp_data, "Memory allocation failed for string");

            ::new (std::uninitialized_copy(begin(), end(), tmp_data)) char{};

            if (is_large())
            {
                const auto old_mem_len = rep_.large.capacity + 1;
                rep_.large.capacity = new_capacity;
                alloc_.deallocate(std::exchange(rep_.large.data, tmp_data), old_mem_len);
            }
            else
            {
                const auto old_size = size();

                rep_.large.data = tmp_data;
                rep_.large.size = old_size;
                rep_.large.capacity = new_capacity;
                rep_.large.is_large = 1;
            }
        }

        constexpr size_type capacity(void) const noexcept
        {
            return is_large() ? rep_.large.capacity : SSO_CAPACITY;
        }

        constexpr void shrink_to_fit(void) noexcept
        {
            if (size() == capacity() || !is_large())
                return;

            if (size() > SSO_CAPACITY)
            {
                const auto tmp_data = static_cast<pointer>(alloc_.allocate(size() + 1));
                kassert(tmp_data, "Memory allocation failed for string");

                ::new (std::uninitialized_copy(begin(), end(), tmp_data)) char{};

                const auto old_mem_len = rep_.large.capacity + 1;
                rep_.large.capacity = size();
                alloc_.deallocate(std::exchange(rep_.large.data, tmp_data), old_mem_len);
            }
            else
            {
                const auto old_data = rep_.large.data;
                const auto old_size = size();
                const auto old_mem_len = rep_.large.capacity + 1;

                rep_.small.is_large = 0;
                rep_.small.available = SSO_CAPACITY - old_size;
                const auto end_it = std::uninitialized_copy(old_data, old_data + old_size, rep_.small.data);
                if (old_size < SSO_CAPACITY)
                    ::new (end_it) char{};

                alloc_.deallocate(old_data, old_mem_len);
            }
        }

    public:
        constexpr void clear(void) noexcept
        {
            ::new (begin()) char{};
            if (is_large())
                rep_.large.size = 0;
            else
                rep_.small.available = SSO_CAPACITY;
        }

        constexpr iterator insert(const_iterator pos, char ch) noexcept
        {
            return insert(pos, std::views::single(ch));
        }

        constexpr iterator insert(const_iterator pos, const char *str) noexcept
        {
            return insert(pos, std::string_view{str});
        }

        template <contract::ContainerCompatibleRange<char> R>
        constexpr iterator insert(const_iterator pos, R &&rg) noexcept
        {
            namespace r = std::ranges;

            if constexpr (r::forward_range<R> || r::sized_range<R>)
            {
                iterator ret{};
                const iterator mut_pos = begin() + (pos - cbegin());

                const auto count = std::ranges::distance(rg);
                if (size() + count > capacity())
                {
                    const auto new_mem_len =
                        std::max(capacity() + capacity() / 2 + 1,
                                 capacity() + count + 1); // Expand strategy: max(1.5 * size, needed size);
                    const auto tmp_data = static_cast<pointer>(alloc_.allocate(new_mem_len));
                    kassert(tmp_data, "Memory allocation failed for string");

                    ret = std::uninitialized_copy(begin(), mut_pos, tmp_data);                      // Copy front
                    const auto tail_start = std::uninitialized_copy(r::begin(rg), r::end(rg), ret); // Copy inserted
                    const auto copy_end = std::uninitialized_copy(mut_pos, end(), tail_start);      // Copy tail
                    ::new (copy_end) char{};                                                        // Add '\0'

                    if (is_large())
                    {
                        const auto old_mem_len = capacity() + 1;
                        rep_.large.capacity = new_mem_len - 1;
                        alloc_.deallocate(std::exchange(rep_.large.data, tmp_data), old_mem_len);
                        rep_.large.size += count;
                    }
                    else
                    {
                        rep_.large.size = size() + count;
                        rep_.large.data = tmp_data;
                        rep_.large.capacity = new_mem_len - 1;
                        rep_.large.is_large = 1;
                    }
                }
                else
                {
                    std::move_backward(mut_pos, end(), end() + count);
                    r::copy(rg, mut_pos);

                    if (is_large())
                    {
                        rep_.large.size += count;
                        ::new (end()) char{};
                    }
                    else
                    {
                        rep_.small.available -= count;
                        if (rep_.small.available)
                            ::new (end()) char{};
                    }

                    ret = mut_pos;
                }

                return ret;
            }
            else
            {
                auto it = pos;
                for (const auto ch : rg)
                    it = insert(it, ch) + 1;

                return {begin() + (pos - cbegin())};
            }
        }

        constexpr iterator erase(const_iterator pos) noexcept
        {
            return erase(pos, pos + 1);
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept
        {
            const iterator mut_first = begin() + (first - cbegin());

            if (first < last)
            {
                ::new (std::copy(last, cend(), mut_first)) char{};
                if (const auto count = last - first; is_large())
                    rep_.large.size -= count;
                else
                    rep_.small.available += count;
            }

            return mut_first;
        }

        constexpr void push_back(char ch) noexcept
        {
            insert(end(), ch);
        }

        constexpr void pop_back(void) noexcept
        {
            erase(end() - 1);
        }

        constexpr string &operator+=(char ch) noexcept
        {
            push_back(ch);
            return *this;
        }

        constexpr string &operator+=(const StringViewLike auto &str) noexcept
        {
            insert(end(), std::string_view{str});
            return *this;
        }

        constexpr string &replace(const_iterator first, const_iterator last, const char *str) noexcept
        {
            return replace(first, last, std::string_view{str});
        }

        constexpr string &replace(const_iterator first,
                                  const_iterator last,
                                  contract::ContainerCompatibleRange<char> auto &&rg) noexcept
        {
            const auto mut_first = begin() + (first - cbegin());
            const auto mut_last = begin() + (last - cbegin());
            const auto [ifirst, ofirst] =
                std::ranges::uninitialized_copy(rg, std::ranges::subrange{mut_first, mut_last});

            if (ifirst != std::ranges::end(rg))
                insert(ofirst, std::ranges::subrange{ifirst, std::ranges::end(rg)});
            else if (ofirst != last)
                erase(ofirst, last);

            return *this;
        }

        constexpr void swap(string &other) noexcept
        {
            std::swap(alloc_, other.alloc_);
            std::swap(rep_, other.rep_);
        }

    public:
        template <typename T>
        constexpr auto find(this T &&self, const StringViewLike auto &str, size_type pos = {}) noexcept
        {
            if (const auto str_pos = std::string_view{self}.find(str, pos); str_pos != std::string_view::npos)
                return self.begin() + str_pos;
            else
                return self.end();
        }

        template <typename T>
        constexpr auto rfind(this T &&self,
                             const StringViewLike auto &str,
                             size_type pos = std::numeric_limits<size_type>::max()) noexcept
        {
            if (const auto str_pos = std::string_view{self}.rfind(str, pos); str_pos != std::string_view::npos)
                return self.begin() + str_pos;
            else
                return self.end();
        }

    public:
        constexpr bool operator==(const StringViewLike auto &str) const noexcept
        {
            return std::string_view{*this} == std::string_view{str};
        }

        constexpr auto operator<=>(const StringViewLike auto &str) const noexcept
        {
            return std::string_view{*this} <=> std::string_view{str};
        }

        constexpr bool starts_with(char ch) const noexcept
        {
            return std::string_view{*this}.starts_with(ch);
        }

        constexpr bool starts_with(const StringViewLike auto &str) const noexcept
        {
            return std::string_view{*this}.starts_with(str);
        }

        constexpr bool ends_with(char ch) const noexcept
        {
            return std::string_view{*this}.ends_with(ch);
        }

        constexpr bool ends_with(const StringViewLike auto &str) const noexcept
        {
            return std::string_view{*this}.ends_with(str);
        }

        constexpr bool contains(char ch) const noexcept
        {
            return std::string_view{*this}.contains(ch);
        }

        constexpr bool contains(const StringViewLike auto &str) const noexcept
        {
            return std::string_view{*this}.contains(str);
        }

        constexpr string substr(const_iterator start) const noexcept
        {
            return {std::ranges::subrange{start, cend()}};
        }

        constexpr string substr(const_iterator start, const_iterator end) const noexcept
        {
            return {std::ranges::subrange{start, end}};
        }
    };

    template <size_t Reserve, typename Alloc, typename T>
    requires StringViewLike<T> || std::same_as<std::remove_cvref_t<T>, char>
    constexpr auto operator+(const string<Reserve, Alloc> &lhs, T &&rhs) noexcept
    {
        auto str{lhs};
        str += rhs;

        return str;
    }

    template <size_t Reserve, typename Alloc, typename T>
    requires StringViewLike<T> || std::same_as<std::remove_cvref_t<T>, char>
    constexpr auto operator+(T &&lhs, const string<Reserve, Alloc> &rhs) noexcept
    {
        string<Reserve, Alloc> str{lhs};
        str += rhs;

        return str;
    }

    template <size_t Reserve, typename Alloc>
    constexpr void swap(string<Reserve, Alloc> &_1, string<Reserve, Alloc> &_2) noexcept
    {
        _1.swap(_2);
    }
}