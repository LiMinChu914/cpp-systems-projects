#include "process.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

namespace ps {

ManagedProcess::ManagedProcess(ProcessSpec spec,
                               std::shared_ptr<IRestartPolicy> restart_policy,
                               EventSink event_sink,
                               ExitSink exit_sink)
    : spec_(std::move(spec)),
      restart_policy_(std::move(restart_policy)),
      event_sink_(std::move(event_sink)),
      exit_sink_(std::move(exit_sink)) {}

ManagedProcess::~ManagedProcess() {
    request_stop(std::chrono::milliseconds(250));
    join_background_threads();
}

bool ManagedProcess::transition(ProcessState expected, ProcessState next) {
    ProcessState old;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ != expected) {
            return false;
        }
        old = state_;
        state_ = next;
    }
    emit({ProcessEvent::Kind::StateChanged, spec_.name, old, next, {}});
    return true;
}

void ManagedProcess::set_state(ProcessState next) {
    ProcessState old;
    {
        std::lock_guard<std::mutex> lock(mu_);
        old = state_;
        state_ = next;
    }
    if (old != next) {
        emit({ProcessEvent::Kind::StateChanged, spec_.name, old, next, {}});
    }
}

void ManagedProcess::emit(ProcessEvent event) const {
    if (event_sink_) {
        event_sink_(event);
    }
}

std::vector<char*> ManagedProcess::build_argv(std::vector<std::string>& storage) {
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (auto& item : storage) {
        argv.push_back(item.data());
    }
    argv.push_back(nullptr);
    return argv;
}

bool ManagedProcess::start() {
    join_background_threads();

    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == ProcessState::Running ||
            state_ == ProcessState::Starting ||
            state_ == ProcessState::Stopping) {
            return false;
        }
    }

    set_state(ProcessState::Starting);
    stop_requested_.store(false, std::memory_order_relaxed);

    if (spec_.argv.empty()) {
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Starting,
              ProcessState::Failed,
              "empty argv"});
        set_state(ProcessState::Failed);
        return false;
    }

    int pipefd[2] = {-1, -1};
    if (spec_.capture_output && pipe(pipefd) == -1) {
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Starting,
              ProcessState::Failed,
              std::string("pipe failed: ") + std::strerror(errno)});
        set_state(ProcessState::Failed);
        return false;
    }

    const pid_t child = fork();
    if (child == -1) {
        if (pipefd[0] != -1) close(pipefd[0]);
        if (pipefd[1] != -1) close(pipefd[1]);
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Starting,
              ProcessState::Failed,
              std::string("fork failed: ") + std::strerror(errno)});
        set_state(ProcessState::Failed);
        return false;
    }

    if (child == 0) {
        // Child: do only minimal async-signal-safe setup before exec.
        if (spec_.capture_output) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }

        if (spec_.working_directory.has_value()) {
            if (chdir(spec_.working_directory->c_str()) == -1) {
                _exit(126);
            }
        }

        std::vector<std::string> storage = spec_.argv;
        auto argv = build_argv(storage);
        execvp(argv[0], argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    // Parent.
    if (spec_.capture_output) {
        close(pipefd[1]);
    }

    {
        std::lock_guard<std::mutex> lock(mu_);
        pid_ = static_cast<int>(child);
        started_at_ = std::chrono::steady_clock::now();
        exit_code_ = 0;
    }
    set_state(ProcessState::Running);

    if (spec_.capture_output) {
        output_thread_ = std::thread(&ManagedProcess::read_output_loop, this, pipefd[0]);
    }
    wait_thread_ = std::thread(&ManagedProcess::wait_loop, this, static_cast<int>(child));
    return true;
}

void ManagedProcess::read_output_loop(int fd) {
    std::array<char, 512> buffer{};
    std::string pending;

    while (true) {
        const ssize_t count = read(fd, buffer.data(), buffer.size());
        if (count > 0) {
            pending.append(buffer.data(), static_cast<std::size_t>(count));
            std::size_t pos = 0;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos + 1);
                pending.erase(0, pos + 1);
                emit({ProcessEvent::Kind::Output,
                      spec_.name,
                      ProcessState::Running,
                      ProcessState::Running,
                      line});
            }
            continue;
        }
        if (count == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Running,
              ProcessState::Running,
              std::string("read output failed: ") + std::strerror(errno)});
        break;
    }

    if (!pending.empty()) {
        emit({ProcessEvent::Kind::Output,
              spec_.name,
              ProcessState::Running,
              ProcessState::Running,
              pending});
    }
    close(fd);
}

void ManagedProcess::wait_loop(int pid) {
    int status = 0;
    pid_t result;
    do {
        result = waitpid(static_cast<pid_t>(pid), &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Running,
              ProcessState::Failed,
              std::string("waitpid failed: ") + std::strerror(errno)});
        set_state(ProcessState::Failed);
        return;
    }

    int code = 0;
    bool signaled = false;
    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        signaled = true;
        code = 128 + WTERMSIG(status);
    }

    {
        std::lock_guard<std::mutex> lock(mu_);
        exit_code_ = code;
        pid_ = -1;
    }

    const bool requested = stop_requested_.load(std::memory_order_relaxed);
    if (requested) {
        set_state(ProcessState::Stopped);
    } else if (!signaled && code == 0) {
        set_state(ProcessState::Exited);
    } else {
        set_state(ProcessState::Failed);
    }

    if (exit_sink_) {
        exit_sink_(spec_.name, code, signaled);
    }
}

bool ManagedProcess::request_stop(std::chrono::milliseconds grace) {
    int pid = -1;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (pid_ <= 0) {
            return false;
        }
        pid = pid_;
    }

    stop_requested_.store(true, std::memory_order_relaxed);
    set_state(ProcessState::Stopping);

    if (::kill(static_cast<pid_t>(pid), SIGTERM) == -1 && errno != ESRCH) {
        emit({ProcessEvent::Kind::Error,
              spec_.name,
              ProcessState::Stopping,
              ProcessState::Stopping,
              std::string("SIGTERM failed: ") + std::strerror(errno)});
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + grace;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (pid_ <= 0) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return force_kill();
}

bool ManagedProcess::force_kill() {
    int pid = -1;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (pid_ <= 0) {
            return false;
        }
        pid = pid_;
    }
    stop_requested_.store(true, std::memory_order_relaxed);
    return ::kill(static_cast<pid_t>(pid), SIGKILL) == 0 || errno == ESRCH;
}

void ManagedProcess::join_background_threads() {
    if (wait_thread_.joinable() && wait_thread_.get_id() != std::this_thread::get_id()) {
        wait_thread_.join();
    }
    if (output_thread_.joinable() && output_thread_.get_id() != std::this_thread::get_id()) {
        output_thread_.join();
    }
}

ProcessSnapshot ManagedProcess::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return {spec_.name, state_, pid_, exit_code_, restart_count_, started_at_};
}

void ManagedProcess::increment_restart_count() {
    std::lock_guard<std::mutex> lock(mu_);
    ++restart_count_;
}

std::uint64_t ManagedProcess::restart_count() const {
    std::lock_guard<std::mutex> lock(mu_);
    return restart_count_;
}

}  // namespace ps
