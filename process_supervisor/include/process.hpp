#pragma once

#include "restart_policy.hpp"
#include "types.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ps {

class ManagedProcess {
public:
    using EventSink = std::function<void(const ProcessEvent&)>;
    using ExitSink = std::function<void(const std::string&, int, bool)>;

    ManagedProcess(ProcessSpec spec,
                   std::shared_ptr<IRestartPolicy> restart_policy,
                   EventSink event_sink,
                   ExitSink exit_sink);
    ~ManagedProcess();

    ManagedProcess(const ManagedProcess&) = delete;
    ManagedProcess& operator=(const ManagedProcess&) = delete;

    bool start();
    bool request_stop(std::chrono::milliseconds grace);
    bool force_kill();
    void join_background_threads();

    ProcessSnapshot snapshot() const;
    const ProcessSpec& spec() const { return spec_; }
    std::shared_ptr<IRestartPolicy> restart_policy() const { return restart_policy_; }
    void increment_restart_count();
    std::uint64_t restart_count() const;

private:
    bool transition(ProcessState expected, ProcessState next);
    void set_state(ProcessState next);
    void emit(ProcessEvent event) const;
    void read_output_loop(int fd);
    void wait_loop(int pid);
    static std::vector<char*> build_argv(std::vector<std::string>& storage);

    ProcessSpec spec_;
    std::shared_ptr<IRestartPolicy> restart_policy_;
    EventSink event_sink_;
    ExitSink exit_sink_;

    mutable std::mutex mu_;
    ProcessState state_{ProcessState::Stopped};
    int pid_{-1};
    int exit_code_{0};
    std::uint64_t restart_count_{0};
    std::chrono::steady_clock::time_point started_at_{};

    std::thread output_thread_;
    std::thread wait_thread_;
    std::atomic<bool> stop_requested_{false};
};

}  // namespace ps
