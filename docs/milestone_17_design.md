# Milestone 17: Sets Design

## Overview
InfernoCache now supports **Sets**: unordered collections of unique strings. 
Sets are mathematically distinct from Lists because they automatically deduplicate elements and offer `O(1)` membership lookup.

## Architecture

1. **Storage Layer (`storage::Set`)**
   - The core data structure powering Sets is `std::unordered_set<std::string>`. 
   - This provides $O(1)$ amortized time complexity for adding, removing, and checking the existence of members.
   - Sets are embedded natively into the polymorphic `InfernoObject` engine under the `ObjectType::SET` type and `ObjectEncoding::HASHTABLE` encoding.

2. **Command Implementations**
   - **`SADD key member [member ...]`**: Inserts elements into the Set. It leverages `std::unordered_set::insert`, which returns a boolean indicating whether the element was genuinely new. It returns the count of strictly *new* elements added.
   - **`SREM key member [member ...]`**: Removes elements from the Set. It tracks the count of actually removed elements. **Cleanup Mechanic:** If removing elements causes the Set to become empty, the `CommandHandler` automatically calls `dict_.del(key)`, purging the empty Set from the cache to prevent memory bloat.
   - **`SISMEMBER key member`**: Queries the Set for the member in $O(1)$ time, returning `1` if it exists or `0` if it does not.
   - **`SMEMBERS key`**: Returns all elements within the Set. Because `std::unordered_set` has no guaranteed ordering, the order of elements returned in the RESP Array is effectively random.

## Concurrency
Just like all operations in InfernoCache, Set operations are inherently thread-safe due to the Reactor Pattern Event Loop architecture. Mutating commands like `SADD` and `SREM` transparently trigger the Optimistic Locking (`WATCH`) mutation hooks inside the `Dict` engine, guaranteeing that CAS transactions observe set changes properly!
