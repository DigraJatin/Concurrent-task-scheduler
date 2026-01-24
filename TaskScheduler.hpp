#pragma once
#include "task.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

class TaskScheduler {
  private:
    std::deque<std::unique_ptr<Task>> taskQueue;
    std::vector<std::thread> threadPool;
    std::mutex taskQueueMutex;
    std::condition_variable taskQueueStatus;
    bool currentlyRunning; // status of threads

    void worker();

  public:
    // RAII Style
    TaskScheduler(int threadCount);
    ~TaskScheduler();

    void addTask(std::unique_ptr<Task> task);

    void seeThreadsInfo();
    void seeAllTasks();
};
