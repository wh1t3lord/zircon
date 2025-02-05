#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

//======================================================================
// 1. Helper trait: is_in_variant<T, Variant>
//    Checks whether T is one of the alternatives of a std::variant.
template <typename T, typename Variant>
struct is_in_variant;

template <typename T, typename... Types>
struct is_in_variant<T, std::variant<Types...>>
    : std::disjunction<std::is_same<T, Types>...> {};

template <typename T, typename Variant>
inline constexpr bool is_in_variant_v = is_in_variant<T, Variant>::value;

//======================================================================
// 2. Recursive helper: compute the index of T in a type list.
template <typename T, typename Head, typename... Tail>
constexpr std::size_t get_variant_index_impl() {
    if constexpr (std::is_same_v<T, Head>)
        return 0;
    else
        return 1 + get_variant_index_impl<T, Tail...>();
}

//======================================================================
// 3. variant_index: compute the index of type T inside a std::variant<...>
template <typename T, typename Variant>
struct variant_index;

template <typename T, typename... Types>
struct variant_index<T, std::variant<Types...>> {
    static constexpr std::size_t value = get_variant_index_impl<T, Types...>();
};

template <typename T, typename Variant>
inline constexpr std::size_t variant_index_v = variant_index<T, Variant>::value;

//======================================================================
// 4. Compile-time type_name<T>() using __PRETTY_FUNCTION__ and std::string_view.
// Adjust the prefix/suffix strings for your compiler.
template <typename T>
constexpr std::string_view type_name() {
#if defined(__clang__)
    std::string_view p = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "std::string_view type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    std::string_view p = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "constexpr std::string_view type_name() [with T = ";
    constexpr std::string_view suffix = "]";
#elif defined(_MSC_VER)
    std::string_view p = __FUNCSIG__;
    constexpr std::string_view prefix = "std::string_view __cdecl type_name<";
    constexpr std::string_view suffix = ">(void)";
#else
    std::string_view p = "unknown";
    constexpr std::string_view prefix = "";
    constexpr std::string_view suffix = "";
#endif
    auto start = p.find(prefix);
    if (start == std::string_view::npos)
        return "unknown";
    start += prefix.size();
    auto end = p.find(suffix, start);
    return p.substr(start, end - start);
}

//======================================================================
// 5. encode_variant_indices<Variant, Ts...>()
//    Returns a compile-time array of uint8_t where each element is the index of type Ts in Variant.
//    We also add a static_assert to ensure that each Ts is in Variant.
template <typename Variant, typename... Ts>
constexpr auto encode_variant_indices() {
    // Validate that each type in Ts... is contained in Variant.
    static_assert((is_in_variant_v<Ts, Variant> && ...),
                  "All types provided to encode_variant_indices must be in the variant");

    return std::array<uint8_t, sizeof...(Ts)>{
        static_cast<uint8_t>(variant_index_v<Ts, Variant>)...
    };
}

//======================================================================
// 6a. Helper: get the type names for all alternatives of a variant.
template <typename Variant, std::size_t... I>
constexpr auto get_variant_type_names_impl(std::index_sequence<I...>) {
    return std::array<std::string_view, sizeof...(I)>{
        type_name<std::variant_alternative_t<I, Variant>>()...
    };
}

template <typename Variant>
constexpr auto get_variant_type_names() {
    return get_variant_type_names_impl<Variant>(std::make_index_sequence<std::variant_size_v<Variant>>{});
}

//======================================================================
// 8b. New decode_variant_indices_sv:
//     Given an encoded array, decode it into an array of std::string_view.
template <typename Variant, std::size_t N>
constexpr auto decode_variant_indices_sv(const std::array<uint8_t, N>& encoded) {
    constexpr auto allNames = get_variant_type_names<Variant>();
    std::array<std::string_view, N> decoded{};
    for (std::size_t i = 0; i < N; ++i) {
        decoded[i] = allNames[encoded[i]];
    }
    return decoded;
}

// Original variant definition
using MyVariant = std::variant<int, float, std::string, char, bool>;

//---------------------------------------------------------------------
// Helper: ExtendVariantWithVectorsHelper
// This helper takes a Variant and an index sequence and builds a new variant type
// that contains for each alternative T in Variant both T and std::vector<T>.
template <typename Variant, typename IndexSeq>
struct ExtendVariantWithVectorsHelper;

template <typename Variant, std::size_t... I>
struct ExtendVariantWithVectorsHelper<Variant, std::index_sequence<I...>> {
    using type = std::variant<
        // Expand all alternatives from Variant...
        std::variant_alternative_t<I, Variant>...,
        // ...and then add, for each alternative, its vector version.
        std::vector<std::variant_alternative_t<I, Variant>>...
    >;
};

// Alias template that automatically generates the index sequence.
template <typename Variant>
using ExtendVariantWithVectors =
    typename ExtendVariantWithVectorsHelper<
        Variant,
        std::make_index_sequence<std::variant_size_v<Variant>>
    >::type;


//======================================================================
// Main: Demonstrate usage.
int main() {
    // For MyVariant, the alternatives are:
    // 0: int, 1: float, 2: double, 3: std::string, 4: char, 5: bool

    // Encode the indices for specific types: int, float, and std::string.
    // Expected: int -> 0, float -> 1, std::string -> 3.
    constexpr auto encoded = encode_variant_indices<ExtendVariantWithVectors<MyVariant>, std::vector<int>, std::vector<float>>();

    std::cout << "Encoded Values: ";
    for (const auto& val : encoded)
        std::cout << static_cast<int>(val) << " ";
    std::cout << "\n";

    // Decode using the new version (std::string_view).
    constexpr auto decodedSV = decode_variant_indices_sv<ExtendVariantWithVectors<MyVariant>>(encoded);
    std::cout << "Decoded Type Names (std::string_view):\n";
    for (const auto& sv : decodedSV)
        std::cout << sv << "\n";

    return 0;
}
