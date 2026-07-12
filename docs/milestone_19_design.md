# Milestone 19: Persistence Upgrades Design (RDB/AOF)

## Overview
As InfernoCache expands to support complex nested types (Lists, Sets, Sorted Sets, Hashes), the Persistence Engine must be upgraded to ensure that these massive, pointer-heavy memory structures can survive server reboots.

## AOF Engine (Append-Only File)
The AOF engine is brilliantly designed around the Command Dispatcher. Because `AOFManager` strictly hooks into the `CommandHandler` to log raw mutating RESP network commands (e.g., `HSET user:1 name Alice`), it is fundamentally agnostic to the underlying `storage::InfernoObject` memory layout.
Therefore, AOF **automatically** gained support for Lists, Sets, Hashes, and ZSets the moment those commands were wired into `dispatchCommand` in Milestones 11, 12, 17, and 18! No further AOF modifications were strictly necessary.

## RDB Engine (Redis Database Snapshots)
Unlike AOF, RDB works by performing deep memory snapshots. It had to be upgraded to serialize nested STL containers and the custom SkipList.

### Serialization Protocol
The `RDBManager::save` algorithm uses a Type-Length-Value (TLV) binary encoding scheme:
1. **Type Byte:** Extracts the enum `storage::ObjectType` from the `InfernoObject`.
2. **Length Prefix:** Writes the `.size()` of the underlying container as a 4-byte unsigned integer.
3. **Payload Loop:**
   - **Lists (`std::deque`):** Iterates linearly, writing each string as a length-prefixed blob.
   - **Sets (`std::unordered_set`):** Iterates the hash buckets, writing each member string.
   - **Hashes (`std::unordered_map`):** Iterates the buckets, writing the Field string, then the Value string.
   - **ZSets (`SkipList`):** Instead of serializing the complex pointer graph of the `SkipList`, it drops to the lowest level (`level 0`) of the graph which functions as a doubly-linked list. It iterates forward, writing the `member` string and the `score` double.

### Deserialization Protocol
`RDBManager::load` reverses the process symmetrically:
1. Reads the Type Byte to determine the correct `create*()` factory method (e.g., `createHash()`).
2. Reads the Length Prefix to determine how many elements to expect.
3. Loops `N` times, reading the raw payloads and inserting them back into the native memory structures (`hash[f] = v;`, `set.insert()`, `zset->add()`).

This provides lightning-fast memory dumping and restoring, completing the Persistence requirements for all modern data types!
