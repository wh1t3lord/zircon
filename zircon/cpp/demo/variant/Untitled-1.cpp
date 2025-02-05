#include <iostream>
#include <vector>
#include <variant>
#include <functional>
#include <map>

// Define an enum to describe argument types
enum class ArgType {
    Int32,
    CString,
    Double,
    String,
    Unknown
};

// Define a variant type that holds all possible input types
using VariantType = std::variant<int32_t, const char*, double, std::string>;

// Function registry: Stores functions taking std::vector<VariantType>
std::map<int, std::function<void(std::vector<VariantType>)>> functionRegistry;

// Struct to store expected argument types
struct LambdaSignature {
    std::vector<ArgType> argumentTypes;
};

// Registry to store lambda signatures (argument type metadata)
std::map<int, LambdaSignature> signatureRegistry;

// Function to map a type to an ArgType enum
template <typename T>
constexpr ArgType getArgType() {
    if constexpr (std::is_same_v<T, int32_t>) return ArgType::Int32;
    else if constexpr (std::is_same_v<T, const char*>) return ArgType::CString;
    else if constexpr (std::is_same_v<T, double>) return ArgType::Double;
    else if constexpr (std::is_same_v<T, std::string>) return ArgType::String;
    else return ArgType::Unknown;
}

// Function to register a lambda (no `typename Func`, explicit argument types required)
template <typename... Args>
void registerLambda(int id, void(*lambda)(std::vector<VariantType>)) {
    // Store the lambda in the function registry
    functionRegistry[id] = lambda;

    // Store the expected argument types
    LambdaSignature signature;
    (signature.argumentTypes.push_back(getArgType<Args>()), ...);
    signatureRegistry[id] = std::move(signature);
}

// Function to convert normal arguments into std::vector<VariantType>
template <typename... Args>
std::vector<VariantType> toVariantVector(Args&&... args) {
    return {VariantType(std::forward<Args>(args))...};
}

// Function to call a registered function with normal arguments
template <typename... Args>
void callFunction(int id, Args&&... args) {
    auto it = functionRegistry.find(id);
    if (it == functionRegistry.end()) {
        throw std::runtime_error("Function not found");
    }

    // Convert arguments into `std::vector<VariantType>`
    std::vector<VariantType> argVector = toVariantVector(std::forward<Args>(args)...);

    // Call the registered function
    it->second(argVector);
}

// Function to print the expected argument types for a registered lambda
void printLambdaSignature(int id) {
    auto it = signatureRegistry.find(id);
    if (it == signatureRegistry.end()) {
        std::cout << "No signature found for function ID " << id << "\n";
        return;
    }

    std::cout << "Lambda signature for function ID " << id << ": ";
    for (ArgType type : it->second.argumentTypes) {
        switch (type) {
            case ArgType::Int32: std::cout << "Int32 "; break;
            case ArgType::CString: std::cout << "CString "; break;
            case ArgType::Double: std::cout << "Double "; break;
            case ArgType::String: std::cout << "String "; break;
            default: std::cout << "Unknown "; break;
        }
    }
    std::cout << "\n";
}

int main() {
    // Register a lambda explicitly as a function pointer
    registerLambda<int32_t, const char*, double>(1, [](std::vector<VariantType> args) {
        int32_t a = std::get<int32_t>(args[0]);
        const char* b = std::get<const char*>(args[1]);
        double c = std::get<double>(args[2]);

        std::cout << "Lambda called with: " << a << ", " << b << ", " << c << "\n";
    });

    // Print the expected argument types
    printLambdaSignature(1);

    // Call the registered function using normal arguments
    callFunction(1, 42, "Hello", 3.14);

    return 0;
}
