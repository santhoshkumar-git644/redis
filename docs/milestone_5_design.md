# Milestone 5: Expiration Engine Design

## Overview
One of Redis's most powerful features is allowing keys to have a Time-To-Live (TTL). When a TTL expires, the key is automatically deleted. The complexity lies in efficiently cleaning up these keys without halting the server or using excessive memory. InfernoCache implements this using a dual-strategy: Lazy Expiration and Active Expiration.

## Architecture

### Components
1. **TTL Tracking inside `DictEntry`**:
   - Each `DictEntry` now contains a `uint64_t expire_time_ms`. A value of `0` means the key is persistent.
   - We maintain a parallel array `std::vector<std::string> keys_with_ttl_` to allow O(1) sampling for the active expiration thread.

2. **Lazy Expiration (Passive Check)**:
   - When any read command (`GET`, `EXISTS`, `TTL`) queries a key, `Dict::get()` inspects the `expire_time_ms`.
   - If the key is past its expiry, it is immediately deleted, and the function returns as if the key never existed.
   - **Advantage**: Guarantees expired keys are never returned.
   - **Disadvantage**: Keys that are never read again will consume memory forever. This is why Active Expiration is needed.

3. **Active Expiration (Background Thread)**:
   - A dedicated `std::thread` is spawned by `TCPServer::start()` that ticks every 100 milliseconds (mirroring Redis's 10hz `serverCron`).
   - The thread calls `Dict::activeExpireCheck()`, which uniformly randomly samples up to 20 keys from `keys_with_ttl_`.
   - If a sampled key is expired, it is deleted.
   - **Advantage**: Gradually reclaims memory in the background without scanning the entire hash table.

### Commands Implemented
- `EXPIRE <key> <seconds>`: Set a timeout on a key.
- `PEXPIRE <key> <milliseconds>`: Set a timeout on a key in milliseconds.
- `TTL <key>`: Get the remaining time to live in seconds. Returns `-1` if persistent, `-2` if not found.
- `PTTL <key>`: Get the remaining time to live in milliseconds.
- `PERSIST <key>`: Removes the expiration associated with the key.

## Tradeoffs
- **Background Thread vs Event Loop Tick**: Redis is purely single-threaded for its core dataset, so it runs the active expiration inside `aeMain` loop callbacks (`serverCron`). Because our `EventLoop` doesn't currently support timer events, we opted to spawn a detached `std::thread` that runs every 100ms. Because our `Dict` is protected by a `std::shared_mutex`, this thread is completely thread-safe and allows background deletion without halting the main connection loop.
- **Random Sampling Array**: To avoid full hash-table scans, we maintain a `keys_with_ttl_` vector. Removing an element takes O(1) time (by swapping with the back element), which is incredibly efficient and matches Redis's randomized expiration algorithm.
