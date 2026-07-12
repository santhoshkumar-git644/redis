#include "dict.h"
#include <functional>

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

} // namespace storage
} // namespace inferno
