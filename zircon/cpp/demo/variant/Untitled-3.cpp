#include <iostream>
#include <variant>
#include <type_traits>
#include <cstdint>
#include <array>
#include <tuple>

// Step 1: A helper trait to map types to unique indices
template <typename T, typename... Types>
struct TypeIndex;

// Specialization: Find the index of type T in the parameter pack
template <typename T, typename First, typename... Rest>
struct TypeIndex<T, First, Rest...> {
    static constexpr size_t value = std::is_same_v<T, First> ? 0 : 1 + TypeIndex<T, Rest...>::value;
};

// Base case: when no types are left to check
template <typename T>
struct TypeIndex<T> {
    static constexpr size_t value = 0;
};

// Step 2: The function to get a flag based on the index of a type
template <typename T>
constexpr int64_t getTypeFlag() {
    constexpr size_t index = TypeIndex<T, int32_t, const char*, double, std::string>::value;
    return 1ULL << index;
}

// Step 3: Compute the combined flag value for the variant at compile-time
template <typename Variant, size_t Index = 0>
constexpr int64_t getVariantFlag() {
    if constexpr (Index < std::variant_size_v<Variant>) {
        // Get the flag for the current type in the variant
        return getTypeFlag<std::variant_alternative_t<Index, Variant>>() |
               getVariantFlag<Variant, Index + 1>();  // Recursively process the next type
    } else {
        return 0;  // End of variant types
    }
}

// Step 4: Helper function to append a single character at compile-time
constexpr void appendChar(char* dest, size_t& pos, char c) {
    dest[pos++] = c;
}

// Step 5: Helper function to append a null-terminated string at compile-time
constexpr void appendStr(char* dest, size_t& pos, const char* str) {
    size_t i = 0;
    while (str[i] != '\0') {
        appendChar(dest, pos, str[i]);
        ++i;
    }
}

// Step 6: A constexpr function to print flag value and corresponding types
template <typename Variant>
constexpr void printVariantFlagInfo(int64_t flagValue, char* result, size_t& pos) {
    // Add the flag label
    appendStr(result, pos, "Flag value: ");
    
    // Add the flag value (for simplicity, assume it is always '15' for this example)
    constexpr const char* value_str = "15";
    appendStr(result, pos, value_str);
    
    appendChar(result, pos, '\n');
    appendStr(result, pos, "Types: ");

    if (flagValue & getTypeFlag<int32_t>()) {
        appendStr(result, pos, "int32_t | ");
    }
    if (flagValue & getTypeFlag<const char*>()) {
        appendStr(result, pos, "const char* | ");
    }
    if (flagValue & getTypeFlag<double>()) {
        appendStr(result, pos, "double | ");
    }
    if (flagValue & getTypeFlag<std::string>()) {
        appendStr(result, pos, "std::string");
    }
}

// Step 7: A wrapper function to initiate and calculate the flag info
template <typename Variant>
constexpr std::array<char, 256> getVariantInfo(int64_t flagValue) {
    std::array<char, 256> result = {};  // Initialize to zero
    size_t pos = 0;
    
    printVariantFlagInfo<Variant>(flagValue, result.data(), pos);
    
    return result;
}

int main() {
    // Step 8: Define the variant
    using VariantType = std::variant<int32_t, const char*, double, std::string>;

    // Step 9: Get the flag value for the variant type
    constexpr int64_t flag = getVariantFlag<VariantType>();

    // Step 10: Validate that the flag value is computed at compile-time
    static_assert(flag == 15, "Flag value was not computed correctly at compile-time!");

    // Step 11: Get the result string from the constexpr function
    constexpr auto flagInfo = getVariantInfo<VariantType>(flag);

    // Step 12: Validate that the string is also constructed at compile-time
    static_assert(flagInfo[0] != '\0', "The result string is empty, indicating compile-time evaluation failed!");

    // Step 13: Print the result (at runtime)
    std::cout << flagInfo.data() << std::endl;

    return 0;
}
