#ifndef INFERNOCACHE_LOGGER_H
#define INFERNOCACHE_LOGGER_H

#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>

namespace inferno {
namespace utils {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X") << "] ";
        
        switch (level) {
            case LogLevel::DEBUG:   std::cout << "[DEBUG] "; break;
            case LogLevel::INFO:    std::cout << "[INFO] "; break;
            case LogLevel::WARNING: std::cout << "[WARN] "; break;
            case LogLevel::ERROR:   std::cerr << "[ERROR] "; break;
        }
        
        if (level == LogLevel::ERROR) {
            std::cerr << message << std::endl;
        } else {
            std::cout << message << std::endl;
        }
    }

private:
    Logger() = default;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) inferno::utils::Logger::instance().log(inferno::utils::LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  inferno::utils::Logger::instance().log(inferno::utils::LogLevel::INFO, msg)
#define LOG_WARN(msg)  inferno::utils::Logger::instance().log(inferno::utils::LogLevel::WARNING, msg)
#define LOG_ERROR(msg) inferno::utils::Logger::instance().log(inferno::utils::LogLevel::ERROR, msg)

} // namespace utils
} // namespace inferno

#endif // INFERNOCACHE_LOGGER_H
