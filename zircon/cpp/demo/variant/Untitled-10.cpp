#include <iostream>
#include <tuple>
#include <variant>
#include <vector>
#include <optional>

// Our variant type. (Add more types if needed.)
using Var = std::variant<int, double, std::string, char, bool>;

// Helper function to safely extract a value of type T from a variant.
template<typename T>
std::optional<T> try_get(const Var& v) {
    if (auto ptr = std::get_if<T>(&v))
        return *ptr;
    return std::nullopt;
}

// --- Generic Implementation ---
//
// This function template works for any tuple type ArgTuple. It uses an
// index sequence to iterate over the tuple elements. For each index I, it
// extracts the value from the corresponding element of the vector as type
// std::tuple_element_t<I, ArgTuple>.
template<typename ArgTuple, std::size_t... I>
std::optional<ArgTuple> extractArgsImpl(const std::vector<Var>& args, std::index_sequence<I...>) {
    // Ensure the number of variants matches the tuple size.
    if (args.size() != sizeof...(I))
        return std::nullopt;

    // Create a tuple of std::optional values (one for each expected type).
    auto optTuple = std::make_tuple( try_get<std::tuple_element_t<I, ArgTuple>>(args[I])... );
    
    // Check if every extraction succeeded.
    bool allValid = ( ... && ( std::get<I>(optTuple).has_value() ) );
    if (!allValid)
        return std::nullopt;

    // All extractions succeeded; build and return the final tuple.
    return std::make_tuple( (*std::get<I>(optTuple))... );
}

// The main extraction function. It deduces the size and types from ArgTuple
// and calls extractArgsImpl with a correctly generated index sequence.
template<typename ArgTuple>
std::optional<ArgTuple> extractArgs(const std::vector<Var>& args) {
    return extractArgsImpl<ArgTuple>(args, std::make_index_sequence<std::tuple_size_v<ArgTuple>>{});
}

// --- Example Usage ---

// For demonstration, here we define a tuple type with five elements.
// This technique works equally well with a tuple of 30, 60, or any number of elements.
using MyTuple = std::tuple<int, double, std::string, char, bool>;

int main() {
    // Create a vector of variants containing values in the same order as MyTuple.
    std::vector<Var> args = { 9, 3.14, std::string("Hello World"), 'A', true };

    // Attempt to extract a tuple of type MyTuple.
    auto extracted = extractArgs<MyTuple>(args);

    if (extracted) {
        // Unpack the tuple and use its elements.
        auto [i, d, s, c, b] = *extracted;
        std::cout << "Extracted values:\n";
        std::cout << "int: "    << i << "\n"
                  << "double: " << d << "\n"
                  << "string: " << s << "\n"
                  << "char: "   << c << "\n"
                  << "bool: "   << b << "\n";
    } else {
        std::cout << "Extraction failed (type mismatch or wrong number of arguments).\n";
    }

    return 0;
}
