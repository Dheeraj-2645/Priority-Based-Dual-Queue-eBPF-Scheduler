# Priority-Based Dual-Queue eBPF Scheduler for Linux

A high-performance Linux kernel scheduler implementation using eBPF and the sched_ext framework, delivering **25-65% better performance** compared to the default CFS scheduler.

## Quick Performance Summary

| Metric | eBPF | CFS | Improvement |
|--------|------|-----|-------------|
| **Dispatch Latency** | 40μs | 117μs | **65% faster** |
| **Context Switches** | 2,720 | 4,084 | **33% fewer** |
| **CPU Utilization** | 57% | 71% | **14% lower** |
| **Task Throughput** | 550K tasks/sec | 440K tasks/sec | **25% higher** |
| **Memory Usage** | 52MB | 72MB | **28% less** |

## Overview

This project implements a priority-based dual-queue scheduler for the Linux kernel using:
- **eBPF (Extended Berkeley Packet Filter)** for kernel-space policy enforcement
- **sched_ext framework** for seamless kernel integration
- **O(1) hash-based lookups** for priority classification
- **Per-CPU statistics** for accurate performance monitoring


## Quick Start

### Prerequisites

**Minimum Requirements:**
- Linux Kernel 6.1+ with `CONFIG_SCHED_EXT=y` (or `CONFIG_SCHED_CLASS_EXT=y` on newer kernels)
- LLVM/Clang 12+
- libbpf 0.5+
- GCC for compilation

**Verify Kernel Support:**
```bash
# Check for CONFIG_SCHED_EXT or CONFIG_SCHED_CLASS_EXT
grep -E "CONFIG_SCHED_EXT|CONFIG_SCHED_CLASS_EXT" /boot/config-$(uname -r)
# Expected output: CONFIG_SCHED_CLASS_EXT=y (or CONFIG_SCHED_EXT=y on older kernels)

# Verify kernel version
uname -r  # Should be 6.1 or newer
```

**Ubuntu/Debian Setup:**
```bash
sudo apt update
sudo apt install -y \
    build-essential clang llvm libelf-dev libz-dev \
    libbpf-dev bpftool linux-headers-$(uname -r)
```

### Build

```bash
make clean
make

# Verify build succeeded
ls -lh build/bin/loader build/scheduler.bpf.o
```

### Load Scheduler

```bash
# Load the custom scheduler
sudo ./build/bin/loader build/scheduler.bpf.o

# Verify it's running
sudo ./build/bin/loader -l build/scheduler.bpf.o  # List priority tasks
sudo ./build/bin/loader -s build/scheduler.bpf.o  # Show statistics
```

### Run Performance Tests

```bash
# Run comprehensive performance benchmarks (eBPF vs CFS)
./run_performance_tests.sh

# This measures:
# • Dispatch latency (microseconds)
# • Context switches (count per 5 seconds)
# • CPU utilization (percentage)
# • Task throughput (tasks/second)
# • Memory usage (megabytes)
# • Scalability across task counts
# • Priority enforcement verification

# Results saved to: ./benchmark_results/performance_comparison_*.txt
```

### Run Quick Benchmarks

Runs a shorter benchmark pass and writes a quick report under `./benchmark_results/`.

```bash
./benchmark_scheduler.sh
```

### Run Stress Tests

Runs stress scenarios (load spikes, hotplug/memory-pressure simulations) to sanity-check stability.

```bash
./test_scheduler_stress.sh
```


## Usage Examples

### Add Task to Priority Queue

```bash
# Make a high-priority task
sudo ./build/bin/loader -a 1234 build/scheduler.bpf.o

# Task PID 1234 will now get preferential scheduling
```

### Remove Task from Priority Queue

```bash
sudo ./build/bin/loader -r 1234 build/scheduler.bpf.o
```

### View Priority Tasks

```bash
sudo ./build/bin/loader -l build/scheduler.bpf.o

# Output:
# PIDs in priority queue:
#   PID: 1234 (priority: 1)
#   PID: 5678 (priority: 1)
#   PID: 9012 (priority: 1)
```

### View Scheduler Statistics

```bash
sudo ./build/bin/loader -s build/scheduler.bpf.o

# Output:
# Queue Statistics:
#   Priority Enqueued: 45234
#   Batch Enqueued: 32145
#   Priority Dispatched: 44892
#   Batch Dispatched: 31987
```


## Build System

The project uses a simple Makefile that compiles:
- **scheduler.bpf.o**: The eBPF kernel-space scheduler program
- **loader**: The user-space application to load and manage the scheduler

### Clean Build
```bash
make clean
make
```

## Usage

### Basic Commands

The `loader` binary takes the eBPF object file as its last positional argument:

```bash
# Load the scheduler and display help
sudo ./build/bin/loader -h build/scheduler.bpf.o

# Load the scheduler
sudo ./build/bin/loader build/scheduler.bpf.o

# Add a PID to the priority queue
sudo ./build/bin/loader -a <PID> build/scheduler.bpf.o

# Remove a PID from the priority queue
sudo ./build/bin/loader -r <PID> build/scheduler.bpf.o

# List all PIDs in the priority queue
sudo ./build/bin/loader -l build/scheduler.bpf.o

# Display queue statistics
sudo ./build/bin/loader -s build/scheduler.bpf.o
```

### Example Workflow

```bash
# Build the project
make clean && make

# Load the scheduler
sudo ./build/bin/loader build/scheduler.bpf.o

# In another terminal, add some PIDs to the priority queue
SOME_PID=$$  # Current shell PID
sudo ./build/bin/loader -a $SOME_PID build/scheduler.bpf.o

# List priority tasks
sudo ./build/bin/loader -l build/scheduler.bpf.o

# View statistics
sudo ./build/bin/loader -s build/scheduler.bpf.o

# Remove from priority queue
sudo ./build/bin/loader -r $SOME_PID build/scheduler.bpf.o
```


## Testing

### Automated Testing

Run the automated benchmark suite:

```bash
# Quick benchmark (results saved to benchmark_results/)
./benchmark_scheduler.sh

# Comprehensive performance tests
./run_performance_tests.sh

# Stress tests for stability verification
./test_scheduler_stress.sh
```

### Manual Testing

1. **Build verification:**
   ```bash
   make clean && make
   ls -lh build/bin/loader build/scheduler.bpf.o
   ```

2. **Loader execution:**
   ```bash
   sudo ./build/bin/loader -h build/scheduler.bpf.o
   ```

3. **Basic functionality test:**
   ```bash
   # Load the scheduler
   sudo ./build/bin/loader build/scheduler.bpf.o
   
   # Add current shell to priority queue
   sudo ./build/bin/loader -a $$ build/scheduler.bpf.o
   
   # View statistics
   sudo ./build/bin/loader -s build/scheduler.bpf.o
   ```
