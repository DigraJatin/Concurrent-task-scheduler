#include "TaskScheduler.hpp"
#include <chrono>
#include <iostream>
#include <thread>

class ComputeTask : public Task {
  public:
    ComputeTask(const std::string &name, TaskPriority p = TaskPriority::Normal)
        : Task(name, p) {}

    void execute() override {
        std::cout << "[Compute] id=" << getId() << " name=\"" << getName()
                  << "\" on thread " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

class IOTask : public Task {
  public:
    IOTask(const std::string &name, TaskPriority p = TaskPriority::Normal)
        : Task(name, p) {}

    void execute() override {
        std::cout << "[IO]      id=" << getId() << " name=\"" << getName()
                  << "\" on thread " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
};

int main() {
    constexpr int POOL_SIZE = 4;
    TaskScheduler scheduler(POOL_SIZE);

    std::cout << "=== Thread Pool (" << scheduler.poolSize()
              << " threads) ===\n";
    scheduler.printThreadsInfo();
    std::cout << '\n';

    std::cout << "=== Submitting tasks ===\n";

    for (int i = 0; i < 4; ++i) {
        scheduler.addTask(std::make_unique<ComputeTask>(
            "compute-" + std::to_string(i), TaskPriority::Normal));
    }

    scheduler.addTask(
        std::make_unique<IOTask>("critical-io", TaskPriority::Critical));

    scheduler.addTask(
        std::make_unique<ComputeTask>("low-compute", TaskPriority::Low));

    scheduler.addTask(
        std::make_unique<IOTask>("high-io", TaskPriority::High));

    std::cout << "Pending after submit: " << scheduler.pendingCount() << '\n';

    std::cout << "\n=== Waiting for all tasks to finish ===\n";
    scheduler.waitUntilEmpty();

    std::cout << "\n=== All tasks completed ===\n";
    scheduler.printPendingTasks();

    return 0;
}
