#pragma once
#include <atomic>
#include <string>

enum class TaskPriority { Low = 0, Normal = 1, High = 2, Critical = 3 };

class Task {
  private:
    std::string taskName;
    int taskId;
    TaskPriority priority;

    static std::atomic<int> nextTaskId;

  public:
    Task(const std::string &name,
         TaskPriority priority = TaskPriority::Normal);
    virtual ~Task() = default;

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    virtual void execute() = 0;

    const std::string &getName() const;
    int getId() const;
    TaskPriority getPriority() const;

    bool operator<(const Task &other) const;
};
