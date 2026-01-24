#include "task.hpp"

int Task::nextTaskId = 0;

// Constructor
Task::Task(std::string tname)
    : taskName(tname), taskId(generateUniqueTaskId()) {}

int Task::generateUniqueTaskId() { return ++nextTaskId; }

// getters
int Task::getId() const { return taskId; }

std::string Task::getName() const { return taskName; }

