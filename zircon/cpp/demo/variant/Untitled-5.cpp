#include <iostream>
#include <variant>
#include <type_traits>
#include <cstdint>
#include <array>

// ----------------------------------------------------------------------
// Step 1: A helper function to compute the index of a type T in a parameter pack.
// This function recurses over the pack.
template <typename T, typename First, typename... Rest>
constexpr size_t variant_find_index(size_t index = 0) {
    if constexpr (std::is_same_v<T, First>)
        return index;
    else
        return variant_find_index<T, Rest...>(index + 1);
}

// Base-case: when no types remain (this should never be instantiated if T is in the pack)
template <typename T>
constexpr size_t variant_find_index(size_t index = 0) {
    return static_cast<size_t>(-1);
}

// VariantIndex: Given type T and a std::variant, compute T’s index in the variant’s type list.
template <typename T, typename Variant>
struct VariantIndex;

template <typename T, typename... Types>
struct VariantIndex<T, std::variant<Types...>> {
    static constexpr size_t value = variant_find_index<T, Types...>(0);
};

// ----------------------------------------------------------------------
// Step 2: getTypeFlag: For a given type T (which is one alternative in Variant),
// return a flag equal to 1 shifted left by its index.
template <typename T, typename Variant>
constexpr int64_t getTypeFlag() {
    constexpr size_t index = VariantIndex<T, Variant>::value;
    return 1ULL << index;
}

// ----------------------------------------------------------------------
// Step 3: getVariantFlag: Recursively compute the combined flag for all alternatives in Variant.
// For a variant with N alternatives, this equals (1 << N) - 1.
template <typename Variant, size_t Index = 0>
constexpr int64_t getVariantFlag() {
    if constexpr (Index < std::variant_size_v<Variant>) {
        return getTypeFlag<std::variant_alternative_t<Index, Variant>, Variant>() |
               getVariantFlag<Variant, Index + 1>();
    } else {
        return 0;
    }
}

// ----------------------------------------------------------------------
// Step 4: getVariantSignature:
// Pack both the count (N) and the combined flag into a single integer.
// We reserve exactly N lower bits for the flag and store the count in the upper bits.
template <typename IntType, typename Variant>
constexpr IntType getVariantSignature() {
    constexpr size_t count = std::variant_size_v<Variant>;
    constexpr IntType flag = static_cast<IntType>((1ULL << count) - 1);
    constexpr size_t flagBits = count;            // We use count bits for the flags.
    constexpr size_t totalBits = sizeof(IntType) * 8;
    constexpr size_t countBits = totalBits - flagBits;
    static_assert(count < (1ULL << countBits), "IntType is too small to encode the count.");
    return (static_cast<IntType>(count) << flagBits) | flag;
}

// ----------------------------------------------------------------------
// decodeCount: Given a signature, find the count by trying candidate values.
// We search for the smallest n (0 <= n < 64) such that shifting signature right by n equals n.
constexpr size_t decodeCount(uint64_t signature) {
    for (size_t n = 0; n < 64; ++n) {
        if ((signature >> n) == n)
            return n;
    }
    return 0; // Should not happen.
}

// decodeFlags: Given a signature and the count, extract the lower count bits.
constexpr uint64_t decodeFlags(uint64_t signature, size_t count) {
    return signature & ((1ULL << count) - 1);
}

// ----------------------------------------------------------------------
// maxArgumentsEncodable: Given an integer type IntType, return the maximum number
// of alternatives (N) that can be encoded. We require that count < 2^(B - N),
// where B is the total bits in IntType.
template <typename IntType, size_t N = 0>
constexpr size_t maxArgumentsEncodableImpl() {
    constexpr size_t totalBits = sizeof(IntType) * 8;
    if constexpr (N < (1ULL << (totalBits - N)))
        return maxArgumentsEncodableImpl<IntType, N + 1>();
    else
        return N - 1;
}

template <typename IntType>
constexpr size_t maxArgumentsEncodable() {
    return maxArgumentsEncodableImpl<IntType>();
}

// ----------------------------------------------------------------------
// Helper functions for compile-time string building.
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
    for (size_t i = 0; i < tpos; ++i) {
        appendChar(dest, pos, temp[tpos - 1 - i]);
    }
}

// ----------------------------------------------------------------------
// getVariantInfo: Builds a compile-time string that contains:
//  - The total count of alternatives,
//  - The signature,
//  - The decoded count,
//  - The decoded flags,
//  - The maximum number of arguments that can be encoded in IntType.
// Fields are separated by newlines.
template <typename Variant, typename IntType>
constexpr std::array<char, 512> getVariantInfo() {
    std::array<char, 512> buffer = {};
    size_t pos = 0;
    
    // Append total count.
    constexpr size_t count = std::variant_size_v<Variant>;
    intToStr(count, buffer.data(), pos);
    appendStr(buffer.data(), pos, ": ");
    
    // Append signature.
    constexpr IntType signature = getVariantSignature<IntType, Variant>();
    intToStr(signature, buffer.data(), pos);
    appendStr(buffer.data(), pos, "\n");
    
    // Append decoded count.
    constexpr size_t decoded = decodeCount(signature);
    appendStr(buffer.data(), pos, "Decoded count: ");
    intToStr(decoded, buffer.data(), pos);
    appendStr(buffer.data(), pos, "\n");
    
    // Append decoded flags.
    constexpr uint64_t dFlags = decodeFlags(signature, decoded);
    appendStr(buffer.data(), pos, "Decoded flags: ");
    intToStr(dFlags, buffer.data(), pos);
    appendStr(buffer.data(), pos, "\n");
    
    // Append maximum arguments encodable in IntType.
    constexpr size_t maxArgs = maxArgumentsEncodable<IntType>();
    appendStr(buffer.data(), pos, "Max encodable: ");
    intToStr(maxArgs, buffer.data(), pos);
    
    return buffer;
}

// ----------------------------------------------------------------------
int main() {
    using VariantType = std::variant<int, double, std::string, const char*, char, wchar_t, unsigned int, long, long long, bool, unsigned char, short>;
    
    // Compute the signature for the variant using uint16_t.
    constexpr uint16_t signature = getVariantSignature<uint16_t, VariantType>();
    // For 4 alternatives, expected signature = (4 << 4) | 15 = (4*16)+15 = 64+15 = 79.
    static_assert(signature == 53247, "Signature value was not computed correctly at compile time!");
    
    // Decode the count and flags.
    constexpr size_t decodedCount = decodeCount(signature);
    constexpr uint64_t decodedFlags = decodeFlags(signature, decodedCount);
    static_assert(decodedCount == 12, "Decoded count is incorrect!");
    static_assert(decodedFlags == 4095, "Decoded flags are incorrect!");
    
    // Compute the maximum arguments encodable in uint16_t.
    constexpr size_t maxArgs = maxArgumentsEncodable<uint16_t>();
    // (For uint16_t, typically maxArgs is around 12.)
    
    // Get the compile-time generated information string.
    constexpr auto info = getVariantInfo<VariantType, uint16_t>();
    
    static_assert(info[0] != '\0', "The generated info string is empty!");
    
    // Print the result at runtime.
    std::cout << info.data() << std::endl;
    
    return 0;
}
