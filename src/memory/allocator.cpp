#include "allocator.h"
#include <cstdlib>
#include <stdexcept>

namespace inferno {
namespace memory {

std::atomic<size_t> Allocator::used_memory_{0};
std::atomic<size_t> Allocator::peak_memory_{0};

#define PREFIX_SIZE sizeof(size_t)

void Allocator::updatePeakMemory() {
    size_t current = used_memory_.load();
    size_t peak = peak_memory_.load();
    while (current > peak) {
        if (peak_memory_.compare_exchange_weak(peak, current)) {
            break;
        }
    }
}

void* Allocator::allocate(size_t size) {
    void* ptr = std::malloc(size + PREFIX_SIZE);
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    // Store the size at the beginning of the block
    *static_cast<size_t*>(ptr) = size;
    
    used_memory_.fetch_add(size + PREFIX_SIZE, std::memory_order_relaxed);
    updatePeakMemory();
    
    return static_cast<char*>(ptr) + PREFIX_SIZE;
}

void Allocator::deallocate(void* ptr) {
    if (!ptr) return;
    
    void* real_ptr = static_cast<char*>(ptr) - PREFIX_SIZE;
    size_t size = *static_cast<size_t*>(real_ptr);
    
    used_memory_.fetch_sub(size + PREFIX_SIZE, std::memory_order_relaxed);
    std::free(real_ptr);
}

void* Allocator::reallocate(void* ptr, size_t size) {
    if (!ptr) return allocate(size);
    if (size == 0) {
        deallocate(ptr);
        return nullptr;
    }

    void* real_ptr = static_cast<char*>(ptr) - PREFIX_SIZE;
    size_t old_size = *static_cast<size_t*>(real_ptr);

    void* new_real_ptr = std::realloc(real_ptr, size + PREFIX_SIZE);
    if (!new_real_ptr) {
        throw std::bad_alloc();
    }

    *static_cast<size_t*>(new_real_ptr) = size;

    if (size > old_size) {
        used_memory_.fetch_add(size - old_size, std::memory_order_relaxed);
        updatePeakMemory();
    } else {
        used_memory_.fetch_sub(old_size - size, std::memory_order_relaxed);
    }

    return static_cast<char*>(new_real_ptr) + PREFIX_SIZE;
}

size_t Allocator::getUsedMemory() {
    return used_memory_.load(std::memory_order_relaxed);
}

size_t Allocator::getPeakMemory() {
    return peak_memory_.load(std::memory_order_relaxed);
}

void Allocator::resetPeakMemory() {
    peak_memory_.store(used_memory_.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

} // namespace memory
} // namespace inferno
