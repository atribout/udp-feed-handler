# HFT UDP Feed Handler & Matching Engine

![C++](https://img.shields.io/badge/std-c%2B%2B20-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Architecture](https://img.shields.io/badge/architecture-lock--free-orange)
![Optimisation](https://img.shields.io/badge/kernel-isolation-red)
![Jitter](https://img.shields.io/badge/jitter-%3C1%C2%B5s-brightgreen)

A complete trading system simulation consuming UDP market data, processing orders via a Lock-Free Ring Buffer, and executing trades with deterministic latency.

## System Architecture

The application uses a **Thread-Per-Core** architecture with **Kernel Isolation** to eliminate OS jitter and context switching:

1.  **Network Thread (Producer - Core 4):**
    * Uses `recvmmsg` for batch packet reception (reducing syscall overhead).
    * Parses raw binary packets (Wire Format).
    * Writes to a Lock-Free Ring Buffer using a **Zero-Copy Claim/Publish** pattern.
    
2.  **Engine Thread (Consumer - Core 5):**
    * **Isolated Core:** Running on a dedicated CPU core (`isolcpus=5`) with `nohz_full` and `rcu_nocbs` to prevent scheduler ticks and interrupts.
    * Polls the Ring Buffer.
    * Updates the Limit Order Book (LOB).
    * Logs execution stats (p50, p99, Max).



## Performance: The Impact of Kernel Tuning

End-to-end latency (Wire-to-Trade) measured on AMD Ryzen 5 5600X (Rocky Linux 9.7).

| Configuration | Median (p50) | Tail Latency (p99) | Worst Case (Max) | Verdict |
| :--- | :--- | :--- | :--- | :--- |
| **1. Standard (STL)** | 70 ns | ~300 ns | **792,000 ns** | Unusable for HFT |
| **2. Optimized Software** | 30 ns | ~100 ns | **2,200 ns** | Fast, but noisy |
| **3. Thread Pinning** | 30 ns | ~100 ns | **418,000 ns** | Pinning != Isolation |
| **4. Full Kernel Isolation** | **20 ns** | **60 ns** | **670 ns** | **Production Ready** |

> **Key Findings:**
> * **Median:** Software optimization (Object Pool/Vector) drove the median down to **20ns**.
> * **Tail Latency (p99):** Kernel Isolation stabilized the p99 at **60ns**, meaning 99% of orders are processed in under 220 CPU cycles.
> * **Jitter (Max):** Only full isolation eliminated the scheduler spikes, reducing the worst-case scenario from >400µs to **0.6µs**.

## Kernel Configuration

To reproduce the "Production Ready" results, the Linux kernel must be booted with isolation parameters to silence the OS on specific cores:

```bash
# Example for isolating cores 4 and 5
grubby --update-kernel=ALL --args="isolcpus=4,5 nohz_full=4,5 rcu_nocbs=4,5"
```
* `isolcpus`: Removes cores from the general SMP balancing and scheduler algorithms.
* `nohz_full`: Stops the scheduling-clock tick on the isolated cores (adaptive ticks).
* `rcu_nocbs`: Offloads RCU callbacks to other CPUs.

## Dependencies

- **LOB Core**: Automatically fetched via CMake from limit-order-book.
- **Google Test**: For unit testing.

## Build & Run

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run simulation

1. **Start the Engine:**

```bash
# Sudo is required for thread pinning/priority
sudo ./feed_handler 
```

2. **Start the Market Simulator (in another terminal):**

```bash
python3 market_sim.py
```

## Protocol

The system accepts binary messages (Little Endian, Packed):

- **Add Order ('A'):** `[Type:1][ID:8][Price:4][Qty:4][Side:1]`

- **Cancel Order ('C'):** `[Type:1][ID:8]`
```
