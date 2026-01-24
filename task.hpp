// Defines Tasks
//
// 1. Creation of Task
// 2. Add Task to Queue
#pragma once
#include <string>

class Task {
  private:
    std::string taskName;
    int taskId;

    static int nextTaskId;
    static int generateUniqueTaskId();

  public:
    // Constructor
    Task(std::string name);

    virtual void execute() = 0; // user defined

    std::string getName() const;
    int getId() const;

    // virtual destructor
    ~Task() {}
};
