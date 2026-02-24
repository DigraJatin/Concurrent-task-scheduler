#include "task.hpp"

std::atomic<int> Task::nextTaskId{0};

Task::Task(const std::string &name, TaskPriority prio)
    : taskName(name), taskId(++nextTaskId), priority(prio) {}

const std::string &Task::getName() const { return taskName; }
int Task::getId() const { return taskId; }
TaskPriority Task::getPriority() const { return priority; }

bool Task::operator<(const Task &other) const {
    return priority < other.priority;
}
