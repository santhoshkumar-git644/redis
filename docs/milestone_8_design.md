# Milestone 8: Sorted Sets (ZSET) Design

## Overview
Sorted Sets (`ZSET`) are one of Redis's most powerful and complex data structures. They act as a hybrid between a Set and a Hash, but every string member is associated with a floating-point number called a *score*. Elements are ordered first by score, and then lexicographically by their string value.

This enables lightning-fast `O(log N)` range queries (`ZRANGE`, `ZRANGEBYSCORE`) alongside instantaneous `O(1)` score lookups (`ZSCORE`).

## Architecture: The Dual-Backing Implementation

To achieve the performance characteristics of `ZSET`, InfernoCache mimics Redis's architecture by pairing two distinct data structures inside a single `ZSet` container object:

1. **Hash Table (`std::unordered_map<std::string, double>`)**:
   - Maps each member string directly to its score.
   - Used for the `ZSCORE` command, guaranteeing `O(1)` time complexity.
   - Also used during `ZADD` to check if a member already exists, allowing the system to update its score rather than inserting a duplicate.

2. **Skip List (`storage::ZSkipList`)**:
   - A probabilistic data structure (`O(log N)` search/insert/delete) that stores `(score, member)` pairs.
   - Elements are strictly ordered by score ascending. If two elements have the identical score, they are ordered lexicographically by the member string.
   - The Skip List is doubly-linked at the lowest level (level 0), allowing for efficient bidirectional traversal (e.g., `ZREVRANGE`, though currently only forward traversal is implemented).
   - Used for `ZRANGE` and `ZRANGEBYSCORE` commands.

### The SkipList Algorithm
Our `ZSkipList` uses a maximum of 32 levels (`ZSKIPLIST_MAXLEVEL`) and a probability factor of 0.25 (`ZSKIPLIST_P`), which are the exact parameters used by Redis. 
When a new node is inserted, a random level is generated. The insertion algorithm routes through the highest available forward pointers to skip large chunks of nodes, dropping down levels until it finds the exact insertion point.

## Commands Implemented

- **`ZADD key score member [score member ...]`**: 
  - Adds one or multiple members with specified scores.
  - If a member already exists, its score is updated (which internally deletes the old node from the Skip List and re-inserts it, while updating the hash table).
- **`ZSCORE key member`**:
  - Instantly fetches the score using the internal hash table.
- **`ZRANGE key start stop`**:
  - Traverses the level-0 linked list starting from the `start` index up to `stop`. Supports negative indices (`-1` = last element).
- **`ZRANGEBYSCORE key min max`**:
  - Uses the Skip List to perform an `O(log N)` leap to the first node with a score `>= min`, then iterates linearly at level 0 until the score exceeds `max`.
  - Supports `-inf` and `+inf` bounds.

## Tradeoffs

- **Memory vs CPU**: The dual-backing architecture means we store the member string twice (once in the hash table, once in the skip list node) and maintain complex pointer arrays. This trades a higher memory footprint for drastically reduced CPU cycles during queries, which is exactly why Redis Sorted Sets are so highly performant for leaderboards and time-series data.
