#include "command_handler.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cctype>

namespace inferno {
namespace server {

std::string CommandHandler::extractString(const protocol::RESPObject& obj) {
    if (std::holds_alternative<protocol::RESPBulkString>(obj)) {
        return std::get<protocol::RESPBulkString>(obj);
    } else if (std::holds_alternative<protocol::RESPSimpleString>(obj)) {
        return std::get<protocol::RESPSimpleString>(obj);
    }
    return "";
}

protocol::RESPObject CommandHandler::handleCommand(const protocol::RESPObject& command) {
    if (!std::holds_alternative<std::shared_ptr<protocol::RESPArray>>(command)) {
        return protocol::RESPError("ERR Protocol error: expected array");
    }

    auto array = std::get<std::shared_ptr<protocol::RESPArray>>(command);
    if (!array || array->elements.empty()) {
        return protocol::RESPError("ERR Protocol error: empty array");
    }

    std::string cmd_name = extractString(array->elements[0]);
    if (cmd_name.empty()) {
        return protocol::RESPError("ERR Protocol error: invalid command name");
    }

    // Convert to upper case for case-insensitive matching
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);

    if (cmd_name == "PING") return cmdPing(array);
    if (cmd_name == "ECHO") return cmdEcho(array);
    if (cmd_name == "SET") return cmdSet(array);
    if (cmd_name == "GET") return cmdGet(array);
    if (cmd_name == "DEL") return cmdDel(array);
    if (cmd_name == "EXISTS") return cmdExists(array);
    if (cmd_name == "MSET") return cmdMSet(array);
    if (cmd_name == "MGET") return cmdMGet(array);
    if (cmd_name == "INCR") return cmdIncr(array);
    if (cmd_name == "DECR") return cmdDecr(array);

    return protocol::RESPError("ERR unknown command '" + cmd_name + "'");
}

protocol::RESPObject CommandHandler::cmdPing(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() > 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'ping' command");
    }
    if (array->elements.size() == 2) {
        return protocol::RESPBulkString(extractString(array->elements[1]));
    }
    return protocol::RESPSimpleString("PONG");
}

protocol::RESPObject CommandHandler::cmdEcho(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'echo' command");
    }
    return protocol::RESPBulkString(extractString(array->elements[1]));
}

protocol::RESPObject CommandHandler::cmdSet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'set' command");
    }
    std::string key = extractString(array->elements[1]);
    std::string val = extractString(array->elements[2]);
    
    dict_.set(key, storage::InfernoObject::create(std::move(val)));
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdGet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'get' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (opt_val) {
        return protocol::RESPBulkString(opt_val->toString());
    }
    return protocol::RESPNull();
}

protocol::RESPObject CommandHandler::cmdDel(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'del' command");
    }
    int64_t count = 0;
    for (size_t i = 1; i < array->elements.size(); ++i) {
        if (dict_.del(extractString(array->elements[i]))) {
            count++;
        }
    }
    return protocol::RESPInteger(count);
}

protocol::RESPObject CommandHandler::cmdExists(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'exists' command");
    }
    int64_t count = 0;
    for (size_t i = 1; i < array->elements.size(); ++i) {
        if (dict_.exists(extractString(array->elements[i]))) {
            count++;
        }
    }
    return protocol::RESPInteger(count);
}

protocol::RESPObject CommandHandler::cmdMSet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3 || array->elements.size() % 2 == 0) {
        return protocol::RESPError("ERR wrong number of arguments for 'mset' command");
    }
    for (size_t i = 1; i < array->elements.size(); i += 2) {
        std::string key = extractString(array->elements[i]);
        std::string val = extractString(array->elements[i+1]);
        dict_.set(key, storage::InfernoObject::create(std::move(val)));
    }
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdMGet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'mget' command");
    }
    auto result = std::make_shared<protocol::RESPArray>();
    for (size_t i = 1; i < array->elements.size(); ++i) {
        std::string key = extractString(array->elements[i]);
        auto opt_val = dict_.get(key);
        if (opt_val) {
            result->elements.push_back(protocol::RESPBulkString(opt_val->toString()));
        } else {
            result->elements.push_back(protocol::RESPNull());
        }
    }
    return result;
}

protocol::RESPObject CommandHandler::cmdIncr(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'incr' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    int64_t val = 0;
    if (opt_val) {
        if (!opt_val->getInt(val)) {
            return protocol::RESPError("ERR value is not an integer or out of range");
        }
    }
    val++;
    dict_.set(key, storage::InfernoObject::create(val));
    return protocol::RESPInteger(val);
}

protocol::RESPObject CommandHandler::cmdDecr(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'decr' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    int64_t val = 0;
    if (opt_val) {
        if (!opt_val->getInt(val)) {
            return protocol::RESPError("ERR value is not an integer or out of range");
        }
    }
    val--;
    dict_.set(key, storage::InfernoObject::create(val));
    return protocol::RESPInteger(val);
}

} // namespace server
} // namespace inferno
