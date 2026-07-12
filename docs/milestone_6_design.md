# Milestone 6: Basic Data Types (Lists) Design

## Overview
Redis provides rich data structures beyond simple strings. The `LIST` data type represents an ordered sequence of strings, supporting constant time insertions and deletions from both the head (`LPUSH`, `LPOP`) and tail (`RPUSH`, `RPOP`).

## Architecture

### Components
1. **`ObjectType::LIST` and `ObjectEncoding::QUICKLIST`**:
   - `InfernoObject`'s variant is expanded to hold `std::deque<std::string>`.
   - Redis originally used a standard doubly-linked list (`adlist.c`) for lists, and later a `ziplist` for small lists. Modern Redis uses a `quicklist`, which is a doubly-linked list of ziplists (combining the memory locality of contiguous arrays with the insert-friendly nature of linked lists).
   - In C++, `std::deque` perfectly models the performance characteristics of a `quicklist`. It is implemented as a dynamically growing array of fixed-size contiguous memory chunks. It provides `O(1)` insertions and deletions at both ends and better cache locality than `std::list` (which suffers from pointer chasing).

2. **WRONGTYPE Protection**:
   - Every list command checks the `ObjectType` of the queried key before proceeding.
   - If a client runs `LPUSH` on a string key, the server correctly replies with `-WRONGTYPE Operation against a key holding the wrong kind of value`.

3. **Auto-Creation and Deletion**:
   - If a list command like `LPUSH` targets a non-existent key, the list is automatically created and inserted into the `Dict`.
   - If a command like `LPOP` removes the very last element of a list, the key is automatically deleted from the `Dict`, freeing memory and maintaining a clean keyspace.

### Commands Implemented
- **`LPUSH key element [element ...]`**: Insert one or multiple elements at the head of the list. Returns the length of the list.
- **`RPUSH key element [element ...]`**: Insert one or multiple elements at the tail of the list. Returns the length of the list.
- **`LPOP key`**: Remove and return the first element.
- **`RPOP key`**: Remove and return the last element.
- **`LLEN key`**: Return the length of the list.
- **`LRANGE key start stop`**: Return the specified elements of the list. Supports negative offsets (e.g., `-1` means the last element).

## Tradeoffs
- **`std::deque` vs `std::list`**: We opted for `std::deque` over a standard doubly-linked list (`std::list`). `std::deque` allocates memory in chunks, meaning iterating through it (e.g., via `LRANGE`) results in far fewer cache misses. Additionally, it avoids allocating a new node (with forward/backward pointers) for every single string inserted, drastically reducing memory overhead compared to `std::list`. This captures the exact architectural intent of Redis's modern `quicklist`.
