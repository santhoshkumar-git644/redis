#include "aof.h"
#include "../server/command_handler.h"
#include "../utils/logger.h"
#include <chrono>
#include <stdexcept>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define FSYNC _commit
#define FILENO _fileno
#else
#include <unistd.h>
#define FSYNC fsync
#define FILENO fileno
#endif

namespace inferno {
namespace persistence {

void AOFManager::start(const std::string& filename, AOFFsyncPolicy policy) {
    if (running_) return;

    filename_ = filename;
    policy_ = policy;
    
    fp_ = fopen(filename_.c_str(), "ab");
    if (!fp_) {
        LOG_ERROR("Failed to open AOF file for appending: " + filename_);
        return;
    }
    
    running_ = true;
    
    if (policy_ == AOFFsyncPolicy::EVERYSEC) {
        fsync_thread_ = std::thread(&AOFManager::fsyncThreadLoop, this);
    }
    
    LOG_INFO("AOF Manager started. File: " + filename_);
}

void AOFManager::stop() {
    if (!running_) return;
    
    running_ = false;
    
    if (fsync_thread_.joinable()) {
        fsync_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (fp_) {
        doFsync();
        fclose(fp_);
        fp_ = nullptr;
    }
    LOG_INFO("AOF Manager stopped.");
}

std::string AOFManager::serialize(const std::shared_ptr<protocol::RESPArray>& command) {
    std::ostringstream oss;
    oss << "*" << command->elements.size() << "\r\n";
    for (const auto& elem : command->elements) {
        if (std::holds_alternative<protocol::RESPBulkString>(elem)) {
            const std::string& str = std::get<protocol::RESPBulkString>(elem);
            oss << "$" << str.length() << "\r\n" << str << "\r\n";
        } else if (std::holds_alternative<protocol::RESPInteger>(elem)) {
            // Technically AOF from clients is almost always bulk strings, but if internal commands use ints:
            int64_t val = std::get<protocol::RESPInteger>(elem);
            std::string str = std::to_string(val);
            oss << "$" << str.length() << "\r\n" << str << "\r\n";
        }
    }
    return oss.str();
}

void AOFManager::append(const std::shared_ptr<protocol::RESPArray>& command) {
    if (!running_ || !fp_) return;
    
    std::string data = serialize(command);
    
    std::lock_guard<std::mutex> lock(mutex_);
    fwrite(data.c_str(), 1, data.length(), fp_);
    
    if (policy_ == AOFFsyncPolicy::ALWAYS) {
        doFsync();
    }
}

void AOFManager::doFsync() {
    if (fp_) {
        fflush(fp_);
        FSYNC(FILENO(fp_));
    }
}

void AOFManager::fsyncThreadLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (fp_ && running_) {
            doFsync();
        }
    }
}

bool AOFManager::load() {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        LOG_INFO("No AOF file found or could not be opened. Starting with empty dataset.");
        return false;
    }

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) return true;

    LOG_INFO("Loading AOF file...");
    
    protocol::RESPParser parser;
    parser.feed(data);
    
    size_t cmd_count = 0;
    while (true) {
        auto obj = parser.parse();
        if (!obj) break;
        
        if (obj->getType() == protocol::RESPObject::Type::ARRAY) {
            auto arr = std::dynamic_pointer_cast<protocol::RESPArray>(obj);
            if (arr) {
                // Execute command locally without going through network
                // We shouldn't write these back to AOF while loading
                bool was_running = running_;
                running_ = false; // Disable appending while loading
                server::CommandHandler::instance().handleCommand(arr);
                running_ = was_running;
                cmd_count++;
            }
        }
    }
    
    LOG_INFO("AOF loaded successfully. Executed " + std::to_string(cmd_count) + " commands.");
    return true;
}

} // namespace persistence
} // namespace inferno
