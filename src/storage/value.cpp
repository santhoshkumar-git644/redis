#include "value.h"
#include <charconv>

namespace inferno {
namespace storage {

void InfernoObject::try_encode_int() {
    if (encoding_ != ObjectEncoding::RAW) return;
    
    const std::string& str = std::get<std::string>(value_);
    if (str.empty()) return;
    
    int64_t val;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
        // Successfully parsed the entire string as an integer
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

} // namespace storage
} // namespace inferno
