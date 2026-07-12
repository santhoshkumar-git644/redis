# Milestone 15: Optimistic Locking (WATCH / UNWATCH) Design

## Overview
Transactions in InfernoCache (`MULTI`/`EXEC`) guarantee that a sequence of commands runs uninterrupted. However, what if a client needs to *read* a value, perform a complex local calculation, and then *write* the result back? 
If another client modifies the value while the first client is calculating, the transaction would blindly overwrite the new data, causing race conditions.

Milestone 15 introduces **Optimistic Locking** via the `WATCH` and `UNWATCH` commands. This implements a Check-And-Set (CAS) mechanism, guaranteeing that a transaction will instantly abort if any monitored keys were modified before execution.

## Architecture

1. **State Tracking (`CommandHandler`)**
   - We introduced three new O(1) tracking structures:
     - `watched_keys_`: Maps a client FD to a set of keys they are monitoring.
     - `key_watchers_`: Maps a specific key to a set of client FDs monitoring it.
     - `dirty_clients_`: A set of client FDs whose watched keys have been modified.

2. **The `storage::Dict` Mutation Interceptor**
   - The core storage engine (`Dict`) has absolutely no concept of network sockets, clients, or transactions. To maintain this clean separation of concerns, `Dict` was given an `on_key_modified_` callback hook.
   - When any mutating action occurs (`SET`, `DEL`, `EXPIRE`, or even active expiration eviction), `Dict` transparently fires this callback.
   - The `CommandHandler` registers itself to this hook upon startup.

3. **Check-And-Set (CAS) Execution Flow**
   - **Phase 1 (Watch):** A client sends `WATCH <key>`. They are registered in the `key_watchers_` table.
   - **Phase 2 (Compute):** The client reads the key, performs logic on their end, and queues their `MULTI` writes.
   - **Phase 3 (Interception):** If *any other client* mutates the watched key, `Dict` fires the callback. `CommandHandler` scans `key_watchers_`, finds the observing client, and flags their socket in `dirty_clients_`.
   - **Phase 4 (Execution):** The client sends `EXEC`. The engine checks if the client exists in `dirty_clients_`. 
     - If true, the transaction is immediately **ABORTED**. The buffer is wiped, and a Null Array is returned to the client, signaling them to retry the calculation.
     - If false, the transaction executes successfully.

4. **Lifecycle Hooks**
   - **UNWATCH / EXEC / DISCARD**: A client's watches are automatically cleared immediately upon calling `EXEC` or `DISCARD`, or manually via `UNWATCH`.
   - **Disconnection**: If a client abruptly disconnects, `handleClientDisconnect` wipes their transaction buffer and fires an automatic `UNWATCH` to safely erase their socket from all global `key_watchers_` tables, preventing memory leaks and zombie observers.
