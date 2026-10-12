#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ps {

enum class ProcessState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Exited,
    Failed,
};

inline const char* to_string(ProcessState state) {
    switch (state) {
        case ProcessState::Stopped: return "STOPPED";
        case ProcessState::Starting: return "STARTING";
        case ProcessState::Running: return "RUNNING";
        case ProcessState::Stopping: return "STOPPING";
        case ProcessState::Exited: return "EXITED";
        case ProcessState::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

struct ProcessSpec {
    std::string name;
    std::vector<std::string> argv;
    std::optional<std::string> working_directory;
    bool capture_output{true};
};

struct ProcessSnapshot {
    std::string name;
    ProcessState state{ProcessState::Stopped};
    int pid{-1};
    int exit_code{0};
    std::uint64_t restart_count{0};
    std::chrono::steady_clock::time_point started_at{};
};

struct ProcessEvent {
    enum class Kind {
        StateChanged,
        Output,
        RestartScheduled,
        Error,
    };

    Kind kind{Kind::StateChanged};
    std::string process_name;
    ProcessState old_state{ProcessState::Stopped};
    ProcessState new_state{ProcessState::Stopped};
    std::string message;
};

}  // namespace ps
