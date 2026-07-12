# Milestone 18: Hashes Design

## Overview
InfernoCache now supports **Hashes**: string-to-string maps stored within a single key.
Hashes are structurally ideal for representing objects (e.g., a User profile with fields like `name`, `email`, and `age`) because they allow clients to read, write, or delete individual fields in $O(1)$ time without having to serialize and transmit the entire object back and forth.

## Architecture

1. **Storage Layer (`storage::Hash`)**
   - The core data structure powering Hashes is `std::unordered_map<std::string, std::string>`.
   - This provides $O(1)$ amortized time complexity for field insertion, deletion, and lookup.
   - Hashes are embedded natively into the polymorphic `InfernoObject` engine under the `ObjectType::HASH` type and `ObjectEncoding::HASHTABLE` encoding.

2. **Command Implementations**
   - **`HSET key field value [field value ...]`**: Inserts or updates fields. It utilizes `std::unordered_map::insert_or_assign` and accurately tracks how many fields were *newly inserted* versus merely *updated*. It returns the count of new fields.
   - **`HGET key field`**: Retrieves the value of a specific field. Returns $O(1)$ directly from the map, or a Null array if the field or key doesn't exist.
   - **`HDEL key field [field ...]`**: Deletes fields from the Hash. **Cleanup Mechanic:** If a deletion causes the Hash to become completely empty, the `CommandHandler` automatically purges the parent key from the dictionary via `dict_.del(key)`. This prevents "zombie" keys from leaking memory.
   - **`HGETALL key`**: Iterates through the entire map and serializes it into a flat RESP Array of alternating fields and values (e.g., `[field1, value1, field2, value2]`).

## Memory & Performance
By wrapping `std::unordered_map`, the system achieves cache-friendly memory utilization for objects while completely bypassing the overhead of JSON parsing on the server. Clients can mutate nested data structures securely, with all operations natively triggering the underlying Optimistic Locking (`WATCH`) CAS transaction interceptors built in Milestone 15!
