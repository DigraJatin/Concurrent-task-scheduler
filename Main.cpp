#include "TaskScheduler.hpp"
#include "task.hpp"
#include <iostream>

struct MyTask1 : public Task {
    MyTask1(const std::string &name) : Task(name) {}

    void execute() override {
        std::cout << "[MyTask1]Task=" << getName() << " executed.\n";
    }
};

struct MyTask2 : public Task {
    MyTask2(const std::string &name) : Task(name) {}

    void execute() override {
        std::cout << "[MyTask2]Task=" << getName() << " executed.\n";
    }
};

int main() {
    TaskScheduler scheduler(4); // 4 threads in the scheduler

    scheduler.seeThreadsInfo(); // will print the thread ids of all the threads
                                // in thread pool

    // create tasks
    for (int i = 0; i < 5; i++) {
        auto task = std::make_unique<MyTask1>("My Task + " + std::to_string(i));
        scheduler.addTask(std::move(task));
    }

    // print all the tasks in the queue
    // scheduler.seeAllTasks();

    return 0;
}
