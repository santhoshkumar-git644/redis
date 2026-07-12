# Milestone 10: RDB Persistence (Snapshots) Design

## Overview
Milestone 10 introduces Redis's other primary persistence paradigm: RDB (Redis Database) Snapshots. Unlike AOF which logs every command as a string, RDB pauses and serializes the exact in-memory binary structures to a highly compact file on disk.

This results in a smaller file size and drastically faster server boot times, making it ideal for daily backups and disaster recovery.

## Architecture & Binary Format

1. **`persistence::RDBManager`**:
   - A thread-safe Singleton managing serialization.
   - It iterates through the global `storage::Dict` and serializes each key-value pair based on its specific `ObjectType`.

2. **Binary Encoding Format**:
   - The file begins with a magic string `"REDIS"` followed by a 4-byte version number.
   - For every key in the database:
     - If it has an active TTL, an `OPCODE_EXPIRETIME_MS` (252) byte is written, followed by the absolute UNIX expiration timestamp (8 bytes).
     - The 1-byte `ObjectType` (String, List, Set, Hash, ZSet).
     - The Key string (4-byte length + raw chars).
     - The Value, serialized sequentially depending on the type (e.g., ZSet writes the length, then member string, then 8-byte double score).
   - The file is capped with an `OPCODE_EOF` (255) byte.

3. **Atomic Writes**:
   - The RDB is written to a temporary file (`inferno.rdb.tmp`). Once the entire database is serialized successfully and the file handle closed, the OS `rename()` command is invoked to atomically overwrite the old `inferno.rdb`. This ensures that a crash *during* a save doesn't corrupt the previous backup.

## The BGSAVE Concurrency Tradeoff

Redis is single-threaded. When `BGSAVE` is called, Redis famously executes a Unix `fork()` system call. Because `fork()` utilizes OS-level Copy-On-Write (COW) semantics, the child process gets a perfect, frozen-in-time snapshot of memory with zero overhead, allowing the parent process to continue serving clients instantly.

Because `fork()` is not natively available on Windows (and heavily restricted or inefficient on some platforms without specific subsystems), InfernoCache takes a different approach suitable for cross-platform modern C++:

- **`std::thread` & `std::shared_mutex`**:
  - `BGSAVE` launches a detached background `std::thread`.
  - Inside `RDBManager::save()`, we call `Dict::forEachReadOnly()`, which grabs a `std::shared_lock<std::shared_mutex>`.
  - This guarantees a consistent point-in-time snapshot because no mutating commands can execute while the shared lock is held. 
  - **The Tradeoff**: Unlike `fork()`, this means the `BGSAVE` thread actively *blocks* any incoming `SET`, `DEL`, or `ZADD` commands from clients until the save finishes (read commands like `GET` still work concurrently). For a massive database on a slow disk, this would introduce unacceptable write latency. For an educational project or small-to-medium datasets, it perfectly demonstrates thread-safe read/write concurrency mechanics without OS-specific hacks.
