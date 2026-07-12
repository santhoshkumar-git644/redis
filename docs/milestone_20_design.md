# Milestone 20: Performance Tuning & Benchmarking

## Overview
InfernoCache was designed from Day 1 to maximize single-core throughput using the Reactor Pattern, $O(1)$ native STL structures, and Lock-Free atomicity.
In this final milestone, we built a raw TCP pipelining benchmark tool to measure the absolute Requests-Per-Second (RPS) capacity of the engine under extreme load.

## The Benchmark Tool
The `inferno_benchmark` tool (located in `tools/benchmark.cpp`) connects directly to the server via standard TCP sockets.
It pre-allocates a massive pipelined string containing 10,000 back-to-back `SET` commands, and streams them into the socket until exactly `1,000,000` operations are completed.
It parses the inbound `+OK\r\n` replies directly from the raw socket buffer, minimizing test client overhead to ensure it purely tests the Server's capacity.

## Performance Architecture Review

1. **The Pipelining Optimization (Milestone 16):**
   - By accumulating thousands of serialized RESP outputs into an `out_buffer` and pushing them via a single `socket.send()`, the server executes pipelined workloads with an $O(1)$ syscall ratio per batch. This minimizes kernel context switching, the primary bottleneck in high-throughput network applications.

2. **The Zero-Lock Transaction Engine (Milestones 14 & 15):**
   - Because InfernoCache relies on a single-threaded Event Loop (Reactor Pattern), complex Operations and `MULTI/EXEC` Transactions require literally **zero mutex locks**. 
   - `WATCH` (Optimistic Locking) uses an elegant observer hook `on_key_modified_` pattern rather than blocking threads, achieving lock-free Check-And-Set semantics at massive concurrency scales.

3. **Polymorphic Storage (Milestones 17 & 18):**
   - We avoid the massive overhead of JSON parsing. `storage::Dict` natively manages `std::unordered_map` (Hashes), `std::unordered_set` (Sets), `std::deque` (Lists), and custom `SkipLists` (ZSets). Read and Write operations are natively evaluated in the C++ memory space at wire speed.

## Conclusion
InfernoCache achieves incredible Requests-Per-Second (RPS) on standard hardware, fully mirroring the architectural techniques that allow enterprise-grade in-memory datastores to dominate the market.
