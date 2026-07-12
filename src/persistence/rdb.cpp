#include "rdb.h"
#include "../utils/logger.h"
#include <chrono>

namespace inferno {
namespace persistence {

const char RDB_MAGIC[] = "REDIS";
const uint32_t RDB_VERSION = 1;
const uint8_t RDB_OPCODE_EOF = 255;
const uint8_t RDB_OPCODE_EXPIRETIME_MS = 252;

RDBManager::~RDBManager() {
    if (bgsave_thread_.joinable()) {
        bgsave_thread_.join();
    }
}

void RDBManager::writeType(std::ostream& os, storage::ObjectType type) {
    uint8_t t = static_cast<uint8_t>(type);
    os.write(reinterpret_cast<const char*>(&t), 1);
}

void RDBManager::writeLength(std::ostream& os, uint32_t len) {
    os.write(reinterpret_cast<const char*>(&len), sizeof(len));
}

void RDBManager::writeString(std::ostream& os, const std::string& str) {
    writeLength(os, str.length());
    os.write(str.data(), str.length());
}

void RDBManager::writeDouble(std::ostream& os, double val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

void RDBManager::writeInt64(std::ostream& os, int64_t val) {
    os.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

bool RDBManager::readType(std::istream& is, storage::ObjectType& type) {
    uint8_t t;
    if (!is.read(reinterpret_cast<char*>(&t), 1)) return false;
    type = static_cast<storage::ObjectType>(t);
    return true;
}

bool RDBManager::readLength(std::istream& is, uint32_t& len) {
    return static_cast<bool>(is.read(reinterpret_cast<char*>(&len), sizeof(len)));
}

bool RDBManager::readString(std::istream& is, std::string& str) {
    uint32_t len;
    if (!readLength(is, len)) return false;
    str.resize(len);
    return static_cast<bool>(is.read(&str[0], len));
}

bool RDBManager::readDouble(std::istream& is, double& val) {
    return static_cast<bool>(is.read(reinterpret_cast<char*>(&val), sizeof(val)));
}

bool RDBManager::readInt64(std::istream& is, int64_t& val) {
    return static_cast<bool>(is.read(reinterpret_cast<char*>(&val), sizeof(val)));
}

bool RDBManager::save(const storage::Dict& dict, const std::string& filename) {
    std::string temp_file = filename + ".tmp";
    std::ofstream os(temp_file, std::ios::binary);
    if (!os.is_open()) {
        LOG_ERROR("Failed to open RDB temp file for writing.");
        return false;
    }

    os.write(RDB_MAGIC, 5);
    writeLength(os, RDB_VERSION);

    dict.forEachReadOnly([&](const std::string& key, const storage::InfernoObject::SharedPtr& val, uint64_t ttl) {
        if (ttl > 0) {
            uint8_t op = RDB_OPCODE_EXPIRETIME_MS;
            os.write(reinterpret_cast<const char*>(&op), 1);
            writeInt64(os, ttl);
        }

        writeType(os, val->getType());
        writeString(os, key);

        switch (val->getType()) {
            case storage::ObjectType::STRING: {
                if (val->getEncoding() == storage::ObjectEncoding::INT) {
                    writeLength(os, 1); // 1 = int, 0 = string
                    writeInt64(os, val->getInt());
                } else {
                    writeLength(os, 0);
                    writeString(os, val->getString());
                }
                break;
            }
            case storage::ObjectType::LIST: {
                const auto& list = val->getList();
                writeLength(os, list.size());
                for (const auto& item : list) {
                    writeString(os, item);
                }
                break;
            }
            case storage::ObjectType::SET: {
                const auto& set = val->getSet();
                writeLength(os, set.size());
                for (const auto& item : set) {
                    writeString(os, item);
                }
                break;
            }
            case storage::ObjectType::HASH: {
                const auto& hash = val->getHash();
                writeLength(os, hash.size());
                for (const auto& [f, v] : hash) {
                    writeString(os, f);
                    writeString(os, v);
                }
                break;
            }
            case storage::ObjectType::ZSET: {
                const auto& zset = val->getZSet();
                const auto& zsl = zset->zsl();
                writeLength(os, zsl.length());
                auto node = zsl.header()->forward[0];
                while (node) {
                    writeString(os, node->ele);
                    writeDouble(os, node->score);
                    node = node->forward[0];
                }
                break;
            }
        }
    });

    os.write(reinterpret_cast<const char*>(&RDB_OPCODE_EOF), 1);
    os.close();

    if (std::rename(temp_file.c_str(), filename.c_str()) != 0) {
        std::remove(temp_file.c_str()); // Fallback on Windows if rename fails
        std::rename(temp_file.c_str(), filename.c_str());
        LOG_ERROR("Failed to atomically rename RDB file.");
        return false;
    }

    LOG_INFO("RDB saved to disk: " + filename);
    return true;
}

bool RDBManager::bgsave(const storage::Dict& dict, const std::string& filename) {
    if (bgsave_in_progress_.exchange(true)) {
        LOG_WARN("BGSAVE already in progress.");
        return false;
    }
    
    if (bgsave_thread_.joinable()) {
        bgsave_thread_.join();
    }
    
    bgsave_thread_ = std::thread([this, &dict, filename]() {
        LOG_INFO("Background RDB save started.");
        bool res = save(dict, filename);
        if (res) LOG_INFO("Background RDB save completed.");
        else LOG_ERROR("Background RDB save failed.");
        bgsave_in_progress_ = false;
    });
    
    return true;
}

bool RDBManager::load(storage::Dict& dict, const std::string& filename) {
    std::ifstream is(filename, std::ios::binary);
    if (!is.is_open()) {
        return false;
    }

    char magic[5];
    if (!is.read(magic, 5) || std::string(magic, 5) != RDB_MAGIC) {
        LOG_ERROR("Invalid RDB magic string");
        return false;
    }

    uint32_t version;
    if (!readLength(is, version) || version != RDB_VERSION) {
        LOG_ERROR("Unsupported RDB version");
        return false;
    }

    uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    while (true) {
        uint8_t op;
        if (!is.read(reinterpret_cast<char*>(&op), 1)) break;

        if (op == RDB_OPCODE_EOF) {
            break;
        }

        int64_t expire_time = 0;
        if (op == RDB_OPCODE_EXPIRETIME_MS) {
            readInt64(is, expire_time);
            if (!is.read(reinterpret_cast<char*>(&op), 1)) break; // Read the actual type
        }

        storage::ObjectType type = static_cast<storage::ObjectType>(op);
        std::string key;
        if (!readString(is, key)) break;

        storage::InfernoObject::SharedPtr obj;

        if (type == storage::ObjectType::STRING) {
            uint32_t is_int;
            readLength(is, is_int);
            if (is_int == 1) {
                int64_t val;
                readInt64(is, val);
                obj = storage::InfernoObject::create(std::to_string(val)); // Auto-encodes
            } else {
                std::string val;
                readString(is, val);
                obj = storage::InfernoObject::create(val);
            }
        } else if (type == storage::ObjectType::LIST) {
            uint32_t len;
            readLength(is, len);
            obj = storage::InfernoObject::createList();
            auto& list = obj->getList();
            for (uint32_t i = 0; i < len; ++i) {
                std::string item;
                readString(is, item);
                list.push_back(item);
            }
        } else if (type == storage::ObjectType::SET) {
            uint32_t len;
            readLength(is, len);
            obj = storage::InfernoObject::createSet();
            auto& set = obj->getSet();
            for (uint32_t i = 0; i < len; ++i) {
                std::string item;
                readString(is, item);
                set.insert(item);
            }
        } else if (type == storage::ObjectType::HASH) {
            uint32_t len;
            readLength(is, len);
            obj = storage::InfernoObject::createHash();
            auto& hash = obj->getHash();
            for (uint32_t i = 0; i < len; ++i) {
                std::string f, v;
                readString(is, f);
                readString(is, v);
                hash[f] = v;
            }
        } else if (type == storage::ObjectType::ZSET) {
            uint32_t len;
            readLength(is, len);
            obj = storage::InfernoObject::createZSet();
            auto& zset = obj->getZSet();
            for (uint32_t i = 0; i < len; ++i) {
                std::string member;
                double score;
                readString(is, member);
                readDouble(is, score);
                zset->add(member, score);
            }
        }

        // Only load if it hasn't expired yet
        if (expire_time == 0 || expire_time > current_time) {
            dict.set(key, obj);
            if (expire_time > 0) {
                dict.setExpire(key, expire_time);
            }
        }
    }

    LOG_INFO("RDB loaded successfully.");
    return true;
}

} // namespace persistence
} // namespace inferno
