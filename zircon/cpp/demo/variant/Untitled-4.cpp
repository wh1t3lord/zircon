#include <iostream>
#include <variant>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

// ---------------------------------------------------------------------
// 1. A compile-time function that returns a string_view containing the
// “demangled” name of type T by parsing the compiler’s function signature.
// For MSVC, we use __FUNCSIG__ and for GCC we use __PRETTY_FUNCTION__.
template <typename T>
constexpr std::string_view type_name() {
#ifdef _MSC_VER
    constexpr std::string_view p = __FUNCSIG__;
    // MSVC __FUNCSIG__ typically looks like:
    // "constexpr std::string_view __cdecl type_name<int>(void)"
    constexpr std::string_view prefix = "type_name<";
    constexpr std::string_view suffix = ">(void)";
#elif defined(__clang__)
    constexpr std::string_view p = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view p = __PRETTY_FUNCTION__;
    // GCC __PRETTY_FUNCTION__ typically looks like:
    // "constexpr std::string_view type_name() [with T = int]"
    constexpr std::string_view prefix = "with T = ";
    constexpr std::string_view suffix = ";";
#else
    return "Unsupported compiler";
#endif
    const size_t start = p.find(prefix) + prefix.size();
    const size_t end = p.rfind(suffix);
    return p.substr(start, end - start);
}

// ---------------------------------------------------------------------
// 2. A helper function to compute the index of a type T in a variant’s type list.
// We define a free, constexpr function that recurses over a parameter pack.
template <typename T, typename First, typename... Rest>
constexpr size_t variant_find_index(size_t index = 0) {
    if constexpr (std::is_same_v<T, First>)
        return index;
    else
        return variant_find_index<T, Rest...>(index + 1);
}

template <typename T>
constexpr size_t variant_find_index(size_t index = 0) {
    return static_cast<size_t>(-1); // Should not occur if T is in the pack.
}

// Now define VariantIndex using variant_find_index.
template <typename T, typename Variant>
struct VariantIndex;

template <typename T, typename... Types>
struct VariantIndex<T, std::variant<Types...>> {
    static constexpr size_t value = variant_find_index<T, Types...>(0);
};

// ---------------------------------------------------------------------
// 3. getTypeFlag: For a given type T (which must be one of the types in Variant),
// return a flag equal to 1 shifted by its index.
template <typename T, typename Variant>
constexpr int64_t getTypeFlag() {
    constexpr size_t index = VariantIndex<T, Variant>::value;
    return 1ULL << index;
}

// ---------------------------------------------------------------------
// 4. getVariantFlag: Recursively compute the combined flag for all alternatives in Variant.
template <typename Variant, size_t Index = 0>
constexpr int64_t getVariantFlag() {
    if constexpr (Index < std::variant_size_v<Variant>) {
        return getTypeFlag<std::variant_alternative_t<Index, Variant>, Variant>() |
               getVariantFlag<Variant, Index + 1>();
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------
// 5. Helper functions for compile-time string building.
constexpr void appendChar(char* dest, size_t& pos, char c) {
    dest[pos++] = c;
}

constexpr void appendStr(char* dest, size_t& pos, const char* str) {
    size_t i = 0;
    while (str[i] != '\0') {
        appendChar(dest, pos, str[i]);
        ++i;
    }
}

// A simple constexpr integer-to-string converter (base 10).
constexpr void intToStr(uint64_t value, char* dest, size_t& pos) {
    if (value == 0) {
        appendChar(dest, pos, '0');
        return;
    }
    char temp[32] = {};
    size_t tpos = 0;
    while (value > 0) {
        temp[tpos++] = '0' + (value % 10);
        value /= 10;
    }
    // Reverse the digits.
    for (size_t i = 0; i < tpos; ++i) {
        appendChar(dest, pos, temp[tpos - 1 - i]);
    }
}

// ---------------------------------------------------------------------
// 6. Recursively collect the type names from a variant into a fixed buffer,
// separating them with a comma and a space.
template <typename Variant, size_t Index = 0>
constexpr void collectTypeNames(char* dest, size_t& pos) {
    if constexpr (Index < std::variant_size_v<Variant>) {
        using T = std::variant_alternative_t<Index, Variant>;
        constexpr std::string_view tn = type_name<T>();
        for (char ch : tn) {
            appendChar(dest, pos, ch);
        }
        if constexpr (Index < std::variant_size_v<Variant> - 1) {
            appendStr(dest, pos, ", ");
        }
        collectTypeNames<Variant, Index + 1>(dest, pos);
    }
}

// ---------------------------------------------------------------------
// 7. A constexpr function that builds a string containing the total number of
// alternatives, a colon, and then the list of type names separated by commas.
template <typename Variant>
constexpr std::array<char, 512> getVariantInfo() {
    std::array<char, 512> buffer = {}; // Zero-initialized
    size_t pos = 0;
    
    // Append the total count.
    constexpr size_t count = std::variant_size_v<Variant>;
    intToStr(count, buffer.data(), pos);
    appendStr(buffer.data(), pos, ": ");
    
    // Now append the type names separated by commas.
    collectTypeNames<Variant>(buffer.data(), pos);
    
    return buffer;
}

// ---------------------------------------------------------------------
int main() {
    using VariantType = std::variant<int, double, std::string, const char*>;
    // For GCC, int is printed as "int", for MSVC it might be "int" or "int32_t" depending on settings.

    constexpr int64_t flag = getVariantFlag<VariantType>();
    // For 4 alternatives, expected flag = (1<<0) | (1<<1) | (1<<2) | (1<<3) = 15.
    static_assert(flag == 15, "Flag value was not computed correctly at compile time!");
    
    constexpr auto info = getVariantInfo<VariantType>();
    static_assert(info[0] != '\0', "The generated info string is empty!");
    
    std::cout << info.data() << std::endl;
    
    return 0;
}
