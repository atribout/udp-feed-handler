# Ultra-Low Latency Market Access Gateway & Passive LOB

![C++](https://img.shields.io/badge/std-c%2B%2B20-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Architecture](https://img.shields.io/badge/architecture-lock--free-orange)
![Performance](https://img.shields.io/badge/latency-16.2ns-brightgreen)
![Zero Dependencies](https://img.shields.io/badge/dependencies-none-success)

This system consumes raw UDP multicast market data, multiplexes multiple instruments in $\mathcal{O}(1)$, and maintains deterministic passive Limit Order Books (LOB) to feed trading algorithms.

Built from scratch with strict **Hardware Sympathy** and zero external dependencies.

## Architectural Feats & C++ Optimizations

* **Intrusive Free List (Zero-Allocation):** The custom `OrderPool` eliminates the `std::vector` overhead for tracking free memory. Deallocated orders recycle their `next` pointer to chain themselves into the free list, achieving $\mathcal{O}(1)$ allocation/deallocation in ~2 CPU cycles.
* **Spatial Locality & Cache Packing:** Internal data structures (`QueueItem`, `Order`) are heavily packed and aligned with `alignas(32)`. This ensures exactly two items fit perfectly into a single 64-byte L1 Cache Line without straddling boundaries, maximizing True Sharing and reducing memory bandwidth.
* **False Sharing Prevention:** The Ring Buffer's atomic control pointers (`head` and `tail`) are isolated using `alignas(hardware_destructive_interference_size)`. This guarantees they reside on completely separate cache lines, eliminating the destructive ping-pong effect (False Sharing) between the Producer and Consumer cores.
* **O(1) Flat Array Routing:** Tickers and strings are eliminated. The `MarketManager` uses Exchange *Locate Codes* (`instrumentId`) to directly address a pre-allocated array of `PassiveOrderBook`. 
* **Lock-Free Transport:** Cross-thread communication relies exclusively on a Single-Producer Single-Consumer (SPSC) Ring Buffer using a Zero-Copy Claim/Publish pattern.
* **Kernel Isolation:** OS jitter is eliminated by pinning threads to isolated cores (`isolcpus`, `nohz_full`, `rcu_nocbs`).

## Performance Metrics

*Hardware Environment: Intel(R) Core(TM) Ultra 9 275HX | OS: Rocky Linux 10 (Isolated Kernel)*

### Latency Distribution
The graph below illustrates the **Gateway Processing Latency** (time elapsed between reading the parsed `QueueItem` and fully updating the multiplexed LOB and Top of Book).

![Gateway Processing Latency Distribution](latency_histogram.png)

## System Pipeline

1. **Network Thread (Producer - Core 4):** Uses `recvmmsg` to batch-receive UDP packets, parses Big-Endian wire formats, and writes to the Lock-Free Ring Buffer.
2. **Engine Thread (Consumer - Core 5):** Polls the Ring Buffer.
3. **MarketManager:** Routes the event to the correct instrument book in $\mathcal{O}(1)$.
4. **PassiveOrderBook:** Blindly applies network states (Add/Cancel/Execute) to mirror the exchange and maintains the BBO (Best Bid & Offer).

## Build & Run

**Zero dependencies.** Standard library only.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)