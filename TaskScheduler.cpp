#include "TaskScheduler.hpp"
#include <iostream>

TaskScheduler::TaskScheduler(int threadCount) : running(true) {
    threadPool.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i) {
        threadPool.emplace_back(&TaskScheduler::worker, this);
    }
}

TaskScheduler::~TaskScheduler() { stop(); }

void TaskScheduler::stop() {
    if (!running.exchange(false)) return;

    queueCV.notify_all();
    for (auto &t : threadPool) {
        if (t.joinable()) t.join();
    }
}

void TaskScheduler::addTask(std::unique_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCV.notify_one();
}

void TaskScheduler::worker() {
    while (true) {
        std::unique_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock,
                         [this] { return !taskQueue.empty() || !running; });

            if (!running && taskQueue.empty()) return;

            task = std::move(const_cast<std::unique_ptr<Task> &>(
                taskQueue.top()));
            taskQueue.pop();
            ++activeTasks;
        }

        task->execute();

        --activeTasks;
        emptyCV.notify_all();
    }
}

void TaskScheduler::waitUntilEmpty() {
    std::unique_lock<std::mutex> lock(queueMutex);
    emptyCV.wait(lock,
                 [this] { return taskQueue.empty() && activeTasks == 0; });
}

size_t TaskScheduler::pendingCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
}

size_t TaskScheduler::poolSize() const { return threadPool.size(); }

void TaskScheduler::printThreadsInfo() const {
    for (const auto &t : threadPool) {
        std::cout << "  Thread ID: " << t.get_id() << '\n';
    }
}

void TaskScheduler::printPendingTasks() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::cout << "  Pending tasks: " << taskQueue.size() << '\n';
    std::cout << "  Active tasks:  " << activeTasks.load() << '\n';
}
