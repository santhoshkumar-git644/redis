#ifndef INFERNOCACHE_DICT_H
#define INFERNOCACHE_DICT_H

#include "value.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <shared_mutex>

namespace inferno {
namespace storage {

struct DictEntry {
    std::string key;
    InfernoObject::SharedPtr value;
    uint64_t expire_time_ms;
    DictEntry* next;

    DictEntry(std::string k, InfernoObject::SharedPtr v) 
        : key(std::move(k)), value(std::move(v)), expire_time_ms(0), next(nullptr) {}
};

class Dict {
public:
    Dict();
    ~Dict();

    // Prevent copy and move for simplicity
    Dict(const Dict&) = delete;
    Dict& operator=(const Dict&) = delete;

    void set(const std::string& key, InfernoObject::SharedPtr value);
    InfernoObject::SharedPtr get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;

    // Expiration API
    bool setExpire(const std::string& key, uint64_t expire_time_ms);
    int64_t getTTL(const std::string& key) const;
    bool removeExpire(const std::string& key);
    void activeExpireCheck();

    size_t size() const { return size_; }

private:
    size_t hashFunction(const std::string& key) const;
    void resize();

    std::vector<DictEntry*> table_;
    std::vector<std::string> keys_with_ttl_; // For fast sampling
    size_t size_;
    
    // For Milestone 3, we use a shared_mutex to allow concurrent reads and exclusive writes.
    // In a fully single-threaded Redis clone, this isn't strictly necessary for the main loop,
    // but it prepares us for thread-pool based background tasks.
    mutable std::shared_mutex mutex_;
    
    static constexpr size_t INITIAL_CAPACITY = 16;
    static constexpr float LOAD_FACTOR_THRESHOLD = 0.75f;
};

} // namespace storage
} // namespace inferno

#endif // INFERNOCACHE_DICT_H
