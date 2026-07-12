#ifndef INFERNOCACHE_RESP_PARSER_H
#define INFERNOCACHE_RESP_PARSER_H

#include "resp_types.h"
#include <vector>
#include <string>
#include <optional>
#include <stack>

namespace inferno {
namespace protocol {

class RESPParser {
public:
    enum class ParseResult {
        OK,             // Successfully parsed one complete object
        NEED_MORE,      // Need more data to complete parsing
        ERROR           // Malformed packet
    };

    RESPParser() = default;

    // Feed data to the parser
    void feed(const char* data, size_t length);

    // Try to parse an object from the accumulated buffer.
    // Returns OK and populates 'out' if a full object is parsed.
    // Returns NEED_MORE if data is incomplete.
    // Returns ERROR if data is malformed.
    ParseResult parse(RESPObject& out);

    // Reset parser state
    void reset();

private:
    std::string buffer_;
    size_t pos_ = 0;

    // Helper methods to read specific types
    ParseResult readLine(std::string& line);
    ParseResult parseInteger(const std::string& line, int64_t& out);

    // Context for parsing arrays incrementally
    struct ArrayContext {
        std::shared_ptr<RESPArray> array;
        size_t target_size;
    };
    
    std::stack<ArrayContext> array_stack_;
    
    // Sometimes we need to remember if we are in the middle of a bulk string
    bool in_bulk_payload_ = false;
    int64_t pending_bulk_length_ = 0;
};

} // namespace protocol
} // namespace inferno

#endif // INFERNOCACHE_RESP_PARSER_H
