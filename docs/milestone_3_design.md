# Milestone 3: Core Storage Engine Design

## Overview
This milestone establishes the foundational memory storage engine for InfernoCache. A robust, custom Dictionary (Hash Table) forms the core database structure, entirely circumventing `std::unordered_map` for educational clarity and specialized performance.

## Architecture

### Components
1. **`InfernoObject`**: An internal value representation wrapping `std::variant<std::string, int64_t>`. It automatically attempts to parse and store incoming strings as 64-bit integers to save memory footprint (acting similarly to Redis's internal `OBJ_ENCODING_INT`).
2. **`Dict`**: A custom Hash Table implementation. 
   - Uses chaining (linked lists) for collision resolution.
   - Provides thread-safe read/write semantics via `std::shared_mutex` (reader-writer lock).
   - Implements dynamic resizing: When the load factor exceeds 0.75, the bucket array doubles in size and elements are rehashed.
3. **`CommandHandler`**: A singleton dispatcher that takes parsed RESP commands (arrays of strings), performs case-insensitive command matching, interacts with the `Dict`, and returns formatted RESP responses.

### Why Redis Uses Hash Tables (vs. Trees)
Redis stores the top-level keyspace in a dictionary.
- **Time Complexity:** Hash tables provide `O(1)` average case lookup, insert, and delete. A balanced tree (like `std::map` / Red-Black Tree) would be `O(log N)`. In an in-memory database, the constant factors of traversing tree pointers dominate cache-miss overhead.
- **Incremental Rehashing:** While our current `Dict::resize()` performs a blocking rehash (which is acceptable for Milestone 3), Redis maintains two hash tables per dictionary. During a resize, it incrementally moves keys from the old table to the new one on every operation, ensuring no single command experiences the latency spike of an `O(N)` rehash.

### Thread Safety Tradeoffs
Redis is famously single-threaded for its core event loop. In Milestone 1, we implemented an `EventLoop`, but left room for worker threads in future milestones (e.g. background saving or threaded I/O). Our `Dict` incorporates a `std::shared_mutex` allowing concurrent reads (`GET`, `EXISTS`) and exclusive writes (`SET`, `DEL`). This sets the stage for potential multi-threaded execution scaling beyond Redis's original design, while maintaining strict memory consistency.
