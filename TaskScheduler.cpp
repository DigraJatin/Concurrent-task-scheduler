#include "TaskScheduler.hpp"
#include "task.hpp"
#include <iostream>

TaskScheduler::TaskScheduler(int threadCount) : currentlyRunning(true) {
    // initialise the thread pool
    for (int i = 0; i < threadCount; ++i) {
        threadPool.emplace_back(&TaskScheduler::worker, this);
    }
}

// join for all threads
TaskScheduler::~TaskScheduler() {
    currentlyRunning = false;
    taskQueueStatus.notify_all(); // notify all the threads

    for (std::thread &curr : threadPool) {
        curr.join();
    }
}

// add to task queue
void TaskScheduler::addTask(std::unique_ptr<Task> task) {
    // lock the task queue
    std::lock_guard<std::mutex> lock(taskQueueMutex);
    taskQueue.push_back(std::move(task));
    taskQueueStatus.notify_one();
}

// worker
void TaskScheduler::worker() {
    while (currentlyRunning) {
        std::unique_ptr<Task> task = nullptr;
        {
            // synchronized block
            std::unique_lock<std::mutex> lock(taskQueueMutex);
            taskQueueStatus.wait(lock, [this]() { return !taskQueue.empty(); });

            if (!currentlyRunning) {
                return; // thread not running, exit-thread;
            }

            // fetch task from queue.
            task = std::move(taskQueue.front());
            taskQueue.pop_front();
        }

        // execute the task
        task->execute();
    }
}

void TaskScheduler::seeThreadsInfo() {
    for (std::thread &currThread : threadPool) {
        std::cout << "ThreadId=" << currThread.get_id() << '\n';
    }
}

void TaskScheduler::seeAllTasks() {
    for (auto it = taskQueue.begin(); it < taskQueue.end(); it += 1) {
        std::cout << it->get()->getName() << ' ';
    }
}
