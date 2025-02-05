#include <iostream>
#include <tuple>
#include <variant>
#include <type_traits>
#include <string>
#include <vector>

// Primary template (not defined)
template <typename T>
struct function_traits;

// Specialization for function pointers
template <typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> {
    using args_tuple = std::tuple<Args...>;
};

// Specialization for function objects and lambdas
template <typename Func>
struct function_traits {
private:
    template <typename ClassType, typename Ret, typename... Args>
    static auto get_args_tuple(Ret(ClassType::*)(Args...) const) -> std::tuple<Args...>;

    template <typename ClassType, typename Ret, typename... Args>
    static auto get_args_tuple(Ret(ClassType::*)(Args...)) -> std::tuple<Args...>;

    template <typename ClassType, typename Ret, typename... Args>
    static auto get_args_tuple(Ret(ClassType::*)(Args...) const noexcept) -> std::tuple<Args...>;

    template <typename ClassType, typename Ret, typename... Args>
    static auto get_args_tuple(Ret(ClassType::*)(Args...) noexcept) -> std::tuple<Args...>;

public:
    using args_tuple = decltype(get_args_tuple(&Func::operator()));
};

// Helper to check if a type is in std::variant (fix: use std::remove_cvref_t)
template <typename T, typename Variant>
struct is_in_variant;

template <typename T, typename... Ts>
struct is_in_variant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};

// Recursive template to check all argument types exist in std::variant
template <typename Tuple, typename Variant, std::size_t Index = 0>
constexpr bool are_args_in_variant() {
    if constexpr (Index >= std::tuple_size_v<Tuple>) {
        return true; // Base case: all types checked
    } else {
        using ArgType = std::tuple_element_t<Index, Tuple>;
        return is_in_variant<ArgType, Variant>::value && are_args_in_variant<Tuple, Variant, Index + 1>();
    }
}

// Wrapper function where MyVariant is a template argument, and lambda is passed as a function argument
template <typename Variant, typename Func>
constexpr bool are_lambda_args_in_variant(Func) {
    return are_args_in_variant<typename function_traits<Func>::args_tuple, Variant>();
}

// Example usage
int main() {
    using MyVariant = std::variant<int, double, float, std::string, const std::string&, std::string&>;

    auto l1 = [](int a, float b) {};          // Should return true
    auto l2 = [](int a, std::string b) {};    // Should return true
    auto l3 = [](const std::string& s) {};   // Should also return true now!
    auto l4 = [](std::string& v) {}; 
    auto l5 = [](std::string* v){};
    auto l6 = [](const std::string* v){};
    auto l7 = [](const std::string* const v){}; 
    auto l8 = [](std::string&& v){};
    auto l9 = [](const std::string&& a){};

    std::cout << std::boolalpha;
    std::cout << "Lambda2 valid: " << are_lambda_args_in_variant<MyVariant>(l2) << "\n"; // true
    std::cout << "Lambda3 valid: " << are_lambda_args_in_variant<MyVariant>(l3) << "\n"; // true
    std::cout << "Lambda4 valid: " << are_lambda_args_in_variant<MyVariant>(l4) << "\n"; // false
    std::cout << "Lambda5 valid: " << are_lambda_args_in_variant<MyVariant>(l5) << "\n"; // false
    std::cout << "Lambda6 valid: " << are_lambda_args_in_variant<MyVariant>(l6) << "\n"; // false
    std::cout << "Lambda7 valid: " << are_lambda_args_in_variant<MyVariant>(l7) << "\n"; // false
    std::cout << "Lambda8 valid: " << are_lambda_args_in_variant<MyVariant>(l8) << "\n"; // false
    std::cout << "Lambda9 valid: " << are_lambda_args_in_variant<MyVariant>(l9) << "\n"; // false

    return 0;
}
