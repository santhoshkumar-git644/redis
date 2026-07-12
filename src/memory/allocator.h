#ifndef INFERNOCACHE_ALLOCATOR_H
#define INFERNOCACHE_ALLOCATOR_H

#include <cstddef>
#include <atomic>

namespace inferno {
namespace memory {

class Allocator {
public:
    static void* allocate(size_t size);
    static void deallocate(void* ptr);
    static void* reallocate(void* ptr, size_t size);

    // Statistics
    static size_t getUsedMemory();
    static size_t getPeakMemory();
    static void resetPeakMemory();

private:
    static std::atomic<size_t> used_memory_;
    static std::atomic<size_t> peak_memory_;

    static void updatePeakMemory();
};

} // namespace memory
} // namespace inferno

// Global overrides for operator new/delete to track all standard library allocations
// Note: We don't override global new/delete for Milestone 4 to avoid conflicts with 
// GTest and the C++ standard library internals, but we will use `Allocator::allocate` 
// explicitly for our custom objects and buffers where memory tracking is crucial.

#endif // INFERNOCACHE_ALLOCATOR_H
