#ifndef INFERNOCACHE_VALUE_H
#define INFERNOCACHE_VALUE_H

#include <string>
#include <variant>
#include <memory>
#include "../memory/allocator.h"

namespace inferno {
namespace storage {

enum class ObjectType {
    STRING,
};

enum class ObjectEncoding {
    RAW,        
    INT         
};

class InfernoObject {
public:
    // Overload new/delete to track memory for objects
    void* operator new(size_t size) {
        return memory::Allocator::allocate(size);
    }
    void operator delete(void* ptr) {
        memory::Allocator::deallocate(ptr);
    }
    
    // Using std::shared_ptr for reference counting
    using SharedPtr = std::shared_ptr<InfernoObject>;

    static SharedPtr create(std::string str);
    static SharedPtr create(int64_t val);
    
    ~InfernoObject() = default;

    ObjectType getType() const { return type_; }
    ObjectEncoding getEncoding() const { return encoding_; }
    
    std::string toString() const;
    bool getInt(int64_t& out) const;

private:
    // Private constructors to enforce use of `create` (factory pattern)
    InfernoObject() : type_(ObjectType::STRING), encoding_(ObjectEncoding::RAW), value_(std::string("")) {}
    explicit InfernoObject(std::string str);
    explicit InfernoObject(int64_t val);

    void try_encode_int();

    ObjectType type_;
    ObjectEncoding encoding_;
    std::variant<std::string, int64_t> value_;
};

// Global pool of shared objects
class SharedObjects {
public:
    static void initialize();
    static InfernoObject::SharedPtr getInteger(int64_t val);

private:
    static std::vector<InfernoObject::SharedPtr> shared_integers_;
};

} // namespace storage
} // namespace inferno

#endif // INFERNOCACHE_VALUE_H
