# Milestone 7: Sets & Hashes Design

## Overview
Redis provides rich associative data structures: Sets (unordered collections of unique strings) and Hashes (maps between string fields and string values). These data structures power many use-cases like tagging (`SET`) and object representation (`HASH`).

## Architecture

### Components
1. **`ObjectType::SET` and `ObjectType::HASH`**:
   - `InfernoObject`'s variant is expanded to hold `std::unordered_set<std::string>` and `std::unordered_map<std::string, std::string>`.
   - Redis traditionally uses its internal `dict.c` implementation for both sets and hashes (as well as the global keyspace). For smaller hashes and sets, Redis often uses a memory-optimized `ziplist` or `listpack` encoding.
   - For InfernoCache, we utilize standard C++ hash tables (`std::unordered_set` and `std::unordered_map`) for the *values* within an object. The global keyspace remains powered by our custom thread-safe `storage::Dict`.
   - This architectural split leverages heavily-optimized standard library structures for localized operations (which don't require shared-mutex concurrency because a single command execution locks the global dictionary to fetch the object), while maintaining strict control over the global memory footprint and TTL expiration.

2. **Nested Memory Management & Lifecycle**:
   - Similar to Lists, Sets and Hashes are created lazily upon the first write operation (`SADD`, `HSET`).
   - Also like Lists, they are destroyed automatically. If `SREM` or `HDEL` removes the very last element of the underlying container, the top-level key is deleted from the global `Dict`, preventing memory leaks.
   - Using `std::unordered_map` implicitly maintains `ObjectEncoding::HASHTABLE`, which corresponds to Redis's `OBJ_ENCODING_HT`.

### Commands Implemented
- **SET Commands**:
  - `SADD key member [member ...]`: Add one or more members to a set. Returns the number of elements newly added.
  - `SREM key member [member ...]`: Remove one or more members from a set. Returns the number of elements removed.
  - `SMEMBERS key`: Return all members of the set as a RESP Array.
  - `SISMEMBER key member`: Returns 1 if member is in the set, 0 otherwise.

- **HASH Commands**:
  - `HSET key field value [field value ...]`: Set one or more field-value pairs in a hash. Returns the number of newly added fields.
  - `HGET key field`: Get the value associated with field.
  - `HDEL key field [field ...]`: Delete one or more fields from a hash.
  - `HGETALL key`: Return all fields and values of the hash.

## Tradeoffs
- **Custom Dict vs Standard Library**: We built a custom `Dict` class for the global keyspace because it needed to integrate natively with our custom memory allocator for tracking exact memory bounds, as well as integrating with `std::shared_mutex` for concurrent access in future threaded reactor scenarios. For the internal representations of `SET` and `HASH`, we used `std::unordered_map`. While we could have nested our custom `Dict`, `std::unordered_map` provides an immediate, battle-tested `O(1)` amortized hash table that requires no manual resizing or tuning on our part, accelerating development without sacrificing raw performance.
