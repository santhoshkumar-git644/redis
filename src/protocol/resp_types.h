#ifndef INFERNOCACHE_RESP_TYPES_H
#define INFERNOCACHE_RESP_TYPES_H

#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <memory>

namespace inferno {
namespace protocol {

// Forward declaration for recursive definition of Arrays
struct RESPArray;

// The types of RESP data
using RESPSimpleString = std::string;
using RESPError = std::string;
using RESPInteger = int64_t;
using RESPBulkString = std::string;
// A null bulk string will be represented as a std::nullopt or an empty variant state,
// but for simplicity in Milestone 2, we can represent it with a special subclass or optional.
// Let's use std::optional for BulkString if needed, or a specific Null struct.
struct RESPNull {};

using RESPObject = std::variant<
    RESPNull,
    RESPSimpleString,
    RESPError,
    RESPInteger,
    RESPBulkString,
    std::shared_ptr<RESPArray> // std::shared_ptr to handle recursive type
>;

struct RESPArray {
    std::vector<RESPObject> elements;
    bool is_null = false; // For null arrays (*-1\r\n)
};

// Helper for type-safe visitation
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// Helper function to serialize an object back to RESP format (useful for testing/echoing)
std::string serializeRESP(const RESPObject& obj);

} // namespace protocol
} // namespace inferno

#endif // INFERNOCACHE_RESP_TYPES_H
