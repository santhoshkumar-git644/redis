#include "resp_types.h"

namespace inferno {
namespace protocol {

std::string serializeRESP(const RESPObject& obj) {
    return std::visit(overloaded {
        [](const RESPNull&) -> std::string {
            return "$-1\r\n"; // Null bulk string by default
        },
        [](const RESPSimpleString& s) -> std::string {
            return "+" + s + "\r\n";
        },
        [](const RESPError& e) -> std::string {
            return "-" + e + "\r\n";
        },
        [](const RESPInteger& i) -> std::string {
            return ":" + std::to_string(i) + "\r\n";
        },
        [](const RESPBulkString& b) -> std::string {
            return "$" + std::to_string(b.length()) + "\r\n" + b + "\r\n";
        },
        [](const std::shared_ptr<RESPArray>& a) -> std::string {
            if (!a || a->is_null) {
                return "*-1\r\n";
            }
            std::string res = "*" + std::to_string(a->elements.size()) + "\r\n";
            for (const auto& elem : a->elements) {
                res += serializeRESP(elem);
            }
            return res;
        }
    }, obj);
}

} // namespace protocol
} // namespace inferno
