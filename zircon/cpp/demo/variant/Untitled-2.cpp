#include <iostream>
#include <variant>
#include <cstdint>
#include <type_traits>

// 1. Define our variant type
using VariantType = std::variant<int32_t, const char*, double, std::string>;

// 2. Compile-time mapping of types to bit positions
template <typename T, typename Variant, size_t Index = 0>
struct TypeIndex;

template <typename T, typename... Ts, size_t Index>
struct TypeIndex<T, std::variant<T, Ts...>, Index> {
    static constexpr size_t value = Index;
};

template <typename T, typename U, typename... Ts, size_t Index>
struct TypeIndex<T, std::variant<U, Ts...>, Index> {
    static constexpr size_t value = TypeIndex<T, std::variant<Ts...>, Index + 1>::value;
};

// 3. Get bit flag for a type inside VariantType
template <typename T>
constexpr int64_t getTypeFlag() {
    return 1LL << TypeIndex<T, VariantType>::value;
}

// 4. Compute flag dynamically based on the active type in the variant
int64_t getVariantTypeFlags(const VariantType& var) {
    return std::visit([](auto&& value) -> int64_t {
        using T = std::decay_t<decltype(value)>;
        return getTypeFlag<T>();
    }, var);
}

// 5. Example usage
int main() {
    VariantType var1 = 42;          // int32_t
    VariantType var2 = "hello";     // const char*
    VariantType var3 = 3.14;        // double

    std::cout << "Flag for int32_t: " << getVariantTypeFlags(var1) << "\n";  // 1 << 0 = 1
    std::cout << "Flag for const char*: " << getVariantTypeFlags(var2) << "\n";  // 1 << 1 = 2
    std::cout << "Flag for double: " << getVariantTypeFlags(var3) << "\n";  // 1 << 2 = 4

    return 0;
}
