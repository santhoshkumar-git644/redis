#include "value.h"
#include <charconv>

namespace inferno {
namespace storage {

std::vector<InfernoObject::SharedPtr> SharedObjects::shared_integers_;

void SharedObjects::initialize() {
    // Pre-allocate integers from 0 to 9999 to mimic Redis shared.integers
    shared_integers_.reserve(10000);
    for (int64_t i = 0; i < 10000; ++i) {
        // Create bypassing the pool to avoid recursion
        shared_integers_.push_back(std::shared_ptr<InfernoObject>(new InfernoObject(i)));
    }
}

InfernoObject::SharedPtr SharedObjects::getInteger(int64_t val) {
    if (val >= 0 && val < 10000) {
        return shared_integers_[val];
    }
    return nullptr;
}

InfernoObject::InfernoObject(std::string str) 
    : type_(ObjectType::STRING), encoding_(ObjectEncoding::RAW), value_(std::move(str)) {
    try_encode_int();
}

InfernoObject::InfernoObject(int64_t val)
    : type_(ObjectType::STRING), encoding_(ObjectEncoding::INT), value_(val) {}

InfernoObject::InfernoObject(ListTag)
    : type_(ObjectType::LIST), encoding_(ObjectEncoding::QUICKLIST), value_(std::deque<std::string>()) {}

InfernoObject::SharedPtr InfernoObject::create(std::string str) {
    // We can check if it parses as an int first, and if so, check the shared pool
    int64_t val;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        auto shared = SharedObjects::getInteger(val);
        if (shared) return shared;
        return std::shared_ptr<InfernoObject>(new InfernoObject(val));
    }
    return std::shared_ptr<InfernoObject>(new InfernoObject(std::move(str)));
}

InfernoObject::SharedPtr InfernoObject::create(int64_t val) {
    auto shared = SharedObjects::getInteger(val);
    if (shared) return shared;
    return std::shared_ptr<InfernoObject>(new InfernoObject(val));
}

InfernoObject::SharedPtr InfernoObject::createList() {
    return std::shared_ptr<InfernoObject>(new InfernoObject(ListTag{}));
}

void InfernoObject::try_encode_int() {
    if (encoding_ != ObjectEncoding::RAW) return;
    
    const std::string& str = std::get<std::string>(value_);
    if (str.empty()) return;
    
    int64_t val;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        encoding_ = ObjectEncoding::INT;
        value_ = val;
    }
}

std::string InfernoObject::toString() const {
    if (encoding_ == ObjectEncoding::INT) {
        return std::to_string(std::get<int64_t>(value_));
    }
    return std::get<std::string>(value_);
}

bool InfernoObject::getInt(int64_t& out) const {
    if (encoding_ == ObjectEncoding::INT) {
        out = std::get<int64_t>(value_);
        return true;
    }
    
    const std::string& str = std::get<std::string>(value_);
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);
    return ec == std::errc() && ptr == str.data() + str.size();
}

std::deque<std::string>& InfernoObject::getList() {
    if (type_ != ObjectType::LIST) {
        throw std::runtime_error("Object is not a list");
    }
    return std::get<std::deque<std::string>>(value_);
}

const std::deque<std::string>& InfernoObject::getList() const {
    if (type_ != ObjectType::LIST) {
        throw std::runtime_error("Object is not a list");
    }
    return std::get<std::deque<std::string>>(value_);
}

} // namespace storage
} // namespace inferno
