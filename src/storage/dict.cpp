#include "dict.h"
#include <functional>
#include <chrono>
#include <random>

namespace inferno {
namespace storage {

Dict::Dict() : size_(0) {
    table_.resize(INITIAL_CAPACITY, nullptr);
}

Dict::~Dict() {
    for (auto entry : table_) {
        while (entry) {
            DictEntry* next = entry->next;
            delete entry;
            entry = next;
        }
    }
}

uint64_t getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Helper to remove key from ttl vector
void removeKeyFromTTL(std::vector<std::string>& vec, const std::string& key) {
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        if (*it == key) {
            // Swap with back and pop for O(1) removal
            std::swap(*it, vec.back());
            vec.pop_back();
            return;
        }
    }
}

size_t Dict::hashFunction(const std::string& key) const {
    // Using std::hash for simplicity, but in a real system we'd use SipHash or MurmurHash
    return std::hash<std::string>{}(key);
}

void Dict::set(const std::string& key, InfernoObject::SharedPtr value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (size_ >= table_.size() * LOAD_FACTOR_THRESHOLD) {
        resize();
    }

    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    
    while (entry) {
        if (entry->key == key) {
            entry->value = std::move(value);
            // SET removes TTL if exists
            if (entry->expire_time_ms > 0) {
                entry->expire_time_ms = 0;
                removeKeyFromTTL(keys_with_ttl_, key);
            }
            return;
        }
        entry = entry->next;
    }

    // Key not found, prepend to list
    DictEntry* new_entry = new DictEntry(key, std::move(value));
    new_entry->next = table_[index];
    table_[index] = new_entry;
    size_++;
}

InfernoObject::SharedPtr Dict::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    
    while (entry) {
        if (entry->key == key) {
            // Lazy expiration check
            if (entry->expire_time_ms > 0 && getCurrentTimeMs() >= entry->expire_time_ms) {
                lock.unlock(); // Unlock shared
                const_cast<Dict*>(this)->del(key); // Call mutating del
                return nullptr;
            }
            return entry->value;
        }
        entry = entry->next;
    }
    
    return nullptr;
}

bool Dict::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    DictEntry* prev = nullptr;
    
    while (entry) {
        if (entry->key == key) {
            if (prev) {
                prev->next = entry->next;
            } else {
                table_[index] = entry->next;
            }
            if (entry->expire_time_ms > 0) {
                removeKeyFromTTL(keys_with_ttl_, key);
            }
            delete entry;
            size_--;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return false;
}

bool Dict::exists(const std::string& key) const {
    return get(key).has_value();
}

void Dict::resize() {
    size_t new_capacity = table_.size() * 2;
    std::vector<DictEntry*> new_table(new_capacity, nullptr);

    for (auto entry : table_) {
        while (entry) {
            DictEntry* next = entry->next;
            
            size_t new_index = hashFunction(entry->key) % new_capacity;
            entry->next = new_table[new_index];
            new_table[new_index] = entry;
            
            entry = next;
        }
    }

    table_ = std::move(new_table);
}

bool Dict::setExpire(const std::string& key, uint64_t expire_time_ms) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    
    while (entry) {
        if (entry->key == key) {
            if (entry->expire_time_ms == 0) {
                keys_with_ttl_.push_back(key);
            }
            entry->expire_time_ms = expire_time_ms;
            return true;
        }
        entry = entry->next;
    }
    return false;
}

int64_t Dict::getTTL(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    
    while (entry) {
        if (entry->key == key) {
            if (entry->expire_time_ms == 0) return -1; // No expiry
            
            uint64_t now = getCurrentTimeMs();
            if (now >= entry->expire_time_ms) {
                // It's expired logically, let lazy expiry clean it later, but return -2 now
                return -2;
            }
            return entry->expire_time_ms - now;
        }
        entry = entry->next;
    }
    return -2; // Key doesn't exist
}

bool Dict::removeExpire(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    size_t index = hashFunction(key) % table_.size();
    DictEntry* entry = table_[index];
    
    while (entry) {
        if (entry->key == key) {
            if (entry->expire_time_ms > 0) {
                entry->expire_time_ms = 0;
                removeKeyFromTTL(keys_with_ttl_, key);
                return true;
            }
            return false;
        }
        entry = entry->next;
    }
    return false;
}

void Dict::activeExpireCheck() {
    // Basic active expiration matching Redis's random sampling
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (keys_with_ttl_.empty()) return;
    
    int num_checks = std::min(static_cast<int>(keys_with_ttl_.size()), 20);
    int expired_count = 0;
    
    std::mt19937 gen(getCurrentTimeMs());
    
    for (int i = 0; i < num_checks && !keys_with_ttl_.empty(); ++i) {
        std::uniform_int_distribution<size_t> dist(0, keys_with_ttl_.size() - 1);
        size_t r = dist(gen);
        std::string key = keys_with_ttl_[r];
        
        size_t index = hashFunction(key) % table_.size();
        DictEntry* entry = table_[index];
        DictEntry* prev = nullptr;
        
        while (entry) {
            if (entry->key == key) {
                if (entry->expire_time_ms > 0 && getCurrentTimeMs() >= entry->expire_time_ms) {
                    if (prev) prev->next = entry->next;
                    else table_[index] = entry->next;
                    
                    // O(1) removal since we know index r
                    std::swap(keys_with_ttl_[r], keys_with_ttl_.back());
                    keys_with_ttl_.pop_back();
                    
                    delete entry;
                    size_--;
                    expired_count++;
                    break;
                }
                break;
            }
            prev = entry;
            entry = entry->next;
        }
    }
}

} // namespace storage
} // namespace inferno
