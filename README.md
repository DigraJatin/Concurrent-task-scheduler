# Concurrent Task Scheduler

A C++ thread-pool framework implementing the **Producer-Consumer pattern** with priority-based task scheduling. Tasks are submitted into a thread-safe priority queue and executed in parallel by a configurable pool of worker threads.

## Features

- **Priority Queue** — tasks are scheduled by priority (Critical > High > Normal > Low)
- **Thread Pool** — fixed set of reusable worker threads (no per-task thread creation overhead)
- **Thread-Safe** — mutex + condition variable synchronization with no data races
- **RAII Lifecycle** — scheduler automatically joins all threads on destruction
- **Wait Support** — `waitUntilEmpty()` blocks until every submitted task has finished
- **Polymorphic Tasks** — derive from `Task`, override `execute()`, submit with `unique_ptr`

## Project Layout

```
task.hpp / task.cpp             Abstract Task base class with priority support
TaskScheduler.hpp / .cpp        Thread pool, priority queue, synchronization
Main.cpp                        Demo with ComputeTask and IOTask examples
compile.sh                      Build script (g++, C++17, pthread)
FLOW.md                         Architecture, flowcharts, and interview prep
```

## Build & Run

```bash
./compile.sh      # compiles with g++ -std=c++17 -O2 -Wall
./Executable       # runs the demo
```

## Usage

```cpp
#include "TaskScheduler.hpp"

class MyWork : public Task {
  public:
    MyWork(const std::string &n) : Task(n, TaskPriority::High) {}
    void execute() override { /* your logic */ }
};

int main() {
    TaskScheduler sched(4);
    sched.addTask(std::make_unique<MyWork>("job-1"));
    sched.waitUntilEmpty();
}
```

## API

| Method | Description |
|--------|-------------|
| `TaskScheduler(int n)` | Create scheduler with `n` worker threads |
| `addTask(unique_ptr<Task>)` | Submit a task (thread-safe) |
| `waitUntilEmpty()` | Block until all tasks are done |
| `stop()` | Signal shutdown, drain queue, join threads |
| `pendingCount()` | Number of queued (not yet started) tasks |
| `poolSize()` | Number of threads in the pool |
| `printThreadsInfo()` | Print thread IDs |
| `printPendingTasks()` | Print queue and active counts |
