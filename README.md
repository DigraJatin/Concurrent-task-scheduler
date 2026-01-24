# Concurrent-Task-Scheduler

A robust C++ framework that orchestrates parallel execution of computational workloads across multiple threads. This project showcases modern concurrent programming patterns, including thread pooling, synchronized queue management, and inter-thread communication mechanisms.

## What You Get

- **Flexible Task Processing:** Create and submit custom workloads that execute independently within a managed thread environment.
- **Intelligent Workload Distribution:** The system automatically balances tasks across available worker threads, maximizing CPU utilization and minimizing execution latency.
- **Worker Thread Pool:** A configurable collection of background threads continuously processes incoming workloads from a shared task queue.
- **Thread-Safe Operations:** Built-in synchronization primitives prevent race conditions and ensure data integrity across concurrent operations.

## Quick Start

### System Requirements
- C++11 compatible compiler or newer
- Familiarity with multithreading concepts and C++ OOP principles

### Project Layout
- `task.hpp/cpp` – The `Task` base class enabling custom workload definitions
- `TaskScheduler.hpp/cpp` – Core scheduling engine managing worker threads and task queue
- `main.cpp` – Practical examples demonstrating the scheduler in action
- `compile.sh` – Automated build script using `g++` with pthread support

### Build and Run

**Step 1: Build the project**
```bash
./compile.sh
```
The build script compiles all source files and links pthread libraries for multi-threading support.

**Step 2: Execute**
```bash
./Executable
```

## How It Works

The architecture follows a producer-consumer pattern. Users submit `Task` objects to the scheduler, which queues them internally. A fleet of worker threads pulls tasks from this queue and processes them sequentially. Mutex locks and condition variables coordinate this activity, preventing conflicts and ensuring smooth task handoff between threads.

## Getting Started with Custom Tasks

To integrate this scheduler into your application:

1. Create a new class inheriting from `Task`
2. Override the `execute()` method with your custom logic
3. Submit instances to the scheduler via `addTask(std::unique_ptr<Task> task)`
4. The scheduler manages the rest—execution, threading, and resource cleanup

All source files must reside in the same directory for proper compilation and linking.

## Contributing

We welcome improvements and enhancements! To contribute:

1. Clone or fork the repository
2. Create a feature branch for your changes
3. Implement and test your improvements
4. Submit a pull request with a clear description of your modifications
