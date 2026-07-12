#include "command_handler.h"
#include "../utils/logger.h"
#include "../memory/allocator.h"
#include "../persistence/aof.h"
#include "../persistence/rdb.h"
#include "pubsub.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <charconv>
#include <unordered_set>

namespace inferno {
namespace server {

void CommandHandler::handleClientDisconnect(network::socket_t client_fd) {
    in_transaction_.erase(client_fd);
    transaction_queues_.erase(client_fd);
    cmdUnwatch(client_fd);
}

void CommandHandler::markKeyDirty(const std::string& key) {
    auto it = key_watchers_.find(key);
    if (it != key_watchers_.end()) {
        for (network::socket_t fd : it->second) {
            dirty_clients_.insert(fd);
        }
    }
}

std::string CommandHandler::extractString(const protocol::RESPObject& obj) {
    if (std::holds_alternative<protocol::RESPBulkString>(obj)) {
        return std::get<protocol::RESPBulkString>(obj);
    } else if (std::holds_alternative<protocol::RESPSimpleString>(obj)) {
        return std::get<protocol::RESPSimpleString>(obj);
    }
    return "";
}

std::optional<protocol::RESPObject> CommandHandler::handleCommand(const protocol::RESPObject& command, network::socket_t client_fd) {
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
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(),
                   [](unsigned char c){ return std::toupper(c); });

    // Transaction queuing interception
    if (in_transaction_.count(client_fd) > 0 && client_fd != 0) {
        if (cmd_name != "MULTI" && cmd_name != "EXEC" && cmd_name != "DISCARD" && cmd_name != "QUIT" && cmd_name != "WATCH" && cmd_name != "UNWATCH") {
            transaction_queues_[client_fd].push_back(array);
            return protocol::RESPSimpleString("QUEUED");
        }
    }

    // Dispatch command
    std::optional<protocol::RESPObject> response;
    
    if (cmd_name == "SUBSCRIBE") {
        response = cmdSubscribe(array, client_fd);
    } else if (cmd_name == "UNSUBSCRIBE") {
        response = cmdUnsubscribe(array, client_fd);
    } else if (cmd_name == "MULTI") {
        response = cmdMulti(client_fd);
    } else if (cmd_name == "EXEC") {
        response = cmdExec(client_fd);
    } else if (cmd_name == "DISCARD") {
        response = cmdDiscard(client_fd);
    } else if (cmd_name == "WATCH") {
        response = cmdWatch(array, client_fd);
    } else if (cmd_name == "UNWATCH") {
        response = cmdUnwatch(client_fd);
    } else {
        response = dispatchCommand(cmd_name, array);
    }

    if (!response) {
        return std::nullopt; // No immediate reply (PubSub handled it)
    }

    // Persist if necessary append to AOF if the command succeeded and it is a mutating command
    if (!std::holds_alternative<protocol::RESPError>(*response)) {
        if (isMutatingCommand(cmd_name)) {
            persistence::AOFManager::instance().append(array);
        }
    }
    
    return *response;
}

protocol::RESPObject CommandHandler::dispatchCommand(const std::string& cmd_name, const std::shared_ptr<protocol::RESPArray>& array) {
    // Moved the massive if-else block logic here
    if (cmd_name == "PING") return cmdPing(array);
    if (cmd_name == "ECHO") return cmdEcho(array);
    if (cmd_name == "SET") return cmdSet(array);
    if (cmd_name == "GET") return cmdGet(array);
    if (cmd_name == "EXISTS") return cmdExists(array);
    if (cmd_name == "DEL") return cmdDel(array);
    if (cmd_name == "MSET") return cmdMSet(array);
    if (cmd_name == "MGET") return cmdMGet(array);
    if (cmd_name == "INCR") return cmdIncr(array);
    if (cmd_name == "DECR") return cmdDecr(array);
    if (cmd_name == "MEMORY") return cmdMemory(array);
    if (cmd_name == "EXPIRE") return cmdExpire(array);
    if (cmd_name == "PEXPIRE") return cmdPExpire(array);
    if (cmd_name == "TTL") return cmdTTL(array);
    if (cmd_name == "PTTL") return cmdPTTL(array);
    if (cmd_name == "PERSIST") return cmdPersist(array);
    if (cmd_name == "SAVE") return cmdSave(array);
    if (cmd_name == "BGSAVE") return cmdBgSave(array);
    if (cmd_name == "CONFIG") return cmdConfig(array);
    if (cmd_name == "PUBLISH") return cmdPublish(array);
    if (cmd_name == "LPUSH") return cmdLPush(array);
    if (cmd_name == "RPUSH") return cmdRPush(array);
    if (cmd_name == "LPOP") return cmdLPop(array);
    if (cmd_name == "RPOP") return cmdRPop(array);
    if (cmd_name == "LRANGE") return cmdLRange(array);
    if (cmd_name == "LLEN") return cmdLLen(array);
    if (cmd_name == "SADD") return cmdSAdd(array);
    if (cmd_name == "SREM") return cmdSRem(array);
    if (cmd_name == "SMEMBERS") return cmdSMembers(array);
    if (cmd_name == "SISMEMBER") return cmdSIsMember(array);
    if (cmd_name == "HSET") return cmdHSet(array);
    if (cmd_name == "HGET") return cmdHGet(array);
    if (cmd_name == "HDEL") return cmdHDel(array);
    if (cmd_name == "HGETALL") return cmdHGetAll(array);
    if (cmd_name == "ZADD") return cmdZAdd(array);
    if (cmd_name == "ZSCORE") return cmdZScore(array);
    if (cmd_name == "ZRANGE") return cmdZRange(array);
    if (cmd_name == "ZRANGEBYSCORE") return cmdZRangeByScore(array);

    return protocol::RESPError("ERR unknown command '" + cmd_name + "'");
}

bool CommandHandler::isMutatingCommand(const std::string& cmd_name) {
    static const std::unordered_set<std::string> mutating_cmds = {
        "SET", "MSET", "INCR", "DECR", "EXPIRE", "PEXPIRE", "PERSIST",
        "LPUSH", "RPUSH", "LPOP", "RPOP", "SADD", "SREM", "HSET", "HDEL", "ZADD"
    };
    return mutating_cmds.count(cmd_name) > 0;
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

protocol::RESPObject CommandHandler::cmdMemory(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'memory' command");
    }
    
    std::string subcmd = extractString(array->elements[1]);
    std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::toupper);
    
    if (subcmd == "STATS") {
        auto result = std::make_shared<protocol::RESPArray>();
        
        result->elements.push_back(protocol::RESPBulkString("peak.allocated"));
        result->elements.push_back(protocol::RESPInteger(memory::Allocator::getPeakMemory()));
        
        result->elements.push_back(protocol::RESPBulkString("total.allocated"));
        result->elements.push_back(protocol::RESPInteger(memory::Allocator::getUsedMemory()));
        
        result->elements.push_back(protocol::RESPBulkString("dict.size"));
        result->elements.push_back(protocol::RESPInteger(dict_.size()));
        
        return result;
    } else if (subcmd == "USAGE") {
        if (array->elements.size() != 3) {
            return protocol::RESPError("ERR wrong number of arguments for 'memory usage' command");
        }
        std::string key = extractString(array->elements[2]);
        auto opt_val = dict_.get(key);
        if (!opt_val) {
            return protocol::RESPNull();
        }
        // Very basic estimation for milestone 4
        size_t usage = sizeof(storage::DictEntry) + key.capacity();
        if (opt_val->getEncoding() == storage::ObjectEncoding::RAW) {
            usage += opt_val->toString().capacity();
        }
        return protocol::RESPInteger(usage);
    }
    
    return protocol::RESPError("ERR unknown subcommand or wrong number of arguments for 'memory' command");
}

protocol::RESPObject CommandHandler::cmdExpire(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'expire' command");
    }
    std::string key = extractString(array->elements[1]);
    int64_t seconds;
    auto str_sec = extractString(array->elements[2]);
    auto [ptr, ec] = std::from_chars(str_sec.data(), str_sec.data() + str_sec.size(), seconds);
    if (ec != std::errc()) {
        return protocol::RESPError("ERR value is not an integer or out of range");
    }
    
    if (!dict_.exists(key)) return protocol::RESPInteger(0);
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    dict_.setExpire(key, now + (seconds * 1000));
    return protocol::RESPInteger(1);
}

protocol::RESPObject CommandHandler::cmdPExpire(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'pexpire' command");
    }
    std::string key = extractString(array->elements[1]);
    int64_t ms;
    auto str_ms = extractString(array->elements[2]);
    auto [ptr, ec] = std::from_chars(str_ms.data(), str_ms.data() + str_ms.size(), ms);
    if (ec != std::errc()) {
        return protocol::RESPError("ERR value is not an integer or out of range");
    }
    
    if (!dict_.exists(key)) return protocol::RESPInteger(0);
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    dict_.setExpire(key, now + ms);
    return protocol::RESPInteger(1);
}

protocol::RESPObject CommandHandler::cmdTTL(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'ttl' command");
    }
    std::string key = extractString(array->elements[1]);
    int64_t ms = dict_.getTTL(key);
    if (ms < 0) return protocol::RESPInteger(ms);
    return protocol::RESPInteger(ms / 1000);
}

protocol::RESPObject CommandHandler::cmdPTTL(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'pttl' command");
    }
    std::string key = extractString(array->elements[1]);
    return protocol::RESPInteger(dict_.getTTL(key));
}

protocol::RESPObject CommandHandler::cmdPersist(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'persist' command");
    }
    std::string key = extractString(array->elements[1]);
    if (dict_.removeExpire(key)) {
        return protocol::RESPInteger(1);
    }
    return protocol::RESPInteger(0);
}

protocol::RESPObject CommandHandler::cmdSave(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 1) {
        return protocol::RESPError("ERR wrong number of arguments for 'save' command");
    }
    
    if (persistence::RDBManager::instance().save(dict_)) {
        return protocol::RESPSimpleString("OK");
    } else {
        return protocol::RESPError("ERR Background save failed");
    }
}

protocol::RESPObject CommandHandler::cmdBgSave(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 1) {
        return protocol::RESPError("ERR wrong number of arguments for 'bgsave' command");
    }
    
    if (persistence::RDBManager::instance().isBgsaveInProgress()) {
        return protocol::RESPError("ERR Background save already in progress");
    }
    
    if (persistence::RDBManager::instance().bgsave(dict_)) {
        return protocol::RESPSimpleString("Background saving started");
    } else {
        return protocol::RESPError("ERR Background save failed");
    }
}

protocol::RESPObject CommandHandler::cmdConfig(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'config' command");
    }
    
    std::string action = extractString(array->elements[1]);
    std::transform(action.begin(), action.end(), action.begin(), ::toupper);
    
    std::string param = extractString(array->elements[2]);
    std::transform(param.begin(), param.end(), param.begin(), ::tolower);
    
    if (action == "SET") {
        if (array->elements.size() != 4) return protocol::RESPError("ERR wrong number of arguments for 'config set'");
        std::string value = extractString(array->elements[3]);
        
        if (param == "maxmemory_keys") {
            try {
                size_t max_keys = std::stoull(value);
                dict_.setMaxKeys(max_keys);
                return protocol::RESPSimpleString("OK");
            } catch (...) {
                return protocol::RESPError("ERR invalid maxmemory_keys value");
            }
        } else if (param == "maxmemory-policy") {
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value == "noeviction") dict_.setEvictionPolicy(storage::EvictionPolicy::NOEVICTION);
            else if (value == "allkeys-lru") dict_.setEvictionPolicy(storage::EvictionPolicy::ALLKEYS_LRU);
            else if (value == "allkeys-lfu") dict_.setEvictionPolicy(storage::EvictionPolicy::ALLKEYS_LFU);
            else if (value == "allkeys-random") dict_.setEvictionPolicy(storage::EvictionPolicy::ALLKEYS_RANDOM);
            else return protocol::RESPError("ERR unsupported maxmemory-policy");
            return protocol::RESPSimpleString("OK");
        }
        return protocol::RESPError("ERR unsupported config parameter");
    } else if (action == "GET") {
        if (param == "maxmemory_keys") {
            auto res = std::make_shared<protocol::RESPArray>();
            res->elements.push_back(protocol::RESPBulkString("maxmemory_keys"));
            res->elements.push_back(protocol::RESPBulkString(std::to_string(dict_.getMaxKeys())));
            return res;
        } else if (param == "maxmemory-policy") {
            auto res = std::make_shared<protocol::RESPArray>();
            res->elements.push_back(protocol::RESPBulkString("maxmemory-policy"));
            std::string policy = "noeviction";
            switch (dict_.getEvictionPolicy()) {
                case storage::EvictionPolicy::ALLKEYS_LRU: policy = "allkeys-lru"; break;
                case storage::EvictionPolicy::ALLKEYS_LFU: policy = "allkeys-lfu"; break;
                case storage::EvictionPolicy::ALLKEYS_RANDOM: policy = "allkeys-random"; break;
                default: break;
            }
            res->elements.push_back(protocol::RESPBulkString(policy));
            return res;
        }
        return protocol::RESPError("ERR unsupported config parameter");
    }
    
    return protocol::RESPError("ERR unknown CONFIG subcommand");
}

protocol::RESPObject CommandHandler::cmdPublish(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'publish' command");
    }
    
    std::string channel = extractString(array->elements[1]);
    std::string message = extractString(array->elements[2]);
    
    int receivers = PubSubManager::instance().publish(channel, message);
    return protocol::RESPInteger(receivers);
}

std::optional<protocol::RESPObject> CommandHandler::cmdSubscribe(const std::shared_ptr<protocol::RESPArray>& array, network::socket_t client_fd) {
    if (array->elements.size() < 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'subscribe' command");
    }
    
    if (client_fd == 0) {
        return protocol::RESPError("ERR client socket required for subscribe");
    }
    
    for (size_t i = 1; i < array->elements.size(); ++i) {
        std::string channel = extractString(array->elements[i]);
        int total_subs = PubSubManager::instance().subscribe(client_fd, channel);
        
        // Push response: *3\r\n$9\r\nsubscribe\r\n$<len>\r\n<channel>\r\n:<total_subs>\r\n
        std::string resp = "*3\r\n$9\r\nsubscribe\r\n$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n:" + std::to_string(total_subs) + "\r\n";
        PubSubManager::instance().sendToClient(client_fd, resp);
    }
    
    return std::nullopt; // Sent directly via PubSubManager
}

std::optional<protocol::RESPObject> CommandHandler::cmdUnsubscribe(const std::shared_ptr<protocol::RESPArray>& array, network::socket_t client_fd) {
    if (client_fd == 0) {
        return protocol::RESPError("ERR client socket required for unsubscribe");
    }
    
    if (array->elements.size() == 1) {
        // Unsubscribe all
        PubSubManager::instance().unsubscribeAll(client_fd);
        // Note: Real Redis lists all channels you unsubscribed from here. 
        // For simplicity, we just send a generic success response.
        std::string resp = "*3\r\n$11\r\nunsubscribe\r\n$-1\r\n:0\r\n";
        PubSubManager::instance().sendToClient(client_fd, resp);
    } else {
        for (size_t i = 1; i < array->elements.size(); ++i) {
            std::string channel = extractString(array->elements[i]);
            int total_subs = PubSubManager::instance().unsubscribe(client_fd, channel);
            
            std::string resp = "*3\r\n$11\r\nunsubscribe\r\n$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n:" + std::to_string(total_subs) + "\r\n";
            PubSubManager::instance().sendToClient(client_fd, resp);
        }
    }
    
    return std::nullopt;
}

protocol::RESPObject CommandHandler::cmdMulti(network::socket_t client_fd) {
    if (client_fd == 0) return protocol::RESPError("ERR client socket required");
    if (in_transaction_.count(client_fd) > 0) {
        return protocol::RESPError("ERR MULTI calls can not be nested");
    }
    in_transaction_.insert(client_fd);
    transaction_queues_[client_fd].clear();
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdExec(network::socket_t client_fd) {
    if (client_fd == 0) return protocol::RESPError("ERR client socket required");
    if (in_transaction_.count(client_fd) == 0) {
        return protocol::RESPError("ERR EXEC without MULTI");
    }
    
    // Check for dirty keys (Optimistic Locking failure)
    if (dirty_clients_.count(client_fd) > 0) {
        in_transaction_.erase(client_fd);
        transaction_queues_.erase(client_fd);
        cmdUnwatch(client_fd); // Automatic unwatch
        return protocol::RESPNull(); // Redis returns null array on CAS failure
    }
    
    auto& queue = transaction_queues_[client_fd];
    auto result = std::make_shared<protocol::RESPArray>();
    
    for (const auto& cmd : queue) {
        // Execute normally but without triggering the queuing logic (we temporarily remove from in_transaction_)
        in_transaction_.erase(client_fd);
        auto opt_res = handleCommand(cmd, client_fd);
        in_transaction_.insert(client_fd); // Restore for safety, though we will clear it at the end
        
        if (opt_res) {
            result->elements.push_back(*opt_res);
        } else {
            result->elements.push_back(protocol::RESPNull());
        }
    }
    
    in_transaction_.erase(client_fd);
    transaction_queues_.erase(client_fd);
    cmdUnwatch(client_fd); // Automatic unwatch
    
    return result;
}

protocol::RESPObject CommandHandler::cmdDiscard(network::socket_t client_fd) {
    if (client_fd == 0) return protocol::RESPError("ERR client socket required");
    if (in_transaction_.count(client_fd) == 0) {
        return protocol::RESPError("ERR DISCARD without MULTI");
    }
    
    in_transaction_.erase(client_fd);
    transaction_queues_.erase(client_fd);
    cmdUnwatch(client_fd); // Automatic unwatch
    
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdWatch(const std::shared_ptr<protocol::RESPArray>& array, network::socket_t client_fd) {
    if (client_fd == 0) return protocol::RESPError("ERR client socket required");
    if (array->elements.size() < 2) return protocol::RESPError("ERR wrong number of arguments for 'watch' command");
    if (in_transaction_.count(client_fd) > 0) return protocol::RESPError("ERR WATCH inside MULTI is not allowed");
    
    for (size_t i = 1; i < array->elements.size(); ++i) {
        std::string key = extractString(array->elements[i]);
        watched_keys_[client_fd].insert(key);
        key_watchers_[key].insert(client_fd);
    }
    
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdUnwatch(network::socket_t client_fd) {
    if (client_fd == 0) return protocol::RESPError("ERR client socket required");
    
    auto it = watched_keys_.find(client_fd);
    if (it != watched_keys_.end()) {
        for (const std::string& key : it->second) {
            auto watcher_it = key_watchers_.find(key);
            if (watcher_it != key_watchers_.end()) {
                watcher_it->second.erase(client_fd);
                if (watcher_it->second.empty()) {
                    key_watchers_.erase(watcher_it);
                }
            }
        }
        watched_keys_.erase(it);
    }
    
    dirty_clients_.erase(client_fd);
    return protocol::RESPSimpleString("OK");
}

protocol::RESPObject CommandHandler::cmdLPush(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'lpush' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) {
        opt_val = storage::InfernoObject::createList();
        dict_.set(key, opt_val);
    } else if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& list = opt_val->getList();
    for (size_t i = 2; i < array->elements.size(); ++i) {
        list.push_front(extractString(array->elements[i]));
    }
    
    return protocol::RESPInteger(list.size());
}

protocol::RESPObject CommandHandler::cmdRPush(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'rpush' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) {
        opt_val = storage::InfernoObject::createList();
        dict_.set(key, opt_val);
    } else if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& list = opt_val->getList();
    for (size_t i = 2; i < array->elements.size(); ++i) {
        list.push_back(extractString(array->elements[i]));
    }
    
    return protocol::RESPInteger(list.size());
}

protocol::RESPObject CommandHandler::cmdLPop(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'lpop' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPNull();
    if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& list = opt_val->getList();
    if (list.empty()) return protocol::RESPNull();
    
    std::string val = std::move(list.front());
    list.pop_front();
    
    if (list.empty()) {
        dict_.del(key);
    }
    
    return protocol::RESPBulkString(val);
}

protocol::RESPObject CommandHandler::cmdRPop(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'rpop' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPNull();
    if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& list = opt_val->getList();
    if (list.empty()) return protocol::RESPNull();
    
    std::string val = std::move(list.back());
    list.pop_back();
    
    if (list.empty()) {
        dict_.del(key);
    }
    
    return protocol::RESPBulkString(val);
}

protocol::RESPObject CommandHandler::cmdLLen(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'llen' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPInteger(0);
    if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    return protocol::RESPInteger(opt_val->getList().size());
}

protocol::RESPObject CommandHandler::cmdLRange(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 4) {
        return protocol::RESPError("ERR wrong number of arguments for 'lrange' command");
    }
    std::string key = extractString(array->elements[1]);
    
    int64_t start, end;
    auto str_start = extractString(array->elements[2]);
    auto str_end = extractString(array->elements[3]);
    
    if (std::from_chars(str_start.data(), str_start.data() + str_start.size(), start).ec != std::errc() ||
        std::from_chars(str_end.data(), str_end.data() + str_end.size(), end).ec != std::errc()) {
        return protocol::RESPError("ERR value is not an integer or out of range");
    }
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return std::make_shared<protocol::RESPArray>();
    if (opt_val->getType() != storage::ObjectType::LIST) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    const auto& list = opt_val->getList();
    int64_t len = list.size();
    
    if (start < 0) start = len + start;
    if (end < 0) end = len + end;
    if (start < 0) start = 0;
    if (start > end || start >= len) {
        return std::make_shared<protocol::RESPArray>();
    }
    if (end >= len) end = len - 1;
    
    auto result = std::make_shared<protocol::RESPArray>();
    auto it = list.begin();
    std::advance(it, start);
    for (int64_t i = start; i <= end; ++i, ++it) {
        result->elements.push_back(protocol::RESPBulkString(*it));
    }
    
    return result;
}

protocol::RESPObject CommandHandler::cmdSAdd(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'sadd' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) {
        opt_val = storage::InfernoObject::createSet();
        dict_.set(key, opt_val);
    } else if (opt_val->getType() != storage::ObjectType::SET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& set = opt_val->getSet();
    int added = 0;
    for (size_t i = 2; i < array->elements.size(); ++i) {
        if (set.insert(extractString(array->elements[i])).second) {
            added++;
        }
    }
    
    return protocol::RESPInteger(added);
}

protocol::RESPObject CommandHandler::cmdSRem(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'srem' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPInteger(0);
    if (opt_val->getType() != storage::ObjectType::SET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& set = opt_val->getSet();
    int removed = 0;
    for (size_t i = 2; i < array->elements.size(); ++i) {
        if (set.erase(extractString(array->elements[i]))) {
            removed++;
        }
    }
    
    if (set.empty()) {
        dict_.del(key);
    }
    
    return protocol::RESPInteger(removed);
}

protocol::RESPObject CommandHandler::cmdSMembers(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'smembers' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return std::make_shared<protocol::RESPArray>();
    if (opt_val->getType() != storage::ObjectType::SET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto result = std::make_shared<protocol::RESPArray>();
    for (const auto& member : opt_val->getSet()) {
        result->elements.push_back(protocol::RESPBulkString(member));
    }
    
    return result;
}

protocol::RESPObject CommandHandler::cmdSIsMember(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'sismember' command");
    }
    std::string key = extractString(array->elements[1]);
    std::string member = extractString(array->elements[2]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPInteger(0);
    if (opt_val->getType() != storage::ObjectType::SET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    if (opt_val->getSet().count(member)) {
        return protocol::RESPInteger(1);
    }
    return protocol::RESPInteger(0);
}

protocol::RESPObject CommandHandler::cmdHSet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 4 || array->elements.size() % 2 != 0) {
        return protocol::RESPError("ERR wrong number of arguments for 'hset' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) {
        opt_val = storage::InfernoObject::createHash();
        dict_.set(key, opt_val);
    } else if (opt_val->getType() != storage::ObjectType::HASH) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& hash = opt_val->getHash();
    int added = 0;
    for (size_t i = 2; i < array->elements.size(); i += 2) {
        std::string field = extractString(array->elements[i]);
        std::string val = extractString(array->elements[i+1]);
        if (hash.insert_or_assign(field, val).second) {
            added++;
        }
    }
    
    return protocol::RESPInteger(added);
}

protocol::RESPObject CommandHandler::cmdHGet(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'hget' command");
    }
    std::string key = extractString(array->elements[1]);
    std::string field = extractString(array->elements[2]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPNull();
    if (opt_val->getType() != storage::ObjectType::HASH) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& hash = opt_val->getHash();
    auto it = hash.find(field);
    if (it != hash.end()) {
        return protocol::RESPBulkString(it->second);
    }
    return protocol::RESPNull();
}

protocol::RESPObject CommandHandler::cmdHDel(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'hdel' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPInteger(0);
    if (opt_val->getType() != storage::ObjectType::HASH) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& hash = opt_val->getHash();
    int removed = 0;
    for (size_t i = 2; i < array->elements.size(); ++i) {
        if (hash.erase(extractString(array->elements[i]))) {
            removed++;
        }
    }
    
    if (hash.empty()) {
        dict_.del(key);
    }
    
    return protocol::RESPInteger(removed);
}

protocol::RESPObject CommandHandler::cmdHGetAll(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 2) {
        return protocol::RESPError("ERR wrong number of arguments for 'hgetall' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return std::make_shared<protocol::RESPArray>();
    if (opt_val->getType() != storage::ObjectType::HASH) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto result = std::make_shared<protocol::RESPArray>();
    for (const auto& [field, value] : opt_val->getHash()) {
        result->elements.push_back(protocol::RESPBulkString(field));
        result->elements.push_back(protocol::RESPBulkString(value));
    }
    
    return result;
}

protocol::RESPObject CommandHandler::cmdZAdd(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() < 4 || array->elements.size() % 2 != 0) {
        return protocol::RESPError("ERR wrong number of arguments for 'zadd' command");
    }
    std::string key = extractString(array->elements[1]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) {
        opt_val = storage::InfernoObject::createZSet();
        dict_.set(key, opt_val);
    } else if (opt_val->getType() != storage::ObjectType::ZSET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& zset = opt_val->getZSet();
    int added = 0;
    for (size_t i = 2; i < array->elements.size(); i += 2) {
        double score;
        auto str_score = extractString(array->elements[i]);
        auto [ptr, ec] = std::from_chars(str_score.data(), str_score.data() + str_score.size(), score);
        if (ec != std::errc()) {
            return protocol::RESPError("ERR value is not a valid float");
        }
        std::string member = extractString(array->elements[i+1]);
        if (zset->add(member, score)) {
            added++;
        }
    }
    
    return protocol::RESPInteger(added);
}

protocol::RESPObject CommandHandler::cmdZScore(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 3) {
        return protocol::RESPError("ERR wrong number of arguments for 'zscore' command");
    }
    std::string key = extractString(array->elements[1]);
    std::string member = extractString(array->elements[2]);
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return protocol::RESPNull();
    if (opt_val->getType() != storage::ObjectType::ZSET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    double score;
    if (opt_val->getZSet()->score(member, score)) {
        // Redis returns scores as strings
        char buf[64];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), score);
        return protocol::RESPBulkString(std::string(buf, ptr));
    }
    return protocol::RESPNull();
}

protocol::RESPObject CommandHandler::cmdZRange(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 4) {
        return protocol::RESPError("ERR wrong number of arguments for 'zrange' command");
    }
    std::string key = extractString(array->elements[1]);
    
    int64_t start, end;
    auto str_start = extractString(array->elements[2]);
    auto str_end = extractString(array->elements[3]);
    
    if (std::from_chars(str_start.data(), str_start.data() + str_start.size(), start).ec != std::errc() ||
        std::from_chars(str_end.data(), str_end.data() + str_end.size(), end).ec != std::errc()) {
        return protocol::RESPError("ERR value is not an integer or out of range");
    }
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return std::make_shared<protocol::RESPArray>();
    if (opt_val->getType() != storage::ObjectType::ZSET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto& zset = opt_val->getZSet();
    const auto& zsl = zset->zsl();
    int64_t len = zsl.length();
    
    if (start < 0) start = len + start;
    if (end < 0) end = len + end;
    if (start < 0) start = 0;
    if (start > end || start >= len) {
        return std::make_shared<protocol::RESPArray>();
    }
    if (end >= len) end = len - 1;
    
    auto result = std::make_shared<protocol::RESPArray>();
    auto node = zsl.header()->forward[0];
    
    for (int64_t i = 0; i < start && node; ++i) {
        node = node->forward[0];
    }
    
    for (int64_t i = start; i <= end && node; ++i) {
        result->elements.push_back(protocol::RESPBulkString(node->ele));
        node = node->forward[0];
    }
    
    return result;
}

protocol::RESPObject CommandHandler::cmdZRangeByScore(const std::shared_ptr<protocol::RESPArray>& array) {
    if (array->elements.size() != 4) {
        return protocol::RESPError("ERR wrong number of arguments for 'zrangebyscore' command");
    }
    std::string key = extractString(array->elements[1]);
    
    double min, max;
    auto str_min = extractString(array->elements[2]);
    auto str_max = extractString(array->elements[3]);
    
    if (str_min == "-inf") min = -std::numeric_limits<double>::infinity();
    else if (std::from_chars(str_min.data(), str_min.data() + str_min.size(), min).ec != std::errc()) {
        return protocol::RESPError("ERR min or max is not a float");
    }
    
    if (str_max == "+inf") max = std::numeric_limits<double>::infinity();
    else if (std::from_chars(str_max.data(), str_max.data() + str_max.size(), max).ec != std::errc()) {
        return protocol::RESPError("ERR min or max is not a float");
    }
    
    auto opt_val = dict_.get(key);
    if (!opt_val) return std::make_shared<protocol::RESPArray>();
    if (opt_val->getType() != storage::ObjectType::ZSET) {
        return protocol::RESPError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    
    auto result = std::make_shared<protocol::RESPArray>();
    auto& zset = opt_val->getZSet();
    auto node = zset->zsl().firstInRange(min, max);
    
    while (node && node->score <= max) {
        result->elements.push_back(protocol::RESPBulkString(node->ele));
        node = node->forward[0];
    }
    
    return result;
}

} // namespace server
} // namespace inferno
