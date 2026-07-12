# Milestone 4: Memory Management Design

## Overview
In an in-memory datastore, tracking and optimizing memory usage is critical. Redis achieves this via a global `zmalloc` wrapper and extensive object sharing. Milestone 4 introduces these exact concepts into InfernoCache.

## Architecture

### Components
1. **`inferno::memory::Allocator`**:
   - A global tracker wrapping `std::malloc` and `std::free` (and `realloc`).
   - Every allocation stores its size in a prefix header, exactly how Redis's `zmalloc` tracks memory when `jemalloc` or `tcmalloc` isn't used.
   - Provides thread-safe, atomic counters for `used_memory_` and `peak_memory_`.

2. **`InfernoObject` Reference Counting**:
   - Refactored from value semantics to heap-allocated objects managed by `std::shared_ptr`.
   - Overloaded `operator new` and `operator delete` for `InfernoObject` so that all value allocations automatically hook into our custom `Allocator`.
   - This achieves automatic lazy deletion: when a key is deleted from the `Dict` (or overwritten), the `std::shared_ptr` drops its ref-count. If it reaches zero, memory is instantly reclaimed without blocking the main event loop unnecessarily (for huge objects, Redis uses background threads, which we can implement in later milestones).

3. **`SharedObjects` Pool**:
   - Redis pre-allocates an array of shared integers (default 0 to 9999).
   - We introduced `SharedObjects`, initialized at startup.
   - When the user runs `SET key 100`, the factory method `InfernoObject::create` checks if the string represents an integer between 0-9999. If so, it returns a `shared_ptr` to the global pool instead of allocating new memory. This dramatically reduces memory footprint and allocation overhead for common values.

### Commands
- **`MEMORY STATS`**: Returns an array with internal statistics (`total.allocated`, `peak.allocated`, `dict.size`).
- **`MEMORY USAGE <key>`**: Estimates the memory overhead (in bytes) of a specific key, taking into account the structural overhead of the dictionary entry and the string capacities.

## Tradeoffs
- **Shared Pointers vs Intrusive Ref Counting**: Redis uses a custom intrusive ref-count inside `robj`. We used `std::shared_ptr`. While `std::shared_ptr` adds an extra 8-16 bytes of overhead for the control block, by using custom `operator new`, we keep the allocation cleanly tracked. This trades a tiny bit of memory overhead for modern C++ safety and RAII semantics.
