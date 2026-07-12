#include "resp_parser.h"
#include "../utils/logger.h"

namespace inferno {
namespace protocol {

void RESPParser::feed(const char* data, size_t length) {
    buffer_.append(data, length);
}

void RESPParser::reset() {
    buffer_.clear();
    pos_ = 0;
    while (!array_stack_.empty()) array_stack_.pop();
    in_bulk_payload_ = false;
    pending_bulk_length_ = 0;
}

RESPParser::ParseResult RESPParser::readLine(std::string& line) {
    size_t crlf_pos = buffer_.find("\r\n", pos_);
    if (crlf_pos == std::string::npos) {
        return ParseResult::NEED_MORE;
    }
    line = buffer_.substr(pos_, crlf_pos - pos_);
    pos_ = crlf_pos + 2; // skip \r\n
    return ParseResult::OK;
}

RESPParser::ParseResult RESPParser::parseInteger(const std::string& line, int64_t& out) {
    try {
        out = std::stoll(line);
        return ParseResult::OK;
    } catch (...) {
        return ParseResult::ERROR;
    }
}

RESPParser::ParseResult RESPParser::parse(RESPObject& out) {
    // If we are currently reading a bulk string payload
    if (in_bulk_payload_) {
        // Need pending_bulk_length_ bytes + 2 for \r\n
        if (buffer_.length() - pos_ < static_cast<size_t>(pending_bulk_length_ + 2)) {
            return ParseResult::NEED_MORE;
        }

        std::string payload = buffer_.substr(pos_, pending_bulk_length_);
        pos_ += pending_bulk_length_;
        
        // Check for CRLF
        if (buffer_[pos_] != '\r' || buffer_[pos_ + 1] != '\n') {
            return ParseResult::ERROR;
        }
        pos_ += 2;
        
        in_bulk_payload_ = false;
        out = RESPBulkString(payload);
        
        // Clean buffer if empty
        if (pos_ == buffer_.length()) {
            buffer_.clear();
            pos_ = 0;
        }
        
        return ParseResult::OK;
    }

    if (pos_ >= buffer_.length()) {
        return ParseResult::NEED_MORE;
    }

    char type = buffer_[pos_];
    
    // We need to peek at the line to see if we can parse the type completely.
    // However, Arrays are recursive. So instead of a simple switch, we handle non-arrays,
    // and if it's an array, we push to a stack.

    std::string line;
    // For anything that requires a full line (everything initially)
    if (readLine(line) == ParseResult::NEED_MORE) {
        return ParseResult::NEED_MORE;
    }

    // line includes the type char at index 0. We need to process it.
    if (line.empty()) return ParseResult::ERROR;

    type = line[0];
    std::string content = line.substr(1);

    RESPObject parsed_obj;
    bool obj_ready = false;

    switch (type) {
        case '+':
            parsed_obj = RESPSimpleString(content);
            obj_ready = true;
            break;
        case '-':
            parsed_obj = RESPError(content);
            obj_ready = true;
            break;
        case ':': {
            int64_t val;
            if (parseInteger(content, val) != ParseResult::OK) return ParseResult::ERROR;
            parsed_obj = RESPInteger(val);
            obj_ready = true;
            break;
        }
        case '$': {
            int64_t len;
            if (parseInteger(content, len) != ParseResult::OK) return ParseResult::ERROR;
            
            if (len == -1) {
                parsed_obj = RESPNull();
                obj_ready = true;
            } else if (len < -1) {
                return ParseResult::ERROR;
            } else {
                in_bulk_payload_ = true;
                pending_bulk_length_ = len;
                // We need to try parsing the payload immediately in case it's already in buffer
                ParseResult payload_res = parse(parsed_obj);
                if (payload_res == ParseResult::OK) {
                    obj_ready = true;
                } else {
                    // Either NEED_MORE or ERROR
                    // If NEED_MORE, we must un-consume the line we just read so the caller 
                    // can retry parse() later. 
                    // Wait, if we are in bulk payload state, the next parse() will just 
                    // look for payload. We don't need to un-consume the line!
                    return payload_res;
                }
            }
            break;
        }
        case '*': {
            int64_t len;
            if (parseInteger(content, len) != ParseResult::OK) return ParseResult::ERROR;
            
            if (len == -1) {
                auto arr = std::make_shared<RESPArray>();
                arr->is_null = true;
                parsed_obj = arr;
                obj_ready = true;
            } else if (len < -1) {
                return ParseResult::ERROR;
            } else if (len == 0) {
                parsed_obj = std::make_shared<RESPArray>();
                obj_ready = true;
            } else {
                // Array with elements. We need to push context.
                ArrayContext ctx;
                ctx.array = std::make_shared<RESPArray>();
                ctx.target_size = len;
                array_stack_.push(ctx);
                
                // Now we need to parse elements
                // We don't return OK until the array is fully constructed
            }
            break;
        }
        default:
            return ParseResult::ERROR;
    }

    if (obj_ready) {
        // If we are building an array, append to it
        if (!array_stack_.empty()) {
            array_stack_.top().array->elements.push_back(parsed_obj);
            
            // Resolve completed arrays
            while (!array_stack_.empty() && 
                   array_stack_.top().array->elements.size() == array_stack_.top().target_size) {
                RESPObject completed_array = array_stack_.top().array;
                array_stack_.pop();
                
                if (array_stack_.empty()) {
                    out = completed_array;
                    
                    if (pos_ > 0 && pos_ == buffer_.length()) {
                        buffer_.clear();
                        pos_ = 0;
                    }
                    return ParseResult::OK;
                } else {
                    array_stack_.top().array->elements.push_back(completed_array);
                }
            }
            
            // Need more elements for the current array
            return parse(out); 
        } else {
            out = parsed_obj;
            if (pos_ > 0 && pos_ == buffer_.length()) {
                buffer_.clear();
                pos_ = 0;
            }
            // Optimization: if buffer is getting too large at the front, shrink it
            if (pos_ > 1024 * 1024) { 
                buffer_.erase(0, pos_);
                pos_ = 0;
            }
            return ParseResult::OK;
        }
    }

    // If we just pushed an array context, try parsing its elements
    if (!array_stack_.empty()) {
        return parse(out);
    }

    return ParseResult::NEED_MORE;
}

} // namespace protocol
} // namespace inferno
