#pragma once
#include "task.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class TaskScheduler {
  private:
    struct TaskCompare {
        bool operator()(const std::unique_ptr<Task> &a,
                        const std::unique_ptr<Task> &b) const {
            return *a < *b;
        }
    };

    std::priority_queue<std::unique_ptr<Task>,
                        std::vector<std::unique_ptr<Task>>, TaskCompare>
        taskQueue;

    std::vector<std::thread> threadPool;
    mutable std::mutex queueMutex;
    std::condition_variable queueCV;
    std::condition_variable emptyCV;
    std::atomic<bool> running;
    std::atomic<size_t> activeTasks{0};

    void worker();

  public:
    explicit TaskScheduler(int threadCount);
    ~TaskScheduler();

    TaskScheduler(const TaskScheduler &) = delete;
    TaskScheduler &operator=(const TaskScheduler &) = delete;
    TaskScheduler(TaskScheduler &&) = delete;
    TaskScheduler &operator=(TaskScheduler &&) = delete;

    void addTask(std::unique_ptr<Task> task);
    void stop();
    void waitUntilEmpty();
    size_t pendingCount() const;
    size_t poolSize() const;

    void printThreadsInfo() const;
    void printPendingTasks() const;
};
