
# Concurrent Task Execution

This repository houses a C++ implementation of a Task Scheduler, leveraging threads to facilitate concurrent task execution. It provides a foundational framework for scheduling and executing tasks in a multi-threaded environment, demonstrating the effective use of threads, mutexes, and condition variables in C++.

## Features

- **Task Creation and Execution:** Define tasks with specific functionalities that can be executed concurrently.
- **Dynamic Task Scheduling:** Tasks are queued and scheduled dynamically across multiple threads for execution.
- **Thread Pool Management:** Utilizes a pool of threads to execute tasks concurrently, optimizing resource use and execution time.
- **Concurrency Control:** Ensures thread safety through mutexes and condition variables, managing access to the task queue.

## Getting Started

### Prerequisites

- A modern C++ compiler that supports C++11 (or later).
- Basic understanding of C++ threading and object-oriented programming.

### File Structure

- `task.hpp/cpp`: Defines the `Task` class for task creation and management.
- `TaskScheduler.hpp/cpp`: Implements the Task Scheduler, managing the task queue and thread pool.
- `main.cpp`: Demonstrates the usage of the Task Scheduler with example tasks.
- `compile.sh`: A shell script to compile the project using `g++` and link the necessary files.

### Compilation and Execution

1. **Compile the Project:**

   Use the provided `compile.sh` script to compile the project. This script compiles `main.cpp`, `task.cpp`, and `TaskScheduler.cpp` with the `-lpthread` flag to enable POSIX threading support.

   ```bash
   ./compile.sh
   ```

2. **Run the Executable:**

   After compilation, run the generated executable to start the task scheduler and execute the tasks:

   ```bash
   ./Executable
   ```

## Implementation Note

- Ensure that `task.hpp/cpp`, `TaskScheduler.hpp/cpp`, and your `main.cpp` are in the same directory. The `TaskScheduler` relies on the functionality provided by `task.hpp/cpp` for task management.
- The `Task` class is designed to be extended for specific tasks. Each task must implement the `execute()` method, defining the task's behavior.
- The `TaskScheduler` class manages a pool of worker threads that execute tasks from a queue. It demonstrates basic concurrency control mechanisms, such as mutexes and condition variables, to safely manage access to the shared task queue.

## Usage

To use the Task Scheduler in your project, define your tasks by extending the `Task` class and implementing the `execute()` method. Add tasks to the scheduler using `addTask(std::unique_ptr<Task> task)`. The scheduler automatically handles the execution of tasks across its thread pool.

## Contribution

Contributions to improve the Task Scheduler or extend its functionality are welcome. Please feel free to fork the repository, make changes, and submit a pull request.
