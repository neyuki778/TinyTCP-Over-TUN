# TinyTCP-Over-TUN: High-Performance User-Space TCP/IP Stack

![C++17](https://img.shields.io/badge/C++-17-blue.svg) ![Build Status](https://img.shields.io/badge/build-passing-brightgreen) ![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)

TinyTCP-Over-TUN is a **user-space TCP/IP protocol stack** built with **C++17**.

This project takes over the operating system's network traffic through Linux TUN/TAP devices, bypassing the kernel TCP protocol stack, and fully implements core protocols including ARP, IP routing, TCP state machine, and congestion control in user space. It aims to explore network programming, internal protocol stack mechanisms, and congestion control algorithms.

## ✨ Core Highlights

* **User-space replacement for kernel implementation**: Interacts with the `Linux kernel` via `TUN devices`, intercepts and processes raw IP datagrams, fully implements TCP three-way handshake, four-way handshake, and data transmission in user space, achieving **functional replacement** of the kernel TCP stack.
* **Congestion control algorithms**: Independently implements the TCP Reno congestion control algorithm, including **Slow Start**, **Congestion Avoidance**, **Fast Retransmit**, and **Fast Recovery**, significantly improving throughput and robustness in weak network environments.
* **Efficient performance optimization**:
    * **Reassembler**: Designed a **ring buffer** with interval tree structure to handle out-of-order data, eliminating the $O(N)$ overhead of `std::vector` head erasure. In heavy out-of-order/overlapping scenarios, test processing delay reduced from **1.71s to 0.05s**, throughput increased by **300%**.
    * **Router**: Implemented longest prefix matching (**LPM**) lookup based on **binary prefix tree**, improving routing forwarding efficiency by **12 times** (from 41k pps to 518k pps).

---

## 🛠️ Technical Architecture

### 0. Architecture Diagram
![Architecture Diagram](docs/架构图.drawio.png)

### 1. Transport Layer
* **Finite State Machine**: Follows RFC 9293 standards, managing TCP connection lifecycle (LISTEN, SYN_SENT, ESTABLISHED, etc.).
* **Reliable Transmission Mechanisms**:
    * Flow control based on **sliding window**.
    * Implements **cumulative acknowledgment** and **timeout retransmission (RTO)** mechanisms, supporting adaptive RTT estimation.
    * Handles timer reset logic in **partial acknowledgment** scenarios.

### 2. Network Layer
* **IP Routing Forwarding**: Implements parsing, TTL processing, and checksum calculation for IPv4 datagrams.
* **Lookup Algorithm Optimization**: Compared performance differences between linear scan ($O(N)$) and **prefix tree ($O(W)$)**, ultimately adopting **Trie tree** for efficient routing table lookup.

### 3. Link Layer
* **ARP Protocol**: Implements generation and parsing of ARP requests/responses, maintains ARP cache table with timeout expiration support.
* **Ethernet Frame Processing**: Encapsulation and decapsulation of `Ethernet Frames`, supporting multiplexing of multiple network interfaces.

---

## 📊 Performance Optimization and Benchmark

### 1. Out-of-Order Reassembler Optimization
For TCP out-of-order arrival scenarios, reconstructed the underlying data structure.
- **V0 Version**: Used `std::vector<bool>` and `std::set`, with lots of memory copying and inefficient head erasure operations.
- **V1 Optimization**: Introduced **ring buffer** with interval merging algorithm (Interval Map).
- **Results**: Eliminated $O(N)$ overhead of head `erase`, reducing processing time from **1.71s to 0.05s** in scenarios with lots of duplicate data insertions, throughput increased by about **3 times**.

| Test Scenario | Before Optimization (Gbit/s) | After Optimization (Gbit/s) | Improvement |
| :--- | :---: | :---: | :---: |
| No Overlap | 20.37 | **70.80** | +247% |
| 10x Overlap | 4.13 | **11.10** | +168% |

*(Data source: [benchmark/reassembler/check1.md](benchmark/reassembler/check1.md))*

### 2. Router Lookup Optimization
Compared forwarding performance of linear lookup table and binary prefix tree (Trie) with 10k routing table entries.

| Implementation | Lookup Complexity | Throughput (packets/sec) | Average Time (ns/packet) |
| :--- | :---: | :---: | :---: |
| Linear Scan | $O(N)$ | 41,229 | 24,254 |
| **Prefix Tree (Binary Trie)** | **$O(W)$** | **518,655** | **1,928** |

*(Data source: [benchmark/router/router_benchmark.md](benchmark/router/router_benchmark.md))*

---

## 🛠️ Build & Run

This project supports `Linux 24.04` environment, ensuring dependency consistency.

### Environment Setup
```bash
# Build and start Docker container

docker-compose run --rm dev /bin/bash
```
### Compilation

```Bash
mkdir build
cmake -S . -B build
cmake --build build
```

### Run Tests
```Bash

# Run all unit tests
cmake --build build --target check6

# Run performance tests
cmake --build build --target speed
```
### Set up Virtual Network Interface and Verify
Use the provided script to create virtual network devices, allowing the TCP stack to bypass the kernel:
```bash
## Start our TCP to replace kernel TCP
./scripts/tun.sh start 144
## In another terminal, run packet capture to check if our TCP is working normally through TUN
sudo tcpdump -i tun144 -n
## Back to the previous terminal, start the pre-written webget program (optional parameter, using stanford.edu/class/cs144/ as example)
./build/apps/webget stanford.edu /class/cs144/
```
Result:
![webget](docs/webget.png)
![Packet Capture](docs/抓包.png)

## 📂 Project Structure
```
├── src/                    # Core source code
│   ├── tcp_sender.cc       # TCP sender, retransmission timer and congestion control logic
│   ├── tcp_receiver.cc     # TCP receiver, ACK generation and window management
│   ├── reassembler.cc      # Ring buffer optimized stream reassembler
│   ├── router.cc           # Trie tree based IP routing lookup
│   └── network_interface.cc # ARP protocol and Ethernet frame processing
│   └── ...
├── examples/               # Example application code
│   ├── bidirectional_stream_copy.cc
│   ├── webget.cc
│   └── ...
├── tests/                  # Unit tests
├── benchmark/              # Performance test records
├── scripts/                # Utility scripts
│
├── CMakeLists.txt          # CMake build configuration
├── Dockerfile              # Docker image configuration
├── docker-compose.yml      # Docker Compose configuration
└── README.md               # Project description
```

## 🎓 Project Background and Innovations
Based on the Stanford CS144 Lab framework, **the following extensions have been made**:
- ✅ Independently implemented TCP Reno congestion control algorithm (not included in original framework)
- ✅ Refactored Reassembler module, designed ring buffer to optimize out-of-order processing (3x performance improvement)
- ✅ Refactored Router module, adopted Trie tree to replace linear lookup (12x performance improvement)
- ✅ Completed Benchmark test suite, quantifying optimization effects

## 📝 Acknowledgments
[Stanford CS144](https://cs144.stanford.edu)