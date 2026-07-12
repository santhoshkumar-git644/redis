#ifndef INFERNOCACHE_COMMAND_HANDLER_H
#define INFERNOCACHE_COMMAND_HANDLER_H

#include "../protocol/resp_types.h"
#include "../storage/dict.h"
#include <memory>
#include <string>

namespace inferno {
namespace server {

class CommandHandler {
public:
    static CommandHandler& instance() {
        static CommandHandler handler;
        return handler;
    }

    // Process a parsed RESP command and return the response
    protocol::RESPObject handleCommand(const protocol::RESPObject& command);

    // Get the global storage dictionary (mainly for testing)
    storage::Dict& getDict() { return dict_; }

    void runActiveExpiration() {
        dict_.activeExpireCheck();
    }

private:
    CommandHandler() = default;

    // Command implementations
    protocol::RESPObject cmdPing(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdEcho(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdSet(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdGet(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdDel(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdExists(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdMSet(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdMGet(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdIncr(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdMemory(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdExpire(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdPExpire(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdTTL(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdPTTL(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdPersist(const std::shared_ptr<protocol::RESPArray>& array);
    
    // List commands
    protocol::RESPObject cmdLPush(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdRPush(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdLPop(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdRPop(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdLRange(const std::shared_ptr<protocol::RESPArray>& array);
    protocol::RESPObject cmdLLen(const std::shared_ptr<protocol::RESPArray>& array);

    // Helper to extract string from RESPObject
    std::string extractString(const protocol::RESPObject& obj);

    storage::Dict dict_;
};

} // namespace server
} // namespace inferno

#endif // INFERNOCACHE_COMMAND_HANDLER_H
