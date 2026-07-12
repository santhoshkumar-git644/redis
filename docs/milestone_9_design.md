# Milestone 9: Persistence (Append Only File) Design

## Overview
Up until this point, InfernoCache has been entirely an in-memory data store. If the server process died, all data was instantly lost. Milestone 9 introduces **Persistence** via an Append Only File (AOF).

Redis offers two main persistence paradigms: RDB (snapshots) and AOF (command logging). We chose to implement AOF for InfernoCache as it provides higher durability guarantees (up to 1-second precision) and is mechanically interesting to implement alongside the Event Loop.

## Architecture

1. **`persistence::AOFManager`**:
   - A thread-safe Singleton that manages the AOF lifecycle.
   - It maintains an open file handle (`fp_`) in append-only binary mode (`ab`).
   - Defines an `AOFFsyncPolicy`, which defaults to `EVERYSEC` (matching Redis's default `appendfsync everysec`).

2. **Command Serialization**:
   - Instead of inventing a custom disk format, AOF logs the exact same RESP protocol strings that the client sent. 
   - `AOFManager::serialize` converts the parsed `RESPArray` back into a raw RESP byte stream. This makes the AOF file perfectly compatible with the existing `RESPParser`.

3. **Background fsync Thread**:
   - Writing to disk is notoriously slow. If we flushed to physical disk (`fsync`) inline with every client command, our blazing-fast in-memory cache would drop to the speed of magnetic rust.
   - Instead, the `CommandHandler` simply appends to a memory buffer (`fwrite` pushes to the OS page cache). 
   - A dedicated background thread (`fsyncThreadLoop`) wakes up every 1 second and executes `fflush()` followed by OS-level `fsync()` (or `_commit()` on Windows), forcing the OS page cache down to physical media. This guarantees that at most 1 second of data is lost in a catastrophic power failure, with almost zero latency impact on client commands.

4. **Command Hooking**:
   - Not all commands are logged. Read-only commands (`GET`, `PING`, `ZRANGE`) are ignored.
   - `CommandHandler::handleCommand` determines if a command successfully mutated state (`SET`, `DEL`, `ZADD`, etc.). If so, it dispatches the command to the `AOFManager` for logging.

5. **Startup Recovery (`AOFManager::load()`)**:
   - In `TCPServer::start()`, *before* the server begins accepting client connections or running the Event Loop, it invokes `AOFManager::load()`.
   - The file is read into a buffer, fed into the `RESPParser`, and the resulting commands are executed linearly via the `CommandHandler` to perfectly reconstruct the in-memory state.

## Tradeoffs: AOF vs Snapshotting
- **AOF** provides phenomenal durability but results in a file that grows infinitely. If a key is updated 10,000 times, the AOF file contains 10,000 commands, whereas a snapshot would contain 1 key. 
- In a production system, an AOF Rewrite engine would be required to compress this log. For InfernoCache, this serves as a solid foundational durability layer.
