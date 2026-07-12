# Milestone 12: Memory Eviction (LRU/LFU) Design

## Overview
A cache without an eviction policy is simply a memory leak waiting to happen. If a cache operates under heavy load with infinite lifetimes, it will eventually exhaust system RAM and be killed by the OS Out-Of-Memory (OOM) killer.

Milestone 12 introduces **Memory Limits and Eviction Algorithms**, allowing InfernoCache to act as a true cache that intelligently discards the least valuable data when under pressure.

## Architecture

1. **Object Metadata**:
   - `InfernoObject` was extended with two lightweight tracking metrics:
     - `uint64_t last_access_time_ms_`: Updated with the current UNIX timestamp on every read/write.
     - `uint32_t access_count_`: Incrementally tracks how many times the object has been accessed.
   - Every operation (e.g., `GET`, `SET`, `ZSCORE`, `SADD`) triggers `recordAccess()` under the hood, continuously maintaining the "heat" of the dataset.

2. **Configuration (`maxmemory_keys`)**:
   - While real Redis can track exact byte counts via custom memory allocators (`jemalloc`), InfernoCache takes a robust alternative: bounding the total number of keys (`maxmemory_keys`).
   - The global `storage::Dict` tracks its size. If an incoming `SET` operation will exceed `maxmemory_keys`, it synchronously triggers the `performEviction()` engine before allowing the write.

3. **Approximated Eviction Engine**:
   - Maintaining a strict, perfectly ordered global LRU linked-list or LFU Min-Heap is incredibly expensive. It ruins O(1) cache access times because every `GET` would require a tree/list rotation and thread locking.
   - Instead, InfernoCache implements **Redis's Sampled Approximation Algorithm**:
     - When memory is full, the engine picks `N` (e.g., 5) completely random keys from the hash table.
     - It evaluates the "score" of each candidate based on the active `EvictionPolicy`.
       - **ALLKEYS_LRU**: Score is `Now - LastAccessTime`. Highest score (oldest) wins.
       - **ALLKEYS_LFU**: Score is `MAX - AccessCount`. Highest score (lowest count) wins.
       - **ALLKEYS_RANDOM**: Score is random.
     - The single best candidate from the sample of 5 is immediately deleted, freeing up space for the new key.
   - This O(1) approach provides 99% of the benefit of a strict LRU with literally zero overhead on read paths!

## Dynamic Configuration
- Added the `CONFIG SET` and `CONFIG GET` commands.
- `CONFIG SET maxmemory_keys 1000`
- `CONFIG SET maxmemory-policy allkeys-lru`
- This allows operators to resize the cache and tune the eviction strategy dynamically without rebooting the server.
