#pragma once

#include "../storage/dict.h"
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <fstream>

namespace inferno {
namespace persistence {

class RDBManager {
public:
    static RDBManager& instance() {
        static RDBManager instance;
        return instance;
    }

    // Synchronously saves the database to the file
    bool save(const storage::Dict& dict, const std::string& filename = "inferno.rdb");

    // Asynchronously saves the database in a background thread
    bool bgsave(const storage::Dict& dict, const std::string& filename = "inferno.rdb");

    // Loads the database from the file
    bool load(storage::Dict& dict, const std::string& filename = "inferno.rdb");

    bool isBgsaveInProgress() const {
        return bgsave_in_progress_.load();
    }

private:
    RDBManager() = default;
    ~RDBManager();

    RDBManager(const RDBManager&) = delete;
    RDBManager& operator=(const RDBManager&) = delete;

    // Helper serialization methods
    void writeType(std::ostream& os, storage::ObjectType type);
    void writeLength(std::ostream& os, uint32_t len);
    void writeString(std::ostream& os, const std::string& str);
    void writeDouble(std::ostream& os, double val);
    void writeInt64(std::ostream& os, int64_t val);

    // Helper deserialization methods
    bool readType(std::istream& is, storage::ObjectType& type);
    bool readLength(std::istream& is, uint32_t& len);
    bool readString(std::istream& is, std::string& str);
    bool readDouble(std::istream& is, double& val);
    bool readInt64(std::istream& is, int64_t& val);

    std::atomic<bool> bgsave_in_progress_{false};
    std::thread bgsave_thread_;
};

} // namespace persistence
} // namespace inferno
