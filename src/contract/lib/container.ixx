module;

#include <concepts>
#include <ranges>
#include <type_traits>

export module contract.container;

export namespace kernel::contract
{
    template <typename R, typename T>
    concept ContainerCompatibleRange =
        std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, T>;

    template <typename T>
    concept ContainerType = std::same_as<T, std::remove_cvref_t<T>>;
}