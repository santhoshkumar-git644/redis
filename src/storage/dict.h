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
    InfernoObject value;
    DictEntry* next;

    DictEntry(std::string k, InfernoObject v) 
        : key(std::move(k)), value(std::move(v)), next(nullptr) {}
};

class Dict {
public:
    Dict();
    ~Dict();

    // Prevent copy and move for simplicity
    Dict(const Dict&) = delete;
    Dict& operator=(const Dict&) = delete;

    void set(const std::string& key, InfernoObject value);
    std::optional<InfernoObject> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;

    size_t size() const { return size_; }

private:
    size_t hashFunction(const std::string& key) const;
    void resize();

    std::vector<DictEntry*> table_;
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
