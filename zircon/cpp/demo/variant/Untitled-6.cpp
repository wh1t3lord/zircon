#include <iostream>
#include <variant>
#include <type_traits>

// Helper to get the index of a type in a variant
template <typename T, typename Variant>
struct variant_index;

template <typename T, typename... Types>
struct variant_index<T, std::variant<Types...>> {
    static constexpr std::size_t value = [] {
        constexpr std::size_t n = sizeof...(Types);
        std::size_t index = n;  // Default to out-of-bounds
        std::size_t i = 0;
        ((std::is_same_v<T, Types> ? (index = i) : 0), ..., i++);
        return index;
    }();
};

// Convenience variable template
template <typename T, typename Variant>
inline constexpr std::size_t variant_index_v = variant_index<T, Variant>::value;

int main() {
    using MyVariant = std::variant<int, float, std::string>;

    static_assert(variant_index_v<int, MyVariant> == 0, "int should be at index 0");
    static_assert(variant_index_v<float, MyVariant> == 1, "float should be at index 1");
    static_assert(variant_index_v<std::string, MyVariant> == 2, "string should be at index 2");

    std::cout << "All static index assertions passed!" << std::endl;
}
