#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace js {

enum class TaskStatus {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled,
};

inline const char* to_string(TaskStatus status) {
    switch (status) {
        case TaskStatus::Queued: return "QUEUED";
        case TaskStatus::Running: return "RUNNING";
        case TaskStatus::Completed: return "COMPLETED";
        case TaskStatus::Failed: return "FAILED";
        case TaskStatus::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

enum class SchedulerState {
    Created,
    Running,
    Paused,
    Stopping,
    Stopped,
};

inline const char* to_string(SchedulerState state) {
    switch (state) {
        case SchedulerState::Created: return "CREATED";
        case SchedulerState::Running: return "RUNNING";
        case SchedulerState::Paused: return "PAUSED";
        case SchedulerState::Stopping: return "STOPPING";
        case SchedulerState::Stopped: return "STOPPED";
    }
    return "UNKNOWN";
}

struct TaskInfo {
    std::uint64_t id{0};
    std::string name;
    int priority{0};
    TaskStatus status{TaskStatus::Queued};
    std::size_t worker_index{static_cast<std::size_t>(-1)};
};

struct SchedulerEvent {
    enum class Kind {
        TaskSubmitted,
        TaskStarted,
        TaskCompleted,
        TaskFailed,
        TaskCancelled,
        SchedulerStateChanged,
    };

    Kind kind{Kind::TaskSubmitted};
    TaskInfo task;
    SchedulerState old_state{SchedulerState::Created};
    SchedulerState new_state{SchedulerState::Created};
    std::string message;
    std::chrono::steady_clock::time_point at{std::chrono::steady_clock::now()};
};

}  // namespace js
