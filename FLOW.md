# Concurrent Task Scheduler — Technical Deep Dive

---

## Table of Contents

1. [What This Project Does](#1-what-this-project-does)
2. [Architecture Overview](#2-architecture-overview)
3. [Class Diagram](#3-class-diagram)
4. [Flow Diagrams](#4-flow-diagrams)
5. [C++ Concepts Used](#5-c-concepts-used)
6. [Design Decisions & Trade-offs](#6-design-decisions--trade-offs)
7. [What Changed From v1 to v2](#7-what-changed-from-v1-to-v2)
8. [Q&A — Digging Deeper](#8-qa--digging-deeper)
9. [Concept Reference Table](#9-concept-reference-table)

---

## 1. What This Project Does

This is a **thread-pool-based concurrent task scheduler** built on the
**Producer-Consumer pattern** with a **priority queue**.

Users define task classes by inheriting from an abstract `Task` base, override
`execute()` with their logic, and submit instances to the scheduler. Internally,
a fixed pool of worker threads pulls from a shared priority queue — higher-priority
work runs first. Coordination between producers and consumers relies on mutexes,
condition variables, and atomics.

This is the same foundational pattern behind web server request handlers, database
connection pools, OS-level job schedulers, and game engine task systems.

---

## 2. Architecture Overview

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                           main()  (Producer)                        │
  │                                                                     │
  │    1. Construct TaskScheduler(N)                                    │
  │       └─► Spawns N worker threads, each running worker()            │
  │                                                                     │
  │    2. Submit tasks via addTask(unique_ptr<Task>)                    │
  │       └─► Lock mutex → push into priority queue → notify_one()      │
  │                                                                     │
  │    3. waitUntilEmpty()  — blocks until all work is done             │
  │                                                                     │
  │    4. Destructor (~TaskScheduler) called on scope exit              │
  │       └─► stop() → set running=false → notify_all → join threads    │
  └───────────────────────────────┬─────────────────────────────────────┘
                                  │
                                  ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │              SHARED PRIORITY QUEUE  (max-heap by priority)          │
  │                                                                     │
  │    Protected by: std::mutex  +  std::condition_variable             │
  │                                                                     │
  │    [Critical] [High] [Normal] [Normal] [Normal] [Low]               │
  └──────┬───────────┬───────────┬───────────┬──────────────────────────┘
         │           │           │           │
         ▼           ▼           ▼           ▼
     Worker 0    Worker 1    Worker 2    Worker 3
     (thread)    (thread)    (thread)    (thread)
         │           │           │           │
         └─── Each: lock → wait → dequeue top → unlock → execute() ────┘
```

### Three Layers

| Layer | Files | Role |
|-------|-------|------|
| **Task Abstraction** | `task.hpp`, `task.cpp` | Abstract `Task` class with priority, unique ID, pure virtual `execute()` |
| **Scheduler Engine** | `TaskScheduler.hpp`, `TaskScheduler.cpp` | Thread pool, priority queue, mutex/CV sync, RAII lifecycle |
| **Client / Driver** | `Main.cpp` | Concrete task classes (`ComputeTask`, `IOTask`), usage example |

---

## 3. Class Diagram

```
  ┌──────────────────────────────────────┐
  │              Task  (abstract)         │
  ├──────────────────────────────────────┤
  │ - taskName : string                  │
  │ - taskId   : int                     │
  │ - priority : TaskPriority            │
  │ - nextTaskId : atomic<int>  [static] │
  ├──────────────────────────────────────┤
  │ + Task(name, priority)               │
  │ + virtual ~Task() = default          │
  │ + virtual execute() = 0              │
  │ + getName() : const string&          │
  │ + getId() : int                      │
  │ + getPriority() : TaskPriority       │
  │ + operator<(other) : bool            │
  └──────────────┬───────────────────────┘
                 │  inherits
        ┌────────┴────────┐
        ▼                 ▼
  ┌─────────────┐  ┌─────────────┐
  │ ComputeTask │  │   IOTask    │
  ├─────────────┤  ├─────────────┤
  │ execute()   │  │ execute()   │
  └─────────────┘  └─────────────┘

  ┌─────────────────────────────────────────────────────┐
  │                   TaskScheduler                      │
  ├─────────────────────────────────────────────────────┤
  │ - taskQueue    : priority_queue<unique_ptr<Task>>   │
  │ - threadPool   : vector<thread>                     │
  │ - queueMutex   : mutex                              │
  │ - queueCV      : condition_variable                 │
  │ - emptyCV      : condition_variable                 │
  │ - running      : atomic<bool>                       │
  │ - activeTasks  : atomic<size_t>                     │
  ├─────────────────────────────────────────────────────┤
  │ + TaskScheduler(threadCount)                        │
  │ + ~TaskScheduler()                                  │
  │ + addTask(unique_ptr<Task>)                         │
  │ + stop()                                            │
  │ + waitUntilEmpty()                                  │
  │ + pendingCount() : size_t                           │
  │ + poolSize() : size_t                               │
  │ + printThreadsInfo()                                │
  │ + printPendingTasks()                               │
  │ - worker()                                          │
  └─────────────────────────────────────────────────────┘

  enum class TaskPriority { Low=0, Normal=1, High=2, Critical=3 }
```

---

## 4. Flow Diagrams

### 4A. Overall Execution Flow

```
  main() starts
      │
      ▼
  TaskScheduler scheduler(4)
      │
      ├──► Thread 0 created → runs worker()
      ├──► Thread 1 created → runs worker()
      ├──► Thread 2 created → runs worker()
      └──► Thread 3 created → runs worker()
             │
             │  (all 4 threads immediately WAIT on queueCV
             │   because queue is empty)
             │
      ▼
  Submit 7 tasks via addTask()
      │
      ├──► Each addTask():
      │      lock(queueMutex)
      │      push task into priority_queue
      │      unlock
      │      notify_one()  → wakes one sleeping worker
      │
      ▼
  waitUntilEmpty()
      │
      └──► Blocks on emptyCV until:
           taskQueue.empty() AND activeTasks == 0
      │
      ▼
  main() returns → ~TaskScheduler()
      │
      ├──► stop()
      │      running.exchange(false)
      │      queueCV.notify_all()  → wakes any still-sleeping workers
      │      for each thread: join()
      │
      ▼
  Program exits cleanly
```

### 4B. Worker Thread Lifecycle (each of the N threads)

```
  worker() starts
      │
      ▼
  ┌──► LOCK queueMutex
  │      │
  │      ▼
  │    queueCV.wait(lock, predicate)
  │      │
  │      │  predicate = [this]{ return !taskQueue.empty() || !running; }
  │      │
  │      │  IF queue empty AND running:
  │      │     → atomically release lock + sleep
  │      │     → (woken by notify_one or notify_all)
  │      │     → re-acquire lock, re-check predicate
  │      │
  │      ▼
  │    !running AND queue empty?
  │      │
  │    YES → RETURN (thread exits)
  │      │
  │     NO → Dequeue highest-priority task
  │          ++activeTasks
  │          UNLOCK queueMutex
  │      │
  │      ▼
  │    task->execute()   ← polymorphic virtual call
  │      │
  │      ▼
  │    --activeTasks
  │    emptyCV.notify_all()   ← wakes waitUntilEmpty() if done
  │      │
  └──────┘  (loop back)
```

### 4C. addTask() — Producer Side

```
  addTask(unique_ptr<Task> task)
      │
      ▼
  {
    lock_guard<mutex> lock(queueMutex)   ← RAII lock
      │
      ▼
    taskQueue.push(std::move(task))      ← ownership transferred
      │
      ▼
  }  ← lock released automatically
      │
      ▼
  queueCV.notify_one()                   ← wake ONE sleeping worker
```

### 4D. Graceful Shutdown (stop / destructor)

```
  stop() called (or ~TaskScheduler())
      │
      ▼
  running.exchange(false)
      │
      │  returns previous value:
      │    true  → first call, proceed
      │    false → already stopped, return early (idempotent)
      │
      ▼
  queueCV.notify_all()
      │
      │  All sleeping workers wake up.
      │  Their predicate: !taskQueue.empty() || !running
      │  Since !running is now true, predicate passes.
      │  Workers see !running && queue empty → return.
      │  Workers with remaining tasks → finish them first, then return.
      │
      ▼
  for each thread: if (joinable()) join()
      │
      ▼
  All threads terminated. Resources freed.
```

### 4E. waitUntilEmpty()

```
  waitUntilEmpty()
      │
      ▼
  unique_lock<mutex> lock(queueMutex)
      │
      ▼
  emptyCV.wait(lock, predicate)
      │
      │  predicate = taskQueue.empty() && activeTasks == 0
      │
      │  Sleeps until:
      │    - Every queued task has been dequeued  (queue empty)
      │    - Every running task has finished      (activeTasks == 0)
      │
      │  Workers call emptyCV.notify_all() after each task completes.
      │
      ▼
  Returns → caller knows all work is done
```

---

## 5. C++ Concepts Used

### 5.1 Abstract Classes & Pure Virtual Functions

```cpp
class Task {
    virtual void execute() = 0;   // pure virtual
    virtual ~Task() = default;    // virtual destructor
};
```

`= 0` makes `execute()` a pure virtual function. Any class with at least one
pure virtual function is an **abstract class** — it cannot be instantiated
directly.

This forces every concrete task to provide its own `execute()` implementation.
The scheduler calls `task->execute()` through a `Task*` base pointer, which
resolves to the correct override at runtime (**runtime polymorphism** via the
vtable).

This is the foundation of the Strategy pattern — the scheduler is completely
decoupled from what any specific task actually does. Virtual dispatch works
through a vtable pointer embedded in each object, pointing to a per-class
vtable of function pointers.

### 5.2 Virtual Destructor

```cpp
virtual ~Task() = default;
```

Without `virtual`, deleting a derived object through a base pointer causes
**undefined behavior** — the derived destructor never runs, so any resources
it owns leak or are left in a bad state.

Since tasks are stored as `unique_ptr<Task>`, the deleter calls `delete` on a
`Task*`. The `virtual` keyword ensures the correct derived destructor runs
first, then the base — proper cleanup in the right order.

### 5.3 std::atomic — Thread-Safe Primitives

```cpp
static std::atomic<int> nextTaskId;   // in Task
std::atomic<bool> running;            // in TaskScheduler
std::atomic<size_t> activeTasks{0};   // in TaskScheduler
```

`std::atomic<T>` provides lock-free, thread-safe read-modify-write operations.
`++nextTaskId` compiles to a single atomic CPU instruction (e.g., `lock xadd`
on x86).

Why each one is needed:
- `nextTaskId` — tasks may be created from multiple threads simultaneously.
  Without atomics, `++nextTaskId` is a data race (read-increment-write is 3
  non-atomic steps).
- `running` — read by workers, written by `stop()`. `atomic<bool>` avoids
  needing a mutex for this flag.
- `activeTasks` — incremented/decremented by different worker threads
  concurrently.

Worth noting: `volatile` is *not* a substitute for `atomic` in C++. `volatile`
only prevents compiler reordering — it says nothing about CPU memory ordering
or atomicity. `atomic` guarantees both.

### 5.4 std::unique_ptr & Move Semantics

```cpp
void addTask(std::unique_ptr<Task> task) {
    taskQueue.push(std::move(task));
}
```

`unique_ptr` is a smart pointer with **exclusive ownership** — exactly one
`unique_ptr` owns the object at any time. It cannot be copied, only moved.

Tasks follow a clear ownership chain:
1. Created by producer → owned by `unique_ptr` in `main()`
2. `std::move` transfers ownership to the queue
3. Worker dequeues → `std::move` transfers ownership to local variable
4. Task executes → `unique_ptr` destructor deletes the task

No reference counting overhead (unlike `shared_ptr`). No manual `delete`. No
double-free. No memory leaks.

Under the hood, `std::move(x)` is just a cast to `T&&` (rvalue reference). It
doesn't move anything by itself — it *enables* the move constructor or move
assignment operator to be selected by overload resolution. After a move, the
source is in a "valid but unspecified state" — for `unique_ptr`, it becomes
`nullptr`.

### 5.5 RAII (Resource Acquisition Is Initialization)

```cpp
TaskScheduler::TaskScheduler(int n) : running(true) {
    for (int i = 0; i < n; ++i)
        threadPool.emplace_back(&TaskScheduler::worker, this);
}

TaskScheduler::~TaskScheduler() { stop(); }
```

Resources (threads) are acquired in the constructor and released in the
destructor. When the `TaskScheduler` object goes out of scope, the destructor
fires automatically — no manual cleanup needed by the caller.

This is C++'s primary mechanism for exception-safe resource management. Even if
an exception is thrown between construction and destruction, stack unwinding
calls destructors, so resources are always freed.

Other RAII instances in this project:
- `lock_guard` / `unique_lock` — acquire mutex in constructor, release in
  destructor
- `unique_ptr` — acquire heap memory, release in destructor

### 5.6 std::mutex and Lock Guards

Two locking strategies are used:

| Lock Type | Used In | Key Property |
|-----------|---------|-------------|
| `lock_guard<mutex>` | `addTask()`, `pendingCount()` | Simple scoped lock. Locks on construction, unlocks on destruction. Cannot be manually unlocked. |
| `unique_lock<mutex>` | `worker()`, `waitUntilEmpty()` | Flexible lock. Can unlock/relock. **Required** by `condition_variable::wait()`. |

Why two types? `condition_variable::wait()` needs to atomically release the
mutex while sleeping and re-acquire it on wakeup. `lock_guard` doesn't support
this — it has no `unlock()` method. `unique_lock` is used wherever that
flexibility is needed, and `lock_guard` everywhere else since it has slightly
less overhead (no ownership-tracking state).

### 5.7 std::condition_variable

```cpp
// Worker (consumer) — WAIT
queueCV.wait(lock, [this]{ return !taskQueue.empty() || !running; });

// addTask (producer) — SIGNAL
queueCV.notify_one();

// stop (shutdown) — BROADCAST
queueCV.notify_all();
```

A condition variable lets threads **sleep** until a condition becomes true,
without busy-waiting (spinning in a loop wastes CPU cycles).

How `wait(lock, predicate)` works internally:
```
while (!predicate()) {
    lock.unlock();
    // atomically sleep and add self to wait queue
    // ... sleeping ...
    // woken by notify_one() or notify_all()
    lock.lock();
}
```

**Spurious wakeups:** The OS is allowed to wake a thread even when nobody called
`notify`. This is documented behavior in both POSIX and the C++ standard. The
predicate re-checks the actual condition after every wakeup. Without it, a
worker could proceed when the queue is still empty — leading to a crash.

Two condition variables are used in this project:
- `queueCV` — "there's work to do, or we're shutting down"
- `emptyCV` — "all work is finished" (for `waitUntilEmpty()`)

Using one CV for both would cause unnecessary wakeups: adding a task would wake
`waitUntilEmpty()`, and task completion would wake idle workers. Separate CVs
keep the signaling precise.

### 5.8 Thread Pool Pattern

```cpp
threadPool.reserve(threadCount);
for (int i = 0; i < threadCount; ++i)
    threadPool.emplace_back(&TaskScheduler::worker, this);
```

A fixed number of threads are created once and reused for many tasks. Workers
loop: wait → dequeue → execute → repeat.

Why not one-thread-per-task?
- Thread creation/destruction is expensive (~1ms, involves kernel syscalls like
  `clone` on Linux or `CreateThread` on Windows)
- Unbounded thread creation can exhaust system resources (stack memory, file
  descriptors, OS thread limits)
- A pool amortizes creation cost and caps concurrency at a sensible level

`emplace_back` constructs the `std::thread` in-place inside the vector using
perfect forwarding. `&TaskScheduler::worker` is a pointer-to-member-function;
`this` is passed as the implicit first argument.

### 5.9 Priority Queue

```cpp
std::priority_queue<std::unique_ptr<Task>,
                    std::vector<std::unique_ptr<Task>>,
                    TaskCompare> taskQueue;
```

A max-heap where `Critical` tasks are dequeued before `Low` tasks.

The custom comparator `TaskCompare` delegates to `Task::operator<`, which
compares the underlying `TaskPriority` enum values. `priority_queue` is a
max-heap by default, so the task with the **highest** enum value sits on top.

**The `const_cast` when dequeuing:** `priority_queue::top()` returns
`const T&` to protect heap invariants. But `unique_ptr` is move-only, and
moving requires a non-const reference. The `const_cast` strips const so we can
`std::move` out of the top element. This is safe because we immediately call
`pop()` afterward — the element is being discarded anyway. An alternative would
be a raw `vector` with `std::make_heap` / `std::pop_heap`, which avoids the
cast but requires more manual heap management code.

### 5.10 enum class (Scoped Enumeration)

```cpp
enum class TaskPriority { Low = 0, Normal = 1, High = 2, Critical = 3 };
```

Compared to a plain `enum`:
- Values are scoped: must write `TaskPriority::High`, not just `High`
- No implicit conversion to `int` — prevents accidental comparisons or
  arithmetic with unrelated integers
- Underlying type can be specified explicitly (defaults to `int`)

### 5.11 Lambda Expressions

```cpp
queueCV.wait(lock, [this]{ return !taskQueue.empty() || !running; });
```

Anatomy: `[capture](params) -> return_type { body }`
- `[this]` captures the `this` pointer by value, giving the lambda access to
  member variables like `taskQueue` and `running`
- No parameters; return type deduced as `bool`

The lambda serves as an inline predicate for the condition variable. The
alternative — a separate named function or functor class — would be more
boilerplate for what amounts to a one-liner.

### 5.12 const Correctness

```cpp
const std::string &getName() const;
size_t pendingCount() const;
mutable std::mutex queueMutex;
```

- `const` after a method means it won't modify the object's observable state.
  This lets it be called on `const` references and signals intent to callers.
- `mutable` on the mutex means it can be locked even inside `const` methods
  like `pendingCount()`. Logically, querying the count doesn't modify the
  scheduler, but the mutex needs to change its internal locking state. `mutable`
  resolves this tension.

### 5.13 Deleted Copy/Move Operations

```cpp
TaskScheduler(const TaskScheduler &) = delete;
TaskScheduler &operator=(const TaskScheduler &) = delete;
TaskScheduler(TaskScheduler &&) = delete;
TaskScheduler &operator=(TaskScheduler &&) = delete;
```

A `TaskScheduler` manages threads and a mutex — both inherently non-copyable.
`std::thread` is movable but not copyable; `std::mutex` is neither. Even if
move semantics worked here, moving a scheduler while workers hold references to
`this` would create dangling pointers. `= delete` makes the intent explicit and
catches mistakes at compile time rather than runtime.

### 5.14 explicit Constructor

```cpp
explicit TaskScheduler(int threadCount);
```

Prevents accidental implicit conversion from `int` to `TaskScheduler`. Without
`explicit`, something like `TaskScheduler s = 4;` or passing `4` where a
`TaskScheduler` is expected would silently compile — almost certainly a bug.

### 5.15 Member Initializer Lists

```cpp
Task::Task(const std::string &name, TaskPriority prio)
    : taskName(name), taskId(++nextTaskId), priority(prio) {}
```

Members are constructed *before* the constructor body runs. Without an
initializer list, each member is default-constructed first, then assigned in
the body — double work for non-trivial types like `std::string`. The
initializer list avoids this by constructing each member directly with the
given value.

### 5.16 reserve() Before emplace_back()

```cpp
threadPool.reserve(threadCount);
```

`vector::emplace_back` may reallocate if capacity is exceeded. Reallocation
copies or moves all existing elements and invalidates iterators. Pre-reserving
the exact capacity avoids any reallocation during the push loop. For threads
specifically, moving `std::thread` objects during reallocation is valid but
wasteful.

---

## 6. Design Decisions & Trade-offs

### Why Producer-Consumer?

It decouples task creation (the "what") from task execution (the "when" and
"where"). The producer doesn't know which thread will handle its task. The
consumer doesn't know where the task came from. This separation makes both
sides independently scalable and testable.

### Why `unique_ptr<Task>` over `shared_ptr`?

Tasks have a strict single-owner chain: producer → queue → worker. There's
never a point where two entities need to own the same task simultaneously.
`shared_ptr` would add atomic reference counting overhead (~2 atomic
increments/decrements per ownership transfer) for no benefit.

### Why Two Condition Variables?

Using a single CV for both "work available" and "all work done" would cause
cross-signal pollution — adding a task would unnecessarily wake
`waitUntilEmpty()`, and task completion would unnecessarily wake idle workers.
Two CVs keep the signaling precise and avoid wasted CPU on spurious checks.

### Why `atomic<bool>` for `running` Instead of Mutex-Protected `bool`?

`running` is a simple flag that is written once (during shutdown) and read
repeatedly (by every worker on each loop iteration). An atomic is cheaper than
locking a mutex for a single boolean read. The mutex is still needed for the
*combination* of checking `running` and checking the queue (which is why the
predicate runs under the lock).

### Unbounded Queue — Intentional

The current queue has no size limit. This is a deliberate simplicity trade-off.
Adding backpressure (a bounded queue where `addTask` blocks when full) is
straightforward: add a `fullCV` condition variable and a `maxSize` parameter,
and have `addTask()` wait when the queue reaches capacity. Workers would
`notify_one()` on `fullCV` after each dequeue.

---

## 7. What Changed From v1 to v2

| Issue | v1 (Original) | v2 (Current) | Impact |
|-------|---------------|--------------|--------|
| **Shutdown deadlock** | `wait` predicate only checks `!taskQueue.empty()` — workers never wake on shutdown if queue is empty | Predicate: `!taskQueue.empty() \|\| !running` | Workers were permanently stuck sleeping on program exit |
| **Non-virtual destructor** | `~Task() {}` | `virtual ~Task() = default` | Deleting derived through base pointer was undefined behavior |
| **Non-atomic static ID** | `static int nextTaskId; ++nextTaskId` | `static std::atomic<int> nextTaskId` | Data race if tasks are created from multiple threads |
| **No copy/move deletion** | Default copy/move generated | Explicitly `= delete` | Prevents accidental copy of non-copyable resource manager |
| **FIFO only** | `std::deque` | `priority_queue` with `TaskPriority` enum | Higher-priority tasks now execute first |
| **No completion tracking** | Caller has no way to know when tasks finish | `waitUntilEmpty()` with `emptyCV` + `activeTasks` counter | Essential for any real workload |
| **No idempotent stop** | Destructor always runs full shutdown | `stop()` uses `atomic::exchange` — safe to call multiple times | Prevents double-join crashes |
| **`seeAllTasks` not thread-safe** | Iterates queue without holding mutex | `printPendingTasks()` locks mutex first | v1 was a data race under concurrent access |
| **Pass-by-value string** | `Task(std::string name)` copies always | `Task(const std::string &name)` | Avoids an unnecessary copy when passing lvalues |
| **No `explicit`** | `TaskScheduler(int)` allows implicit conversion | `explicit TaskScheduler(int)` | Prevents `TaskScheduler s = 4;` from compiling |
| **No `mutable` mutex** | Could not have `const` query methods | `mutable std::mutex` | Proper const correctness for read-only methods |
| **Build flags** | No `-std` flag, no optimization | `-std=c++17 -O2 -Wall -Wextra -Wpedantic` | Catches more bugs at compile time, better codegen |

---

## 8. Q&A — Digging Deeper

### Architecture

**How does the system work end-to-end?**

It's a Producer-Consumer system with three layers. The `Task` abstract class
defines the work interface — any concrete task inherits from it and overrides
`execute()`. The `TaskScheduler` is the engine: it owns a thread pool (N
threads created at construction) and a thread-safe priority queue. Producers
call `addTask()` which locks the mutex, pushes the task, and signals a worker
via `notify_one()`. Workers loop: wait on the condition variable, dequeue the
highest-priority task, release the lock, and call `execute()`. On destruction,
the scheduler sets `running=false`, wakes all threads, and joins them — full
RAII-managed lifecycle.

**Why a thread pool instead of spawning a thread per task?**

Thread creation involves kernel syscalls — typically around 1ms each. With
10,000 tasks, that's 10 seconds of pure overhead just creating threads. On top
of that, unbounded threads exhaust OS resources: stack memory (usually 1-8 MB
per thread), file descriptors, and OS-level thread limits. A pool amortizes
the creation cost, bounds concurrency, and lets threads be reused across tasks.

**Why a priority queue over a plain FIFO?**

A FIFO queue treats all tasks equally. In real systems, some work is more
urgent — handling a user request vs. writing a background log entry, for
example. The priority queue ensures `Critical` tasks are serviced before `Low`
ones using a simple enum comparison, without needing multiple queues or complex
scheduling algorithms.

**What design patterns are at play?**

Two main ones:
- **Producer-Consumer** (also called Bounded Buffer, though this implementation
  is unbounded) — decouples task creation from execution
- **Strategy** — the scheduler doesn't know what `execute()` does; each
  concrete task encapsulates its own algorithm behind a common interface

---

### Concurrency & Synchronization

**Walk through the synchronization primitives and why each is needed.**

Three primitives coordinate the system:
1. `std::mutex` (`queueMutex`) — ensures mutual exclusion when accessing the
   shared queue. Only one thread touches the queue at a time.
2. `std::condition_variable` (`queueCV`) — workers sleep here when idle.
   `addTask()` calls `notify_one()` to wake one. `stop()` calls `notify_all()`
   to wake everyone for shutdown.
3. `std::condition_variable` (`emptyCV`) — `waitUntilEmpty()` sleeps here.
   Workers signal it after each task completion.

**What was the shutdown deadlock in v1 and how was it fixed?**

The v1 wait predicate was `[this]{ return !taskQueue.empty(); }`. When the
destructor set `currentlyRunning = false` and called `notify_all()`, workers
woke up, re-checked the predicate, saw the queue was empty, and went right back
to sleep — permanent deadlock. The fix: change the predicate to
`!taskQueue.empty() || !running`. Now the `!running` arm lets workers exit even
when the queue is empty.

**What are spurious wakeups?**

The OS is allowed to wake a thread waiting on a condition variable even when
nobody called `notify`. This is documented behavior in POSIX and the C++
standard. The predicate lambda re-checks the actual condition after every
wakeup. If the condition is still false (queue empty, still running), the
thread goes back to sleep. Without it, a worker could try to dequeue from an
empty queue and crash.

**Why two condition variables instead of one?**

`queueCV` signals "there's work to do or shutdown is happening" — consumed by
workers. `emptyCV` signals "a task just finished" — consumed by
`waitUntilEmpty()`. Using one CV for both would cause cross-talk: adding a task
would spuriously wake `waitUntilEmpty()`, and task completion would spuriously
wake idle workers. Separate CVs keep wakeups targeted.

**Why `lock_guard` in some places and `unique_lock` in others?**

`condition_variable::wait()` needs to atomically release the mutex while
sleeping and re-acquire it on wakeup. `lock_guard` can't do this — it has no
`unlock()` method. `unique_lock` supports manual lock/unlock and is the only
RAII wrapper accepted by `wait()`. `lock_guard` is used everywhere else because
it has slightly less runtime overhead (no ownership-tracking flag).

**Is `running` safe to access without a mutex?**

Yes, because it's `atomic<bool>`. All reads and writes are atomic operations
with sequential consistency by default. Workers read it, `stop()` writes it —
no mutex needed for this single variable. However, the *combination* of
checking `running` and checking the queue must happen under the mutex — that's
why the predicate is evaluated while holding `queueMutex`.

**What happens if `addTask()` is called after `stop()`?**

The task is pushed into the queue and a worker is notified. Since `running` is
`false`, the worker's predicate passes (queue is non-empty), so it will still
dequeue and execute the task. The current design drains the queue on shutdown.
To reject post-stop submissions, you would add an `if (!running) throw ...;`
guard at the top of `addTask()`.

---

### Memory & Ownership

**Why `unique_ptr<Task>` instead of raw pointers or `shared_ptr`?**

Raw pointers require manual `delete` — error-prone, exception-unsafe, and easy
to leak. `shared_ptr` adds reference-counting overhead (an atomic
increment/decrement on every ownership transfer). Tasks have a clear
single-owner chain: producer → queue → worker. `unique_ptr` enforces this at
compile time. You can't accidentally copy it, and the object is automatically
deleted when the `unique_ptr` goes out of scope.

**Trace the ownership of a task through the system.**

1. `main()` creates a `unique_ptr<ComputeTask>` via `make_unique` — main
   owns it.
2. `addTask(std::move(task))` — the move transfers ownership to the function
   parameter. Main's pointer is now `nullptr`.
3. `taskQueue.push(std::move(task))` — ownership moves into the priority queue's
   internal storage.
4. Worker dequeues via `std::move(const_cast<...>(taskQueue.top()))` then
   `taskQueue.pop()` — ownership transfers to the worker's local variable.
5. `task->execute()` runs. The function returns → the local `unique_ptr` is
   destroyed → the task object is deleted. Clean, deterministic, no leaks.

**What's the `const_cast` about when dequeuing?**

`priority_queue::top()` returns `const T&` to protect the heap invariant. But
`unique_ptr` is move-only — moving requires a non-const reference. The
`const_cast` strips the const so `std::move` can transfer ownership. This is
safe because we immediately call `pop()` — we're taking ownership of something
the queue is about to discard. The alternative — using a raw `vector` with
`make_heap` / `pop_heap` — avoids the cast but adds more manual heap management
code.

---

### C++ Language Details

**What's the difference between `virtual`, `override`, and `= 0`?**

- `virtual` — enables dynamic dispatch through the vtable
- `= 0` — makes the function pure virtual (abstract); the class can't be
  instantiated; derived classes must provide an implementation
- `override` — a compiler-checked annotation that says "this overrides a base
  class virtual function." If the base signature ever changes, `override`
  causes a compile error instead of silently creating a new unrelated function

**Why `= default` instead of an empty body `{}` for the destructor?**

`= default` tells the compiler to generate its default implementation. The
compiler may apply trivial-destructor optimizations that an empty body `{}`
prevents. It also communicates intent more clearly: "I want the standard
behavior, not a custom empty one."

**Why `= delete` the copy and move constructors?**

A `TaskScheduler` owns threads and a mutex. `std::mutex` is not copyable or
movable. `std::thread` is movable but not copyable. Even if moving worked,
doing so while workers hold references to `this` would create dangling pointers.
`= delete` makes the compiler reject these operations at compile time.

**What does `mutable` do on the mutex?**

It allows the mutex to be locked inside `const` member functions like
`pendingCount()`. Logically, querying the count doesn't modify the scheduler's
observable state, so the method should be `const`. But locking a mutex mutates
its internal state. `mutable` says "this member can change even in a const
context."

**What's `atomic::exchange` and why use it in `stop()`?**

`running.exchange(false)` atomically sets `running` to `false` and returns the
previous value. If it was already `false` (someone already called `stop()`), we
return early — no double-joining, no double-notification. This makes `stop()`
idempotent.

---

### Extending the System

**How would you add task cancellation?**

Add a `std::atomic<bool> cancelled` flag to each `Task` with a `cancel()`
method. In `execute()`, users check `isCancelled()` at appropriate checkpoints
and return early. Workers can also skip cancelled tasks before calling
`execute()`.

**How would you add backpressure (bounded queue)?**

Add a `maxQueueSize` parameter and a third condition variable (`fullCV`). In
`addTask()`, wait on `fullCV` until `taskQueue.size() < maxQueueSize`. Workers
signal `fullCV` after each dequeue. This blocks fast producers when consumers
can't keep up — the standard bounded-buffer extension.

**How would you implement work-stealing?**

Replace the single shared queue with per-thread local queues. When a thread's
own queue is empty, it steals from another thread's queue (typically from the
back to reduce contention). This improves cache locality and reduces mutex
contention under high load. It's the approach used by Intel TBB, Tokio (Rust),
and Go's goroutine scheduler.

**What happens under heavy contention?**

The queue grows unboundedly — memory usage increases proportionally. Workers
contend on the mutex at dequeue time, but the critical section is tiny (one
heap pop), so lock hold time is minimal. The real bottleneck is task execution
time, not synchronization. Mitigations: increase pool size, add backpressure,
or move to lock-free queues (e.g., Michael-Scott queue).

**How does this compare to `std::async` / `std::future`?**

`std::async` can either spawn a new thread or defer execution. It returns a
`future` for the result. This scheduler is designed for fire-and-forget
workloads with high task counts — it avoids per-task thread creation, supports
priority ordering, and provides `waitUntilEmpty()` for batch completion. To add
return-value support, you would wrap tasks in `std::packaged_task` and hand
back the associated `std::future`.

---

## 9. Concept Reference Table

| Concept | Where Used | What It Does |
|---------|-----------|-------------|
| Abstract class | `Task` | Has pure virtual `execute()` — cannot be instantiated |
| Pure virtual function | `execute() = 0` | Forces derived classes to provide implementation |
| Virtual destructor | `virtual ~Task()` | Ensures correct destruction through base pointer |
| `override` keyword | `ComputeTask::execute()` | Compile-time check that the function actually overrides |
| `enum class` | `TaskPriority` | Scoped, type-safe enumeration without implicit int conversion |
| `std::atomic<T>` | `nextTaskId`, `running`, `activeTasks` | Lock-free thread-safe reads and writes |
| `std::unique_ptr` | Task ownership chain | Exclusive ownership, automatic deletion, zero overhead |
| `std::move` | `addTask`, queue operations | Casts to rvalue reference, enabling move semantics |
| `std::make_unique` | `Main.cpp` | Exception-safe heap allocation returning `unique_ptr` |
| RAII | `TaskScheduler`, `lock_guard` | Acquire in constructor, release in destructor |
| `std::mutex` | `queueMutex` | Mutual exclusion for shared data access |
| `std::lock_guard` | `addTask()` | Simple scoped RAII mutex lock |
| `std::unique_lock` | `worker()` | Flexible RAII lock compatible with `condition_variable` |
| `std::condition_variable` | `queueCV`, `emptyCV` | Efficient sleep/wake signaling between threads |
| `notify_one` / `notify_all` | Producer / shutdown | Wake one waiting thread or all of them |
| Predicate lambda | `wait(lock, pred)` | Guards against spurious wakeups |
| Thread pool | `threadPool` vector | Fixed set of threads reused across many tasks |
| `emplace_back` | Thread creation | In-place construction, avoids extra move |
| `reserve` | `threadPool.reserve(n)` | Pre-allocate to avoid reallocation during pushes |
| `explicit` | `TaskScheduler(int)` | Prevents implicit type conversions |
| `= delete` | Copy/move constructors | Prevents compiler-generated unsafe operations |
| `= default` | Virtual destructor | Uses compiler-generated default implementation |
| `mutable` | `queueMutex` | Allows mutation inside `const` methods |
| `const &` return | `getName()` | Returns reference without copying |
| `const` methods | Getters, queries | Promises not to modify observable state |
| Member initializer list | Both constructors | Direct member construction, avoids default + assign |
| `priority_queue` | `taskQueue` | Max-heap ordering by task priority |
| `exchange` | `stop()` | Atomic read-and-set, enables idempotent shutdown |
| `constexpr` | `POOL_SIZE` in main | Compile-time constant evaluation |
| Pointer-to-member-function | `&TaskScheduler::worker` | Passed to `std::thread` constructor with `this` |
