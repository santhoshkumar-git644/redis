#pragma once

#include "../protocol/resp_parser.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdio>

namespace inferno {
namespace persistence {

enum class AOFFsyncPolicy {
    ALWAYS,
    EVERYSEC,
    NO
};

class AOFManager {
public:
    static AOFManager& instance() {
        static AOFManager instance;
        return instance;
    }

    void start(const std::string& filename = "inferno.aof", 
               AOFFsyncPolicy policy = AOFFsyncPolicy::EVERYSEC);
    void stop();

    // Appends a mutating command to the AOF buffer/file
    void append(const std::shared_ptr<protocol::RESPArray>& command);

    // Loads commands from AOF file and executes them
    bool load();

    // Helper to serialize RESPArray to string
    std::string serialize(const std::shared_ptr<protocol::RESPArray>& command);

private:
    AOFManager() = default;
    ~AOFManager() { stop(); }
    
    AOFManager(const AOFManager&) = delete;
    AOFManager& operator=(const AOFManager&) = delete;

    void fsyncThreadLoop();
    void doFsync();

    std::string filename_;
    AOFFsyncPolicy policy_ = AOFFsyncPolicy::EVERYSEC;
    
    FILE* fp_ = nullptr;
    std::mutex mutex_;
    
    std::atomic<bool> running_{false};
    std::thread fsync_thread_;
};

} // namespace persistence
} // namespace inferno
