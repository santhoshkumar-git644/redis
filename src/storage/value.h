#ifndef INFERNOCACHE_VALUE_H
#define INFERNOCACHE_VALUE_H

#include <string>
#include <variant>

namespace inferno {
namespace storage {

enum class ObjectType {
    STRING,
    // Future types for Milestone 8: LIST, HASH, SET, ZSET
};

enum class ObjectEncoding {
    RAW,        // Standard string
    INT         // 64-bit integer
};

class InfernoObject {
public:
    InfernoObject() : type_(ObjectType::STRING), encoding_(ObjectEncoding::RAW), value_(std::string("")) {}
    
    // Construct from string
    explicit InfernoObject(std::string str) 
        : type_(ObjectType::STRING), encoding_(ObjectEncoding::RAW), value_(std::move(str)) {
        // Try to encode as integer to save space, mimicking Redis
        try_encode_int();
    }
    
    // Construct from integer
    explicit InfernoObject(int64_t val)
        : type_(ObjectType::STRING), encoding_(ObjectEncoding::INT), value_(val) {}

    ObjectType getType() const { return type_; }
    ObjectEncoding getEncoding() const { return encoding_; }
    
    // Get string representation regardless of encoding
    std::string toString() const;
    
    // If it's an int, return it. If it's a string that can be parsed as int, return it.
    // Otherwise return std::nullopt or throw/return false.
    bool getInt(int64_t& out) const;

private:
    void try_encode_int();

    ObjectType type_;
    ObjectEncoding encoding_;
    std::variant<std::string, int64_t> value_;
};

} // namespace storage
} // namespace inferno

#endif // INFERNOCACHE_VALUE_H
