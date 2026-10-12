#pragma once

#include "types.hpp"

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ps {

// Observer Pattern: ProcessManager publishes process lifecycle/output events
// without knowing who consumes them.
class IProcessObserver {
public:
    virtual ~IProcessObserver() = default;
    virtual void on_process_event(const ProcessEvent& event) = 0;
};

class ConsoleObserver final : public IProcessObserver {
public:
    void on_process_event(const ProcessEvent& event) override {
        std::lock_guard<std::mutex> lock(mu_);
        switch (event.kind) {
            case ProcessEvent::Kind::StateChanged:
                std::cout << "[state] " << event.process_name << ": "
                          << to_string(event.old_state) << " -> "
                          << to_string(event.new_state) << '\n';
                break;
            case ProcessEvent::Kind::Output:
                std::cout << "[out] " << event.process_name << ": "
                          << event.message;
                if (!event.message.empty() && event.message.back() != '\n') {
                    std::cout << '\n';
                }
                break;
            case ProcessEvent::Kind::RestartScheduled:
                std::cout << "[restart] " << event.process_name << ": "
                          << event.message << '\n';
                break;
            case ProcessEvent::Kind::Error:
                std::cerr << "[error] " << event.process_name << ": "
                          << event.message << '\n';
                break;
        }
    }

private:
    std::mutex mu_;
};

class MetricsObserver final : public IProcessObserver {
public:
    void on_process_event(const ProcessEvent& event) override {
        if (event.kind == ProcessEvent::Kind::RestartScheduled) {
            restart_events_.fetch_add(1, std::memory_order_relaxed);
        }
        if (event.kind == ProcessEvent::Kind::Error) {
            error_events_.fetch_add(1, std::memory_order_relaxed);
        }
        if (event.kind == ProcessEvent::Kind::StateChanged) {
            std::lock_guard<std::mutex> lock(mu_);
            last_state_[event.process_name] = event.new_state;
        }
    }

    std::uint64_t restart_events() const {
        return restart_events_.load(std::memory_order_relaxed);
    }

    std::uint64_t error_events() const {
        return error_events_.load(std::memory_order_relaxed);
    }

    void print() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::cout << "metrics: restarts=" << restart_events()
                  << " errors=" << error_events() << '\n';
        for (const auto& [name, state] : last_state_) {
            std::cout << "  " << name << " -> " << to_string(state) << '\n';
        }
    }

private:
    std::atomic<std::uint64_t> restart_events_{0};
    std::atomic<std::uint64_t> error_events_{0};
    mutable std::mutex mu_;
    std::unordered_map<std::string, ProcessState> last_state_;
};

}  // namespace ps
